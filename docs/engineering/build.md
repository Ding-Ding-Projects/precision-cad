# Website build and deployment

The current executable build scope is the project website. The desktop CAD application is not implemented yet, so this pipeline cannot produce a desktop installer.

`build.bat` invokes the PowerShell bootstrap, which acquires Node.js 22.23.2 from its official distribution and verifies its ZIP SHA-256 before extraction. The website uses a committed npm lockfile. Run `npm run build` under that runtime to export the React/Vinext application and stage its public output in `dist/`.

The website workflow runs on `windows-2025`, bootstraps its own dependencies and publishes GitHub Pages. It contains no tests or lint jobs. Local runtime verification is reported separately. This documentation-only pipeline does not claim a CAD release.

## Dependency inventory

| Component | Source and role |
| --- | --- |
| Node.js 22.23.2 x64 | Official nodejs.org ZIP; hash pinned in bootstrap |
| npm | Bundled in the pinned Node.js distribution |
| React, Vinext, Vite | Exact versions and integrity records in website/package-lock.json |
| Material Web | Official registered components; packaged locally |
| Git and PowerShell | Windows runner environment; exact source and script execution |
| Pages publication actions | Workflow records the checkout, configure, upload and deploy actions |

No CDN fonts, analytics, customer data or server credentials are required by the static page. The static output excludes server intermediates. Build provenance records the actual source commit and build timestamp.

## Known limitations

The initial bootstrap checks the extracted Node version but does not yet implement the complete warm-file hash inventory, journalled atomic activation, or fresh-machine negative-regression coverage. Those remaining platform contracts are explicitly incomplete. The website's complete universal feature inventory also remains open.
