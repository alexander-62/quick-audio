# Security

For suspected vulnerabilities, use GitHub's private vulnerability reporting if
available on this repository. Otherwise report a minimal, non-sensitive description
in an issue and ask for a private contact before sharing exploit details.
Do not post credentials, personal logs, or machine-identifying data publicly.

The agent runs as the current user and communicates with a DLL in ShellHost through
session-local, user-specific Windows objects. It does not expose a network service.
Windows shell internals are not a stable extension API, so compatibility failures
should be reported with both application and OS version numbers.

Version 0.1.0 is the first public release; there is no long-term support branch yet.
