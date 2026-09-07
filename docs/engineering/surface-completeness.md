# Cross-surface feature completeness

Tracking issue: [#3](https://github.com/Ding-Ding-Projects/precision-cad/issues/3).

This hand-written inventory applies independently to the desktop application and public website. **No row is complete yet.** `Partial` means some behavior exists, not that the feature contract or evidence is sufficient. Planned CAD features are separately tracked in the roadmap.

Every complete row must link its implementation, localized copy, persistence or documented non-applicability, categorized article, focused tests, built interaction receipt, per-action and final captures, accessibility/privacy review, and a deliberate negative regression. Evidence must identify exact source, built output hash and screen/state/theme/language/viewport/scale. Missing evidence cannot be replaced with a checkbox.

| ID | Required capability | Desktop | Website |
| --- | --- | --- | --- |
| CORE-001 | Running version and recorded build/release local time | Missing | Partial |
| CORE-002 | Hand-written per-surface completeness inventory | Partial | Partial |
| CORE-003 | Status Hub registration and native status surface | Missing | Missing |
| CORE-004 | Shared-link preview graphic and served metadata | Missing | Partial |
| I18N-001 | English, Cantonese and bilingual modes | Missing | Partial |
| I18N-002 | Independent English and Cantonese tone-level controls | Missing | Missing |
| I18N-003 | Dialog/message emoji preference | Missing | Missing |
| I18N-004 | Local private-vocabulary JSON upload, validation, replace and clear | Missing | Missing |
| I18N-005 | Shared renamable School mode and live protected unlock | Missing | Missing |
| NARR-001 | Optional event narration and serialized queue | Missing | Missing |
| NARR-002 | Per-language installed voices, rate, pitch, fallback and persistence | Missing | Missing |
| SCHED-001 | Scheduled language and appearance | Missing | Missing |
| SCHED-002 | Validated external settings and Home Assistant integration | Missing | Missing |
| DELIGHT-001 | Non-blocking dim-sum startup surprise | Missing | Missing |
| DELIGHT-002 | Release code names and verified public image catalog | Missing | Missing |
| UI-001 | Registered Material Design 3 primitives and complete conformance | Missing | Partial |
| UI-002 | Accessibility, focus, contrast, reduced motion and responsive sizing | Missing | Partial |
| UI-003 | Functional controls, truthful empty states and disclosed examples | Missing | Partial |
| UI-004 | Guided forms, enumerations, defaults, validation and browse controls | Missing | Missing |
| UI-005 | Rich controls for editable values | Missing | Partial |
| UI-006 | Settings explanations and default provenance | Missing | Partial |
| SEARCH-001 | Full regex construction, explanation, testing, profiling and debugging | Missing | Missing |
| SEARCH-002 | Field-owned search and adjacent isolated regex workbench everywhere | Missing | Partial |
| FOCUS-001 | Persistent ADHD presentation modes | Missing | Missing |
| NOTICE-001 | Actionable notifications, progress and notification history | Missing | Partial |
| SAFETY-001 | Dual-key and slider destructive confirmation with emergency exit | Missing | Missing |
| APPEAR-001 | Complete global appearance editor and reset | Missing | Partial |
| APPEAR-002 | Per-element appearance editor, undo, import/export and persistence | Missing | Missing |
| BRAND-001 | Logo presets, local image processing, crop/fit/background and sizes | Missing | Missing |
| CONVERT-001 | Categorized offline file converter and bounded persistent queue | Missing | Missing |
| CONVERT-002 | Bundled offline PDF inspection and editing tools | Missing | Missing |
| OLLAMA-001 | Local runtime manager, exhaustive model catalog, fit and pull queue | Missing | Missing |
| OLLAMA-002 | Local model chat, parameters, attachments, history and safe export | Missing | Missing |
| TABS-001 | Browser-style destination and document tabs | Missing | Partial |
| TABS-002 | Tab grouping, pinning, four searches and bulk actions | Missing | Missing |
| LOCK-001 | Lockable tabs and appearance with recovery-safe state | Missing | Missing |
| LOCK-002 | Progressive unlock ladder and Support Tickets | Missing | Missing |
| AUTH-001 | Two-factor registration and built-in authenticator management | Missing | Missing |
| DOCS-001 | Complete landing page and categorized offline feature articles | Missing | Partial |
| DOCS-002 | In-product documentation browser | Missing | Partial |
| DOCS-003 | Sanitized instruction copies appropriate to the publication boundary | Partial | Partial |
| HANDOFF-001 | External editor handoff for records and exports | Missing | Missing |
| EXPORT-001 | Faithful multi-format export and loss disclosure | Missing | Partial |
| BULK-001 | Paged unlimited-length selection and bounded batch actions | Missing | Missing |
| HISTORY-001 | Append-only local history, deletion preservation and restoration | Missing | Missing |
| PRESET-001 | Useful verified presets for blank editors | Missing | Missing |
| CHANGELOG-001 | Searchable date-filterable changelog and verified commit links | Missing | Missing |
| PALETTE-001 | Ctrl+Shift+F rich command palette and exact destination focus | Missing | Partial |
| OVERLAY-001 | Opaque bounded scrollable overlays and resizable panels | Missing | Partial |
| MENU-001 | Context-menu shortcuts derived from live bindings | Missing | Missing |
| PROGRESS-001 | Origin-local progress, cancellation and duplicate-run protection | Missing | Missing |
| EXTDL-001 | Browser-extension Start Download surface | Missing | Missing |
| EXTDL-002 | Separate Downloading surface with transfer controls and errors | Missing | Missing |
| EXTDL-003 | Always-on-top completion surface and three-state evidence | Missing | Missing |
| RECOVERY-001 | Contextual recovery and reauthentication | Missing | Missing |
| CONTENT-001 | Isolated rendering of provider-authored markup | Missing | Missing |
| PUBLISH-001 | Multi-account forge publication and owner selection | Missing | Missing |
| FILTER-001 | Collapsible filters/statistics with active-state disclosure | Missing | Partial |
| RELEASE-001 | Original logo and packaged application icon | Missing | Partial |
| RELEASE-002 | Unsigned Squirrel.Windows installer and explicit-restart updates | Missing | Missing |
| RELEASE-003 | Release timing, line counts, effort estimates and catalog photo | Missing | Missing |
| CAPTURE-001 | Real built-surface capture inventory and interaction evidence | Missing | Partial |
| CAPTURE-002 | Real built-surface screen recording | Missing | Missing |
| CAPTURE-003 | Deterministic design-reference parity where references exist | Missing | Missing |

## Current evidence boundary

The initial website source is under `website/app/`; its behavior and limitations are documented in [website.md](website.md). Seven local runtime states were exercised at `463040400b37c98e09ec4e89269699f9bc307fd8`, with selected genuine captures in `docs/evidence/`. This proves only those observed states.

The native application currently has no mounted surface. The document and geometry foundations are being implemented independently. None of the rows above may inherit website evidence as desktop evidence.

The complete regex workbench is not implemented: literal match options do not satisfy it. The current keyboard shortcut only focuses documentation search and is not the required rich palette. The global settings suite, vocabulary loader, converter and local model manager remain absent. These are open requirements, not exemptions or future-release promises.

## Executable checks

`node scripts/check-surface-completeness.mjs --inventory-only` checks the explicit 64-feature, two-surface row structure against an independent required-ID list. `node --test tests/contracts/surface-completeness.test.mjs` deliberately removes each of the 64 rows and verifies rejection, plus duplicate, renamed, malformed and unjustified-completion cases.

The default completion check, `node scripts/check-surface-completeness.mjs`, is intentionally red while any surface row remains incomplete. The evidence verifier for marking a row complete is not implemented yet, so a manually changed `Complete` label is always rejected. Structural validity is not feature completeness.
