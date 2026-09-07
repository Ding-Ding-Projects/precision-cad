# Native modelling workspace

`src/app` provides the initial Windows Qt 6/QML host for Precision CAD. It creates and edits a revisioned `precision::core::Document` only through transactions. A proposed edit is made on a copied document, sent through a revision-bound geometry-worker graph, and becomes the displayed and persisted document only after every required worker operation returns successfully. Failed, timed-out, cancelled, stale, oversized, malformed, or identity-mismatched worker results leave the prior document and mesh in place.

The surface supports dimensioned boxes and cylinders, selection of two bodies, union/cut/intersection, suppression, undo/redo, native `.pcad` save/open, and truthful mesh measurements. Save passes the last loaded revision to `DocumentStorage`, which rejects a stale sequential writer. The QML file dialogs only use a locally selected path.

The viewport is a `QQuickPaintedItem` framebuffer projection of the tessellation returned by the Open CASCADE worker. It draws actual returned triangle coordinates, supports orbit, right-button pan, wheel zoom, and fit. It currently does painter ordering rather than a native depth-buffered OpenGL scene. This is a bounded initial rendering route, not a claim of full CAD visualization accuracy. The worker protocol and BREP cache remain the geometry authority.

The host uses Material controls from `QtQuick.Controls.Material`. English is currently the only shipped UI wording. Cantonese and bilingual modes, native preference persistence, feature parameter editing, fillet/transform authoring, unsaved-work confirmation, full history, CAM, FEA, exports, and the broader surface-completeness contract are unfinished. They remain tracked as missing in `surface-completeness.md` and must not be inferred from this implementation.

Build provenance comes from `PRECISION_CAD_VERSION` and `PRECISION_CAD_BUILD_TIME` supplied by the top-level build. The UI shows those values verbatim and does not invent a launch-time timestamp.
