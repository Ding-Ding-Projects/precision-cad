# Release readiness

Precision CAD has a published development prerelease, `v0.1.0-dev.20260907.1`, at source `06f872e`. Its attached unsigned development assets include `Setup.exe`, `RELEASES`, and a full package. This historical prerelease does not establish installer execution, installed application launch, automatic updates, or a complete product release.

The final native source `6a6902c90b199351a998b3a18d6af6aeaf24c2a4` completed 6/6 focused CTest targets in 23.05 seconds. The canonical `build-installer.bat /s` command exited successfully and wrote its candidate-bound receipt and unsigned Squirrel output under `artifacts/native/squirrel-windows/6a6902c90b19`. The wrapper accepts its documented silent aliases, preserves the child exit code, and the native runtime retains the exact `Precision CAD` organization identity used by the existing preferences store.

The `b26085aeaa83238b18edca6b4a8e2942e4998143` unpacked runtime has an actual 19-state per-click ledger covering focused model creation, editing, selection, undo, redo, and minimum-size behavior. A final default-renderer capture at `artifacts/verification/runtime-6a6902c/initial-dark-bilingual-150.png` confirms the application icon, and the 800 by 600 state from the same isolated profile was inspected. The version sits below the fold in that tuple, so these captures do not establish a complete front-screen or display-scale matrix.

Those tests and runtime observations establish targeted native behavior, not exhaustive desktop interaction or release readiness. The hand-written surface inventory remains structurally valid with 128 incomplete cells, so its normal completion command intentionally returns red:

```text
node scripts/check-surface-completeness.mjs --inventory-only
node --test tests/contracts/surface-completeness.test.mjs
node scripts/check-surface-completeness.mjs
```

The first two commands are expected to pass. The last command is expected to report all incomplete cells until each one has implementation, focused tests, built interaction receipts, promoted captures, privacy review, and deliberate negative-regression evidence.

## Current blockers

- The native UI foundation is mounted, but the complete desktop feature contract remains unfinished.
- Raw native screenshots are private per-pass evidence and have not been promoted as public completeness proof. Issue [#4](https://github.com/Ding-Ding-Projects/precision-cad/issues/4) owns the runtime ledger.
- Installer execution and installed-runtime proof do not exist. Automatic updater behavior is not implemented.
- The signer observer's exhaustive process observation is blocked by `AccessDenied`; its bounded polling result is incomplete rather than a substitute for exhaustive proof.

No new full release is claimed by this document. A full release remains blocked until all feature cells, installation and updater evidence, capture promotion, release verification, and the required observer proof are complete.
