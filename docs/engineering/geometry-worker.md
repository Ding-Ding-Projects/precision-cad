# Geometry worker

`precision_geometry_worker` handles exactly one protocol version 1 JSON object from standard input and emits one JSON object on standard output. Its process boundary is the cancellation boundary: the caller terminates its owned worker instead of attempting to interrupt Open CASCADE inside a mutable kernel operation.

The envelope is revision-bound and echoed in every reply:

```json
{"protocolVersion":1,"operationId":"box-17","documentId":"part-42","revision":8,"operation":"box","parameters":{"dx":10,"dy":20,"dz":30}}
```

Successful replies include a deterministic BREP base64 payload, validity, volume, bounds and, except for `validate`, a triangulated mesh. Supported operations are `box`, `cylinder`, `extrude`, `union`, `cut`, `intersection`, `translate`, `rotate`, `fillet`, `validate`, and `tessellate`. Boolean operands and transforms accept BREP only, so kernel handles never leave the worker.

Requests are limited to 4 MiB, successful replies to 8 MiB, BREP payloads to 2 MiB and meshes to 60,000 vertices. Dimensions, coordinates and transforms must be finite and bounded. Invalid topology, unknown operations, Open CASCADE exceptions and malformed serialized shapes return a structured `ok:false` reply. The worker writes no diagnostic text to standard output.

STEP path import and export intentionally are not protocol operations. This worker never accepts an ambient filesystem path. A future host-owned exchange adapter must pass an approved workspace handle or bounded byte payload, validate it outside the document transaction, then invoke the geometry layer. This prevents arbitrary path access from the worker protocol.

At runtime, deploy `Qt6Core.dll` plus the Open CASCADE DLLs selected by the executable's dependency scan, from the pinned OCCT `win64/vc14/bin` directory. Development uses the CMake package under the extracted OCCT `cmake` directory.
