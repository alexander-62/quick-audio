# QuickAudio

[Русская документация](README.ru.md) · [Download](https://github.com/alexander-62/quick-audio/releases) · [Report a problem](https://github.com/alexander-62/quick-audio/issues)

**Audio output and microphone selection directly inside the Windows 11 Quick Settings panel.**

QuickAudio adds two native lists above the standard controls in **Win + A**.
Select a device with one click, hide unused devices, and give them names you recognize.

## Features

- Independent output and microphone selectors in the original system panel.
- Confirmed system defaults and live device connection/disconnection updates.
- Optional synchronization with the Windows communications device role.
- Device visibility settings and custom display names, saved by endpoint ID.
- Settings page with scrolling, contained inside Quick Settings.
- Progress indication that does not change the panel's height.
- Windows startup toggle in both the panel settings and the standalone control window.
- Version number shown in settings, executable properties, installer, and release assets.
- Per-user installation and an uninstaller; no administrator privileges required.
- Manual update checking and verified installer downloads from GitHub Releases.

## Install

Download `QuickAudio-Setup-0.2.2-x64.exe` from [Releases](https://github.com/alexander-62/quick-audio/releases).
Run the installer and choose whether to start QuickAudio with Windows. Then press **Win + A**.
The panel, controls, update messages, and installer support Russian and English.
Choose **Language → Automatic / Русский / English** below **Start with Windows**
in QuickAudio controls or the Win + A panel settings. The selection applies immediately,
is shared between both windows, and is saved for future launches.
Automatic uses Russian for a Russian Windows display language and English otherwise.

Open **QuickAudio** from the Start menu to turn the panel on/off or change startup behavior.
Turning the panel off for this session and disabling startup are independent controls.

The portable ZIP contains the same application: run `QuickAudio.exe` to open its controls,
then click **Enable**. Keep the folder in a stable location if you enable startup.

## Device settings

Use the small gear next to **Also for calls**.
The settings page stays inside the panel, with its own scrollbar and a Back button.

- A checked device is visible in our selector; unchecking it does not disable its hardware.
- Each device occupies one row: visibility checkbox, name, and pencil button.
- Click the pencil to edit the name in place; the button becomes **✓**.
- Press **✓** or **Enter** to save, or **Esc** to cancel.
- Clear the field and save to restore the original Windows name.
- The tooltip retains the original name when an alias is used.

Changing a system default does not override devices explicitly selected by individual apps.
Some apps apply the change only when a recording/playback session is restarted.

## Requirements and compatibility

- Windows 11 **24H2 or later**, running natively on x64. ARM64 is not supported.
- Tested on Windows 11 build **26300.9457**; other builds are not yet verified.
- No separate Visual C++ Redistributable is required; the runtime is linked statically.

This is an early **0.2.2** release. Windows does not offer a stable third-party extension API
for this panel. QuickAudio uses XAML Diagnostics and the undocumented PolicyConfig interface.
Windows updates may require adaptations. Unsupported layouts are logged instead of patched blindly.
Release binaries are currently unsigned.

## Uninstall and local data

Uninstall through Windows **Installed apps → QuickAudio**, or the Start menu shortcut.
Startup is removed and the original panel layout is restored. Installation updates and
uninstallation close the specific ShellHost instance holding our DLL; Quick Settings can
be reopened afterward. This does not require restarting the computer.

Preferences remain in `%LOCALAPPDATA%\QuickAudio\settings.ini` for future reinstalls.
Local diagnostic logs are stored there too and rotate at approximately 1 MiB.
If the shell cannot access that log location, its diagnostic log falls back to
`QuickAudio.Hook.log` in the application directory.
No microphone recordings, audio streams, telemetry, or network requests are made by the agent.
The GitHub button opens the project's public page only when clicked.
The update window makes HTTPS requests to GitHub only when the user requests a check
or download. Downloaded installers are kept under `%LOCALAPPDATA%\QuickAudio\Updates`.

## Build from source

1. Install Visual Studio 2022 C++ tools and a Windows SDK with C++/WinRT (tested with 10.0.26100.0).
2. Run `scripts\build.cmd` from a Windows command prompt.
3. To package, install [Inno Setup 6](https://jrsoftware.org/isinfo.php) and run:

```powershell
./scripts/package.ps1
```

Pass `-Compiler 'path\to\ISCC.exe'` if the compiler is installed elsewhere.
Installer, portable ZIP, and SHA-256 checksums are written to `dist/`.
See [development notes](docs/DEVELOPMENT.md) and [testing](docs/TESTING.md).

## Versioning

Releases use `MAJOR.MINOR.PATCH`, currently **0.2.2**.
User-facing version numbers and the internal IPC protocol version are separate.
Use `scripts/set-version.ps1` when preparing a new release; see [CHANGELOG](CHANGELOG.md).
Click **Check for updates** in QuickAudio controls, or use the
same button in the Win + A settings page. If a newer stable release exists, click
**Install X.Y.Z**. The installer is downloaded in the background,
checked against the release asset's exact size and SHA-256, then opened as a normal
installation wizard. Complete the wizard to update; preferences are preserved.
No background polling, prerelease installation, or downgrade is performed.
Portable copies open the release page for a manual ZIP update instead.

## License and credits

GPL-3.0-or-later. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).
You may use, modify, and redistribute QuickAudio, including commercially.
When distributing it or a modified version, provide the corresponding source code
under GPL-3.0-or-later and preserve the license and attribution notices.
The program is provided without warranty; the full license contains the binding terms.
The XAML Diagnostics COM connection pattern is adapted from [m417z/UWPSpy](https://github.com/m417z/UWPSpy).
The [Windows 11 Notification Center Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp)
and its [styling guide](https://github.com/ramensoftware/windows-11-notification-center-styling-guide)
provided reference material for the system panel's native layout.

