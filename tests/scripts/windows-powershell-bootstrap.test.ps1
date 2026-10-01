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
    if ($name -ne 'build-installer.bat' -and $content -notmatch '-File [^\r\n]+ %\*') { throw "$name does not forward command-line arguments to its PowerShell script." }
    if ($content -notmatch 'exit /b %errorlevel%') { throw "$name does not return its child exit code." }
}

$installerWrapper = Get-Content -LiteralPath (Join-Path $root 'build-installer.bat') -Raw
foreach ($alias in '/s', '--silent', 'SILENT') {
    if ($installerWrapper -notmatch [regex]::Escape($alias)) { throw "build-installer.bat does not support $alias." }
}
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('precision-cad-installer-wrapper-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
try {
    $fixture = Join-Path $fixtureRoot 'fixture.ps1'
    @'
[CmdletBinding()]
param([switch]$Silent, [string]$Version)
[ordered]@{ silent=[bool]$Silent; version=$Version } | ConvertTo-Json -Compress
exit [int]$env:PRECISION_CAD_FIXTURE_EXIT
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $wrapper = (Get-Content -LiteralPath (Join-Path $root 'build-installer.bat') -Raw).Replace('"%~dp0scripts\build-native-installer.ps1"', ('"' + $fixture + '"'))
    $wrapperPath = Join-Path $fixtureRoot 'build-installer.bat'
    Set-Content -LiteralPath $wrapperPath -Value $wrapper -Encoding ascii
    function Invoke-InstallerWrapper([string[]]$Arguments, [bool]$ExpectedSilent, [string]$ExpectedVersion, [int]$ExpectedExit = 0, [bool]$UseSilentEnvironment = $false) {
        $oldSilent = $env:SILENT
        $oldExit = $env:PRECISION_CAD_FIXTURE_EXIT
        try {
            if ($UseSilentEnvironment) { $env:SILENT = '1' } else { Remove-Item Env:SILENT -ErrorAction SilentlyContinue }
            $env:PRECISION_CAD_FIXTURE_EXIT = $ExpectedExit
            $output = & $env:ComSpec /d /s /c ('call "' + $wrapperPath + '" ' + ($Arguments -join ' ')) 2>&1
            if ($LASTEXITCODE -ne $ExpectedExit) { throw "Wrapper exit code $LASTEXITCODE did not match $ExpectedExit." }
            if ($ExpectedExit -eq 0) {
                $received = $output | Select-Object -Last 1 | ConvertFrom-Json
                if ([bool]$received.silent -ne $ExpectedSilent -or $received.version -ne $ExpectedVersion) { throw "Wrapper arguments were not typed correctly: $output" }
            }
        } finally {
            if ($null -eq $oldSilent) { Remove-Item Env:SILENT -ErrorAction SilentlyContinue } else { $env:SILENT = $oldSilent }
            if ($null -eq $oldExit) { Remove-Item Env:PRECISION_CAD_FIXTURE_EXIT -ErrorAction SilentlyContinue } else { $env:PRECISION_CAD_FIXTURE_EXIT = $oldExit }
        }
    }
    Invoke-InstallerWrapper @('/s') $true $null
    Invoke-InstallerWrapper @('/s', '-Version', '0.1.0') $true '0.1.0'
    Invoke-InstallerWrapper @('--silent') $true $null
    Invoke-InstallerWrapper @('-Silent') $true $null
    Invoke-InstallerWrapper @() $false $null
    Invoke-InstallerWrapper @('-Version', 'not-semver') $false 'not-semver'
    Invoke-InstallerWrapper @() $true $null 0 $true
    Invoke-InstallerWrapper @('/s') $true $null 37
} finally {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
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
