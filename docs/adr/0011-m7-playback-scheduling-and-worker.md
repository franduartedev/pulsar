# ADR 0011: M7 Playback Scheduling and Worker

## Estado

Aceptado

## Contexto

M7 completa playback headless. No introduce UI, audio, pixels, Vulkan, hardware decode, proxies, effects ni caches generales. El objetivo es coordinar tiempo de playback, selección temporal de muestras, seek de presentación y trabajo de medios sin bloquear el hilo de control ni filtrar FFmpeg hacia application.

ADR 0010 separa monotonic time, playback position, timeline time, source media time y stream timestamp. M7 conserva esa separación.

## Decisión

Application define contratos backend-neutral:

- `VideoSourceDescriptor`: origen de stream opcional, time base opcional, duración reportada opcional y seekability. La duración reportada no es el final exacto de presentación.
- `VideoSample`: ordinal estable, presentation timestamp opcional y duración opcional. No contiene pixels, `AVFrame`, buffers ni handles GPU.
- `VideoReadOutcome` y `VideoSeekOutcome`: distinguen sample, EOF, cancelación y error. No se usa `optional` para colapsar estados distintos.
- `PlaybackMediaWorker`: port asíncrono por generaciones que acepta read y presentation seek y devuelve eventos pequeños.

`SingleStreamPlaybackClockMap` implementa el mapping explícito de M7 fuente única:

```text
playback/source-relative time -> absolute stream timestamp
```

El mapping usa el origen de stream resuelto una vez al abrir/describir la fuente. No renormaliza cada seek y soporta non-zero start y timestamps negativos si el container/decoder los exponen.

`PresentationScheduler` decide qué muestra corresponde para una `PlaybackPosition`. No decodifica, no duerme, no controla transporte y no conoce FFmpeg. Mantiene estado acotado: current + lookahead + EOF. La fuente falsa de tests puede usar vectores, pero el scheduler productivo no guarda el video completo.

Reglas de scheduling:

- Con PTS estrictamente crecientes y sin duración explícita: sample_i ocupa `[PTS_i, PTS_(i+1))`.
- Con duración explícita válida: sample_i ocupa `[PTS_i, PTS_i + duration)`.
- Si duración explícita termina antes del próximo PTS, existe gap.
- Si falta timestamp, es `TemporalError`.
- Si timestamps decrecen, es `TemporalError`.
- Timestamps duplicados se resuelven de forma determinista: antes de presentar ese instante, la última muestra reemplaza a la anterior; si llega después de presentada, es error temporal.
- Si falta lookahead para decidir un target posterior al PTS actual, el resultado es `Starved`.
- El último sample solo termina con duración explícita o final autorizado conocido. Si el final es desconocido, se reporta explícitamente como tal.

El adapter FFmpeg agrega `seek_to_presentation_timestamp()` sin modificar `seek_to_stream_timestamp()` de M6. La nueva operación busca hacia atrás, flushea decoder, decodifica en orden de presentación, conserva el último frame con PTS `<= target`, avanza hasta obtener el próximo PTS `> target` o EOF, devuelve el candidato y retiene el próximo frame como lookahead para continuidad secuencial.

`FFmpegPlaybackWorker` vive en el adapter y es dueño exclusivo de `FFmpegVideoDecodeSession`. Solo su thread llama decode, seek y destrucción de esa sesión. El hilo de control manda requests y recibe eventos. Hay un worker por fuente de playback en M7.

Generations identifican operaciones. `PlaybackSession` descarta samples, EOF, errors, cancellations y seek completions que no coinciden con la generación vigente. Seek coalescing inicial: un seek activo en el worker y un seek pendiente reemplazable. Colas de eventos son acotadas; si se llena la cola, se descarta el evento más viejo y se registra en stats.

Cancelación es cooperativa: se cancelan requests pendientes y se descartan resultados obsoletos por generation. No se promete cancelar instantáneamente una llamada FFmpeg en curso, no se matan threads y no hay detached threads.

`PlaybackSession` separa intención y disponibilidad:

```text
Intent:       Paused | Playing
Availability: Empty | Preparing | Ready | Seeking | Ended | Error
```

Seek mientras reproduce pausa el transporte en el target lógico, resuelve la presentación, reancla después de preparar el target y continúa si la intención sigue siendo `Playing`. Pause durante seek conserva el target y no arranca al completar. Play durante seek arranca al completar si el seek vigente queda preparado. Play desde Ended no reinicia implícitamente; un seek posterior revive la sesión si es válido.

## Consecuencias

M7 produce decisiones temporales verificables, no pixels. La futura arquitectura de frame podrá añadir identity/time, image descriptor, storage ownership y synchronization sin reemplazar el scheduler temporal.

Application sigue sin FFmpeg. Domain sigue libre de application, FFmpeg, UI, Vulkan, SQLite y APIs Linux.

La instrumentación inicial registra latencias de open/read/seek del worker, frames decodificados por seek, high-water de cola, eventos descartados por capacidad, requests aceptados y eventos obsoletos descartados. No hay umbrales de performance todavía.
