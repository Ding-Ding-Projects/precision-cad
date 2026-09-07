# Precision CAD engineering contract

This repository is a new Windows x64 mechanical CAD application. Use C++20, Qt 6/QML and Open CASCADE. Source licensing is GPL-3.0-or-later with compatible dependencies.

## Product invariants

- Document edits are transactional. Invalid operations preserve the last valid document.
- Geometry workers consume immutable revision-bound input. Stale results never overwrite newer edits.
- Unknown or ambiguous topology references are diagnosed rather than silently reassigned.
- Model undo and Git project history are distinct. Restoration first preserves unsaved work.
- Native documents use versioned deterministic records, explicit units and stable identifiers.
- Save and export validate outputs before reporting success. Never overwrite the only valid saved version during recovery.
- CAD, CAM and FEA operate offline. External-service failures cannot prevent local saving.
- Analysis and machining results identify exact geometry/settings revisions and disclose validation limits.
- Do not expose credentials, private user dictionaries, project secrets or host-specific configuration in source, diagnostics or exports.
- Document actual implementation and verification. Do not label a prototype, disabled control or planned feature as delivered.

Read the categorized feature documentation for the subsystem being changed. Keep the roadmap and handoff factual. Preserve unrelated work and isolate parallel writers.
