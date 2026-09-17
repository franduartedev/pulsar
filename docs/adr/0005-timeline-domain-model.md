# ADR 0005: Timeline Domain Model

## Estado

Aceptado

## Contexto

PULSAR necesita sus primeras entidades de edición sin depender de UI, reproducción, archivos multimedia reales ni frameworks externos. El modelo debe expresar colocación temporal exacta usando el modelo de tiempo de M1.

## Decisión

Los IDs de `Timeline`, `Track`, `Clip` y `Media` son tipos fuertes basados en `std::uint64_t`. El valor `0` queda reservado como inválido para detectar entradas externas incompletas. El dominio recibe IDs; no los genera aleatoriamente ni usa UUIDs todavía.

`TimeRange` usa intervalos half-open `[start, end)`: el inicio es inclusivo y el fin exclusivo. Esto permite que dos clips consecutivos, como `[0s, 5s)` y `[5s, 10s)`, no se solapen. `TimeRange` permite duración cero como primitiva matemática, pero `Clip` exige duración fuente estrictamente positiva.

Un `Clip` referencia un `MediaId`, un `source_range` usado del media y un `timeline_start`. Su rango ocupado en timeline se deriva como `[timeline_start, timeline_start + duration)`. La validación contra duración real de media queda para una capa posterior.

Una `Track` contiene clips ordenados por posición temporal y no permite solapamientos dentro de la misma pista. La composición simultánea se representa mediante múltiples tracks. Transiciones, velocidad y edición interactiva se diseñarán en hitos posteriores.

Cuando una `Track` pertenece a una `Timeline`, la timeline es dueña de esa instancia y solo expone acceso read-only a sus tracks y clips. Cualquier mutación que pueda afectar invariantes globales debe pasar por `Timeline`.

Los punteros, referencias y referencias a vectores devueltos por `Timeline` o `Track` son observaciones únicamente. Mutaciones posteriores pueden invalidarlos por reallocations de vectores, erase de elementos o reemplazo de tracks copiados. Application, playback y la futura UI deben conservar IDs o datos de presentación copiados a través de mutaciones, no punteros del dominio retenidos.

`ClipId` es único dentro de toda la `Timeline`, no solo dentro de una `Track`. En M2 esta invariante se comprueba con búsqueda lineal para evitar caches e índices prematuros. Al agregar una `Track` pre-poblada, la timeline valida todos sus clips antes de incorporarla.

`TrackKind` distingue inicialmente `Video` y `Audio`, pero no valida compatibilidad con streams multimedia porque todavía no existe probing ni catálogo de media.

## Extensiones M9.0

El lookup de clips por posición temporal usa la semántica half-open: una posición `T` pertenece a un clip si y solo si `start <= T < end`. Una posición fuera de todos los clips representa naturalmente un gap; no se introduce un objeto `Gap` en el dominio. El lookup devuelve una copia de `Clip` para que una observación no pueda retener referencias a almacenamiento mutable a través de una edición.

M9 usa edición no-ripple. Mover un clip conserva su `ClipId`, `MediaId` y rango fuente, rechaza posiciones negativas y solapamientos en la pista destino, y no desplaza vecinos. Los gaps son válidos.

El trim M9 es únicamente hacia adentro. Para un clip `[A, B)` con fuente `[S0, S1)`, un trim de inicio a `T`, con `A < T < B`, produce timeline `[T, B)` y fuente `[S0 + (T - A), S1)`. Un trim de fin produce timeline `[A, T)` y fuente `[S0, S0 + (T - A))`. Los límites y duraciones cero se rechazan.

El split solo acepta `A < T < B`. Produce dos clips contiguos: el clip original conserva su `ClipId` a la izquierda y el lado derecho recibe un `ClipId` suministrado por la capa superior. La fuente se divide con el mismo offset exacto, sin gap ni solapamiento. El dominio asigna ningún ID y valida/construye el estado resultante antes de mutar la pista, por lo que un fallo deja la timeline intacta.

## Consecuencias

El modelo es testeable, exacto y libre de dependencias multimedia. Las operaciones relevantes son simples y lineales, adecuadas para M2. La duración de una timeline se calcula como el mayor final temporal de sus clips y no se almacena de forma redundante.

El coste es que búsquedas e inserciones no están optimizadas para timelines grandes. Índices, interval trees o caches se considerarán solo cuando existan benchmarks o necesidades reales.
