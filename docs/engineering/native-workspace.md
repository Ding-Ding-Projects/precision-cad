# Native modelling workspace

The Windows Qt 6/QML host creates and edits revisioned native documents through copied candidates. The current document, persisted path, dirty state, mesh and measurements remain committed state until every active feature has regenerated successfully. Opening another file uses the same staged transaction. A successful open is clean; an unsuccessful open leaves the complete previous state intact. Inch-based documents are explicitly rejected before mutation; the current authoring workspace supports millimetres only.

## Worker transaction boundary

Each request has a unique operation identifier, protocol version, document identifier and revision. Each process callback additionally captures an immutable transaction epoch and its exact process instance. Commit verifies that the source document identifier and revision still match the transaction base. Requests made while busy report a busy message without cancelling the active candidate. Startup errors, timeout, cancellation, malformed replies and worker exit failures clear the candidate deterministically without committing it.

Before writing request input, the parent assigns the Windows worker to a kill-on-close Job Object with a 512 MiB process and job memory limit and one active process. Each process has a maximum 45-second timer, including startup. Both stdout and stderr are drained incrementally in at most 64 KiB reads; each channel is capped at 8 MiB. The aggregate candidate result cache has a 64 MiB compact-JSON byte budget, checked before retaining each new result. The committed cache is independently bounded by the same budget and remains alive until a successful replacement; Qt container overhead and the bounded in-flight reply are additional memory. `PRECISION_WORKER_CACHE_BYTES` can lower this budget for deterministic tests but cannot raise its production maximum. Cancellation disconnects old callbacks, invalidates the epoch, terminates the child and performs at most one second of bounded cleanup waiting. Subsequent requests use fresh process objects.

Replies must match the full identity envelope and exact supported success/result/mesh field counts. Validation requires a nonempty BREP string, a true validity flag, finite nonnegative volume, finite ordered bounds and vertices within absolute coordinate magnitude 1e9, matching finite normals, positive tessellation tolerances and complete triangles whose integer indices address existing vertices. This is transport and display validation, not independent BREP kernel revalidation. The geometry worker remains authoritative for shape validity.

## Save, selection and presentation

Saving compares the target's absolute normalized identity with the currently loaded path. Saving the same document supplies its last persisted revision, including after multiple unsaved edits. Save As to a new destination supplies no prior revision. Existing unrelated destinations remain protected by the native storage contract. Local file dialogs pass QUrl values through C++ conversion; nonlocal URLs are refused.

Selecting a tree body displays that body's committed mesh and measurements. Suppressing every body clears the viewport. The viewport projects actual returned triangles using QQuickPaintedItem, with orbit, pan, zoom and fit. Painter ordering remains a limited initial visualization method; it is not depth-buffered CAD rendering. Unknown or stale body identifiers do not silently select another body.

The QML host imports the Material controls explicitly and binds light/dark/system appearance, accent, font scale and the viewport palette to native preferences. Toolbar actions wrap; settings scroll. The English, Cantonese and bilingual selectors use the backend's en/yue/both codes. Two independent integer tone controls span 1 through 5 and select distinct language-specific versions of the body-selection guidance. Other current labels retain precise bilingual wording. Tone styling of every message category, full narration and the broader universal feature inventory remain unfinished and are not implied by these controls.

Current shipped labels pass through a single private presentation adapter after language selection. Document labels, identifiers, dimensions, paths, build facts and raw diagnostic facts are never passed to private vocabulary substitution. Document-controlled labels and diagnostics render as plain text. Vocabulary loading accepts only a locally selected file; the validated cache remains in the private profile. The UI exposes load, replace, clear and generic success/refusal state. It never displays payloads or source paths.

Save/Discard/Cancel protects New, Open and Close. Cancelling the save picker or a failed save clears the deferred action, so a later unrelated save cannot trigger an earlier destructive intent. A discard-close explicitly authorizes that one close. Active geometry must finish or be cancelled before closing.

Build provenance is supplied by the top-level build through PRECISION_CAD_VERSION and PRECISION_CAD_BUILD_TIME. Missing metadata remains unavailable. This slice does not claim complete localized provenance formatting, all universal surface contracts, CAM, FEA, depth-buffered rendering or full project history.

## Focused verification

`tests/app` can configure independently with Qt 6.8 and MSVC, or participate in the parent build. It builds the actual application and two QtTest executables. Test runtime PATH derives from the imported Qt Core target. Tests write bounded result files into the build directory.

The controller suite covers startup failure, invalid dimensions, thirteen malformed/oversized/mismatched result cases, busy rejection, cancellation and restart, timeout, Save As, multiple edits between saves, failed/successful open, explicit imperial refusal, body selection, clearing all geometry, coordinate acceptance at 1e9/refusal above 1e9, and aggregate cache rejection after multiple individually admissible results. Failure tests start with a nonempty committed document. The fake subprocess checks the real Job Object limits after receiving input.

The QML test instantiates the actual Main.qml with isolated temporary preferences and validates all three language modes, all five independent tone values, light/dark propagation into the viewport, local URL conversion and deferred-save cancellation/failure. This is runtime binding evidence, not a screenshot or visual layout certification. Headless built-window captures and the wider display-scale matrix are separate evidence owned by the integration task.

Commands, after entering the supported MSVC x64 environment:

```powershell
cmake -S tests/app -B build/app-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt-6.8-msvc-prefix>
cmake --build build/app-msvc --parallel 4
ctest --test-dir build/app-msvc --output-on-failure
```
