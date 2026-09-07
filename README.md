# Precision CAD

An open-source mechanical CAD, CAM, and structural-analysis application for Windows x64. **Development is in progress. There is no production-ready release yet.**

The product combines a native C++20/Qt desktop interface with Open CASCADE geometry, parametric documents, and local Git project history. The intended scope includes assemblies, associative drawings, sheet metal, 2.5D/three-axis milling, and linear static/modal analysis.

## Build

The supported entry point being implemented is:

```powershell
.\build.bat --run
```

Until the bootstrap and packaged application are verified, this command is not claimed to work on a fresh installation. See [ROADMAP.md](ROADMAP.md) and [HANDOFF.md](HANDOFF.md) for the actual delivery state.

## Documentation

- [Architecture](docs/engineering/architecture.md)
- [Feature documentation](docs/README.md)
- [Roadmap](ROADMAP.md)
- [Current implementation evidence](HANDOFF.md)

Source is intended for distribution under GPL-3.0-or-later. Third-party components retain their own licenses. This application does not command connected machinery or certify structural safety.
