# Security

For suspected vulnerabilities, use GitHub's private vulnerability reporting if
available on this repository. Otherwise report a minimal, non-sensitive description
in an issue and ask for a private contact before sharing exploit details.
Do not post credentials, personal logs, or machine-identifying data publicly.

The agent runs as the current user and communicates with a DLL in ShellHost through
session-local, user-specific Windows objects. It does not expose a network service.
Windows shell internals are not a stable extension API, so compatibility failures
should be reported with both application and OS version numbers.

Manual updates trust the repository's GitHub Releases over verified HTTPS. The
updater matches the exact installer asset, validates its size and SHA-256, and
keeps a read lock during final verification and launch. SHA-256 establishes
consistency with GitHub metadata; it is not a publisher signature. A compromised
GitHub publishing account can publish a matching malicious binary and digest.
Release binaries are currently unsigned. The updater sends no audio, preferences,
device IDs, authentication tokens, or logs to GitHub.

Version 0.1.0 is the first public release; there is no long-term support branch yet.
