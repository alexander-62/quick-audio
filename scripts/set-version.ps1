param([Parameter(Mandatory)][string]$Version)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Use MAJOR.MINOR.PATCH, for example 0.1.1.' }
$parts = $Version.Split('.') | ForEach-Object { [int]$_ }
if ($parts | Where-Object { $_ -gt 65535 }) { throw 'Windows file version components must be <= 65535.' }
$root = Split-Path $PSScriptRoot -Parent
$header = Join-Path $root 'src\version.h'
$old = [regex]::Match((Get-Content -Raw $header), '#define QA_VERSION "([^"]+)"').Groups[1].Value
foreach ($relative in 'src\version.h','src\app.manifest','scripts\build.cmd','installer\QuickAudio.iss','README.md','README.ru.md') {
    $path = Join-Path $root $relative
    $text = (Get-Content -Raw $path).Replace($old, $Version)
    if ($relative -eq 'src\version.h') { $text = [regex]::Replace($text, '(?m)^#define QA_VERSION_NUM .*$', "#define QA_VERSION_NUM $($parts -join ','),0") }
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}
Write-Output "Version set to $Version. Add a CHANGELOG entry, rebuild, and tag v$Version."
