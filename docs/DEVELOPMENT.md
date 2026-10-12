# Development

## Architecture

`src/backend.cpp` owns Core Audio enumeration, IMMNotificationClient, PolicyConfig
switching, persistent preferences, and shell supervision. `src/panel.cpp` provides
native Windows.UI.Xaml controls through XAML Diagnostics. `src/manager.h` provides
standalone controls, graceful shutdown, and installer preparation.

The DLL is inserted at row 0 of Quick Settings' L1Grid. It preserves the original
row assignments and explicit heights and restores them when disabled. The settings
page replaces the selector content within the same container and uses a scroll
viewport based on the current monitor's available work area.

Named IPC objects include the user SID, session namespace, and protocol major
version. Requests have sequence numbers; the UI displays backend-confirmed state.
The shared protocol version is independent of the product's semantic version.
Protocol v4 also carries the backend-confirmed language. The native hook does not
read language preferences from ShellHost's environment or local INI cache. Language
changes increase the state revision; the UI rebuilds translated controls on its
dispatcher once the current command completes. Controls wake the backend after a
preference write so a change does not have to wait for its recovery poll.

`build/language-ipc.exe`, built by `tests/build-language.cmd`, exercises the live
agent with a client profile deliberately set to a different language. It restores
the original language and verifies unchanged audio defaults, aliases, and visibility.

Preferences are UTF-16 INI data under LocalAppData, keyed by endpoint ID. Original
system names and custom display names remain separate. Aliases are limited to 255
characters; control characters are rejected. Empty aliases restore system names.

The progress indicator occupies a fixed slot; busy/idle does not add or remove rows.
Device connections or intentional visibility changes can legitimately change height.

## Commands

| Command | Purpose |
| --- | --- |
| `--settings` (default) | Open the standalone control window |
| `--updates` | Open controls and start a user-requested update check |
| `--run` | Run the background agent without a console |
| `--stop` | Gracefully stop the agent and remove its UI |
| `--autostart` | Enable startup for the current user |
| `--remove-autostart` | Remove startup |
| `--version` | Print the product version |
| `--list` | Enumerate active endpoints and current defaults |
| `--check` | Reassign current defaults to themselves and confirm all roles |
| `--prepare-update` | Installer-only: stop and release our loaded shell DLL |

`--prepare-update` closes only same-session ShellHost processes containing a
QuickAudio.Hook DLL loaded from the invoking executable's installation directory.
This releases Windows' DLL file lock for updates and clean uninstall.

## Release procedure

1. Run `scripts/set-version.ps1 -Version X.Y.Z` and update CHANGELOG.md.
2. Build, run smoke checks, package, and complete Windows integration checks.
3. Commit reviewed source and documentation; never commit user settings or logs.
4. Add bilingual release notes at `docs/releases/X.Y.Z.md`.
5. Push a release commit to `main` with a message beginning `Release `.
   GitHub Actions builds, tests, and packages first. Its publication job creates
   a draft release at the exact commit, uploads the installer, portable ZIP, and
   SHA256SUMS.txt, verifies asset sizes and digests, then publishes it as latest.
   Ordinary commits and pull requests only build; they do not publish releases.

The manual updater in `src/updater.h` uses WinHTTP and Windows.Data.Json. It accepts
stable numeric three-part tags only, matches the exact x64 installer asset and
repository download URL, and requires the GitHub asset's SHA-256 and exact size.
Redirects are followed manually through a small HTTPS GitHub/CDN allowlist.
Requests have timeouts, total download limits, and no authentication or cookies.
SHA-256 is verified before writing and again from a read-locked file before launch.
Network work runs on worker threads; no requests occur in the audio agent's loop.
IPC request 8 opens `--updates` in a separate process so ShellHost does not make requests.
Portable installations open Releases rather than overwrite their running folder.

`tests/build-updater.cmd` and `build/updater-test.exe` test metadata and validation.
`build/updater-test.exe --live` additionally checks GitHub and downloads/verifies
the current installer without executing it; its temporary download is removed.

Keep the Inno Setup AppId stable across releases
so installing a later version updates the existing per-user product.
