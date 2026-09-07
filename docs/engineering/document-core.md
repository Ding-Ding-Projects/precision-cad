# Native document core

`precision_core` owns editable native document records. It deliberately uses Qt Core types only and has no geometry-kernel or UI dependency.

## Record contract

The deterministic JSON record has exactly `schemaVersion`, `documentId`, `revision`, `units`, and ordered `features`. Version 1 supports `mm` and `in`. A feature contains exactly `id`, `type`, `label`, `inputRefs`, `parameters`, and `suppressed`. Inputs, identifiers, string sizes, feature counts, nesting, and parameter collection sizes are bounded. Parameters may contain JSON scalars, arrays, and objects. Numeric parameter values must be finite.

The parser rejects unknown or missing fields and unsupported schema versions. It validates duplicate IDs, unknown input references, and cycles before exposing a record. Serialization orders fields and parameter object keys, so the same valid record always produces identical bytes.

## Transactions and revision binding

Add, update, remove, and suppression commands accept an expected revision. A stale request is rejected. Candidate records are fully validated before they replace the committed record, preserving the previous valid state after an invalid transaction. Successful commands, undo, and redo each issue a new monotonic revision. Undo and redo therefore cannot make a previously queued asynchronous result look current.

## Persistence and recovery

Saving validates the record, takes a writer lock, preserves the prior valid bytes as `.bak`, then uses `QSaveFile` for atomic replacement. A lock held by another writer rejects the save. Loading never falls through to malformed content. Recovery first tries the active file and then its valid `.bak`; it reports failure if neither validates and does not overwrite either input during recovery.

Focused checks cover transactional rollback, duplicate and cyclic graph references, stale revision rejection, monotonic undo/redo, deterministic round trips, rejected unknown schemas, atomic-save backup recovery after corruption, and writer locking.
