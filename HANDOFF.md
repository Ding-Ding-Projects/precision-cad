# Current implementation state

## Baseline

Started from an empty workspace on 2026-09-07. The public source repository is `Ding-Ding-Projects/precision-cad`. No pre-existing CAD implementation was imported.

## Implemented

The product architecture, roadmap and public repository are established. The initial website implements an overview, seven planned-feature articles, the roadmap, documentation search and Markdown export, English/Cantonese/bilingual reading modes, light/dark themes and browser-local preference persistence. The native foundation includes a mounted Qt Quick desktop surface, C++20 document, history, preference, geometry-worker and Qt desktop targets, plus an unsigned Squirrel.Windows package route. This remains an early development foundation, not a complete professional CAD release.

## Native development packaging

The native development route is `build-installer.bat /s`. It builds with `BUILD_TESTING=OFF`, stages Qt and pinned Open CASCADE runtime files, creates a NuGet input package, and calls genuine Squirrel.Windows `Squirrel.com --releasify` synchronously. Its required outputs are `Setup.exe`, `RELEASES`, and one or more `*-full.nupkg` files in a candidate-specific `artifacts/native/squirrel-windows/<commit>` directory. The published development prerelease `v0.1.0-dev.20260907.1` at `06f872e` carries those unsigned assets. Neither that historical prerelease nor local package construction proves installer execution, an installed runtime, automatic updates, a fresh-machine bootstrap, or a full release.

All root batch bootstrap wrappers now run the inbox Windows PowerShell host with a child-only Windows PowerShell module path. This preserves built-in hash and utility commands when the parent environment contains PowerShell 7 module locations. The regression script is `tests/scripts/windows-powershell-bootstrap.test.ps1`; it checks the wrapper contract and a polluted module-path child process.

The installer wrapper accepts `/s`, `--silent`, and `-Silent`, forwards an optional package version, preserves the child process exit code, and honors the existing silent environment switch. The native runtime keeps the exact `Precision CAD` organization identity, so the executable identity repair does not relocate the established preferences store.

## Next work

Complete fresh-machine acquisition for Qt and MSVC, updater behavior, broader CAD capabilities, exhaustive signer observation, and the remaining professional-suite acceptance scenarios. The full release remains unpublished. Keep all subsequent increments explicitly incomplete until their acceptance scenarios pass.

## Evidence

The static website build passed locally with Node.js 22.23.2 after updating the generated starter's vulnerable packages. The package audit reported zero known vulnerabilities. Seven built-page runtime states passed after repairing the narrow-screen source link: overview, documentation and roadmap at 1440 px, Cantonese/dark preferences at 1440 px, bilingual/dark preferences at 390 px, and overview at 390 and 320 px. These checks reported no body overflow, unnamed controls, runtime exceptions or failed resources. This is focused evidence, not the complete surface/scale matrix.

The final native source `6a6902c90b199351a998b3a18d6af6aeaf24c2a4` completed 6/6 focused CTest targets in 23.05 seconds. The canonical `build-installer.bat /s` command then exited successfully and produced its candidate-bound receipt and unsigned Squirrel output under `artifacts/native/squirrel-windows/6a6902c90b19`. These results establish focused tests and local package construction, not installer execution or installed application runtime.

The unpacked native runtime at `b26085aeaa83238b18edca6b4a8e2942e4998143` has one actual per-click interaction ledger containing 19 inspected states, sequences 0 through 18. It covers launch, box and cylinder creation and editing, selection, operand preservation, undo, redo, full-window capture, and an 800 by 600 client resize. The final `6a6902c` default-renderer run additionally confirms the application icon in `artifacts/verification/runtime-6a6902c/initial-dark-bilingual-150.png`; the 800 by 600 state from the same isolated profile was also inspected. The version is below the fold in that tuple, so this is not complete front-screen or display-scale evidence. No installed application runtime, promoted native screenshot, automatic-updater, or full-release verification is claimed.

Verified website source: `463040400b37c98e09ec4e89269699f9bc307fd8`.
GitHub Pages run `34095376173` succeeded. The public page and a project-scoped JavaScript asset returned HTTP 200, and `/precision-cad/provenance.json` identified that exact source commit. Its recorded build timestamp is `2026-09-07T07:26:10.939Z`.
The owner-private Sites deployment also succeeded for the same source. Its URL is `https://precision-cad.dayteetjer.chatgpt.site`.

## Repository setup

Public repository: `Ding-Ding-Projects/precision-cad`. Discussions and wiki are enabled. Organization Project 36, `Precision CAD delivery`, is formally linked. The public website is `https://ding-ding-projects.github.io/precision-cad/`; the repository homepage points there. Setup progress is on issue #1 and rolling Discussion #2. The complete cross-surface feature contract is tracked in issue #3.

The wiki is enabled but its Git endpoint is not initialized and reports repository-not-found. No wiki page is claimed published. Required documentation is currently committed under `docs/` and exposed in the website.
