# Topology references

The geometry worker keeps its protocol at version 1. A standard request produces the existing result shape. A caller explicitly opts into topology with `topologyVersion: 1` and a UUID `producerFeatureId`; the returned entity source uses that exact feature identifier, never the per-run operation identifier.

The opt-in result has `result.topology.version`, `brepSha256`, `producer`, a shared `entities` array, `triangleFaceIds`, `edgePolylines`, and `vertices`. `brepSha256` binds the topology map to the exact serialized BREP bytes. Each triangle face identifier names one face entity. Edge polylines contain OCCT-evaluated points in shape units plus endpoint vertex identifiers. Counts and coordinates share the worker limits. The worker rejects a result exceeding its eight MiB response boundary instead of sending incomplete topology.

An entity source stores `featureId`, `entityKind`, and a geometric `signature`. The signature is a resolver candidate only. Primitive box faces, edges, and vertices additionally expose semantic role keys such as `box.face.xmax`. Other shape operations do not invent stable keys from OCCT explorer indexes.

`resolveTopologyReference(entities, reference)` is separate from execution. It returns `resolved` only when one entity matches. A missing candidate returns `missing`; symmetric or otherwise multiple candidates return `ambiguous`. Callers must surface those states rather than silently choosing an entity.

Feature operations that consume topology must validate a source feature identifier, entity kind, and semantic key or a unique resolver result before changing geometry. The worker does not interpret a geometric signature as proof of identity. Legacy fillet requests without `topologyInput` retain their all-edge behavior. Selective fillet is explicit: `parameters.topologyInput` requires `version: 1`, `mode: "selectedEdges"`, `producerFeatureId`, `producerOperation: "box"`, the exact `brepSha256`, and a nonempty `edgeRefs` array. Each edge reference supplies the matching `featureId`, `entityKind: "edge"`, and semantic key. The worker validates BREP-map consistency and rejects absent, duplicate, missing, or mismatched selected topology. The host binds producer metadata to its feature history.

## Preservation handoff, pending topology review repairs

Commits `df03350fce5b44d013f85215a4859f083d63eaaf` and `7daf312b327627611dd139ea7006a3a307d07f44` establish the opt-in response shape, exact serialized-BREP hash, producer metadata, box semantic-role keys, and legacy fillet compatibility. They do not complete the topology foundation.

The worker is stateless. It can prove that a supplied hash names the exact BREP bytes it is currently asked to process and can validate geometry against that BREP. It cannot prove that a caller's producer feature identity or operation history is authentic. The host must bind producer identity, type, cached topology reply, and BREP hash to its computed feature history before it issues a selected-edge request.

The following six review repairs remain open and must be completed before integration claims a typed topology foundation:

1. Make the resolver validate UUIDs, complete schema shapes, finite exact signature objects, and unique entity identifiers. Malformed input must return `invalid`, distinct from `missing` and `ambiguous`.
2. Derive every triangle face identifier with `faces.FindIndex(face)`, then add geometric-membership tests including compound shapes and shared faces.
3. Replace fixed 17-point edge sampling with adaptive chord-tolerance subdivision, with bounded point counts and explicit degenerate-edge behavior.
4. Move every topology-only count and indexed-map traversal behind the opt-in request. Existing mesh-only requests must not pay topology bookkeeping cost.
5. Add a focused serialized-response boundary fixture proving that a topology result beyond eight MiB returns the typed size failure without a partial topology object.
6. Make selected fillet consume the cached upstream topology map rather than reconstructing box keys from an arbitrary BREP. It must reject a map-kind, hash, producer, or reference mismatch and must not synthesize primitive semantic roles for nonprimitive geometry.
