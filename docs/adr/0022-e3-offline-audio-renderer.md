# ADR 0022 — E3 offline audio renderer

## Estado

Aceptado para E3. Este ADR describe el contrato previo a AAC/muxing; no es un
contrato de preview.

## Decisiones normativas

- `ExportPlan` es la única verdad editorial. El renderer produce bloques
  interleaved `Float32`, estéreo, a 48 kHz, con índices globales de muestra.
- La muestra `k` representa `[k/48000,(k+1)/48000)`. Los bloques terminan
  exactamente en `ExportPlan::audio_sample_count()`. No se acumula un paso
  floating-point: cada posición se deriva nuevamente desde el índice entero y
  los helpers racionales de E1.
- Los límites de clips son half-open. El contributor lookup conserva el orden
  estable de tracks. El audio embedded se toma solamente del master video y el
  audio independiente se mezcla después en orden editorial.
- Mono se duplica a L/R; stereo conserva sus canales; más de dos canales falla
  con `UnsupportedChannels`. No hay normalización, AGC, limiter ni clipping en
  E3. La suma usa acumulación `long double` y se convierte a Float32 al final.
- El remuestreo se define por la posición racional de cada muestra de salida:
  se toma el sample fuente en `floor(position)` y el siguiente, con la
  fracción racional entre ambos. Esto cubre 44.1, 48 y 96 kHz sin `step +=`
  floating-point ni estado dependiente del tamaño de bloque.
- `AVStream::start_time` se conserva como origen raw en el adapter. El
  renderer resta ese origen antes de comparar contra `source_range`.
- Un gap temporal positivo dentro del source produce silencio; una
  superposición se recorta determinísticamente por el final ya aceptado; un
  bloque sin PTS falla. EOF antes del `source_range` prometido falla y nunca se
  convierte en silencio. Un gap editorial sí es silencio válido.
- Un source activo mantiene decoder y reservoir entre bloques. El renderer no
  hace seek por muestra, no crea un decoder por bloque y no acumula PCM del
  proyecto. El límite de salida por defecto es 4096 frames y el reservoir se
  mantiene acotado.

## Fronteras

`application/export` conoce sólo `OfflineAudioSource` y su factory neutral.
`media/ffmpeg` adapta `FFmpegAudioDecodeSession`, mapea errores y resuelve un
locator lógico mediante una función inyectable. La resolución de
`pulsar-sound:<id>` no vive en application ni depende de paths Linux.

`AudioMixSession`, dispositivos, clocks, starvation/recovery y
`StreamingAudioConverter` pertenecen al preview y no se reutilizan.

## Consecuencias

El renderer es determinista y testeable con fuentes fake sin Qt/FFmpeg. La
interpolación directa hace explícito el coste de CPU de E3; futuras mejoras
pueden reemplazar el adapter/resampler sin cambiar la verdad editorial. La
selección de codec, AAC, muxing y política de fallo del archivo final quedan
para E4.
