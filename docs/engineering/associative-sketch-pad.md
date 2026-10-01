# Associative sketch and pad

This bounded modeling workflow creates an actual constrained planar sketch, then a separate pad feature referencing one of its stable analytic regions. The guided editor supports a rectangle with one circular hole on XY, YZ, or ZX. It is not a freehand sketch editor and does not claim arc, spline, or arbitrary constraint editing.

## Use

Choose **Sketch**, set the datum, width, height, hole radius and local U/V center coordinates, and save the sketch. The worker solves horizontal, vertical, distance, radius and fixed-point constraints with the pinned SolveSpace 3.2 library. The model tree retains that sketch as an independent feature. Its planar preview and degrees of freedom come from the solved worker result, not from the editor's tentative fields.

Select the sketch, choose **Pad**, select its stable region and enter a positive length in millimetres. Open CASCADE constructs analytic straight and circular edges, oriented outer and hole wires, a face on the sketch datum, and a prism along the positive datum normal. The circular hole remains analytic rather than being approximated by a polygon or a separate cylinder subtraction.

Select the sketch and choose **Edit selected sketch** to change dimensions. The sketch and all downstream pads regenerate together. Select a pad and use **Edit selected dimensions** to change its length. Undo and redo replay complete model revisions. Saving and reopening stores feature definitions and references and regenerates geometry through the worker.

## Records and consistency

Native document schema 1 remains unchanged. A `sketch` feature has no inputs and exactly `parameters.model`, holding the complete version-1 sketch model. A `pad` feature has one sketch input reference and exactly `parameters.regionId` and `parameters.length`. Region identifiers derive from persistent sketch entity identity. They remain stable when dimensions change without changing the profile topology.

There is no duplicate editor dimension cache. Guided editing derives dimensions from the actual distance/radius constraints and fixed center point, and verifies the complete model against the supported record shape before allowing changes. Other valid imported line/circle sketch models can solve and produce pads, but are not misrepresented as editable guided rectangles.

Worker protocol 1 retains its original solid request and result behavior. Protocol 2 adds `sketch` and `pad` operations using the same document, revision and operation identity envelope. A sketch result is explicitly `kind: sketch`; it has no solid BREP, volume or triangle mesh. It contains the source model, producer feature ID, document ID, revision, solved status/DoF/conflicts and point/radius associations, analytic loops/regions and planar preview segments. The controller checks exact entity kind and endpoint associations, complete point/curve coverage, segment coordinates against returned solved records, canonical loop/region identities and one-time outer/hole ownership. This is structural consistency validation; numerical correctness still relies on the isolated worker. Pad input includes the producer sketch feature ID and the complete current-revision solved result. The worker re-solves that source model and compares the whole result before consuming the region. This validates consistency, not authenticity of externally supplied history.

## Transaction and resource boundaries

The UI constructs records only. It never calls the solver. The existing asynchronous process boundary applies to both operations: 45-second timeout, 512 MiB worker job limit on Windows, 4 MiB request limit, 8 MiB response limit and 64 MiB aggregate result cache. Failed, cancelled, stale, mismatched, overconstrained, touching or invalid profiles preserve the previously committed document and visible scene. Suppression propagates through the dependency graph. Candidate records become current only after every required feature completes successfully.

## Verification boundary

`SketchOperationsTests.cpp` exercises the real solver and Open CASCADE, including analytic hole volume, all three datum normals, stable region identity, rejected source tampering/revision mismatch, profile rejection and solver conflict. `tst_associative_workspace.cpp` drives the real controller with a test-only dispatcher sharing the production process parser and the actual numerical implementations. The adapter adds only protocol-2 routing; it supplies no synthetic geometry. Production dispatcher integration is separately required before shipping. Existing protocol-1 controller tests remain transport fixtures and are not numerical proof.

This feature slice does not publish an installer. Runtime captures and final production-dispatch acceptance belong to the integrated application verification pass.

## Preservation handoff, 2026-09-08

Implementation candidate `bff6defc6897c0ace2b2811529209e3a5229458b` completed the one-job native build, including the desktop executable, real numerical test executable, real process adapter, corrupted-reply adapter and controller/QML test executables. No tests ran at this final candidate before the user requested preservation and stopped new work. The source was clean and the build process had completed when this handoff was written.

Earlier candidate `42cd9169e5cd35a2c5ed5ddfb55768494d6ffa48` passed `sketch-operations`, all 6 associative workspace cases and all 36 existing controller cases. Its QML suite reported 9 passed and 1 failed: the real sketch/pad/edit QML workflow passed, while the older toolbar inventory lacked the two new controls. That inventory was corrected in `d0109a5`, but the later candidates have not been retested. The subsequent structural reply validator, nine malicious transport cases and extreme datum normalization cases are compiled but unverified.

Next owner: integrate the two-line protocol-2 dispatcher into `GeometryWorker.cpp`, then run the focused numerical/controller/QML suites against that exact integrated candidate. Run the profile regression after the shared identity-helper extraction, verify standalone geometry configuration/build, and perform the actual native UI interaction/capture pass. The test-only dispatcher is not production wiring. No installer was published, and no release or complete-CAD claim is warranted.
