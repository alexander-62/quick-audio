# Contributing

Please include the QuickAudio version, Windows build, architecture, display scale,
and reproduction steps in bug reports. Redact device names and endpoint IDs from
logs if you consider them private. Do not upload your settings.ini or full local
logs without reviewing their contents.

Build instructions are in README.md. Changes to the audio backend and XAML panel
must preserve independent render/capture selection, communication-role preferences,
confirmed defaults, stable panel height during switching, and shutdown cleanup.

Test changes on Windows 11 rather than treating a successful compile as full validation.
Do not replace system files or require global administrator permissions.
Use small pull requests and describe which Windows builds and flows you tested.

Contributions are licensed under GPL-3.0-or-later.
