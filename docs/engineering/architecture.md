# Architecture

## Components

- `src/core`: versioned document records, command transactions, units, feature dependencies, undo/redo and persistence. Qt Core is allowed; no GUI or geometry-kernel types cross this boundary.
- `src/geometry`: Open CASCADE operations and exchange. A worker process consumes versioned JSON requests and returns structured results plus separate shape/mesh payloads.
- `src/history`: Git CLI process adapter and semantic document comparison. Commands use argument arrays, never shell interpolation.
- `src/app`: Qt desktop host, QML presentation and revision-aware asynchronous controllers.
- `src/cam` and `src/analysis`: immutable setup/study records, isolated computation and revision-bound results.

## Document and commands

Native records contain `schemaVersion`, `documentId`, `revision`, `units`, ordered features and stable references. Features have stable IDs, a type, a label, explicit input references, parameters and suppression state. Numeric values must be finite and dimensionally valid.

Every successful transaction advances the revision once. Validation and recomputation failure cannot partially change committed state. Undo/redo creates a new observable revision rather than making an asynchronous request look current again.

Persistence uses deterministic JSON, atomic replacement, checksums and a recovery journal. Geometry caches are derived outputs, not an alternative source of editable history. Unknown schema versions are rejected without changing the original file.

## Geometry process interface

Requests contain `protocolVersion`, `operationId`, `documentId`, `revision`, `operation` and `parameters`. Replies echo their identity and provide `ok`, `result` or a typed `error`. Native kernel handles never enter document records or cross process boundaries. The controller accepts a reply only when operation identity and revision remain current.

Workers must have bounded input, output, time and memory. Cancelling an operation may terminate its owned worker process; document state survives. Repeated crashes are reported rather than retried indefinitely.

## History

Recovery snapshots protect working sessions. Git commits are deliberate project milestones. Native document diffs identify parameter, feature, reference and assembly changes. Automatic merging is limited to independent records; geometry conflicts require explicit resolution. Private application settings and private vocabulary are excluded from project history.

## Acceptance

Use reference mechanical parts, assembly edits, sheet-metal round trips, interrupted saves, adversarial input, Git conflict/restoration fixtures, machining collisions, and analytic structural/modal benchmarks. Record the source commit, component versions and exact fixture for each verdict. UI evidence must come from the built application, and numerical validity is not inferred from rendering.
