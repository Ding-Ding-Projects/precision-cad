# Constrained sketches

The native sketch foundation stores a deterministic, versioned record with stable numeric entity and constraint IDs. It solves only datum-plane coordinates in this stage. The datum plane carries an explicit origin and normal, while every point persists its local `(u, v)` coordinates.

`src/sketch/sketch_model.h` defines serializable point, line, circle, and arc records. Lines and arcs refer to their point IDs, rather than indexes into a mutable vector. Constraint records refer to the same stable IDs and support coincident, horizontal, vertical, parallel, perpendicular, tangent, equal, distance, radius, angle, and fixed placement. Fixed placement exists only to anchor a profile to its datum plane. It is not an implicit solver behavior.

The adapter calls the official SolveSpace 3.2 `libslvs` C API. Its source is cloned from `https://github.com/solvespace/solvespace.git` at `27b6a080c8b669421bd4d444650c3b8eddec5687` (the `v3.2` tag), with `include/slvs.h` SHA-256 `b342bfeab4bd64b747ef3722f3218b64927939eaabfc34afed53819bd248552e`. `scripts/bootstrap-sketch-solver.ps1` fetches that exact source and the pinned Eigen and mimalloc solver submodules into `%LOCALAPPDATA%/material-virtualbox-toolchain/solvespace-v3.2`. The checkout stays outside the source tree and is never committed.

The solver result reports the exact solver outcome, remaining degrees of freedom, any conflicting stable constraint IDs, and solved point/radius values sorted by stable ID. A successful solve with remaining degrees of freedom is explicitly under-constrained. Inconsistency is explicitly over-constrained. The model does not silently relabel, drop, or repair a constraint.

`tests/sketch/sketch_model_tests.cpp` exercises a fixed rectangle with a circular hole at zero remaining degrees of freedom, a dimension edit followed by a fresh solve, an under-constrained line, and incompatible distance constraints that return conflicts. The tests invoke `libslvs`, not a custom numerical solver.
