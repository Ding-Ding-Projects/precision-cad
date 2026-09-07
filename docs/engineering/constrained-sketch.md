# Constrained sketch foundation

The native foundation stores a strict schema-version-1 sketch record with stable unsigned 64-bit entity and constraint IDs. It solves datum-plane coordinates through the official SolveSpace 3.2 `libslvs` C API. This is a library foundation, not an integrated editing workflow: controller/document wiring, interactive tools, profile extraction, and downstream solid operations are not implemented by this module.

`src/sketch/sketch_model.h` defines point, line, circle, and arc records. Curve references identify point records explicitly. Each point persists local `(u, v)` coordinates. The supported unit is `mm`; unsupported units are rejected rather than silently reinterpreted.

## Datum orientation

The datum carries an explicit origin and non-zero finite normal. The adapter normalizes that normal using scaling to avoid overflow and underflow. It projects the global positive X axis into the plane to construct U, switching to positive Y when `abs(normalizedNormal.x) > 0.9`. V is `normal cross U`. That deterministic right-handed basis feeds the actual libslvs workplane quaternion. The returned point contains solved local coordinates and world `(x, y, z)` coordinates computed from the same quaternion basis and origin. For the default XY plane, U is +X and V is +Y.

## Supported relation signatures

| Relation | Accepted operands and meaning |
| --- | --- |
| Coincident | Two distinct point records |
| Horizontal / vertical | One line |
| Parallel / perpendicular | Two distinct lines |
| Tangent | Arc-line in either operand order, or two arcs, sharing exactly one endpoint ID |
| Equal | Two lines, or two circular entities (circles and/or arcs) |
| Distance | Two distinct points, non-negative distance in mm |
| Radius | One circle, positive radius in mm |
| Angle | Two distinct lines, unsigned included angle from 0 through 180 degrees |
| Fixed | One point, anchored at its input local coordinates |

Tangency selects the shared arc endpoint using the native `other` and `other2` flags. Arc-line constraints always place the arc in native operand A. Circle-line, circle-circle, line-line, disconnected curves, and ambiguous two-endpoint matches are rejected. General tangency and explicit endpoint selectors are future work; unsupported pairs never reach the native solver. Coincident coordinates alone do not establish shared topology for tangency.

Unary relations require an absent second ID (serialized as an empty string). Non-dimensional relations require a zero value. A circle radius is converted to the native diameter constraint only after validation. Fixed placement is explicit, never an implicit solver behavior.

## Persistence and validation

`fromJson` requires every documented field with the correct JSON type and rejects unknown fields, unknown versions/kinds, missing fields, malformed decimal IDs, unsupported units, duplicate IDs, invalid relation arity/kinds, references to non-points, missing references, and degenerate line/arc endpoints. Decimal IDs are canonical strings to preserve all 64 bits. Curve fields that do not belong to a record kind must be zero. Labels and identities are limited to 1,024 UTF-16 code units. There must be 1 through 4,096 entities and at most 4,096 constraints. Coordinates, stored radii, and dimension values must be finite with absolute magnitude at most 1e9. Normals may use any finite non-zero magnitude. The solver can return its native too-many-unknowns result below the record cap; the cap is a persistence bound, not a promise that every bounded model solves.

Parsing stages and validates a complete candidate before replacing the output. Any rejection preserves the previous output model. Direct C++ callers go through the same semantic validation before any libslvs allocation. Persistent IDs are mapped into checked, separate dense entity, parameter, and constraint namespaces, so sparse and maximum-width IDs cannot collide or wrap native handles. Failed-constraint handles map back to persistent IDs.

## Solver results and safety

The result distinguishes solved, under-constrained, over-constrained, did-not-converge, and invalid-model states. It retains native result/DoF diagnostics and conflicting stable constraint IDs. Successful results are sorted by stable ID. Failed solves return no tentative replacement geometry. Neither a successful solve nor a rejected solve mutates the input. Calls are serialized because libslvs owns process-global model and temporary storage. This module does not apply results to a document or promise profile validity for solid construction.

## Pinned source provenance

The approved upstream source is `https://github.com/solvespace/solvespace.git` at `27b6a080c8b669421bd4d444650c3b8eddec5687` (`v3.2`). The `include/slvs.h` SHA-256 is `b342bfeab4bd64b747ef3722f3218b64927939eaabfc34afed53819bd248552e`.

`scripts/bootstrap-sketch-solver.ps1` acquires that exact source plus its pinned Eigen and mimalloc submodules under `%LOCALAPPDATA%/material-virtualbox-toolchain/solvespace-v3.2`. It refuses existing local changes before checkout. `src/sketch/verify_solver_source.cmake` uses Git revision resolution, full source/submodule status, and recursive submodule revision checks. It rejects dirty tracked source, unexpected untracked source, absent or mismatched submodules, and an incorrect source revision. Validation runs at configuration and again before solver compilation, including incremental builds. The source cache is outside the application repository. These checks prove source state when checked; they do not lock the external cache against another process modifying it mid-compilation.

## Verification

`tests/sketch/sketch_model_tests.cpp` invokes the real libslvs solver for every supported relation kind, line/circle/arc equal-radius signatures, all eight arc-line operand/endpoint orderings, all four arc-arc endpoint combinations, non-tangent rejection, unsupported tangency rejection, strict JSON mutations with output preservation, invalid typed references, extreme IDs, rotated/scaled datum normals, dimension editing, and rectangle-with-hole solved/under/over states. The rectangle is constrained by dimensions and horizontal/vertical relations, with one fixed corner and a fixed hole center. Failed solves are checked for no replacement geometry and no input mutation.

`tests/sketch/solver_provenance_tests.ps1` uses isolated disposable Git fixtures to prove rejection and restoration of altered source, altered submodule content, missing submodules, and incorrect revision pins. It retains those fixtures below the selected build directory for inspection. No real solver cache is modified by the negative tests.

Configure the standalone tests with CMake from `tests/sketch`, the project's MSVC toolchain, and Qt 6.8 Core/Test. Build the resulting directory, then run `ctest --test-dir <build-directory> --output-on-failure`. The standalone suite verifies this foundation only; it does not prove packaged UI or document integration.
