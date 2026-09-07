param([switch]$InstallOnly, [switch]$Run)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$version = '22.23.2'
$archive = "node-v$version-win-x64.zip"
$expected = '1177b4137ba5adaa56354ae40f1080c7450e8ae09cecb47da459d1c52ac99f97'
$cache = Join-Path $env:LOCALAPPDATA 'precision-cad-toolchain'
$nodeHome = Join-Path $cache "node-v$version-win-x64"
$node = Join-Path $nodeHome 'node.exe'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
if (-not (Test-Path -LiteralPath $node)) {
    $zip = Join-Path $cache $archive
    if (-not (Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
        Invoke-WebRequest "https://nodejs.org/dist/v$version/$archive" -OutFile $zip
    }
    if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Pinned Node.js archive hash mismatch.' }
    Expand-Archive -LiteralPath $zip -DestinationPath $cache -Force
}
if ((& $node --version) -ne "v$version") { throw 'Unexpected Node.js version.' }
$env:PATH = "$nodeHome;$env:PATH"
Push-Location $root
try {
    & (Join-Path $nodeHome 'npm.cmd') --prefix website ci
    if ($LASTEXITCODE -ne 0) { throw "Website dependencies failed: $LASTEXITCODE" }
    if (-not $InstallOnly) {
        & (Join-Path $nodeHome 'npm.cmd') run build
        if ($LASTEXITCODE -ne 0) { throw "Website build failed: $LASTEXITCODE" }
    }
    if ($Run) { Write-Output 'Static website built in dist/. The desktop application is not implemented yet.' }
} finally { Pop-Location }
