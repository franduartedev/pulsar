# ADR 0006: Command-Based Editing

## Estado

Aceptado

## Contexto

PULSAR necesita una forma determinista de aplicar operaciones de edición que pueda servir como base para undo, redo, shortcuts, UI, scripting futuro, journaling y recuperación posterior.

La UI no debe modificar entidades internas de timeline directamente ni conocer todos los detalles necesarios para preservar invariantes.

## Decisión

Los commands viven en `pulsar_application`, no en `pulsar_domain`. El dominio define `Timeline`, `Track`, `Clip` y sus mutaciones válidas. La capa application define las acciones solicitadas, su ejecución y el historial.

Los commands son value types concretos almacenados en `std::variant`, no una jerarquía virtual. M3 introduce únicamente `AddClipCommand`, `RemoveClipCommand` y `MoveClipCommand`.

`CommandProcessor` opera sobre una `Timeline&` no propietaria. Esto mantiene ownership claro: quien crea la timeline conserva su lifetime, y el processor coordina mutaciones e historial sobre esa instancia.

Cada command ejecutado exitosamente se almacena como una operación forward junto con su operación inversa exacta. Undo ejecuta la inversa y mueve la entrada al redo stack. Redo ejecuta la operación forward y la devuelve al undo stack.

El historial tiene una capacidad máxima simple, por defecto 256 operaciones. Cuando se excede, se descarta la operación deshacible más antigua. Una capacidad de `0` significa que la edición se ejecuta, pero el historial queda deshabilitado. Este límite es por cantidad de entries, no por bytes; límites basados en memoria vendrán cuando existan payloads grandes.

Los entries del historial no contienen punteros, referencias, iteradores ni referencias a elementos internos de `std::vector`. Solo contienen IDs y value types.

`Timeline` expone mutaciones globales mínimas para `insert_clip`, `remove_clip` y `move_clip`. Estas operaciones preservan invariantes como unicidad global de `ClipId`, no-overlap por track y orden temporal.

`Track` mantiene un helper privado de remoción usado por `Timeline` para evitar reconstrucciones O(n²) al remover o mover clips. No se introducen índices globales ni estructuras paralelas en M3.

La serialización de commands, journaling, autosave y crash recovery quedan fuera de M3.

## Consecuencias

El modelo es determinista, testeable y compatible con journaling futuro sin introducir dependencias ni allocations polimórficas para commands.

Undo/redo de operaciones pequeñas no requiere snapshots completos de timeline. A cambio, cada command debe capturar o derivar una operación inversa precisa al ejecutarse.
