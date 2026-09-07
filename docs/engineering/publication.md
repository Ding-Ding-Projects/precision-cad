# Publication checks

The local `pre-push` hook checks the actual outgoing source revision through a private rules module, when the contributor has that module installed. The public repository contains neither the private dictionary nor a pinned dictionary digest.

Enable the hook with `git config core.hooksPath .githooks`. It runs `scripts/check-public-source.mjs` against each non-deletion outgoing revision and reports only file locations and match counts. It does not repeat detected private terms. A contributor without the private source receives an explicit skip, so the public project remains buildable by outsiders.

This check scans the current source tree, not every historical commit or semantic context. It is not a guarantee that every possible private-data disclosure will be detected. Public records still require review before publication, and already published history is not rewritten by the hook.

The website publication workflow remains build-and-deploy only. This local publication check is not a test or lint job in the hosted workflow.
