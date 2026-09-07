# Native preferences backend

`precision::preferences::PreferencesStore` is the native Qt Core settings boundary for the first mounted desktop settings surface. It persists a versioned JSON object in user application configuration storage, or a caller-supplied path for tests. The store is not itself a settings screen and does not claim a mounted QML surface.

## Contract

The QObject exposes Q_PROPERTY values and QML-callable setters for language mode (`en`, `yue`, `both`), independent English and Cantonese tone levels (1 through 5, default 5), dialog emoji decoration, theme (`light`, `dark`, `system`), font scale (0.75 through 2.0), opaque accent colour, reduced motion, ADHD presentation mode, narration enablement, narration language, language-specific voice identifiers, narration rate, and narration pitch.

Every mutation is validated before it is written. The store uses `QLockFile` to refuse concurrent writers and `QSaveFile` for atomic replacement. A failed write leaves the in-memory state unchanged and emits `errorOccurred`. Load accepts only schema version 1 and the exact known key set. Corrupt, unsupported, incomplete, or invalid stored data leaves defaults active without partially applying a record.

`exportPublicPreferences()` deliberately omits per-language voice identifiers. This backend has no personal-vocabulary loader because this Oak Kay does not contain the canonical neutral schema generator. A loader must not be invented from private data. When that generator is available, its bounded versioned schema can be implemented in a separate owned lane, with cache-only storage, duplicate-key rejection, and no path or payload in exports, history, diagnostics, or logs.

## Focused verification

`tests/preferences` builds independently with Qt Core and Qt Test. It covers valid persistence, invalid setter rollback, corrupt-record fallback, writer-lock refusal, and public-export omission of voice identifiers.
