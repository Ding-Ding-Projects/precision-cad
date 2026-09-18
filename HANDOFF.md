# Current implementation state

## 2026-09-18 final preservation and cleanup result

The primary checkout was fetched from `origin`, refreshed on `main`, and dewed at `e9e628707207016bad9b656fc796a916b9368b0f`. The external archive was created and fully tested before removal: `C:\Users\cntow\OneDrive\OakKayBackups\precision-cad\zips\precision-cad-20260918T173107Z.7z`, 2,319,380 bytes, 3,145 files, 292 folders, Git admin `HEAD` present, and 7-Zip CRC test `Everything is Ok`. The source list contained 2,265 entries and ignored build output was excluded according to Git's ignore rules.

The following clean, completed feature branches were proven locally and through `git ls-remote` to be ancestors of `origin/main`, then their linked checkouts and local and remote refs were removed: `feat/constrained-sketch-core` at `5ec0a87f3a334253d23dca5c02c0c0e437e82dc1`, `feat/dimensional-expressions` at `b5a2fe6fc93db5b6f84e3a39715482d706019b24`, `feat/geometry-topology-map` at `1cedfbf8480dee958556d9be9894824ee953fab7`, `feat/native-3d-viewport` at `222b3bace25bd9c943e5c368b2b0617f781f8ac9`, `feat/native-capture-diagnostics` at `96e98820519c47ee826bf27a4c3b2eb06202c086`, `feat/native-update-client` at `6e81a4d9b772b40d26c4996dfc8c6cd27f0851ef`, `feat/product-acceptance-contract` at `584c754e7a6487c8cc7796ccc7355615c9240fbe`, `feat/release-environment` at `b7c96ab0c995996f13a623a0775a30f453bc4a74`, `feat/sketch-profiles` at `40aba038e6fa1880b22c882d7fbe20c0e2f2b6df`, and `feat/typed-model-evaluator` at `621a9363bdfd2243568007ef23d8749c36b78a29`.

`feat/associative-sketch-pad` remains active and unmerged at `1bb308ac6bc6fb402a458af5c09c2c24d6c6dc`. Its five commits remain on the matching `origin/feat/associative-sketch-pad` ref, but the branch is five commits behind `origin/main` and is not an ancestor of `main`. Its own preservation note states that production dispatcher wiring, final regressions, standalone linkage, and native sketch/pad workflow evidence remain pending. Its linked checkout, local ref, and remote ref were retained.

No conflict resolution was required because every index was already resolved and no conflict markers were present. No preservation commit was required because every discovered checkout and the stash namespace were clean. One non-obvious Windows cleanup choice is recorded: `git worktree remove` pruned metadata for the clean `feat/constrained-sketch-core` checkout but left its directory shell after reporting `Filename too long`; after the archive passed its full CRC test, that exact proven directory was removed and the ref was deleted. The remaining clean directories were then removed, stale metadata was pruned, and remote-tracking refs were fetched with pruning. No release, installer, or unrelated issue work was performed. Open issues #1, #3, and #4 remain open because their documented website, cross-surface contract, and associative-workflow completion criteria are outside this preservation-only closeout.

廣東話：主工作區先由 `origin` 更新，再由 `main` dewed 到指定版本。十條已經整合嘅 feature branch 經 ancestry 同 `git ls-remote` 雙重核對後安全清理；associative sketch branch 仍然未完成，保留原位，唔會扮完成而合併或刪除。外部 archive 先完成並通過 CRC，先至做 Cup Chun。今次冇衝突需要處理，亦冇遺留 stash 或未保存工作，唔搞 release，唔郁無關 issue。

## 2026-09-18 preservation and integration inventory

The primary checkout was fetched from `origin` before inspection and is clean at `0c73992b94f7de373ba590cc2b7f918074589322`. The repository has 11 linked checkouts under `C:\Users\cntow\Documents\GitHub\gerk tong hui`, all belonging to this repository. Every linked checkout was inspected and was clean, with no unmerged index entries, no conflict markers, and no untracked non-ignored files. No Git stash exists.

Ten feature branches are already ancestors of `origin/main`: `feat/constrained-sketch-core`, `feat/dimensional-expressions`, `feat/geometry-topology-map`, `feat/native-3d-viewport`, `feat/native-capture-diagnostics`, `feat/native-update-client`, `feat/product-acceptance-contract`, `feat/release-environment`, `feat/sketch-profiles`, and `feat/typed-model-evaluator`. Their linked checkouts are retained until the external archive and ancestry checks complete.

`feat/associative-sketch-pad` remains active and unmerged at `1bb308ac6bc6fb402a458af5c09c2c24d6c6d6dc`. Its five commits are present on the matching `origin/feat/associative-sketch-pad` ref, but the branch is five commits behind `origin/main` and is not an ancestor of `main`. Its own preservation note states that production dispatcher wiring, final regressions, standalone linkage, and native sketch/pad workflow evidence remain pending. It is therefore retained as active unfinished work and is not merged or removed in this closeout.

