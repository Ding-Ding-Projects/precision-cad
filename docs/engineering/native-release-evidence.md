# Native release identity and provenance

Precision CAD's native executables carry Windows version resources. The desktop executable and geometry worker report the product version, company, product name, file description, internal name and original filename. Both bind the same project-owned multi-resolution icon derived from `website/public/favicon.svg`, with 16, 24, 32, 48, 64, 128 and 256 pixel entries. The Squirrel packaging call validates and supplies the same ICO for `Setup.exe`.

`scripts/build-native-installer.ps1` writes `squirrel-provenance.json` beside the local Squirrel output. It follows the version-1 Squirrel evidence contract: exact source commit, package identity, package version, x64 architecture, clean-source and clean-output assertions, actual build start/end timestamps, an immutable build-log path and hash, and the staged runtime payload inventory. It separately records unsigned signer controls and a bounded process audit. A signer observation stops the package path rather than being represented as unsigned.

The receipt remains construction evidence. It does not claim installer execution, a successful launch, update behavior, code signing, publication, or a release.
