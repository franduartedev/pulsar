# ADR 0016: M9 Timeline Clip Resolver

## Estado

Aceptado

## Contexto

M9.4 necesitará convertir una posición exacta de timeline en una decisión
editorial antes de consultar decoders. Esa verdad debe poder evaluarse sin
threads, FFmpeg, playback o mutaciones del documento.

## Decisión

El resolver vive en `pulsar_application/timeline` y recibe un
`const ProjectSnapshot&`, un `TrackId` y una `MediaTime`. Es una función pura,
sin estado persistente, que devuelve un `std::variant` de cuatro valores:
`ResolvedClip`, `TimelineGap`, `MissingClipMedia` o `TimelineEnded`.

El lookup reutiliza `Timeline::find_clip_at`, cuya pista está ordenada por inicio
temporal. Los rangos son half-open `[start, end)`: el final de un clip no lo
incluye y un clip adyacente comienza exactamente en ese instante.

Para un clip timeline `[A, B)` y source `[S0, S1)`, en `A <= T < B`:

```text
offset      = T - A
source_time = S0 + offset
```

La aritmética usa `MediaTime`/`Rational` exactos. `ResolvedClip` conserva ambos
rangos, posición, offset, source time, locator e índice de stream. No se clampa
contra la duración del asset.

Una posición negativa es entrada inválida y lanza `std::invalid_argument`. Un
track inexistente también es error. Si no hay clip, una posición menor que la
duración de la timeline es `TimelineGap`; una posición igual o posterior es
`TimelineEnded`. En una timeline vacía, cero es `TimelineEnded`.

Un clip con asset ausente o `Unavailable` es `MissingClipMedia`, nunca gap. El
resolver no consulta filesystem ni vuelve a hacer probe, y no modifica el
snapshot. La razón de indisponibilidad se copia si existe.

La correlación con `history_state_id` y `document_generation` queda junto al
snapshot, no se duplica en cada resultado. Gap y MissingMedia no detienen el
reloj lógico; M9.4 decidirá la presentación neutral/offline mientras el tiempo
continúa.

## Consecuencias

La resolución editorial es determinista, testeable con snapshots y separada de
SourceError, EOF temprano, starvation y estados de decoder, que pertenecen a
M9.4. El lookup de assets permanece lineal hasta que mediciones justifiquen un
índice.
