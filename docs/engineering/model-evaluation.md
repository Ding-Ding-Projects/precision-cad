# Deterministic model evaluation

`precision_core` evaluates an immutable `DocumentRecord` before any geometry worker receives a request. The evaluator has no geometry-kernel or process dependency. It validates the registered worker feature types and returns a stable topological plan, preserving original document order between equally ready features.

The current registry accepts `box`, `cylinder`, `extrude`, `union`, `cut`, `intersection`, `translate`, `rotate`, `fillet`, `validate`, and `tessellate`. Each type has an exact parameter shape, finite numeric values, bounded coordinates, and explicit input arity. Values use the document's explicit `mm` or `in` unit record. Unsupported feature types and invalid parameters fail before dispatch.

Suppression is an evaluation state, not a missing result. A directly suppressed feature suppresses every transitive dependent and records its direct suppression source. The controller schedules only `Ready` entries, so no dependent worker request is constructed with a missing BREP.

The plan contains no worker output cache. Geometry results remain revision-bound controller state. A candidate keeps its document identity and revision through worker dispatch and commits only when those values still match the current document. Failed validation or worker execution discards the candidate and preserves the last committed record.
