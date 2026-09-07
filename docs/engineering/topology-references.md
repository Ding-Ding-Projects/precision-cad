# Topology references

The geometry worker keeps its protocol at version 1. A standard request produces the existing result shape. A caller explicitly opts into topology with `topologyVersion: 1` and a UUID `producerFeatureId`; the returned entity source uses that exact feature identifier, never the per-run operation identifier.

The opt-in result has `result.topology.version`, a shared `entities` array, `triangleFaceIds`, `edgePolylines`, and `vertices`. Each triangle face identifier names one face entity. Edge polylines contain OCCT-evaluated points in shape units plus endpoint vertex identifiers. Counts and coordinates share the worker limits. The worker rejects a result exceeding its eight MiB response boundary instead of sending incomplete topology.

An entity source stores `featureId`, `entityKind`, and a geometric `signature`. The signature is a resolver candidate only. Primitive box faces, edges, and vertices additionally expose semantic role keys such as `box.face.xmax`. Other shape operations do not invent stable keys from OCCT explorer indexes.

`resolveTopologyReference(entities, reference)` is separate from execution. It returns `resolved` only when one entity matches. A missing candidate returns `missing`; symmetric or otherwise multiple candidates return `ambiguous`. Callers must surface those states rather than silently choosing an entity.

Feature operations that consume topology must validate a source feature identifier, entity kind, and semantic key or a unique resolver result before changing geometry. The worker does not interpret a geometric signature as proof of identity. Fillet uses `parameters.topologyInput.sourceFeatureId` and a nonempty `edgeRefs` array. Each edge reference supplies the matching `featureId`, `entityKind: "edge"`, and semantic key. The current worker accepts the demonstrated box semantic keys, and rejects absent, duplicate, missing, or ambiguous input instead of filleting every edge.
