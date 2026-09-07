# Native viewport

The desktop viewport uses Qt Quick 3D with a native `QQuick3DGeometry` mesh. Vertex positions, normals and indices are uploaded as interleaved float attributes and 32-bit indices. Qt Quick 3D performs depth-buffered triangle rendering, so draw order does not determine visible surfaces.

`MeshGeometry` accepts the controller's existing flat `meshVertices` and `meshIndices` lists. It accepts optional normals; when a legacy result has none, it derives smooth normals from the indexed triangles. The next controller contract is `meshParts`, a list of `{bodyId, vertices, indices, normals, topology}` records. That keeps each body independently addressable and leaves room for topology identifiers without breaking older workers.

The reusable C++ camera model lives in `src/app/viewport/viewport_camera.*`. It supports orthographic and perspective projection, isometric plus six standard views, screen projection and world rays. Rendering uses local float coordinates only; authoritative Open CASCADE geometry remains in the document and worker paths using their existing precision.

`ViewportPicker` ray-tests the indexed mesh and returns the nearest positive triangle. If the ray is within the supplied world tolerance of a vertex or triangle boundary, it returns a mesh vertex or visual edge in preference to the face. Those identifiers are explicitly mesh-level until the worker supplies `triangleFaceIds`, `edgePolylines`, and topology entities. The renderer must not present them as persistent CAD topology before that mapping exists.

`tests/app/tst_viewport_math.cpp` covers standard-view rays, center projection, nearest-depth selection, vertex/edge priority and invalid mesh input. Run it through the normal application CTest build after Qt 6.8.3 includes the `qtquick3d` addon.
