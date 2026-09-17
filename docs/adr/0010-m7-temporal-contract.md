# ADR 0010: M7 Temporal Contract

## Estado

Aceptado

## Contexto

M7 introduce playback headless sin UI, audio, Vulkan, pixels ni workers en M7.1. El transporte debe ser determinista y exacto, y no debe mezclar coordenadas temporales que hoy pueden tener el mismo valor numérico por accidente.

M6 ya tiene un contrato público de seek de stream en el adapter FFmpeg. Ese contrato no cambia en M7.

## Decisión

PULSAR distingue cinco coordenadas temporales:

1. Monotonic time: tiempo transcurrido observado por un clock monotónico del proceso. No tiene significado editorial y no proviene del reloj del sistema.
2. Playback position: posición lógica que el transporte reproduce o materializa cuando pausa.
3. Timeline time: posición dentro de una secuencia editada.
4. Source media time: posición relativa dentro del asset o rango de origen seleccionado.
5. Stream timestamp: timestamp absoluto en la coordenada temporal del stream multimedia seleccionado.

M7.1 implementa solo transporte. La posición inicial es `Paused @ 0`. Mientras reproduce a 1x:

```text
position(now) = anchor_position + elapsed(now, anchor_clock)
```

Cada evento captura `now` una sola vez. Play repetido es idempotente. Pause materializa la posición exacta. Seek pausado cambia posición y conserva pausa. Seek reproduciendo cambia posición, reinicia el anchor clock y conserva intención `Playing`. El transporte no duerme, no consulta FFmpeg, no usa FPS nominal y no redondea la posición lógica al frame.

`seek_to_stream_timestamp(target)` de M6 mantiene su política: devuelve el primer frame con presentation timestamp `>= target` en coordenadas absolutas del stream. Playback futuro necesitará una operación diferente:

```text
presentation seek(target) -> frame whose presentation interval contains target
```

Esa operación debe conservar el frame precedente y lookahead suficiente para conocer el intervalo de presentación. No debe implementarse como un wrapper superficial del seek M6.

Playback y background work no retendrán iteradores, punteros ni referencias hacia una timeline mutable. Cuando una futura sesión necesite evaluar playback de una timeline, deberá trabajar sobre IDs, value types o una revisión estable publicada para reproducción.

## Consecuencias

Los tipos nominales pequeños en application pueden representar `PlaybackPosition`, `StreamTimestamp` y `MonotonicInstant` cuando eviten mezclas reales de coordenadas. Deben reutilizar `MediaTime` y `Rational` como autoridad exacta en lugar de duplicar la aritmética temporal del dominio.

El clock real puede adaptarse a `std::chrono::steady_clock`, pero los tests usan un fake clock y nunca hacen `sleep`.

Errores de overflow o de rango en aritmética temporal deben convertirse en comportamiento definido mediante excepción o resultado de error, nunca en UB ni drift silencioso.
