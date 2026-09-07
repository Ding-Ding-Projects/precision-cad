[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$expectedHost = '"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"'
$expectedModules = 'set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"'

foreach ($name in 'build.bat', 'build-desktop.bat', 'build-installer.bat', 'download-dependencies.bat') {
    $content = Get-Content -LiteralPath (Join-Path $root $name) -Raw
    if ($content -notmatch [regex]::Escape($expectedModules)) { throw "$name does not normalize PSModulePath for its child process." }
    if ($content -notmatch [regex]::Escape($expectedHost)) { throw "$name does not use the inbox Windows PowerShell host." }
    if ($content -notmatch '-File [^\r\n]+ %\*') { throw "$name does not forward command-line arguments to its PowerShell script." }
    if ($content -notmatch 'exit /b %errorlevel%') { throw "$name does not return its child exit code." }
}

$systemPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$polluted = 'C:\Program Files\PowerShell\Modules;C:\Users\codex\PowerShell\Modules'
$parentModulePath = $env:PSModulePath
$normalizedModulePath = "$env:ProgramFiles\WindowsPowerShell\Modules;$env:SystemRoot\System32\WindowsPowerShell\v1.0\Modules"
try {
    $env:PSModulePath = $polluted
    $env:PRECISION_CAD_TEST_MODULE_PATH = $normalizedModulePath
    $result = & $systemPowerShell -NoProfile -Command '$env:PSModulePath = $env:PRECISION_CAD_TEST_MODULE_PATH; Get-Command Get-FileHash,Get-ChildItem,Join-Path | Select-Object -ExpandProperty Source' 2>&1
} finally {
    $env:PSModulePath = $parentModulePath
    Remove-Item Env:PRECISION_CAD_TEST_MODULE_PATH -ErrorAction SilentlyContinue
}
if ($LASTEXITCODE -ne 0) { throw "The child Windows PowerShell command failed with exit code ${LASTEXITCODE}: $result" }
foreach ($module in 'Microsoft.PowerShell.Utility', 'Microsoft.PowerShell.Management') {
    if ($result -notcontains $module) { throw "Expected inbox module $module was not loaded under a polluted PSModulePath." }
}
Write-Output 'Windows PowerShell bootstrap regression passed.'
