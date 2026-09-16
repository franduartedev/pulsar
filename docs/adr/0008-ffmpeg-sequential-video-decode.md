# ADR 0008: FFmpeg Sequential Video Decode

## Estado

Aceptado

## Contexto

M5 necesita obtener frames de video reales, en orden de presentación, con timestamps exactos. Todavía no está decidido el modelo definitivo de frame de PULSAR, ni la relación futura con Vulkan, libplacebo, hardware decoding, cache o memoria GPU.

## Decisión

Se agrega `FFmpegVideoDecodeSession` dentro de `pulsar::media::ffmpeg` como una sesión stateful de decoding secuencial. La sesión se abre con un path local y un índice de stream de video explícito. No se crea todavía un port abstracto en application ni un modelo global como `VideoFrame`, `FrameSurface`, `CPUFrame` o `GPUFrame`.

`decode_next()` devuelve `std::optional<DecodedVideoFrameInfo>`. El value type contiene solamente metadata segura: stream index, dimensiones reales del frame, nombre diagnóstico de pixel format y presentation timestamp opcional. No expone `AVFrame`, punteros, buffers, `data[]`, `linesize[]` ni ownership de memoria del decoder.

El timestamp sale de `AVFrame::best_effort_timestamp` cuando FFmpeg lo informa. Se convierte exactamente a `pulsar::domain::MediaTime` usando el `time_base` del stream. No se reconstruyen PTS desde FPS nominal, frame index ni `double`.

La sesión usa la API moderna `avcodec_send_packet` / `avcodec_receive_frame`, con flujo receive-first. Al EOF de demux se envía un packet NULL una única vez, se drena el decoder hasta `AVERROR_EOF`, y las llamadas posteriores a `decode_next()` devuelven EOF estable.

La validación de path, apertura de input, lectura de stream info, conversión de errores FFmpeg y RAII de `AVFormatContext` se comparten en helpers privados del adapter. Los tipos FFmpeg siguen confinados al target `pulsar_media_ffmpeg`.

La integración usa RAII para:

* `AVFormatContext`, creado por `avformat_open_input` y cerrado con `avformat_close_input`;
* `AVCodecContext`, creado por `avcodec_alloc_context3` y liberado con `avcodec_free_context`;
* `AVPacket`, creado por `av_packet_alloc`, liberado con `av_packet_free` y unreferenced después de cada `av_read_frame`;
* `AVFrame`, creado por `av_frame_alloc`, liberado con `av_frame_free` y unreferenced después de copiar metadata.

Si construir `DecodedVideoFrameInfo` lanza por allocation, la sesión mantiene recursos destruidibles sin UB ni leaks; no promete repetir exactamente el mismo frame tras la excepción.

## Consecuencias

M5 prueba decoding real sin comprometer la futura arquitectura de frame ownership. No hay seek, reproducción, threading, RGB, swscale ni hardware decoding.

`domain` y `application` siguen sin headers ni símbolos de FFmpeg. `pulsar_media_ffmpeg` continúa enlazando solamente `libavformat`, `libavcodec` y `libavutil`.
