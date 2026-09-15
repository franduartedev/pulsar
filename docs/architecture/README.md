# Architecture

PULSAR starts with two product layers and one executable target:

- `pulsar_domain`: core product concepts and invariants. It depends only on the C++ standard library.
- `pulsar_application`: application-level orchestration. It may depend on `pulsar_domain`.
- `pulsar-engine-lab`: a small executable used to verify that applications can be built on top of the core.

Dependency direction is inward:

```text
pulsar-engine-lab -> pulsar_application -> pulsar_domain
```

`pulsar_domain` does not know about frameworks, operating-system APIs, multimedia libraries, storage, UI, or infrastructure. This keeps the product logic testable and allows future implementations to be replaced without forcing external technology choices into the domain.

`pulsar_application` is where use-case coordination will live when real product behavior exists. It can call into the domain, but the domain must never call back into application code.

Engine Lab will become the place to experiment with playback, decoding, seeking, GPU work, and performance. For this foundation milestone, it only proves that the project can build and run an executable over the core targets.
