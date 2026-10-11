$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$expected = [regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), '#define QA_VERSION "([^"]+)"').Groups[1].Value
$exe = Join-Path $root 'build\QuickAudio.exe'
$output = Join-Path $root 'build\version.txt'
$process = Start-Process -FilePath $exe -ArgumentList '--version' -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $output
if ($process.ExitCode -ne 0) { throw 'QuickAudio --version failed.' }
if ((Get-Content -Raw $output).Trim() -ne $expected) { throw 'Runtime version differs from source.' }
if ((Get-Item $exe).VersionInfo.ProductVersion -ne $expected) { throw 'File metadata version differs from source.' }
foreach ($file in 'installer\QuickAudio.iss','scripts\build.cmd','src\app.manifest') {
    if (-not (Get-Content -Raw (Join-Path $root $file)).Contains($expected)) { throw "Version missing from $file." }
}
Write-Output "Smoke passed: QuickAudio $expected."
