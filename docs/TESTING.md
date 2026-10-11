# Testing

Run `tests/smoke.ps1` after compiling to check runtime/file/build version consistency.
For an installed, running agent, compile `tests/build-integration.cmd` and run
`build/integration.exe`. This temporarily changes one alias, visibility, and startup
state and restores the previous values. It also confirms unchanged audio defaults.
Run it while no one is editing the panel preferences.
GitHub Actions builds and packages on Windows; it does not claim to test a real
desktop Quick Settings panel or connected audio hardware.

## Desktop integration checklist

- Install as a normal user; verify Start menu and Installed apps registration.
- Open Win + A and verify that both selectors precede the standard controls.
- Select an output and microphone; confirm Windows defaults and native selection marks.
- Check that switching does not change panel height.
- Open settings; verify that the heading and first item are reachable at your display scale.
- Rename a device using ✓ and Enter. Clear the name to restore the original.
- Verify that a settings row is single-line until its pencil is clicked, editing
  replaces the label in place, Enter/✓ save, and Esc restores the saved label.
- Hide/show an endpoint, close/reopen the panel, and restart the agent; verify persistence.
- Enable/disable startup in each UI; confirm the current-user Run entry and Windows startup behavior.
- Test communications synchronization both enabled and disabled.
- Disconnect/reconnect USB or Bluetooth endpoints and change defaults in another application.
- Stop/start the agent. Restart ShellHost and the audio service in a controlled test environment.
- Reinstall over a running version, then uninstall; verify file and startup cleanup.

The public release notes should distinguish completed tests from untested hardware
and OS variants. This project has so far been tested on x64 build 26300.9457.

## Completed for 0.1.0

- Local MSVC build and version smoke checks.
- Real per-user install, reinstall over the running application, and uninstall/reinstall.
- Confirmed removal of executable, DLL, and startup registration during uninstall.
- Preferences retained across uninstall/reinstall; prototype preferences migrated locally.
- Unicode and quoted alias roundtrip, alias restoration, visibility restoration,
  startup toggle/restoration, and unchanged audio defaults through real IPC.
- Native hook activation and row-0 insertion confirmed through diagnostics.
- The earlier prototype's output/microphone switching was confirmed by the user;
  busy-state height stability was measured on the real XAML panel.

Hardware disconnection variants, ARM64, and other Windows builds have not been verified.
The user supplied a screenshot of the contained settings page. The compact rename
interaction introduced in 0.1.1 still needs user visual confirmation.

## Completed for 0.1.1

- Local C++20 build and version smoke checks.
- Updated the running per-user installation from 0.1.0 to 0.1.1.
- Real IPC checks passed: Unicode/quoted alias roundtrip, visibility and startup
  restoration, and unchanged audio defaults.
- Compact rename code reviewed for fixed row height, single-line overflow,
  keyboard cancellation, and completion of the specific save request.
