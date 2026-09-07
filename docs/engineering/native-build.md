# Native desktop build

The native build is being integrated. It is not yet a verified fresh-machine or release-packaging route.

Run `build-desktop.bat -Test` to configure the C++20/Qt/Open CASCADE targets and execute local tests once the implementation modules are integrated. The script reads `manifests/native-dependencies.json`, uses a user-scoped cache, verifies the official Open CASCADE archive SHA-256 values, and activates the detected x64 MSVC environment for this process only.

The root CMake project composes the independent document, geometry, preference, history and desktop targets. Generated build provenance records the version, exact Git source commit and actual UTC configuration time. The native command interface and geometric worker remain separate processes.

## Current acquisition boundary

Open CASCADE 8.0.1 and its supporting archive are acquired automatically from official upstream release assets with pinned hashes. The release archives contain nested ZIPs; each layer is extracted into its own destination to avoid replacing an open archive with a same-named inner member.

Qt 6.8.3 and MSVC are discovered from supported local installation/cache routes. Their fully automatic fresh-machine acquisition, cache-content integrity inventory, concurrent activation journal, and final runtime bundling are not implemented by this initial route. Missing tools fail with an explicit message; the script does not modify unrelated toolchains.

The desktop installer is not implemented yet. A successful native development build is not evidence of an unsigned Squirrel.Windows package or an installed application.

## Development runtime staging

After a successful committed-source build, `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/stage-native-runtime.ps1` deploys the Qt imports detected from the real QML source and recursively resolves the actual application and geometry-worker DLL imports. The pinned release TBB and jemalloc directories are included; debug support binaries are not selected. The helper records application and worker hashes in `build/native/runtime-receipt.json` and refuses a changed source revision.

This is an unpacked development runtime, not an installer. Native captures use `--profile-directory` with an isolated directory. That mode also initializes file dialogs in its own empty `documents` folder to avoid showing unrelated user files during verification. The front-screen build time is rendered locally with seconds and its timezone, from recorded build provenance.
