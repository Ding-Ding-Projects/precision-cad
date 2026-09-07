[CmdletBinding()]
param(
    [switch]$Silent,
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'manifests/native-dependencies.json') | ConvertFrom-Json
$head = (git -C $root rev-parse --verify HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve the installer candidate commit.' }
if (git -C $root status --porcelain) { throw 'Installer packaging requires an unchanged committed source tree.' }
if (-not $Version) { $Version = (Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern '^project\(PrecisionCAD VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1).Matches[0].Groups[1].Value }
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Installer version '$Version' must be numeric semantic version text." }

function Get-VerifiedFile([string]$Uri, [string]$Path, [string]$ExpectedHash) {
    if (-not (Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) {
        $stage = "$Path.download-$([Guid]::NewGuid().ToString('N'))"
        Invoke-WebRequest -Uri $Uri -OutFile $stage
        if ((Get-FileHash -LiteralPath $stage -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) { Remove-Item -LiteralPath $stage -Force -ErrorAction SilentlyContinue; throw "SHA-256 mismatch for $Uri" }
        Move-Item -LiteralPath $stage -Destination $Path -Force
    }
    return $Path
}

$toolchain = Join-Path $env:LOCALAPPDATA 'precision-cad-toolchain'
$squirrelRoot = Join-Path $toolchain ("squirrel-" + $manifest.squirrel.version)
New-Item -ItemType Directory -Force -Path $squirrelRoot | Out-Null
$squirrelPackage = Get-VerifiedFile $manifest.squirrel.archiveUrl (Join-Path $squirrelRoot $manifest.squirrel.archiveName) $manifest.squirrel.sha256
$squirrelExtract = Join-Path $squirrelRoot 'package'
$squirrel = Join-Path $squirrelExtract $manifest.squirrel.toolPath
if (-not (Test-Path -LiteralPath $squirrel)) { Expand-Archive -LiteralPath $squirrelPackage -DestinationPath $squirrelExtract -Force }
if (-not (Test-Path -LiteralPath $squirrel)) { throw 'Pinned Squirrel.Windows package does not contain Squirrel.exe.' }
$nugetRoot = Join-Path $toolchain ("nuget-" + $manifest.nuget.version)
New-Item -ItemType Directory -Force -Path $nugetRoot | Out-Null
$nuget = Get-VerifiedFile $manifest.nuget.url (Join-Path $nugetRoot 'nuget.exe') $manifest.nuget.sha256

& (Join-Path $PSScriptRoot 'build-native.ps1')
if ($LASTEXITCODE -ne 0) { throw "Native build failed with exit code $LASTEXITCODE." }
& (Join-Path $PSScriptRoot 'stage-native-runtime.ps1')
if ($LASTEXITCODE -ne 0) { throw "Native runtime staging failed with exit code $LASTEXITCODE." }

$staged = Join-Path $root 'build/native/bin'
$candidateShort = $head.Substring(0, 12)
$output = Join-Path $root ("artifacts/native/squirrel-windows/" + $candidateShort)
$artifactsRoot = [IO.Path]::GetFullPath((Join-Path $root 'artifacts/native/squirrel-windows')) + [IO.Path]::DirectorySeparatorChar
if (-not ([IO.Path]::GetFullPath($output) + [IO.Path]::DirectorySeparatorChar).StartsWith($artifactsRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer output must remain inside the task-owned artifacts directory.' }
if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Item -ItemType Directory -Force -Path $output | Out-Null
$packageRoot = Join-Path $root ("build/native/squirrel-package/" + $candidateShort)
$packageRootGuard = [IO.Path]::GetFullPath((Join-Path $root 'build/native/squirrel-package')) + [IO.Path]::DirectorySeparatorChar
if (-not ([IO.Path]::GetFullPath($packageRoot) + [IO.Path]::DirectorySeparatorChar).StartsWith($packageRootGuard, [StringComparison]::OrdinalIgnoreCase)) { throw 'Squirrel package workspace must remain inside the task-owned build directory.' }
if (Test-Path -LiteralPath $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
$packageLib = Join-Path $packageRoot 'lib/net45'
New-Item -ItemType Directory -Force -Path $packageLib | Out-Null
$stagedItems = @(Get-ChildItem -LiteralPath $staged -Force)
if ($stagedItems.Count -eq 0) { throw 'Native runtime staging produced no packageable files.' }
foreach ($item in $stagedItems) { Copy-Item -LiteralPath $item.FullName -Destination $packageLib -Recurse -Force }
$nuspec = Join-Path $packageRoot 'PrecisionCAD.nuspec'
@"
<?xml version="1.0"?>
<package>
  <metadata>
    <id>PrecisionCAD</id>
    <version>$Version</version>
    <title>Precision CAD</title>
    <authors>Precision CAD</authors>
    <owners>Precision CAD</owners>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>Precision CAD native development release.</description>
  </metadata>
</package>
"@ | Set-Content -LiteralPath $nuspec -Encoding utf8
& $nuget pack $nuspec -BasePath $packageRoot -OutputDirectory $output -NoPackageAnalysis -NonInteractive
if ($LASTEXITCODE -ne 0) { throw 'NuGet package construction failed.' }
$nupkg = Join-Path $output ("PrecisionCAD.$Version.nupkg")
if (-not (Test-Path -LiteralPath $nupkg)) { throw 'NuGet package construction did not produce the expected input package.' }
Push-Location $output
try { & $squirrel --releasify $nupkg --releaseDir $output --no-msi } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw "Squirrel.Windows releasify failed with exit code $LASTEXITCODE." }
$setup = @(Get-ChildItem -LiteralPath $output -File -Filter 'Setup.exe')
$releases = @(Get-ChildItem -LiteralPath $output -File -Filter 'RELEASES')
$full = @(Get-ChildItem -LiteralPath $output -File -Filter '*-full.nupkg')
if ($setup.Count -ne 1 -or $releases.Count -ne 1 -or $full.Count -lt 1) { throw 'Squirrel.Windows did not produce exactly one Setup.exe, one RELEASES file, and at least one full nupkg.' }
if ((Get-AuthenticodeSignature -LiteralPath $setup[0].FullName).Status -ne 'NotSigned') { throw 'Setup.exe must be unsigned for this development release.' }
$releaseText = Get-Content -Raw -LiteralPath $releases[0].FullName
foreach ($package in $full) { if ($releaseText -notmatch [regex]::Escape($package.Name)) { throw "RELEASES does not reference $($package.Name)." } }
$receipt = [ordered]@{ schemaVersion=1; sourceCommit=$head; version=$Version; packagingCommand='build-installer.bat /s'; signing='unsigned'; artifacts=[ordered]@{ setup=[ordered]@{name=$setup[0].Name;bytes=$setup[0].Length;sha256=(Get-FileHash $setup[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()}; releases=[ordered]@{name=$releases[0].Name;bytes=$releases[0].Length;sha256=(Get-FileHash $releases[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()}; fullPackages=@($full | ForEach-Object { [ordered]@{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} }) } }
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'squirrel-artifact-receipt.json') -Encoding utf8
Get-ChildItem -LiteralPath $output -File | Sort-Object Name | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name } | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
Write-Output "Unsigned Squirrel.Windows installer artifacts: $output"
