# ADR 0014: M9 Project Document and Media Asset References

## Estado

Aceptado

## Contexto

M9 necesita un agregado de aplicación que pueda reunir la timeline editable
con un catálogo estable de medios, sin hacer que el dominio conozca rutas,
probing, archivos de proyecto o estados de disponibilidad. La reproducción y
la persistencia necesitarán snapshots de valores independientes del objeto
mutable.

## Decisión

`ProjectDocument` vive en `pulsar_application` y es dueño de una `Timeline`,
de un catálogo de `MediaAsset` y de una única `TrackId` de video designada para
el alcance M9. La timeline sigue siendo el modelo genérico del dominio y
conserva soporte para más tracks.

`MediaId` es identidad de asset. El source locator es metadata de aplicación,
actualmente una `std::filesystem::path`, y nunca es la identidad de un clip o
asset. Dos importaciones del mismo locator pueden recibir IDs distintos.
Cada asset conserva el índice explícito del stream de video seleccionado, la
duración positiva exacta reportada por ese stream y metadata de video por valor.
La importación selecciona el primer stream de video entregado por el puerto
`MediaProbe`; no reinterpreta silenciosamente el stream al reabrir.

La disponibilidad persistente del catálogo se limita a `Available` y
`Unavailable`. Un fallo de probe no agrega un asset ni cambia el documento.
Los estados de EOF, errores de decode y gaps pertenecen a playback posterior.

Los IDs se asignan de forma monotónica y determinista, comenzando en 1. No se
reutilizan durante la vida del documento. El estado del allocator se copia en
`ProjectSnapshot` para que una futura persistencia pueda continuar la
secuencia sin ambigüedad.

`ProjectSnapshot` copia la timeline, los assets, el track designado, el estado
del allocator, los identificadores de estado lógico y la generación. No
contiene punteros, Qt, FFmpeg ni handles de workers, y permanece válido aunque
el documento vivo cambie.

La identidad lógica actual y la guardada son separadas: `is_dirty()` compara
`current_history_state_id` con `saved_history_state_id`. La generación del
documento es monotónica y sirve para invalidación, correlación y diagnóstico;
no es la verdad de dirty state. M9.2 asociará comandos con estados antes y
después y podrá volver a una identidad lógica anterior mediante undo.

## Consecuencias

La capa application puede preparar edición, reproducción y persistencia sin
filtrar FFmpeg o Qt al dominio. La importación es atómica respecto del catálogo:
probe, validación y duración exacta ocurren antes de consumir el ID o mutar el
documento. La generación de ClipId queda disponible para los comandos futuros.

La búsqueda de assets y la copia de snapshots son lineales y deliberadamente
simples; no se introduce una base de datos ni un índice especulativo en M9.1.
