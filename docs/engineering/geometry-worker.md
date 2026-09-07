# Geometry worker

`precision_geometry_worker` handles exactly one protocol version 1 JSON object from standard input and emits one JSON object on standard output. Its process boundary is the cancellation boundary: the caller terminates its owned worker instead of attempting to interrupt Open CASCADE inside a mutable kernel operation.

The envelope is revision-bound and echoed in every reply:

```json
{"protocolVersion":1,"operationId":"box-17","documentId":"part-42","revision":8,"operation":"box","parameters":{"dx":10,"dy":20,"dz":30}}
```

Successful replies include a deterministic BREP base64 payload, validity, volume, bounds and, except for `validate`, a triangulated mesh. Supported operations are `box`, `cylinder`, `extrude`, `union`, `cut`, `intersection`, `translate`, `rotate`, `fillet`, `validate`, and `tessellate`. Boolean operands and transforms accept BREP only, so kernel handles never leave the worker.

Requests are read with a 4 MiB streaming ceiling before parsing, successful replies are limited to 8 MiB, BREP payloads to 2 MiB and meshes to 60,000 vertices. Base64 BREP input uses strict decoding. Envelope versions must be exactly integer protocol version 1, identity strings are bounded, and revisions are nonnegative safe integers. Dimensions, coordinates and transforms must be finite and bounded. A zero translate vector is a valid no-op, while extrusion and rotation axes require a nonzero direction. Invalid topology, unknown operations, Open CASCADE exceptions and malformed serialized shapes return a structured `ok:false` reply. The worker writes no diagnostic text to standard output.

Mesh normals are evaluated per triangulation node from the Open CASCADE face surface and transformed into world coordinates. Reversed faces reverse both normal direction and triangle winding, so the mesh convention remains outward-facing across caps, curved faces, and boolean-created internal surfaces.

Before tessellation, the worker rejects shapes with more than 5,000 faces or 15,000 edges. It also rejects output that would exceed 60,000 vertices or 100,000 triangles, or any face that lacks complete triangulation UV data. Tessellation uses an explicit scale-aware chordal deflection of `clamp(boundsDiagonal * 0.001, 0.0001, 10.0)`, with angular deflection `0.5` radians. The returned mesh states `absoluteDeflection`, `targetRelativeDeflection`, and the truthful `effectiveRelativeDeflection`, so a caller never mistakes a clamped size-conditioned mesh for silent precision. Worker-level limits do not replace host process limits: the host owns wall-clock cancellation and process memory ceilings.

STEP path import and export intentionally are not protocol operations. This worker never accepts an ambient filesystem path. A future host-owned exchange adapter must pass an approved workspace handle or bounded byte payload, validate it outside the document transaction, then invoke the geometry layer. This prevents arbitrary path access from the worker protocol.

At runtime, deploy `Qt6Core.dll` plus the Open CASCADE DLLs selected by the executable's dependency scan, from the pinned OCCT `win64/vc14/bin` directory. Development uses the CMake package under the extracted OCCT `cmake` directory.
