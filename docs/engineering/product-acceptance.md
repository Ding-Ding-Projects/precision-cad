# Product acceptance contract

This is a finite acceptance-membership framework for the initial CAD suite. Fixture consistency is not a product or release verdict.

`scripts/check-product-acceptance.mjs` contains the hand-written `approvedCapabilityMembers` groups and a duplicate-free `requiredCapabilityIds` list of 189 independently required identifiers. The list covers the frozen foundation, sketch, viewport, solid and surface, exchange, daily workspace, assembly, drawing, sheet-metal, CAM, analysis, command API, shared-surface, installation, update, and release members. It is never discovered from a candidate record.

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

## Mechanical bracket workflow

The required sequence is `CAD-SKETCH-ENTITIES-001`, `CAD-SKETCH-CONSTRAINTS-001`, `CAD-SKETCH-EXPRESSIONS-001`, `CAD-SKETCH-PROFILE-001`, `CAD-SOLID-EXTRUDE-001`, `CAD-MODIFY-FILLET-001`, `CAD-DRAWING-PDF-001`, and `CAD-EXCHANGE-STEP-001`. Every record uses fixture ID `mechanical-bracket-v1`; revision lineage must advance and the sequence cannot move backwards.

## Evidence consistency and release acceptance

`contracts/product-acceptance-candidate.schema.json` defines the fixture record shape. `node scripts/check-product-acceptance.mjs --fixture-consistency path/to/candidate.json` checks only:

- source commit availability and the SHA-256 of its recursive source-tree listing;
- membership in the 189-ID list, duplicate rejection, and complete coverage;
- required strings and predecessor/result revision lineage;
- existence and SHA-256 consistency of local log and capture files;
- JSON interaction logs bound to the matching capability and fixture, with structured expected, actual, comparison, and passing assertions.

The default command also invokes the canonical 64-feature, two-surface completion check and hard-fails because no real runtime acceptance collector is registered. Hashes prove local file consistency only. They do not prove rendered pixels, runtime behavior, package provenance, installation, update behavior, or release publication.

The framework excludes proprietary native formats, turning and simultaneous five-axis machining, nonlinear/contact/thermal/fluid analysis, cloud collaboration, non-Windows delivery, and connected-machine control, as recorded in `ROADMAP.md`.
