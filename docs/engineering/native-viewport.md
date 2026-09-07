# Native 3D viewport

The production workspace uses Qt Quick 3D `View3D` with depth testing, custom indexed triangle geometry and lit normals. `ViewportCamera` is the single QObject controlling both QML cameras and the screen ray used for mesh selection. The former painted mesh is no longer instantiated by the workspace.

## Navigation and selection

Left drag orbits around the current target. Right drag, middle drag or Shift + left drag pans on the target plane. The wheel zooms; Fit and double-click frame the complete current scene using its bounds and viewport aspect ratio. Toolbar controls choose perspective or orthographic projection and seven standard views. Fit preserves the selected orientation. A left click without a drag selects the nearest intersected visible body.

`meshParts` supplies separate `{bodyId, vertices, indices, normals}` records. A controller without this property can supply the legacy selected mesh arrays. Rendering combines the current visible parts into one index buffer and records each part's triangle range. Picking returns `{kind: "meshTriangle", bodyId, triangleIndex, position}`. The triangle index belongs to that combined render mesh revision. It is not a persistent CAD face, edge, vertex or topology reference and must never be stored as one.

## Precision and validation

Source positions remain doubles in controller data. The geometry adapter computes global bounds and their midpoint in double precision, subtracts that midpoint, scales the bounding sphere to radius 100, and only then converts to GPU floats. The camera and picker use these same normalized coordinates. Returned hit coordinates are transformed back to the source coordinate frame. This preserves small relative shapes translated far from the origin but does not promise precision finer than double source input or float rasterization within the local scene.

Every index must be finite, integral and in range before upload. Nonfinite coordinates, malformed arrays and zero-extent geometry clear the render buffers and cannot return a hit. `setBounds` publishes the same normalized bounds used by fit. Valid supplied normals are normalized and uploaded; missing or invalid normals use area-weighted triangle normals. That fallback is a display approximation, not a reconstructed analytic CAD normal. Backface culling is disabled so the visible triangle surface and two-sided ray tests agree.

## Verification

`viewport-math` covers camera rays and nearest triangle, edge and vertex math. `workspace-qml` additionally instantiates production Main.qml and exercises actual camera/geometry bindings, projection and standard-view controls, fit, pan, orbit, click selection, translated multi-body input and invalid-buffer clearing. CTest prepends the configured Qt runtime directory. These checks do not replace inspection of the built application's depth rendering on a real graphics backend.

## Selected-body appearance

The render buffer includes per-vertex color data derived from the same body triangle ranges used for selection. The selected body uses the window accent color; other bodies use the foreground color. Changing selection only updates color bytes, preserving the camera and model positions. The tree marks the current body with a localized Selected label and the native accessibility selected state. Boolean operand tracking remains separate from the current body.

The scoped native follow-up uses existing Qt Quick Controls and Qt Quick 3D primitives. The required Material Designer creation/export flow was unavailable because no runnable build existed in the inspected project or tool cache; no design preview is claimed as production evidence.

## CAD coordinate convention

Model coordinates use Z up. Top looks down positive Z onto XY, with X to the right and Y toward the top of the screen. Front looks from negative Y with positive Z up. Right looks from positive X with positive Z up. The isometric view is above the positive-X, negative-Y quadrant. A native label beside the view controls identifies the convention. The exact pole basis is computed analytically, so Top and Bottom do not rely on a nearly vertical approximation.
