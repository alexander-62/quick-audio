# Changelog

## 0.2.4 — 2026-10-12

- Move Also for calls into settings and put a smaller gear beside Output devices.
- Reserve the measured native row heights so injected controls do not squeeze system tiles.
- Bound both custom pages by a shared scroll viewport, accounting for monitor work area, scaling, and vertically adjacent cards.
- Recalculate on layout changes; restore native row minima and heights on removal.
- Add layout-budget regression checks for media-card presence, small screens, and DPI conversion.

## 0.2.3 — 2026-10-12

- Fix language synchronization between controls and the native Quick Settings panel.
- Publish the selected language through IPC rather than reading ShellHost's local settings path.
- Wake the backend immediately when controls change the language; rebuild panel labels without restarting.
- Add regression checks with different backend/client settings paths and language restoration.
- Bump the IPC protocol to v4 for the new confirmed language field.

## 0.2.2 — 2026-10-12

- Manual Automatic / Русский / English language selectors below startup in controls and panel settings.
- Persist the selected language and refresh both interfaces without restarting or changing audio defaults.
- Test language overrides, automatic fallback, persistence, external changes, and preservation of aliases.

## 0.2.1 — 2026-10-11

- Russian and English panel, controls, accessibility labels, and update messages.
- Select Russian for a Russian Windows display language; fall back to English otherwise.
- Explain the existing GPL-3.0-or-later redistribution terms in both READMEs.
- Include English and Russian documentation in installer and portable packages.

## 0.2.0 — 2026-10-11

- Manual update checking in controls and an entry point in the inline panel settings.
- Background installer download with exact release asset size and SHA-256 validation.
- Stable semantic version comparison, HTTPS GitHub/CDN restrictions, bounded requests.
- Installer handoff preserving preferences; portable copies open the release page.
- Update validation tests in CI; display scaling applied to the control window.

## 0.1.1 — 2026-10-11

- Compact, single-line device settings: visibility checkbox, display name, and pencil button.
- Rename in place; the pencil becomes a save checkmark. Enter saves and Esc cancels.
- Keep the row height unchanged while editing and retain input when saving fails.
- Use C++20 for compatibility with current MSVC and isolate portable packages by version.

## 0.1.0 — 2026-10-11

First public release.

- Native audio output and microphone lists above the Windows Quick Settings controls.
- Device visibility preferences and custom display names.
- Contained, scrollable settings page replacing the clipped popup.
- Progress indicator with no panel resize during switching.
- Per-user installer, Start menu controls, and uninstaller.
- Windows startup control in both settings surfaces.
- Visible version numbers, portable package, SHA-256 checksums, and build workflow.

The earlier experimental builds numbered 1.0/1.2 were local prototypes,
not public product releases. Product release numbering starts at 0.1.0.
