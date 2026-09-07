# Product acceptance contract

This contract defines a finite completion target for the initial professional CAD suite. It is an acceptance boundary, not a promise to implement every possible CAD feature.

The mandatory capability identifiers are hand-written in `scripts/check-product-acceptance.mjs`. The candidate evidence must contain exactly one real built-artifact interaction record for each identifier. The validator rejects an unregistered identifier, a duplicate, or a missing identifier. It does not discover the required list from candidate JSON or documentation.

| Capability | Acceptance scope |
| --- | --- |
| `CAD-DOC-001` | Exercise versioned document records, transactions, and monotonic undo and redo. |
| `CAD-SAVE-001` | Prove atomic save, an exclusive-writer conflict, and recovery. |
| `CAD-GEOMETRY-001` | Prove revision-bound geometry cancellation and stale-result rejection. |
| `CAD-VIEWPORT-001` | Exercise built viewport selection and feature/property editing. |
| `CAD-HISTORY-001` | Exercise local Git comparison, preservation, and non-destructive restoration. |
| `CAD-SKETCH-001` | Create and constrain the bracket profile with dimensions and expressions. |
| `CAD-SOLID-001` | Produce a bracket solid from the sketch using documented feature operations. |
| `CAD-REF-001` | Preserve and repair stable references after an edited upstream feature. |
| `CAD-ASSEMBLY-001` | Place the bracket in an assembly, solve mates, report interference, and produce a bill of materials. |
| `CAD-DRAWING-001` | Produce an associative bracket drawing and export PDF plus DXF. |
| `CAD-SHEET-001` | Create a sheet-metal part, unfold it, and refold it without losing the documented model relation. |
| `CAD-EXCHANGE-001` | Round-trip the declared neutral exchange formats for the acceptance fixture. |
| `CAD-CAM-001` | Validate 2.5D and three-axis stock, tools, setup, and generated toolpaths. |
| `CAD-CAM-002` | Simulate a toolpath, report collisions, and export generic G-code. |
| `CAD-MESH-001` | Generate the declared Gmsh mesh through the adapter. |
| `CAD-ANALYSIS-001` | Run a linear-static study through the CalculiX adapter against its analytic benchmark. |
| `CAD-ANALYSIS-002` | Run a modal study through the CalculiX adapter against its analytic benchmark. |
| `CAD-SCRIPT-001` | Exercise the versioned command API against the acceptance fixture. |
| `CAD-SURFACE-001` | Prove the shared 64×2 desktop and website contract through its executable inventory Chut. |
| `CAD-RELEASE-001` | Prove final installed-runtime or Squirrel installer, update, publication, and download provenance. |

## Mechanical bracket workflow

The first required workflow is a mechanical bracket. It starts with `CAD-SKETCH-001`, produces the solid through `CAD-SOLID-001`, edits an upstream feature and proves `CAD-REF-001`, emits the associated drawing through `CAD-DRAWING-001`, then verifies neutral exchange with `CAD-EXCHANGE-001`. The final candidate also needs the remaining finite capability records above. The validator makes the bracket sequence explicit, so an unrelated collection of evidence cannot substitute for it.

## Evidence candidate

`contracts/product-acceptance-candidate.schema.json` defines the candidate shape. `scripts/check-product-acceptance.mjs` is the executable Chut and adds conditions that a general JSON-schema reader cannot establish:

- `sourceCommit` must resolve to a commit in the candidate repository and `sourceTreeSha256` must equal the SHA-256 of that commit's recursive `git ls-tree --full-tree` listing.
- `releasePhase` is exactly `final-single-full-release`. An intermediate installer is never a completion candidate.
- `actualProof` names an installed runtime or Squirrel installer, a hash-bound provenance receipt with the same source commit, and the artifact's SHA-256 hash.
- Each interaction records its fixture, language, theme, viewport, display scale, screen, state, action, expected outcome, a hash-bound structured log, and a hash-bound PNG capture. The log requires actual assertion values, passing verdicts, a privacy verdict, and an accessibility verdict.
- Every local evidence path must stay under the supplied candidate root, exist, and match its recorded SHA-256 hash.

No sample candidate is checked in because invented evidence would be worse than an empty record. A candidate is complete only when it carries actual proof from the built artifact. Run the validator from the candidate root:

```powershell
node scripts/check-product-acceptance.mjs path/to/candidate.json
```

The contract deliberately excludes native proprietary CAD formats, turning and simultaneous five-axis machining, nonlinear/contact/thermal/fluid analysis, cloud collaboration, non-Windows delivery, and connected-machine control. Those exclusions remain recorded in `ROADMAP.md` and cannot be satisfied by this finite acceptance evidence.
