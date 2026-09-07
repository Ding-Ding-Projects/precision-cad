# Native document core

`precision_core` owns editable native document records. It deliberately uses Qt Core types only and has no geometry-kernel or UI dependency.

## Record contract

The deterministic JSON record has exactly `schemaVersion`, `documentId`, `revision`, `units`, and ordered `features`. Version 1 supports `mm` and `in`. A feature contains exactly `id`, `type`, `label`, `inputRefs`, `parameters`, and `suppressed`. Inputs, identifiers, string sizes, feature counts, nesting, and parameter collection sizes are bounded. Parameters may contain JSON scalars, arrays, and objects. Numeric parameter values must be finite.

The parser rejects duplicate JSON object fields before parsing, then rejects unknown or missing fields and unsupported schema versions. It validates duplicate IDs, unknown input references, and cycles before exposing a record. Revisions are exact JSON-safe integers from zero through `9007199254740991`; operations refuse the limit instead of wrapping. Serialization orders fields and parameter object keys, so the same valid record always produces identical bytes. Constructing `Document` with an invalid record throws `std::invalid_argument`.

## Transactions and revision binding

Add, update, remove, and suppression commands accept an expected revision. A stale request is rejected. Candidate records are fully validated before they replace the committed record, preserving the previous valid state after an invalid transaction. Successful commands, undo, and redo each issue a new monotonic revision. Undo and redo therefore cannot make a previously queued asynchronous result look current.

## Persistence and recovery

Saving validates the record, takes a writer lock, and requires an expected persisted revision for an existing file. A differing on-disk revision rejects a stale sequential writer. New documents pass no expected revision. Existing bytes are bounded before reading and must parse as the supported schema before normal save can preserve them as `.bak` and use `QSaveFile` for atomic replacement. A lock held by another writer rejects the save. Loading never falls through to malformed content. Recovery first tries the active file and then its valid `.bak`; it reports failure if neither validates and does not overwrite either input during recovery.

Focused checks cover transactional rollback, duplicate and cyclic graph references, stale revision rejection, monotonic undo/redo, deterministic round trips, rejected unknown, fractional, duplicate-field, and unsafe-large versions, invalid initial construction, bounded on-disk reads, stale-writer rejection, atomic-save backup recovery after corruption, and writer locking.
