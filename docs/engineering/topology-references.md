# Topology references

The geometry worker keeps protocol version 1. A standard request produces the existing result shape. A caller explicitly opts into topology with `topologyVersion: 1` and a UUID `producerFeatureId`; every returned entity source uses that feature identifier, never the per-run operation identifier. An opted-in `validate` request includes the topology mesh; an ordinary `validate` request retains its original non-mesh result.

## Map contract

`result.topology` contains exactly `version`, `brepSha256`, `producer`, `entities`, `triangleFaceIds`, `edgePolylines`, and `vertices`. `version` is 1. `producer` contains `featureId` and `operation`. `brepSha256` hashes the decoded, exact serialized BREP bytes in the same result.

Entity identifiers are positive integers unique across the entire reply: vertices first, edges offset by the vertex count, and faces offset by both counts. They are local indices for that evaluated BREP, not persistent selection identities. Each entity has exactly `id` and `source`. A source contains a UUID `featureId`, `entityKind` (`face`, `edge`, or `vertex`), and a geometric `signature`; primitive box sources additionally carry a semantic `key`. A signature has exactly `kind` and six finite ordered `bounds` coordinates. For example, a box face can use `box.face.xmax`. Other operations never invent primitive keys from explorer indices.

Every `triangleFaceIds` entry names the face of the corresponding mesh triangle. The face is located using `faces.FindIndex(face)`, plus the documented global-ID offset. Explorer order is not an identity, particularly when a compound contains a shared face more than once.

Each edge polyline contains exactly `edgeId`, two `vertexIds`, `points`, `degenerate`, and `chordTolerance`. Coordinates are in shape units. Ordinary straight edges contain two points. Closed edges repeat their geometric endpoint and may name the same endpoint vertex twice. OCCT-degenerate edges have one point, `degenerate: true`, and coincident endpoint geometry. They are never invented short line segments.

Curves are converted to positive-weight rational Bezier arcs. Adaptive subdivision continues until every control point lies within the chord tolerance of the endpoint segment. The positive-weight convex-hull property bounds the entire arc, including inflections that midpoint sampling could miss. The tolerance uses the mesh's bounded absolute deflection. Each edge has at most 1,025 output points and subdivision depth 24; exceeding either limit returns `geometry_error` with `topology_polyline_too_large`, without a partial result. Nonpositive weights, nonfinite geometry, or absent curve/endpoint data are rejected. Internal control points may lie outside the shape's coordinate bounds; emitted geometry still obeys the coordinate limit.

## Resolution and selected fillets

`resolveTopologyReference(entities, reference)` validates the complete input before matching. UUIDs, known entity kinds, exact schema fields, finite ordered signature bounds, integer identifiers, and global identifier uniqueness are required. Malformed input returns `invalid`. A valid absent candidate returns `missing`; more than one candidate returns `ambiguous`; exactly one returns `resolved` and its entity. A reference has `featureId`, `entityKind`, and a `key`, a `signature`, or both. When both are supplied, both must match. Callers must require repair for invalid, missing, or ambiguous selections rather than guessing.

Legacy fillets without a `topologyInput` property retain all-edge behavior. A present malformed, null, or empty property is an explicit invalid selection request and cannot fall back to all edges. Selected fillets require this complete `parameters.topologyInput`:

```json
{
  "version": 1,
  "mode": "selectedEdges",
  "producerFeatureId": "123e4567-e89b-42d3-a456-426614174000",
  "producerOperation": "box",
  "brepSha256": "<exact upstream BREP SHA-256>",
  "topology": "<complete cached upstream topology object, not a string>",
  "edgeRefs": [
    {
      "featureId": "123e4567-e89b-42d3-a456-426614174000",
      "entityKind": "edge",
      "key": "<key from the cached edge source>"
    }
  ]
}
```

The placeholder `topology` above must be replaced with the actual JSON object. The worker requires the cached producer metadata and hash to match the selection envelope and the exact input BREP. It independently derives topology from that BREP and compares every entity signature, vertex, edge polyline, and triangle owner, with exact discrete IDs and at most 1e-7 coordinate tolerance. Extra fields, incomplete arrays, duplicate selected entities, malformed references, and inconsistent geometry are rejected. Cached box roles are checked against actual axis-aligned planar faces, straight edges, box corners, and one-solid box topology. A missing cached map is never repaired by manufacturing box keys on arbitrary geometry. Other producer operations can select uniquely resolved signature references and do not acquire primitive keys.

**Historical authenticity belongs to the host.** The worker is stateless. A same-caller hash proves byte equality, not who produced those bytes or whether a feature UUID belongs to the document history. Before dispatch, the host must bind the producer UUID, operation, exact BREP bytes/hash, and cached topology reply to its evaluated upstream feature. A self-consistent caller-supplied replacement does not constitute cryptographic origin proof. This worker change does not implement the host's feature-history trust boundary or persistent-selection UI.

## Limits and compatibility

The request remains bounded at four MiB, decoded BREP at two MiB, and serialized compact-JSON reply at eight MiB. Passing the complete cached map does not override the request limit. The response limit is checked with the actual production serializer by `boundResponse`; an oversized response returns `response_too_large` and omits `result` entirely.

Topology-only indexed maps and entity limits (5,000 faces, 15,000 edges, 15,000 vertices) execute only on explicit opt-in or explicit selected-map validation. Ordinary mesh-only requests keep their original mesh limits and do not inherit topology-count restrictions. Mesh limits remain 60,000 vertices and 100,000 triangles.

## Focused verification

`tests/geometry/GeometryWorkerTests.cpp` exercises actual OCCT shapes and the production serializer:

- Primitive volume, Boolean operations, BREP round trips, mesh normals/winding, and legacy fillets.
- Semantic box roles under changed dimensions and globally unique reply IDs.
- Triangle-corner geometric membership on their named faces, including a compound with a repeated shared face, reversed occurrence, and translated solid.
- Exact resolver schema, UUID, ordered/finite signature, duplicate-ID, missing, and ambiguity cases.
- Straight, inflected Bezier, closed-circle, and OCCT-degenerate edges, dense geometric distance checks, and bounded subdivision refusal.
- A 5,001-face located compound that remains legal for a legacy mesh request but exceeds topology limits when explicitly opted in.
- Exactly eight MiB, one byte beyond eight MiB, and escaping expansion through the real compact-JSON serializer. The oversized fixture extends an actual topology reply inside the serializer test; it does not claim an end-to-end multi-megabyte CAD model was generated.
- Successful cached-map selective fillets, generic translated-shape selection, legacy all-edge behavior, and missing, malformed, hash/producer, geometry-array, semantic-key, and duplicate-selection refusals.

The initial six review repairs are implemented in this lane. Final verification evidence and the integration commit are recorded by the owning integration task. The full product and the host's historical provenance boundary remain unfinished.
