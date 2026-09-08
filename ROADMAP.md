# Delivery roadmap

Checked items require implementation and verification. Intermediate builds are not completion of the professional suite.

## Verified implementation foundations
- [x] Deterministic typed evaluation and transitive suppression with legacy parameter compatibility
- [x] Real SolveSpace adapter with strict parsing, stable native handles, datum orientation and provenance regressions
- [x] Analytic line/circle profile regions and holes with scale-aware contact and orientation tests
- [x] Bounded dimensional-expression parser and named-parameter evaluation
- [x] Native multi-body rendering and direct selection, with a real radius-edit interaction at `f29406b`
- [x] Native diagnostic collection with a sealed normal-exit receipt at `8b092b5`
- [x] Worker topology map validation, triangle ownership, cached-map selective fillets and bounded curve serialization
- [ ] Complete sketch-to-pad UI and associative document workflow (built candidate preserved on `feat/associative-sketch-pad`; production dispatch and final verification pending)
- [ ] Complete topology reference repair and selective feature integration in the host and UI (worker foundation accepted)
- [ ] Mount and verify the reviewed updater backend in the installed application
- [ ] Promote fresh native captures and complete the required surface matrix

## Foundation
- [x] Publish development prerelease `v0.1.0-dev.20260907.1` with unsigned `Setup.exe`, `RELEASES`, and a full `.nupkg` (installer execution, installed-runtime proof, updater behavior, and full release remain incomplete)
- [ ] Reproducible Windows bootstrap and native build (final source completed 6/6 focused CTest targets and the canonical local Squirrel package route; fresh-machine acquisition remains incomplete)
- [ ] Versioned document model, transactions, undo and redo
- [ ] Atomic save, exclusive writer protection and crash recovery
- [ ] Isolated cancellable geometry operations
- [ ] Native viewport and feature/property editing
- [ ] Local Git history, semantic comparison and non-destructive restoration

## Mechanical design
- [ ] Constrained sketches and dimension expressions
- [ ] Solid and surface feature operations
- [ ] Stable reference tracking and explicit repair
- [ ] Assemblies, mates, interference and bills of materials
- [ ] Associative drawings and PDF/DXF output
- [ ] Sheet metal, unfolding and refolding
- [ ] STEP/IGES/STL/3MF exchange and round-trip verification

## Manufacturing and analysis
- [ ] 2.5D/three-axis CAM, stock, tools and setup validation
- [ ] Toolpath simulation, collision reporting and generic G-code export
- [ ] Gmsh mesh and CalculiX solver adapters
- [ ] Linear static and modal studies with analytic benchmarks
- [ ] Revision-bound results and convergence comparison

## Product completion
- [ ] Implement the explicit desktop and website rows in [surface completeness](docs/engineering/surface-completeness.md), tracked in issue #3
- [ ] Versioned scripting/command API
- [ ] Complete per-surface feature inventory and negative regressions
- [ ] Accessible localized desktop and offline documentation
- [ ] Deterministic design parity and built UI evidence (19 focused per-click native states and final icon/minimum-size captures exist; complete front-screen and display-scale evidence remains incomplete)
- [ ] Unsigned Squirrel.Windows installer and explicit-restart updates (local Squirrel package route exists; updater work remains)
- [ ] Verified public release, download assets and provenance

## Deliberately excluded from this release
- Native proprietary CAD formats: use neutral exchange in the initial suite.
- Turning and simultaneous five-axis machining: require separate validated machine capabilities.
- Nonlinear/contact/thermal/fluid simulation: outside the agreed initial analysis scope.
- Cloud collaboration, other operating systems and connected-machine control: outside the agreed release.
