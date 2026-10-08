# Contributing to PULSAR

Thanks for looking. PULSAR is a young Linux video editor; feedback is the most useful contribution right now.

## The most helpful thing: edit a real video

Install the [latest beta](https://github.com/franduartedev/pulsar/releases), make a complete video with your own footage and
tell us where you got stuck. [`docs/release/creator-test.md`](docs/release/creator-test.md) is the exact test we use, with a
log of what to write down. Every "I would have used another editor for…" becomes backlog, ordered by how often it comes up.

## Reporting a problem

Open an issue with the **Bug report** template: version (`./pulsar --version`), distro, desktop, and the steps. A `.pulsar`
project that reproduces it helps a lot.

## Sending code

1. Read [`docs/development/engineering.md`](docs/development/engineering.md) and the module map in
   [`docs/architecture`](docs/architecture/README.md). Dependencies point one way: `domain ← application ← adapters ← app`.
2. Build and test (see the README). Tests for new behaviour are expected, and anything that changes what is drawn or heard
   needs a preview-versus-export check.
3. Keep changes small and focused; match the surrounding style (`.clang-format`).
4. Open a pull request using the template.

The 0.1.x line is in **feature freeze**: bug fixes and polish only. Larger ideas belong in the
[roadmap](docs/release/roadmap.md) discussion.
