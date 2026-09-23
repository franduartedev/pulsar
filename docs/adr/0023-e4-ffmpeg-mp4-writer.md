# ADR 0023 — E4 FFmpeg H.264/AAC MP4 writer

## Estado

Aceptado para E4. E5 conserva la responsabilidad de job, cancelación,
progreso y publicación segura.

## Contrato

`ExportMediaWriter` es una frontera neutral de application. Recibe únicamente
`OfflineVideoFrame` RGBA8888 y `OfflineAudioBlock` Float32 interleaved stereo a
48 kHz. El adapter FFmpeg implementa esa frontera; application no conoce
`AVFrame`, `AVPacket`, codecs ni muxers.

El perfil único Beta es MP4, H.264 mediante `libx264`, `yuv420p`, CFR con el
time base racional `D/N`, AAC-LC a 48 kHz estéreo y 192 kb/s. La disponibilidad
de `libx264`, AAC y MP4 se valida explícitamente; no existe fallback de codec.

## Video

Se reutilizan un `SwsContext`, un `AVFrame` YUV420P y un `AVPacket`. El PTS de
cada frame es exactamente su índice de `ExportPlan`; sólo se hace rescale
racional al time base del stream. Se selecciona `preset=medium`, `crf=23`,
GOP aproximado de dos segundos y `max_b_frames=0`. Desactivar B-frames es una
decisión Beta deliberada: mantiene DTS/PTS monotónicos y hace auditable la
duración CFR sin alterar la verdad editorial.

## Audio

Los bloques deben llegar contiguos desde sample index cero hasta
`audio_sample_count`. El writer rechaza saltos, formato incorrecto, NaN/Inf y
overflow. Antes de AAC aplica únicamente hard clip determinista a `[-1,+1]`;
no aplica normalización, limiter ni AGC. El FIFO conserva sólo bloques parciales
y frames AAC completos; el frame AAC nominal de esta instalación es 1024
muestras. El último frame parcial se envía con su tamaño real y luego se hace
flush explícito.

El encoder AAC nativo de la instalación acepta `FLTP`; el adapter convierte
interleaved Float32 a planar sin cambiar sample rate ni canales. El MP4
resultante validado por ffprobe expresa `initial_padding=0` y duración lógica
de audio exacta para los fixtures Beta. El decoder AAC puede entregar samples
de cola física adicionales al hacer flush (256 samples en el decoder FFmpeg de
esta instalación), aunque el stream y los packet durations declaran exactamente
`audio_sample_count`; por eso la validación de alineación usa la duración
lógica del stream/packets y no cuenta ciegamente el padding físico posterior.
E5 no debe inventar otra corrección de priming.

## Muxing y ownership

Los packets se rescalean desde los time bases de codec al time base de stream y
se escriben con `av_interleaved_write_frame`. El caller alimenta el writer
incrementalmente; E4 no almacena todos los frames, PCM ni packets. `finish()`
flushea ambos encoders, escribe el trailer y cierra el output. Un fallo lanza
un error tipado y deja el writer destruible. El writer no decide nombres
`.part`, rename atómico, fsync ni reemplazo de destinos: son responsabilidades
de E5.

## Portabilidad

El seam neutral permite reemplazar el backend FFmpeg por libx264/AAC de otra
plataforma, MediaCodec o Media Foundation sin modificar `ExportPlan`, E2 ni
E3. El código nuevo no usa POSIX, shell, QProcess, paths Linux ni lifecycle de
desktop.
