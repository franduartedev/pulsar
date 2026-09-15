# Engineering Guide

PULSAR grows by small, explicit milestones.

## Rules

- Start a milestone from a clean working tree.
- Keep one clear responsibility per milestone.
- Run build and tests before commit.
- Do not mix unrelated refactors into feature work.
- Keep `pulsar_domain` free of framework and infrastructure dependencies.
- Design for performance, but optimize only with measurements.
- Fix sanitizer findings instead of suppressing them.
- Justify any new dependency before adding it.
- Future UI code must not run heavy media work on the UI thread.
- Public APIs should preserve invariants by design.

## Code Agents

When Codex, OpenCode, or another coding agent modifies PULSAR:

- respect module boundaries;
- do not add dependencies unless explicitly requested;
- do not expand scope beyond the milestone;
- run the relevant tests;
- report decisions that were not specified;
- do not commit unless explicitly asked.