No conflict resolution was required because all index entries were resolved before this inventory and no conflict markers were present. No preservation commits were needed because every discovered checkout and the stash namespace were already clean. No release, installer, or unrelated issue work was performed. Open issues #1, #3, and #4 remain open because their documented website, cross-surface contract, and associative-workflow completion criteria are outside this preservation-only closeout.

廣東話：主工作區先由 `origin` 更新，再逐個檢查 11 個屬於本 repository 嘅 linked checkout。全部都乾淨，冇未合併 index、冇衝突標記、冇未追蹤非忽略檔案，亦冇 stash。十條 feature branch 已經喺 `main` 入面；associative sketch branch 仍然未完成，保留原位，唔會扮完成而合併或刪除。今次只做保存、整合盤點同安全收尾，唔做 release。

## 2026-09-08 preservation closeout

The user stopped new implementation and requested preservation and cleanup. Accepted work is integrated through `b8e0aaa428ceed2c86cacbe9ca93d0169f7e2af9`, including the repaired worker topology maps. The topology suite passed 18 QtTest rows, zero failed or skipped, with independent acceptance. Historical producer binding and persistent topology selection in the UI remain separate unfinished work. The website workflow for that source succeeded at run `34289239422`.

Unfinished associative modeling is preserved on `feat/associative-sketch-pad` at `1bb308ac6bc6fb402a458af5c09c2c24d6c6d6dc`. Its implementation source `bff6defc6897c0ace2b2811529209e3a5229458b` completed the native build, but final tests did not run after the preservation request. Earlier source `42cd916` passed numerical sketch operations, six associative-controller cases and 36 existing controller cases; its QML result was nine passed and one failed because the toolbar inventory lacked the two new actions. That assertion and later structural/datum findings are source-repaired, compiled, and awaiting final verification.

The next integration must add `#include "SketchOperations.h"` and route protocol version 2 from `executeRequest()` to `executeSketchRequest(request)` in the production geometry dispatcher. Preserve protocol version 1 and the accepted topology map contract. Then run the numerical, associative-controller, existing controller, QML, and sketch-profile regressions against the exact integrated commit; verify standalone geometry linkage; and drive the real sketch/pad/edit/save/reopen workflow on a hidden desktop. The test-only dispatcher is not production wiring. Retain this branch and linked checkout until those checks and integration are complete.

All completed task branches were inventoried for a fresh verified OneDrive archive and ancestry-proven cleanup. Generated build probes were moved into their owning ignored build directories before archiving. The unfinished associative checkout and historical branches with uncertain ownership are retained. No installer, full release, installed update, complete shared-surface inventory, or complete CAD suite is claimed.

## 2026-09-08 continuation

The integrated source baseline is `8b092b53fa5854e79107536d8baebe0a8d01231e`. The product is still incomplete and no intermediate installer release is authorized. The only published installer remains `v0.1.0-dev.20260907.1`.

Implemented foundations now include deterministic topological evaluation with suppression propagation and version-one parameter compatibility; multi-body Qt Quick 3D rendering with live picking, Z-up standard views, selection preservation and highlighting; a persistent version/provenance footer; the real pinned SolveSpace adapter; analytic line/circle profile extraction; bounded dimensional expressions; a native HTTPS/Squirrel update backend; protected disposable-environment preflight; an explicit 189-member acceptance registry; and sealed native diagnostic collection.

Verification is commit-specific. The combined native build at `d52e989444a6ac2323cc48ec30ed261da1652993` passed all 13 CTest targets in 239.64 seconds. After diagnostics integration, the application and relevant targets built at `8b092b5`; `native-diagnostics` passed in 2.62 seconds and `workspace-qml` in 2.51 seconds. The acceptance framework passed 4 tests, including removal of each of 189 distinct members; its release mode intentionally remains red while 128 surface cells and real acceptance collectors are incomplete.

Actual hidden-desktop interaction at `f29406b` showed two bodies, direct viewport selection, and a cylinder radius edit from 12 to 10 mm while retaining the selected body and its 45 mm height. Its volume became 14137.1669412 mm3. Those raw captures remain private and were not promoted. A new `8b092b5` front-screen run on 2026-09-08 ended normally with exit code zero and a matching sealed, complete diagnostics receipt with zero Qt/QML warnings, criticals, or fatals. This proves only that inspected run, not installed-runtime or full feature completeness.

The next active slice is first-class editable sketch and dependent pad features through isolated worker version 2. The existing solver, profiles and expressions are not yet wired into a complete modeling workflow. The updater backend is not mounted in the shell, and this host's broad AppData ancestry is refused by its private-staging checks without changing permissions. Hyper-V inventory is authorization-refused; no disposable guest or installed update proof exists.

At the start of this continuation, `feat/geometry-topology-map` was preserved at `0048dd67fdfc858044d1c4e18c5fc2648703f4dc` with six review defects. Those worker defects were subsequently repaired and integrated at `b8e0aaa`; see the preservation closeout above. Assemblies, drawings, sheet metal, exchange, CAM, analysis, the complete shared contract and the final release remain unfinished.

The sections below retain earlier development evidence and do not supersede this continuation record.

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
