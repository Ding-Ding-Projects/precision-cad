# Local Git project history

`precision_history` provides a local-only Qt Core adapter for an explicitly chosen CAD project directory. It uses the supplied Git executable through `QProcess` argument arrays, disables interactive authentication, has no network or remote commands, and verifies that Git's top-level directory is exactly the selected directory.

Supported project history files are `.pcad` and `.cad.json` native records, bounded to 32 MiB. Selected paths must be relative descendants of the selected project root. Absolute paths, parent traversal, `.git` components, symlinks, junction escapes, unsupported names, and oversized files are refused.

The service exposes structured status, bounded commit listing, local branch listing and creation, selected-file commits, semantic native-record comparisons, and a two-step restore. A commit refuses any pre-existing staged state and stages only the caller-selected supported native files. An empty selected diff is a successful no-op.

Restore begins with a validated preview of a revision and path. Apply copies the current working document to a timestamped sibling first, validates the historical bytes as a native record, then writes replacement content through a temporary file. It never resets, checks out, deletes, switches refs, overwrites the only current document without preservation, or modifies Git history.

Native parsing currently validates the exact record boundary: `schemaVersion`, `documentId`, `revision`, `units`, and `features`. The adapter remains independent of `precision_core`; a future parent build may link stronger core validation without changing the local-history safety boundary.
