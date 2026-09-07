# Changelog

## 0.1.0 development foundation

- Fixed native and website batch bootstrap wrappers so a PowerShell 7 parent cannot hide the Windows PowerShell built-in hash and utility modules.
- Completed the installer wrapper's documented silent aliases and child exit-code propagation.
- Preserved the native runtime's `Precision CAD` organization identity so existing preference storage does not move after executable identity changes.
- Verified the final native source with 6/6 focused CTest targets, a successful local unsigned Squirrel package command, and focused default-renderer runtime captures. Installed-runtime and updater work remain incomplete.

- Added a local unsigned Squirrel.Windows packaging route for the native C++20/Qt/Open CASCADE foundation.
- The route stages the native runtime and produces `Setup.exe`, `RELEASES`, and a full `.nupkg` for one exact committed candidate.
- Local package construction does not claim installer execution, application runtime verification, automated updates, fresh-machine bootstrap, or public release publication.
