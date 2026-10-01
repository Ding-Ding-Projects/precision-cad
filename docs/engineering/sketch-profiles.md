# Sketch profiles

`precision::sketch::solveAndExtractProfiles` solves one immutable `SketchModel` and then converts that exact result into analytic profile records for a later geometry worker. It returns solver state and profiles, so an under-constrained profile always discloses remaining solver DoF. It does not construct BRep faces, publish Open CASCADE types, or mutate the input.

Each emitted segment retains its stable source entity ID, exact solved line endpoints or circle center/radius, and the model's datum remains the governing sketch basis. A loop ID is derived from sorted source entity IDs, so dimension edits that retain topology do not alter identity. Regions identify one outer loop and zero or more hole loop IDs. Nested islands become independent outer regions.

Only lines and circles currently extract. Arcs receive an explicit `unsupported-curve` diagnostic because the stored model does not yet define a directed sweep convention for a profile boundary. The extractor never replaces an arc with chords or polygons. There is no construction-geometry flag in schema version 1, so all model curve entities participate.

Endpoints connect only when they share a point ID or an explicit `Coincident` constraint joins their point IDs. Equal solved coordinates alone never close a gap. Every line component must be degree two and return to its starting topological endpoint. The result diagnoses dangling/open/branched graph topology, self-intersection, touching or intersecting loops, missing solver association, and ambiguous nesting. Any diagnostic yields no valid loops or regions.

Containment uses exact line segment tests and analytic circle tests. Segment-circle contact solves the finite segment's quadratic circumference intersection, so a segment wholly inside or wholly outside a circle is not falsely called boundary contact. Tolerances scale from the local solved geometry and coordinate magnitude, with determinant bounds derived from segment lengths; this retains conservative contact handling for very small profiles, large profiles, and valid profiles translated near the model coordinate limit. Output remains analytic; it never uses a chord approximation for returned geometry.

After nesting classification, each outer loop is canonical CCW and each hole loop CW. Line loops rotate to their lowest stable source entity ID. Loop, region, and hole output are then stable-ID sorted, so entity storage order does not affect the profile result.

`tests/sketch/profile_tests.cpp` uses actual SolveSpace results for a constrained rectangle with a circular hole, verifies stable source and loop IDs after a dimension edit, and covers dangling, branched, and touching geometry rejection. This is a profile-core Chut only. The geometry lane owns BRep face construction and downstream validation.
