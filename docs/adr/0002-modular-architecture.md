# ADR 0002: Modular Architecture With Independent CMake Targets

## Status

Accepted

## Context

PULSAR should be able to grow without collapsing unrelated responsibilities into one build unit or one source tree.

## Decision

PULSAR is organized as explicit CMake targets. The initial targets are `pulsar_domain`, `pulsar_application`, and `pulsar_engine_lab`.

## Consequences

Dependencies are visible in CMake instead of existing only as folders. This makes architectural boundaries easier to review and test.
