# ADR 0024 — E5 export job and failure safety

## Estado

Aceptado para E5. E6 conserva la responsabilidad de UI desktop y del botón
Exportar.

## Ownership y snapshot

`ExportJob::run()` recibe un `ExportRequest` por valor semántico que contiene
un `ProjectSnapshot` inmutable. Construye `ExportPlan` desde esa copia y nunca
consulta `ProjectDocument`. El owner del worker posee el job, renderers,
factories y writer durante toda la ejecución. Una instancia de `ExportJob` no
admite dos `run()` concurrentes; destinos distintos deben coordinarse en el
owner superior.

Application depende sólo de `OfflineVideoSourceFactory`,
`OfflineAudioSourceFactory`, `ExportMediaWriterFactory` y
`ExportOutputPublisher`. No incluye FFmpeg, Qt ni APIs de sistema.

## Orchestration

El job renderiza un frame de video, lo entrega al writer y avanza el audio
hasta el horizonte temporal racional del siguiente frame. Tras el último frame
consume el audio restante. Video, audio, packets y buffers de proyecto nunca
se materializan completos. El progreso combina 50% frames de video y 50%
samples de audio; antes de publicar se limita a `0.99` y sólo un éxito
publicado emite `1.0`.

## Temporary sibling y publicación

El publisher genera un nombre temporal único en el mismo directorio del
destino. El writer termina sobre ese path; sólo después de `finish()`, cierre,
verificación de existencia y chequeo de cancelación se llama a
`publish_replace_existing_safely()`.

La implementación Linux inicial usa `std::filesystem::rename` sin borrar el
destino primero. En Linux esto reemplaza atómicamente el destino en el mismo
filesystem; si falla, el destino anterior no se toca. Windows/Android deben
proveer un adapter que cumpla el mismo contrato o falle antes de destruir el
destino. La semántica de reemplazo no queda dispersa en application.

## Cancelación, errores y cleanup

La cancelación es cooperativa y se revisa antes de construir el plan, entre
frames, entre bloques, antes de `finish()` y antes de publicar. Cancela con
`ExportErrorCode::Cancelled`, llama `abort()`, elimina el sibling temporal y
preserva el destino previo. Todas las excepciones posteriores a la creación
del temporal abortan el writer cuando corresponde e intentan cleanup sin
ocultar el error original.

Los errores de plan, source, decode/render, writer, mux/finalize y publicación
se convierten a `ExportErrorCode`; el texto original se conserva como
diagnóstico. No se usa texto como control flow.

## Portabilidad y límites

El publisher es la frontera de filesystem/publicación. El código application
usa exclusivamente `std::filesystem::path` como dato neutral y no contiene
POSIX, shell, QProcess, `/tmp` ni paths Linux. El adapter actual es Linux-first
y no pretende resolver todavía durabilidad `fsync` ni UI; E5 no inicia E6.
