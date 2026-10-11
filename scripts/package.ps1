param([string]$Compiler)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Compiler) {
    $candidates = @('C:\Program Files (x86)\Inno Setup 6\ISCC.exe', 'C:\Program Files\Inno Setup 6\ISCC.exe')
    $Compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Compiler) { throw 'Inno Setup 6 compiler is required. Pass -Compiler with its ISCC.exe path.' }
& $Compiler (Join-Path $root 'installer\QuickAudio.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer compiler failed: $LASTEXITCODE" }
$version = [regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), '#define QA_VERSION "([^"]+)"').Groups[1].Value
$staging = Join-Path $root 'build\portable'
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'build\QuickAudio.exe'), (Join-Path $root "build\QuickAudio.Hook.$version.dll"), (Join-Path $root 'LICENSE'), (Join-Path $root 'THIRD_PARTY_NOTICES.md'), (Join-Path $root 'README.ru.md') -Destination $staging
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath (Join-Path $root "dist\QuickAudio-Portable-$version-x64.zip") -Force
Get-ChildItem (Join-Path $root 'dist') -File | Where-Object { $_.Extension -in '.exe','.zip' } | ForEach-Object {
    $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($_.Name)"
} | Set-Content -LiteralPath (Join-Path $root 'dist\SHA256SUMS.txt') -Encoding utf8
