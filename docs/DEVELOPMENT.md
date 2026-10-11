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

Preferences are UTF-16 INI data under LocalAppData, keyed by endpoint ID. Original
system names and custom display names remain separate. Aliases are limited to 255
characters; control characters are rejected. Empty aliases restore system names.

The progress indicator occupies a fixed slot; busy/idle does not add or remove rows.
Device connections or intentional visibility changes can legitimately change height.

## Commands

| Command | Purpose |
| --- | --- |
| `--settings` (default) | Open the standalone control window |
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
4. Ensure GitHub Actions passes and tag `vX.Y.Z`.
5. Publish installer, portable ZIP, and SHA256SUMS.txt on that release.

There is no automatic updater. Keep the Inno Setup AppId stable across releases
so installing a later version updates the existing per-user product.
