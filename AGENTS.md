# Agent instructions — Wolodrive PS5 ↔ web parity

These instructions apply to **every change in this repository**, including work delegated to sub-agents. **Preserve both PS5/web parity and AI Studio compatibility; neither requirement may be ignored.**

## Non-negotiable product requirement

**The PS5 app is the reference implementation. The web app must look and behave like the PS5 app.** Treat web parity as part of the definition of done for any user-facing change, not as optional follow-up work.

- Keep the **same screens, navigation structure, ordering, labels, language choices, content, artwork selection, layout proportions, focus/selection states, dialogs, notifications, and user-visible action results** in the PS5/native and web apps.
- Match the same interaction semantics for controller (D-pad, Cross, Circle, etc.), keyboard, and pointer wherever those inputs are supported. Device-specific input mechanisms may differ; the resulting app state and visible behaviour should not.
- Prefer shared data, constants, design tokens, and test fixtures instead of independently invented web behaviour. **Do not create web-only screens, styling, fake features, or alternative workflows** without an explicit product requirement.
- A responsive layout may adapt to narrow screens, but it must retain the same content, actions, semantics, and visual identity. For equivalent 16:9 viewports, aim for visually matching compositions.
- Do not silently relax this requirement because the implementation uses different technologies. If faithful parity cannot be implemented in the current task, call out the exact gap and any required PS5/browser support rather than claiming parity.

## Non-negotiable AI Studio compatibility (all edits)

**Every change must preserve the working AI Studio web preview.** This requirement applies to native/PS5 code, frontend code, build scripts, assets, dependencies, configuration, and documentation whenever they could affect how the project builds or runs in AI Studio. **Never trade away AI Studio compatibility to achieve PS5/web parity.** Meet both requirements, or document the conflict and request a decision before knowingly breaking either.

Treat the current deployment contract as the baseline unless an explicitly tested replacement has been agreed:

- **Static preview must work without Node or Vite at runtime.** AI Studio can host the generated `static-preview/` directory as ordinary static files from the URL root. No production Node server, build step on page load, terminal command, PHP/Python backend, WebSocket, proxy, or PS5 connection may be required just to display and navigate the web UI.
- **Preserve the root entry point and assets.** Keep `static-preview/index.html` usable over HTTP, along with all emitted JS/CSS, bundled catalog metadata, and versioned artwork. Preserve correct root-relative `/assets/...` URLs unless the AI Studio host and the static-preview test are updated and verified together. Do not introduce absolute local filesystem paths, `file://` requirements, or routes that assume an SPA history fallback AI Studio may not provide.
- **Avoid incompatible runtime assumptions.** No direct Node.js APIs, local filesystem reads, PS5 SDK/ELF calls, unsupported browser APIs, or mandatory external credentials in browser-executed code. Feature-detect optional browser APIs and offer a working fallback or honest unavailable state. Existing remote artwork/catalog integrations may remain optional; unavailable remote resources must not prevent the core preview from loading.
- **No new runtime dependencies on private services or secrets.** Build tooling can use Node and npm, but the produced preview must load independently of developer machines, environment variables, API keys, localhost-only servers, and unconfigured external endpoints. Keep secrets out of the client bundle.
- **Do not delete, rename, relocate, or replace the existing AI Studio preview workflow** or its entry point without explicit approval and successful deployment verification. Keep `npm run build:static`, the `static-preview/` output, and normal static HTTP serving operational.
- **Generated preview freshness is mandatory.** After any change affecting React, styling, browser behaviour, shared data, build configuration, dependency resolution, or artwork, run `npm run build:static` and include the updated output. Never present the old preview after newer source changes.
- **Protect existing AI Studio functionality.** Confirm that startup, Home, sliding sidebar, search, category navigation, dialogs, catalog rendering, images/fallbacks, keyboard/mouse controls, and Settings still function. Regressions in the preview are blockers even if the PS5/Linux build passes.
- **Prefer the smallest compatible edit.** Before changing build tooling, dependencies, output paths, HTML asset references, or shared data generators, identify the AI Studio impact and a safe rollback plan. Avoid broad refactors unrelated to the user's request.

### AI Studio verification gate — apply to every change

1. Check whether the change affects static hosting, its assets, browser runtime, or the shared data/build pipeline. If yes, inspect the existing AI Studio deployment contract **before** editing.
2. For relevant changes, run `npm run lint`, `npm run test:web`, `npm run build:static`, and `python tests/check_static_preview.py` (or `python3` where appropriate).
3. Serve `static-preview/` via plain HTTP **without Vite** at the URL root. Check that the page and emitted JS/CSS load, asset paths resolve, core UI works, and the browser console has no new fatal errors. If direct AI Studio preview access is available, check it there too.
4. If a verification fails because of a pre-existing issue (for example, missing local tracked artwork), identify that issue clearly, separately verify the affected output where possible, and **do not claim the AI Studio gate passed**.
5. If AI Studio itself cannot be accessed, report **"AI Studio deployment not verified"** along with the local build/static-server results. Do not conflate a successful Vite build with a successful AI Studio deployment.
6. **Do not mark a change ready when AI Studio compatibility is broken or unknown without stating the risk explicitly.** Resolve regressions before completion when feasible; if the environment prevents verification, give concrete manual validation steps.

## Current architecture and source of truth

The interfaces are **not** currently a shared renderer:

