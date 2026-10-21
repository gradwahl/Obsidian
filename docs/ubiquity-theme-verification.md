# Sole Ubiquity interface — 2026-10-21

Appearance and its Netflix/Glass and Ubiquity buttons are removed from native
and browser Settings. Native startup selects Ubiquity without loading the old
`.theme` preference. Browser startup no longer reads or writes the old
`poldrive-ui-theme` key. Home, console libraries, navigation and details always
use Ubiquity. Filter reset does not change the interface.

The removed native rows no longer receive pointer, keyboard or controller
selection: Reset is now row 5. Browser Settings arrows and gamepad navigation
reach the remaining source/format controls and Reset; source/format disclosure
and checkbox controls also support Enter and Space.

## Local verification

- `npm run lint`, `npm run test:web`, `npm run build:static` passed.
- `python tests/check_static_preview.py` passed: root HTML, JS/CSS, catalog pages
  and artwork served over ordinary HTTP without Node at runtime.
- `static-preview/` was regenerated locally. It remains ignored by the existing
  repository configuration; no deployment/build paths or dependencies changed.
- Browser smoke testing used Python HTTP serving at the URL root: startup,
  Home, PS4 category selection, Search, Ubiquity details and closing the dialog,
  Settings, and keyboard opening of source selection. No browser console errors
  were captured during these checks.
- WSL Ubuntu `make build-linux`, `make test-native-snapshot` and
  `make test-settings` passed. The new Settings test covers keyboard/controller
  selection bounds, controller/pointer reset, and native rendering. Its capture
  is `build/settings-native.png`.
- `make test` remains blocked by existing regression tests referencing missing
  artwork helpers, including `library_alpha_bounds` and `prepare_library_logo`.
  The full native suite did not pass.

## Remaining verification and parity gaps

**AI Studio deployment not verified.** Upload/serve the rebuilt `static-preview/`
directory at the AI Studio URL root, hard refresh, and repeat Home, sidebar,
Search, categories, details and Settings checks. Confirm the console has no
fatal errors and assets load without a Vite/Node server.

**PS5 runtime not verified.** On device, confirm a previous Netflix preference
does not restore that interface, Settings has no Appearance section, and D-pad,
Cross and Circle navigate filters, Reset and back correctly.

The native capture and browser Settings review confirm removal of Appearance,
but do not establish complete visual parity. An existing difference remains:
native Settings presents source and format lists side by side, while browser
Settings presents a system dashboard with expandable filter rows. Aligning
those pre-existing layouts requires separate UI work. Compare at 1920×1080 with
the same language/catalog, including Home, library, sidebar, details and overlays;
actual PS5 verification is still required after the Linux comparison.
