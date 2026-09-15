# ADR 0003: Keep the Domain Free of External Dependencies

## Status

Accepted

## Context

The domain should express PULSAR product logic without being shaped by UI frameworks, multimedia libraries, storage engines, graphics APIs, or operating-system details.

## Decision

`pulsar_domain` depends only on the C++ standard library. External technologies must be introduced outside the domain when real implementation needs appear.

## Consequences

The domain remains easier to test, reuse, and evolve. Integration code will be added later in separate modules when PULSAR has concrete infrastructure to integrate.
