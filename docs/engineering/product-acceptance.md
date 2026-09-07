# Product acceptance contract

This contract defines a finite, explicit acceptance inventory for the initial professional CAD suite. It is an acceptance boundary, not a promise to implement every possible CAD feature. Its evidence-consistency mode validates only a declared fixture record. It never proves a release.

The mandatory capability identifiers are hand-written in `scripts/check-product-acceptance.mjs`. The summary table below is historical orientation only, not the complete identifier list. `approvedCapabilityMembers` is the authoritative current membership table and expands each named capability into an independently required ID. The validator rejects an unregistered identifier, a duplicate, or a missing identifier. It does not discover the required list from candidate JSON or documentation.

## Explicit member groups

| Group | Independently required members |
| --- | --- |
| Foundation | transactions, undo, redo, atomic save, exclusive writer, recovery, cancellation, stale result, bounds, migration |
| Sketch | datum, point, line, polyline, rectangle, circle, arc, spline, construction, snap, trim, offset, closed profile, coincident, horizontal, vertical, parallel, perpendicular, tangent, equal, distance, radius, angle, degrees of freedom, conflict, unit expressions |
| Viewport | depth, normals, multibody, orthographic, perspective, standard views, section, face, edge and vertex picking, orbit, pan, zoom, fit |
| Solid and surface | pad, pocket, revolve, groove, hole, union, cut, intersection, translate, rotate, fillet, chamfer, shell, draft, mirror, linear pattern, circular pattern, loft, sweep, surface trim, sew, offset, thicken |
| Exchange | import and export for STEP, IGES, STL and 3MF, units, source identity, healing, loss disclosure |
| Daily workspace | document tabs, tree, properties, measure, recent, recovery, history compare, restore, keyboard, examples |
| Assembly | components, nested instances, fixed, coincident, concentric, parallel, perpendicular, distance, angle, degrees of freedom, conflicts, interference, exploded view, bill of materials |
| Drawing | base, projected, section, detail, dimensions, tolerances, annotations, title blocks, bill of materials, revision, PDF, DXF |
| Sheet metal | thickness, bend parameters, base wall, flange, bend, relief, cut, unfold, refold, bend table, flat DXF |
| CAM | stock, tools, fixtures, WCS, facing, profile, pocket, drill, three-axis roughing, three-axis finishing, simulation, collision, generic G-code |
| Analysis | material, load, restraint, mesh, static, modal, results, revision, convergence |
| Command API | API, discovery, diagnostics, batch, script client |

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

`contracts/product-acceptance-candidate.schema.json` defines the candidate shape. `scripts/check-product-acceptance.mjs --fixture-consistency` is the executable fixture-consistency Chut. It does not claim built-runtime success. Its default release-acceptance mode additionally invokes the canonical 64-feature, two-surface completion check and always fails closed until a real runtime collector is registered.

- `sourceCommit` must resolve to a commit in the candidate repository and `sourceTreeSha256` must equal the SHA-256 of that commit's recursive `git ls-tree --full-tree` listing.
- `releasePhase` is exactly `final-single-full-release`. An intermediate installer is never a completion candidate.
- `actualProof` names an installed runtime or Squirrel installer, a hash-bound provenance receipt with the same source commit, and the artifact's SHA-256 hash.
- Each interaction records its fixture, language, theme, viewport, display scale, screen, state, action, expected outcome, a hash-bound structured log, and a hash-bound PNG capture. The log requires actual assertion values, passing verdicts, a privacy verdict, and an accessibility verdict.
- Every local evidence path must stay under the supplied candidate root, exist, and match its recorded SHA-256 hash.

No sample candidate is checked in because invented evidence would be worse than an empty record. Hashes and captures prove file consistency, not that pixels represent a truthful running product. The collector registration must bridge that gap with actual runtime, installer, update and release receipts before a full verdict can exist. Run the default Chut from the candidate root:

```powershell
node scripts/check-product-acceptance.mjs path/to/candidate.json
```

For record-consistency development only, use `--fixture-consistency`. That mode is deliberately not a release claim.

The contract deliberately excludes native proprietary CAD formats, turning and simultaneous five-axis machining, nonlinear/contact/thermal/fluid analysis, cloud collaboration, non-Windows delivery, and connected-machine control. Those exclusions remain recorded in `ROADMAP.md` and cannot be satisfied by this finite acceptance evidence.
