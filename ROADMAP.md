# Delivery roadmap

Checked items require implementation and verification. Intermediate builds are not completion of the professional suite.

## Foundation
Current status: native modules and development executable exist, but complete acceptance is pending. The latest QML verification process times out after its assertions pass. The preservation candidate and exact limitations are recorded in HANDOFF.md.
- [ ] Reproducible Windows bootstrap and native build
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
- [ ] Deterministic design parity and built UI evidence
- [ ] Unsigned Squirrel.Windows installer and explicit-restart updates
- [ ] Verified public release, download assets and provenance

## Deliberately excluded from this release
- Native proprietary CAD formats: use neutral exchange in the initial suite.
- Turning and simultaneous five-axis machining: require separate validated machine capabilities.
- Nonlinear/contact/thermal/fluid simulation: outside the agreed initial analysis scope.
- Cloud collaboration, other operating systems and connected-machine control: outside the agreed release.
