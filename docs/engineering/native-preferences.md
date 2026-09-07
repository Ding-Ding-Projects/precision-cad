# Native preferences backend

`precision::preferences::PreferencesStore` supplies Qt Core properties and QML-callable setters for language mode, independent English and Cantonese tone levels, dialog emoji decoration, theme, font scale, accent colour, reduced motion, ADHD presentation, and narration settings. `PersonalVocabularyStore` supplies local vocabulary loading, private UI text rendering, and explicit clearing. These classes provide backend behavior; they do not establish that a desktop settings screen is mounted or verified.

## Settings persistence and recovery

Settings use schema version 1 and an exact field inventory. Schema version and tone levels must be integral numbers with valid values. Input is limited to 64 KiB using reads of the limit plus one detection byte. Before parsing the complete record, a shared lexical prepass rejects duplicate decoded keys within each object and container nesting beyond four levels. The prepass is not a replacement JSON parser: Qt validates JSON syntax afterward. Unknown fields and incomplete records are refused.

Each write holds `QLockFile`, compares the current bounded bytes with the loaded SHA-256 revision, and commits through `QSaveFile`. Writes require the exact expected byte count and verify committed settings bytes against the serialized candidate. A refused write leaves properties unchanged. Another writer's revision requires reconstructing the store before retrying.

Corrupt data leaves defaults active and blocks ordinary setters. Explicit reset acquires the same writer lock and checks the revision before preserving original bytes to `.corrupt` and writing defaults. An existing identical preservation file may be reused; different recovery evidence is never overwritten. Oversized records, unreadable records, changed revisions, locked files, and incompatible existing recovery evidence require external recovery and remain untouched.

## Personal vocabulary

The neutral schema is `{"schemaVersion":1,"entries":{"source":"replacement"}}`. No real personal dictionary is part of the implementation or fixtures. Source and startup-cache reads are bounded to 1 MiB plus one detection byte; the loader also checks the byte limit internally. Validation requires the exact root fields, version 1, at most 10,000 entries, keys no longer than 256 UTF-16 code units, and string values no longer than 512 UTF-16 code units. Empty keys and the reserved keys `__proto__`, `prototype`, and `constructor` are refused. Empty replacement strings are permitted. A valid empty entries object is also loaded state, independently of mapping count. Startup revalidation restores that state. A successful clear transitions it to unloaded; a refused clear retains it. `loadedChanged` notifies a loaded-state transition or a changed mapping, while identical reloads and repeated clears remain silent.

Both loaders use the object-scoped decoded-key prepass before whole-record Qt parsing. An entry named `schemaVersion` is legal because it belongs to a different object. Key-looking quoted text in a value is data. Literal backslash-u sequences remain distinct from Unicode escapes unless both decode to the same key.

Valid source bytes are atomically copied to the cache without retaining the selected source path as member state. Validation or cache-write refusal preserves the prior mapping and cache bytes. Clearing reports success only after the cache is absent; a locked cache leaves the prior mapping active and emits a generic error. Invalid startup cache data is not applied and is not rewritten.

Private UI replacement scans the original input from left to right, choosing the longest matching key with lexical ordering as the stable tie-breaker. Replacement output is never scanned again. Only callers of `applyPrivateUiText` receive mapped text.

`exportPublicPreferences()` has an explicit field allowlist. It omits voice identifiers, vocabulary entries, cache paths, and selected source paths. The vocabulary class exposes no public export operation. The backend links Qt Core and uses local file APIs; this source boundary does not prove that every application caller avoids network transmission, logging, history, or exporting rendered text. Those integration boundaries require their own tests.

## Focused verification

`tests/preferences` is a standalone CMake project requiring CMake 3.22 or later, Qt 6.8 Core and Test, and a compatible C++20 compiler. For Windows, use the MSVC Qt distribution with MSVC, not MinGW objects. Its CTest definition adds the selected Qt Core binary directory to the test process path, so a separate shell path modification is unnecessary.

The Release suite currently reports 47 Qt Test cases, including initialization and cleanup, in one executable: 45 behavioral/data cases, zero failures, and zero skips on Windows. It covers persistence, setter rollback, public-export allowlisting, exact bytes, fractional values, escaped duplicate keys, legal object scopes, depth limits and deeply nested unknown fields, source and startup-cache size limits, invalid reload preservation, actual Windows file-sharing refusal for cache replacement and clear, optimistic writer rejection, and revision-bound corruption reset. The empty-cache transition and locked-clear tests were observed failing against the previous implementation before the loaded-state repair. All vocabulary fixtures are neutral and generated in temporary directories.

A deliberate local mutation disabled the settings prepass: the three duplicate-key cases and deep-unknown-field case failed (exit 4). Restoring the prepass returned the complete CTest executable to green.

No desktop interaction, screen capture, application-wide privacy audit, or full canonical feature-completeness claim follows from this backend suite.
