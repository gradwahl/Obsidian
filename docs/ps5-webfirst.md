# Obsidian PS5 manual Web-first Media launch (recovery)

For the current **single-file installer**, run `make ps5` and follow
[the single-file setup](ps5-build.md#one-distributable-obsidianelf).
It bundles/starts the service and configures autoload itself on port 8098.
The older two-file procedure below remains available on port 8097 for recovery.

This mode removes the interim Payload Manager `OK` browser page **once the
background server is running**. The previous Media tile navigates to
`http://127.0.0.1:8084/loadpayload:...` (so the browser displays the
payload-loader's `OK`, exits, and Obsidian's web fallback launches another
browser). The new tile goes **directly** to `http://127.0.0.1:8097/`.

## What gets built

Run `make ps5-webfirst` with the configured PS5 SDK (WSL is supported).
This deliberately produces three distinct ELF files:

- `build/obsidian-web-service.elf`: **background server**, fixed port 8097,
  browser launch disabled, no native SDL startup, no installer/helper startup.
  Serves the same React UI and file manager bridge on all IPv4 interfaces.
  LAN clients can open `http://<PS5-IP>:8097/`, including Files through the
  existing same-origin/session checks. Use on a trusted LAN.
- `build/obsidian-webfirst-shortcut.elf`: **one-time Media tile update**,
  installs `http://127.0.0.1:8097/` as the deeplink of the existing owned
  `WOLO00001` Media tile. It does **not** start the server.
- `build/obsidian-legacy-shortcut.elf`: **rollback Media tile**,
  restores the original `http://127.0.0.1:8084/loadpayload:/data/pldmgr/
  payloads/obsidian/obsidian.elf` deeplink (older builds). Current rollback
  targets `/data/pldmgr/payloads/obsidian/obsidian-app.elf`, saved by running the current `obsidian.elf` installer.

The current `build/obsidian.elf` single-file installer is **not** replaced. Its browser-opening helper no longer attempts to silently
re-register the old Media shortcut during fallback.

## Console setup — do this IN ORDER

1. **Back up** your existing `/data/pldmgr/payloads/obsidian/obsidian.elf`,
   shortcut metadata `/user/app/WOLO00001/sce_sys/param.json`, and other
   known-good payloads. Keep Payload Manager reachable by its own app or web
   UI. Do not uninstall an existing title.
2. Copy the *three new ELFs* to a folder Payload Manager can import,
   for example `/data/pldmgr/payloads/obsidian/`, keeping their exact names.
   If an older Obsidian web fallback is still running on 8097, stop it
   through Payload Manager's process controls or restart the exploited
   environment before attempting the new fixed-port service.
3. In **Payload Manager**, add `obsidian-web-service.elf` to its **autoload
   list**, enable autoload, and save. Payload Manager documents that its
   autoload list runs when **Payload Manager starts**. It does not start
   merely when a powered-off/unjailbroken PS5 boots; run the homebrew
   environment/loader and Payload Manager first. If supported by your
   installation, turn off automatic opening of *Payload Manager's own*
   browser page to avoid unrelated popup windows.
4. **Before modifying the Media tile**, launch `obsidian-web-service.elf`
   once from Payload Manager. In the PS5 browser visit
   `http://127.0.0.1:8097/api/wolodrive/health`.
   A successful response contains `wolodrive-webfirst-v1-files-lan` and
   a build checksum. Or check `/data/wolodrive-web.log` for
   `Web-first background service ready; browser launch disabled.`
   Verify `http://127.0.0.1:8097/` opens your Obsidian React UI too.
   **Do not continue until this works.**
5. Only now, run `obsidian-webfirst-shortcut.elf` **once through Payload
   Manager**, not by selecting the old Media tile. The shortcut installer
   updates the already-owned Media title without deleting it, and writes
   `/data/wolodrive-shortcut.log`. Confirm
   `Web-first Media target: http://127.0.0.1:8097/` and
   `Shortcut status: INSTALLED` or `CURRENT` in that log.
   Returning to Media (or refreshing it after a reboot) should show the
   existing Obsidian icon with the new direct browser route.
6. Select Obsidian from Media. It should open the existing web UI **without**
   displaying the `OK` response or launching a second browser. Controller
   UI, games catalogue and local Files behave as in the existing web fallback.

## If anything fails

- A browser connection failure after clicking Media means the background
  service isn't listening on port 8097. Start Payload Manager, confirm its
  autoload list, and run the service. This Media tile cannot itself execute
  an ELF or recover from an unavailable service.
- If the log says `Web-first port 8097 is busy`, stop the old Obsidian
  web fallback or another program using 8097. The new server refuses to
  choose another port since that would invalidate the Media shortcut.
- If the shortcut registration log reports an AppInstUtil failure, **do not**
  assume the Media link changed. Check the title marker and repeat the
  installer only after fixing the registration problem.
- **Rollback**: run `obsidian-legacy-shortcut.elf` through Payload Manager.
  This restores the prior `/loadpayload:` deeplink. Keep the installed
  `/data/pldmgr/payloads/obsidian/obsidian-app.elf` file so the restored tile works.
  Disable Obsidian service autoload afterward if it is no longer wanted.
- Console file permissions, Payload Manager versions, and the PS5's
  browser-launch behaviour must be verified on real hardware; local SDK
  and HTTP simulation tests cannot confirm the full user-facing launch.

The old Media deeplink was a *browser URL to the payload loader*, not a
native executable shortcut. This opt-in web-first path intentionally avoids
the extra first web page; it is not a fix for PS5 SDL/VideoOut failures.
