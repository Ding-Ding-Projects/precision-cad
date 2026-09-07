# Project website

The initial website presents the development-stage product, seven planned feature specifications, a six-stage roadmap and local reading preferences. It does not provide a CAD editor or installer download.

## Implementation

The source lives in `website/`, using the generated Sites React/Vinext starter. Official Material Web buttons provide registered interactive components. Documentation search uses bounded literal text, with prefix and suffix options; it is not the full planned regular-expression workbench. English, Cantonese and bilingual copy and light/dark preferences persist in local browser storage. Documentation exports as Markdown.

The root `npm run build` exports and stages the static website in `dist/`. The registered hosting project is recorded in `.openai/hosting.json`. Build provenance is generated with version, source commit and build timestamp, included in the exported website, and displayed on its initial screen. This version describes the website, not a released desktop application.

## Design route

Material Designer was inspected before implementation. Its project-creation and complete-export documents explicitly describe source implementation with built/runtime interaction proof still pending. A candidate executable exists, but the required complete creation/export/handoff route was not verified. This website uses the sanctioned Sites implementation path. No Material Designer output or design-parity evidence is claimed.

## Current boundaries

The initial website does not yet satisfy the complete canonical per-surface feature contract. Advanced appearance editing, complete regex workbench, private-vocabulary loader, universal mode, narration, converter, local model management, status integration and related evidence remain open. Reading preferences and literal search must not be described as equivalents for those larger features.

Visual and interaction verification is recorded separately in the handoff. A successful static build alone does not prove accessibility, responsive layout or runtime interaction.