| Concern | PS5 / native reference | Browser counterpart |
| --- | --- | --- |
| App and rendering | `src/main.c`, `src/ui_netflix.c`, `src/wolodrive.h` (SDL2) | `src/main.tsx`, `src/App.tsx`, `src/components/*` (React) |
| Appearance / geometry | SDL rendering logic and layout constants in `src/ui_netflix.c` | `src/layout.ts`, `src/native.css`, `src/index.css`, React components |
| Catalog and actions | `src/catalog_pegasus.c`, `src/sys_bridge.c`, native settings | `src/catalog.ts`, `src/App.tsx`, shared catalog metadata |
| Catalog snapshot | Generated `src/native_catalog_snapshot.h` | `src/catalogData.json` and configured source metadata |
| Built web deliverable | n/a | `static-preview/` (generated via `npm run build:static`) |

Use **actual PS5 behaviour** as the final reference when it is testable; the Linux SDL2 build is a useful native proxy, but **a WSL/Linux screenshot is not proof of PS5 runtime behaviour**.

The native C/SDL2 application is the source of truth for existing UI/UX. Keep a mirrored browser implementation until the project deliberately migrates to one shared browser frontend. A future architecture change may replace the duplicate renderers only if it preserves PS5 support and parity.

## Required workflow for every UI/UX change

1. **Inspect both implementations before editing.** Identify the native behaviour and its React/CSS counterpart. Do not implement or approve a one-sided visual or behavioural change.
2. **Create or update both sides in the same change.** Keep screen hierarchy, geometry, fonts where available, image handling, colors, spacing, transitions, card/hero/detail layout, selection and focus, empty/error/loading states, and copy aligned.
3. **Keep data semantics aligned.** Both apps should agree about game identity, catalog categories, deduplication, sorting, filtering, release dates, ratings, cover and fallback priority, and the item selected on first load for the same data. Where one side fetches live data and the other is a snapshot, do not present the results as identical; either unify their data source or explicitly identify the unresolved difference.
4. **Mirror interactions and action outcomes.** Match navigation through Home, Search, Games/Roms, Emulators, Files, Payloads, Downloads, Settings, and Details; back behaviour; modal focus; carousel and grid movement; refresh; toasts; and localization. When native backend functionality is unavailable in a standalone web preview, show a truthful, explicit unsupported/unavailable state. Do not simulate successful installs/downloads, PS5 telemetry, or other console-only operations.
5. **Update shared fixtures and generated artifacts when their inputs change.** Keep the native snapshot in sync with shared catalog inputs using `scripts/generate_native_snapshot.py`, and **rebuild `static-preview/` after every React, CSS, or web asset change**. A modified React source alongside an older committed preview is a regression.
6. **Add or adjust parity checks.** Extend `tests/test_react_layout.cjs` and the relevant native tests for changed geometry, content, category ordering, semantics, focus, and interactions. Where automated parity cannot be shown, provide a concise manual cross-platform comparison plan.
7. **Visually verify before declaring parity.** Compare equivalent screens, selected games, language, catalog/filter state, and 16:9 resolution side by side in the native Linux renderer and browser; test on PS5 when an SDK/device is available. Check Home, carousels, game details, Search, library, navigation rail, overlays and responsive variants. Note font-engine differences rather than disguising them as a pass.
8. **Report verification honestly.** Distinguish successful lint/tests/builds from browser screenshot review, native Linux review, and actual PS5 verification. State blockers and remaining discrepancies explicitly.

### Checklist for changes to either UI

- [ ] Native and web implementations inspected; both updated if affected
- [ ] Same user-visible content, brand/name, artwork and fallback behaviour
- [ ] Same categories, sorting, filtering, selected item and navigation outcomes for the same catalog data
- [ ] Same visual geometry, screen composition, typography intent, focus and interaction states
- [ ] PS5/controller, keyboard and pointer semantics assessed; platform-only actions report truthful states
- [ ] Tests updated and run; known gaps documented
- [ ] `static-preview/` regenerated and checked whenever browser source/assets changed
- [ ] Browser vs native visual comparison performed, or explicitly marked unverified
- [ ] PS5-specific behaviour tested on device if possible, otherwise explicitly marked unverified
- [ ] AI Studio static-hosting requirements preserved, generated preview rebuilt if impacted, and deployment verified or explicitly marked unverified

## Verification commands

From the repository root:

```bash
npm run lint
npm run test:web
npm run build:static
python3 tests/check_static_preview.py
```

Use `python` instead of `python3` on Windows when necessary.

For the native side on a machine with the required SDL2/toolchain dependencies:

```bash
make build-linux
make test-native-snapshot
make test
```

Run `make ps5` only with the PS5 SDK available, and **do not claim PS5 validation from a successful Linux build**. If a relevant command cannot run, record why; do not misreport its outcome.

Serve and visually inspect the generated web preview at the HTTP root (not via `file://`), for example `python -m http.server 3000 --directory static-preview`. After rebuilding, refresh the browser to avoid using an old JS/CSS bundle.

## Release / review gate

**Do not describe a change as finished or parity-complete when the web preview still differs from the PS5/native reference for the task's affected screens or behaviour.** Any unavoidable difference must be explicitly documented with the cause and next steps. Never allow a stale `static-preview/` build to be presented as the current web app.

**AI Studio must remain usable after every edit.** A change that breaks the AI Studio static preview is not ready for release; if direct AI Studio verification is unavailable, state that explicitly and provide the local static-server test results instead of asserting compatibility.

For purely backend, tooling, or documentation changes, assess whether user-visible parity is affected; don't force unrelated UI rewrites when nothing visible changes.
