# Release readiness

Precision CAD has a published development prerelease, `v0.1.0-dev.20260907.1`, at source `06f872e`. Its attached unsigned development assets include `Setup.exe`, `RELEASES`, and a full package. This historical prerelease does not establish installer execution, installed application launch, automatic updates, or a complete product release.

The native source baseline at `0ea2a35` completed 6/6 focused CTest targets. Those tests establish targeted native behavior, not exhaustive desktop interaction or release readiness. The hand-written surface inventory remains structurally valid with 128 incomplete cells, so its normal completion command intentionally returns red:

```text
node scripts/check-surface-completeness.mjs --inventory-only
node --test tests/contracts/surface-completeness.test.mjs
node scripts/check-surface-completeness.mjs
```

The first two commands are expected to pass. The last command is expected to report all incomplete cells until each one has implementation, focused tests, built interaction receipts, promoted captures, privacy review, and deliberate negative-regression evidence.

## Current blockers

- The native UI foundation is mounted, but the complete desktop feature contract remains unfinished.
- Raw native screenshots are private per-pass evidence and have not been promoted as public completeness proof. Issue [#4](https://github.com/Ding-Ding-Projects/precision-cad/issues/4) owns the runtime ledger.
- No installed-runtime or automatic-updater proof exists.
- The signer observer's exhaustive process observation is blocked by `AccessDenied`; its bounded polling result is incomplete rather than a substitute for exhaustive proof.

No new full release is claimed by this document. A full release remains blocked until all feature cells, installation and updater evidence, capture promotion, release verification, and the required observer proof are complete.
