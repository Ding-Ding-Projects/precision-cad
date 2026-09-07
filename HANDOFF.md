# Current implementation state

## Baseline

Started from an empty workspace on 2026-09-07. The public source repository is `Ding-Ding-Projects/precision-cad`. No pre-existing CAD implementation was imported.

## Implemented

The product architecture, roadmap and public repository are established. The initial website implements an overview, seven planned-feature articles, the roadmap, documentation search and Markdown export, English/Cantonese/bilingual reading modes, light/dark themes and browser-local preference persistence. The native CAD product is not implemented.

## Native development packaging

The native development release route is `build-installer.bat /s`. It builds with `BUILD_TESTING=OFF`, stages Qt and pinned Open CASCADE runtime files, creates a NuGet input package, and calls genuine Squirrel.Windows `Squirrel.exe --releasify`. Its required local outputs are `Setup.exe`, `RELEASES`, and one or more `*-full.nupkg` files in `artifacts/native/squirrel-windows`. The package is intentionally unsigned. It does not publish a release, run an installer, or prove a fresh-machine bootstrap.

## Next work

Establish the native build and document/geometry interfaces, then implement the foundation increment. Keep all subsequent increments explicitly incomplete until their acceptance scenarios pass.

## Evidence

The static website build passed locally with Node.js 22.23.2 after updating the generated starter's vulnerable packages. The package audit reported zero known vulnerabilities. Seven built-page runtime states passed after repairing the narrow-screen source link: overview, documentation and roadmap at 1440 px, Cantonese/dark preferences at 1440 px, bilingual/dark preferences at 390 px, and overview at 390 and 320 px. These checks reported no body overflow, unnamed controls, runtime exceptions or failed resources. This is focused evidence, not the complete surface/scale matrix. No native build or numerical verification is claimed.

Verified website source: `463040400b37c98e09ec4e89269699f9bc307fd8`.
GitHub Pages run `34095376173` succeeded. The public page and a project-scoped JavaScript asset returned HTTP 200, and `/precision-cad/provenance.json` identified that exact source commit. Its recorded build timestamp is `2026-09-07T07:26:10.939Z`.
The owner-private Sites deployment also succeeded for the same source. Its URL is `https://precision-cad.dayteetjer.chatgpt.site`.

## Repository setup

Public repository: `Ding-Ding-Projects/precision-cad`. Discussions and wiki are enabled. Organization Project 36, `Precision CAD delivery`, is formally linked. The public website is `https://ding-ding-projects.github.io/precision-cad/`; the repository homepage points there. Setup progress is on issue #1 and rolling Discussion #2. The complete cross-surface feature contract is tracked in issue #3.

The wiki is enabled but its Git endpoint is not initialized and reports repository-not-found. No wiki page is claimed published. Required documentation is currently committed under `docs/` and exposed in the website.
