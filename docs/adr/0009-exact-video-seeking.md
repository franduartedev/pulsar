# ADR 0009: Exact Video Seeking

## Estado

Aceptado

## Contexto

M6 necesita responder "dame el frame correspondiente a este instante del medio" sobre la sesión de decoding secuencial de M5. No introduce playback, cache, índice de keyframes, RGB, threading, audio ni hardware decoding.

## Decisión

`FFmpegVideoDecodeSession` expone `seek_to_stream_timestamp(MediaTime)`. El target es un timestamp absoluto de presentación en la coordenada temporal del stream de video seleccionado, no tiempo relativo de clip ni tiempo de UI. El adaptador FFmpeg no normaliza `AVStream::start_time` a cero: si FFmpeg informa start time, PULSAR lo expone como metadata diagnóstica; si es desconocido, queda ausente. Las capas futuras de clip/playback deben mapear su tiempo relativo de source/clip a timestamps absolutos del stream antes de llamar a esta API.

La política de resultado es "primer frame con presentation timestamp mayor o igual al target". Frames sin `best_effort_timestamp` no sirven para decidir que el target fue alcanzado; se descartan lógicamente para la comparación y el decode continúa. Si se llega a EOF sin encontrar un frame adecuado, el resultado contiene `std::nullopt`.

El target `MediaTime` se convierte al time base del stream con aritmética entera exacta. Cuando el target cae entre ticks del stream, se redondea hacia abajo antes de llamar a FFmpeg. Ese redondeo favorece no saltarse el frame requerido, porque el seek físico se hace hacia un punto decodificable anterior y la selección final se decide con PTS de presentación del frame decodificado.

La estrategia usa `av_seek_frame(format_context, stream_index, timestamp, AVSEEK_FLAG_BACKWARD)`. No se usa `AVSEEK_FLAG_ANY` por defecto, para preservar reconstrucción correcta en codecs inter-frame. Después de un seek exitoso se llama a `avcodec_flush_buffers`, se descarta cualquier packet pendiente, se limpia el `AVFrame` reusable y el estado lógico vuelve a `Reading`.

La operación reutiliza la maquinaria validada en M5: después del seek físico, `decode_next()` avanza hasta alcanzar la política temporal. El state machine sigue siendo `Reading`, `Draining`, `Finished`; seek es una operación síncrona que puede llevar cualquier estado válido de vuelta a `Reading`.

Un seek exitoso desde EOF revive la sesión. Múltiples seeks consecutivos reutilizan el mismo demuxer y decoder. Un seek fallido descarta packet/frame pendientes, flushea el decoder y deja la sesión en EOF lógico. La sesión sigue destruible y puede recibir otro seek posterior, pero no promete strong guarantee de continuidad desde la posición anterior porque FFmpeg puede haber mutado estado interno antes de fallar.

## Consecuencias

M6 es frame-accurate en el sentido editorial de PULSAR: puede buscar físicamente a un keyframe anterior, decodificar dependencias y devolver el primer frame que cumple la política temporal. No promete salto físico directo a cualquier frame comprimido.

Timestamps negativos no se rechazan automáticamente porque la API trabaja en coordenadas absolutas del stream. Targets que no pueden convertirse de forma segura al rango de timestamps FFmpeg producen `InvalidSeekTarget`.

No hay cache ni índice propio de keyframes. La latencia depende de GOP, demuxer, codec y distancia desde el punto decodificable elegido por FFmpeg.

Las pruebas automáticas cubren seek/reset en una fixture simple. La validación frame-accurate con H.264+B-frames se ejecuta manualmente con un clip sintético generado por FFmpeg CLI para no convertir el encoder externo en dependencia de CTest.
