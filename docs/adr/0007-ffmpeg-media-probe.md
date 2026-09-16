# ADR 0007: FFmpeg Media Probe

## Estado

Aceptado

## Contexto

PULSAR necesita inspeccionar archivos multimedia locales reales sin introducir FFmpeg en el dominio ni convertir tipos externos en el modelo interno del producto.

M4 solamente obtiene metadata. No reproduce, no decodifica frames, no hace seek y no abre decoder contexts.

## Decisión

FFmpeg se integra como adapter de infraestructura en el target `pulsar_media_ffmpeg`. El adapter implementa el port `pulsar::application::MediaProbe`.

Los tipos de FFmpeg, como `AVFormatContext`, `AVStream`, `AVCodecParameters`, `AVRational` o `AVChannelLayout`, no cruzan la frontera del adapter ni aparecen en headers públicos de `pulsar_domain` o `pulsar_application`.

La integración usa `pkg-config` con imported targets para `libavformat`, `libavcodec` y `libavutil`. No se agregan `libswscale`, `libswresample`, `libavfilter` ni `libavdevice` porque M4 no las necesita.

El probe acepta solamente archivos locales regulares. URLs, esquemas remotos y `file:` como protocolo se rechazan antes de llamar a FFmpeg.

Las duraciones y time bases se convierten a tipos propios exactos: `AVRational` se transforma en `pulsar::domain::Rational`, y las duraciones se expresan como `MediaTime` cuando FFmpeg informa un valor conocido. Una duración desconocida se representa con `std::nullopt`.

El frame rate informado por FFmpeg se expone como `nominal_frame_rate`. No se trata como autoridad temporal absoluta, porque un medio puede ser VFR. Si la razón es inválida o cero, se representa como ausencia.

El adapter usa RAII para cerrar `AVFormatContext` con `avformat_close_input`, incluso si `avformat_find_stream_info` falla.

Los errores se traducen a `MediaProbeError` con códigos propios de PULSAR. Los códigos negativos de FFmpeg pueden aparecer en mensajes diagnósticos, pero no son semántica pública.

## Consecuencias

La arquitectura queda preparada para decoding en un hito posterior sin contaminar domain/application con FFmpeg.

PULSAR sigue sin catálogo multimedia, asignación de `MediaId`, thumbnails, proxies, cachés, journaling o persistencia.

Las implicaciones de distribución/licencia de la build concreta de FFmpeg deberán revisarse antes de empaquetar PULSAR. Este ADR no decide la licencia del proyecto.
