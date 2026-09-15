# ADR 0004: Exact Time Model

## Estado

Aceptado

## Contexto

A video editor must represent important temporal values without accumulating drift from floating-point arithmetic. Frame rates such as `24000/1001`, `30000/1001`, and `60000/1001` are common, and converting them eagerly to decimal seconds would lose exactness at the foundation of editing operations.

## Decisión

PULSAR represents rational values with signed 64-bit integer numerators and denominators. Fractions are normalized by moving the sign to the numerator, keeping the denominator positive, and reducing by greatest common divisor. Zero is canonicalized as `0/1`.

Normalization is performed with widened intermediates so representable `INT64_MIN` cases such as `INT64_MIN/1` and `INT64_MIN/2` remain valid. Inputs whose normalized form cannot fit in signed 64-bit storage, such as `1/INT64_MIN` or `INT64_MIN/-1`, throw `std::overflow_error`.

`FrameRate` stores a strictly positive `Rational`. `FrameNumber` is a signed 64-bit value type; PULSAR frame indices are zero-based, so the first frame is `0`.

`MediaTime` stores `value * time_base`, where `value` is a signed 64-bit integer and `time_base` is a strictly positive `Rational` measured in seconds. Negative temporal values are represented by `value`, not by a negative base. Time is independent from frame rate so future media, audio, and timeline layers can use their own exact bases.

Frame-to-time conversion uses the exact frame duration derived from the frame rate. Time-to-frame conversion requires an explicit `RoundingMode`: `Floor`, `Ceil`, or `Nearest`. `Nearest` resolves exact ties away from zero.

Cross multiplication and conversion math use encapsulated signed `__int128` intermediates on GCC/Clang to avoid undefined behavior from overflowing 64-bit multiplication. Multiplications that could exceed even the widened intermediate range are checked before multiplying. Public stored values remain `std::int64_t`; operations that cannot be represented throw standard exceptions such as `std::overflow_error`.

## Consecuencias

Advantages:

- exact frame-rate representation;
- no drift caused by using `double` as the primary time model;
- no dependency on FFmpeg or `AVRational`;
- easier future adaptation to multiple media time bases.

Costs:

- more complexity than storing seconds as `double`;
- overflow must be considered explicitly;
- conversions are intentionally explicit.
- arithmetic is exact only within the documented 64-bit storage and widened-intermediate limits, not arbitrary precision.
