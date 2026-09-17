# ADR 0015: M9 Document Edit History

## Estado

Aceptado

## Contexto

M9 necesita que timeline, catálogo de media, allocators e identidad de documento
cambien de forma coherente. Un historial que solo referencia `Timeline&` no puede
representar importaciones, IDs de assets ni el estado lógico guardado.

## Decisión

`CommandProcessor` recibe `ProjectDocument&`, es no copiable/no movable y es el
único escritor editorial activo. `ProjectDocument` no expone una timeline mutable.
Los comandos permanecen como tipos concretos en `std::variant`; las entradas de
historial contienen forward, inverse, `before_state_id` y `after_state_id`, todos
por valor.

Cada edición nueva recibe una identidad de estado nunca usada y aumenta una vez
`document_generation`. Undo restaura `before_state_id` y redo restaura
`after_state_id`; ambos aumentan la generación, pero no asignan identidades nuevas.
Después de undo, una edición exitosa obtiene una identidad nueva y descarta redo.
Un fallo deja intactos documento, allocator, estados y stacks.

Los IDs de clips y media conservan high-water marks durante undo. La inspección de
un candidato no muta el documento. Un Add o Split consume el ID candidato solo
cuando la operación completa valida y se aplica; redo restaura el ID histórico.

Trim usa un inverse de restauración del `Clip` completo, porque un trim hacia afuera
no pertenece al contrato M9. Split usa una restauración atómica del clip original y
la eliminación de su lado derecho. Estas operaciones no relajan las invariantes
domain de duración, rangos half-open u overlaps.

La importación se prueba y valida antes de mutar. Se registra como comando con el
`MediaAsset` completo; undo elimina el asset solo si ningún clip aplicado lo
referencia y redo restaura el mismo valor sin volver a probar el medio.

## Consecuencias

El dirty state sigue siendo la comparación entre `current_history_state_id` y
`saved_history_state_id`, nunca el tamaño de los stacks ni la generación. La
capacidad histórica continúa acotada a 256 entradas por defecto y no introduce
snapshots completos ni dependencias de Qt, FFmpeg o persistencia.
