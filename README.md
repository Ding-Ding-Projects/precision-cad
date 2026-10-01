# Precision CAD

An open-source mechanical CAD, CAM, and structural-analysis application for Windows x64. **Development is in progress. There is no production-ready release yet.**

The product combines a native C++20/Qt desktop interface with Open CASCADE geometry, parametric documents, and local Git project history. The intended scope includes assemblies, associative drawings, sheet metal, 2.5D/three-axis milling, and linear static/modal analysis.

## Build

Build the native desktop development payload with:

```powershell
.\build-desktop.bat
```

Create the local unsigned Squirrel.Windows development package with:

```powershell
.\build-installer.bat /s
```

The native commands use the supported local Qt 6.8.3 and MSVC caches plus the pinned Open CASCADE archives. Fresh-machine Qt and MSVC acquisition is incomplete. `build.bat` remains the separate website bootstrap and static-site build entry point.

## Documentation

- [Live project website](https://ding-ding-projects.github.io/precision-cad/)

- [Architecture](docs/engineering/architecture.md)
- [Feature documentation](docs/README.md)
- [Roadmap](ROADMAP.md)
- [Current implementation evidence](HANDOFF.md)

Source is intended for distribution under GPL-3.0-or-later. Third-party components retain their own licenses. This application does not command connected machinery or certify structural safety.

## Development surfaces

- [Delivery project](https://github.com/orgs/Ding-Ding-Projects/projects/36)
- [Discussions](https://github.com/Ding-Ding-Projects/precision-cad/discussions)
- [Issue tracker](https://github.com/Ding-Ding-Projects/precision-cad/issues)
- [Website build details](docs/engineering/build.md)

It writes `Setup.exe`, `RELEASES`, and a full `.nupkg` under the candidate-specific directory `artifacts/native/squirrel-windows/<commit>`. It does not publish, tag, or create a release. The installer is unsigned and Windows will show an unknown-publisher warning.

## Website evidence

The following is a genuine capture of the built website at source `463040400b37c98e09ec4e89269699f9bc307fd8`, not a CAD application capture. The bracket drawing is an explicitly labelled concept illustration.

![Built Precision CAD website](docs/evidence/website-overview.png)

Follow [setup issue #1](https://github.com/Ding-Ding-Projects/precision-cad/issues/1), [the rolling progress discussion](https://github.com/Ding-Ding-Projects/precision-cad/discussions/2), and [the cross-surface feature contract](https://github.com/Ding-Ding-Projects/precision-cad/issues/3).
