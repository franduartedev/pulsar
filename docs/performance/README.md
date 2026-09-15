# Performance

PULSAR treats efficiency as a design requirement.

Official policy:

> Design for performance from the beginning. Optimize with evidence.

An important optimization should answer:

1. What is the problem?
2. How was it measured?
3. What was the result before?
4. What changed?
5. What is the result after?
6. What trade-off does it introduce?

"It feels faster" is not enough evidence for a structural optimization.

Local benchmark support lives in `benchmarks/`. The current harness is useful for trends while the domain model grows, but it is not a scientific benchmark suite and does not define CI thresholds yet.

Linux profiling tools such as `perf`, `heaptrack`, and `valgrind` may be used later when there is enough real runtime behavior to diagnose.
