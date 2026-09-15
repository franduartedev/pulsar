# Performance Budgets

These are future product targets, not guarantees of the current foundation builds.

## Startup

On a normal PC, future PULSAR startup should target less than 2 seconds.

## Idle RAM

Future idle memory usage should target less than 400 MB.

## Simple 1080p Project

A simple 1080p editing project should target roughly 1-1.5 GB RAM or less.

## PULSAR Chill

PULSAR Chill is a design goal for extremely low resource usage.

Initial reference target: about 700 MB RAM or less during proxy-based editing when that becomes technically viable. This cannot be validated yet because PULSAR does not have a media engine.

## UI Thread

The future UI thread must not perform heavy work such as:

- decoding;
- encoding;
- proxy generation;
- waveform generation;
- heavy multimedia analysis;
- export;
- costly effects processing.

## Caches

Any significant future cache must have:

- an explicit limit;
- an eviction policy;
- observability.

Unbounded cache growth is not acceptable.

## Playback

Audio/video playback will take priority over secondary background work.

## Hardware Targets

- 8 GB RAM: supported.
- 16 GB RAM: normal and recommended experience.
- 32 GB RAM: useful for large workloads, never a basic requirement.

PULSAR Chill should aim below 8 GB RAM through controlled degradation, proxies, and small bounded caches.
