# Current implementation state

## Baseline

Started from an empty workspace on 2026-09-07. The public source repository is `Ding-Ding-Projects/precision-cad`. No pre-existing CAD implementation was imported.

## Implemented

The product architecture, roadmap and public repository are established. The initial website implements an overview, seven planned-feature articles, the roadmap, documentation search and Markdown export, English/Cantonese/bilingual reading modes, light/dark themes and browser-local preference persistence.

The native foundation now builds: versioned documents and atomic persistence, an isolated Open CASCADE worker, a Qt Quick workspace, local preferences and local personal-vocabulary loading, and a local Git history service. The history service is not mounted in the interface. The viewport is a painter-order orthographic mesh preview, not a depth-buffered professional renderer. This is an incomplete development foundation, not the full CAD product.

## Next work

Repair the native QML test shutdown timeout first, then verify the complete native interaction and layout matrix. Mount the history service and implement the missing canonical surface features through the explicit inventory in issue #3. Keep all increments incomplete until their acceptance scenarios pass.

## Evidence

The static website build passed locally with Node.js 22.23.2 after updating the generated starter's vulnerable packages. The package audit reported zero known vulnerabilities. Seven built-page runtime states passed after repairing the narrow-screen source link: overview, documentation and roadmap at 1440 px, Cantonese/dark preferences at 1440 px, bilingual/dark preferences at 390 px, and overview at 390 and 320 px. These checks reported no body overflow, unnamed controls, runtime exceptions or failed resources. This is focused evidence, not the complete surface/scale matrix.

Native module verification covered document, geometry, preferences, local history and workspace controller tests. The initial aggregate run passed five of six CTest targets; geometry initially lacked its release allocator DLL, then passed its focused rerun after correcting runtime discovery. These verdicts do not verify later source changes.

The latest candidate is `43718ce25852aa122caf2664848cdf1447f1d4a0`. Its QML test reports three passing Qt records (including initialization and cleanup), but the process does not exit and CTest fails at the 30-second timeout. Software rendering and the basic render loop did not resolve shutdown. Do not report this target as passed. Logs remain in `build/native/Testing/Temporary/LastTest.log` and `build/native/tests/app/workspace-qml.txt`.

The real native build was driven off-screen to create and save a 40 by 30 by 20 mm box, reporting volume 24000. Subsequent native geometry measurements show the toolbar flow within its 64 px header and the inspector content within its 266 px width at 1280 by 820. The full after-change capture matrix is not complete. The native observer reports one unclassified warning, zero critical messages and zero classified binding warnings. Formal evidence promotion remains unfinished.

The latest three native diagnostic/layout/test commits are being preserved on `checkpoint/native-foundation-20260907`, not represented as verified default-branch integration. Published main baseline before this preservation is `2512747f81aed89bdd2eb54151181ec4feae0c1b`. No release or installer exists. Cached MSVC/Qt builds work; fully automatic fresh-machine acquisition remains incomplete.

All 64 desktop and website feature inventory rows remain Missing or Partial. Structural inventory tests do not prove implementation completeness. Sketch constraints, assemblies, drawings, sheet metal, exchange adapters, CAM, FEA, automatic updates and full canonical features remain unfinished.

Verified website source: `463040400b37c98e09ec4e89269699f9bc307fd8`.
GitHub Pages run `34095376173` succeeded. The public page and a project-scoped JavaScript asset returned HTTP 200, and `/precision-cad/provenance.json` identified that exact source commit. Its recorded build timestamp is `2026-09-07T07:26:10.939Z`.
The owner-private Sites deployment also succeeded for the same source. Its URL is `https://precision-cad.dayteetjer.chatgpt.site`.

## Repository setup

Public repository: `Ding-Ding-Projects/precision-cad`. Discussions and wiki are enabled. Organization Project 36, `Precision CAD delivery`, is formally linked. The public website is `https://ding-ding-projects.github.io/precision-cad/`; the repository homepage points there. Setup progress is on issue #1 and rolling Discussion #2. The complete cross-surface feature contract is tracked in issue #3.

The wiki is enabled but its Git endpoint is not initialized and reports repository-not-found. No wiki page is claimed published. Required documentation is currently committed under `docs/` and exposed in the website.
