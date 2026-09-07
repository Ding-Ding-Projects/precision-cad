[CmdletBinding()]
param(
  [string]$Root = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Require([bool]$Condition, [string]$Message) {
  if (-not $Condition) { throw "Native release metadata validation failed: $Message" }
}

$icon = Join-Path $Root 'assets/precision-cad.ico'
Require (Test-Path -LiteralPath $icon -PathType Leaf) 'the multi-resolution ICO source is missing'
$bytes = [IO.File]::ReadAllBytes($icon)
Require ($bytes.Length -ge 6) 'the ICO source is truncated'
Require ([BitConverter]::ToUInt16($bytes, 0) -eq 0 -and [BitConverter]::ToUInt16($bytes, 2) -eq 1) 'the icon source is not an ICO file'
$count = [BitConverter]::ToUInt16($bytes, 4)
Require ($count -eq 7) 'the icon must contain exactly the required seven resolutions'
$sizes = @()
for ($index = 0; $index -lt $count; $index++) {
  $offset = 6 + ($index * 16)
  Require (($offset + 16) -le $bytes.Length) 'an icon directory entry is truncated'
  $width = $bytes[$offset]; if ($width -eq 0) { $width = 256 }
  $height = $bytes[$offset + 1]; if ($height -eq 0) { $height = 256 }
  Require ($width -eq $height) "icon entry $index is not square"
  $payloadLength = [BitConverter]::ToUInt32($bytes, $offset + 8)
  $payloadOffset = [BitConverter]::ToUInt32($bytes, $offset + 12)
  Require ($payloadLength -gt 0 -and ([uint64]$payloadOffset + [uint64]$payloadLength) -le [uint64]$bytes.Length) "icon entry $index points outside the file"
  $sizes += [int]$width
}
Require (@(Compare-Object -ReferenceObject @(16,24,32,48,64,128,256) -DifferenceObject $sizes).Count -eq 0) 'icon resolutions differ from 16, 24, 32, 48, 64, 128, 256'

$resource = Get-Content -Raw -LiteralPath (Join-Path $Root 'resources/native-version.rc.in')
foreach ($field in @('CompanyName', 'FileDescription', 'FileVersion', 'InternalName', 'OriginalFilename', 'ProductName', 'ProductVersion')) {
  Require ($resource.Contains("VALUE `"$field`"")) "version resource does not declare $field"
}
Require ($resource.Contains('IDI_PRECISION_CAD_ICON ICON')) 'version resource does not bind the application icon'

$appCmake = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/app/CMakeLists.txt')
$workerCmake = Get-Content -Raw -LiteralPath (Join-Path $Root 'src/geometry/CMakeLists.txt')
$installerScript = Get-Content -Raw -LiteralPath (Join-Path $Root 'scripts/build-native-installer.ps1')
Require ($appCmake.Contains('precision_cad_version.rc')) 'desktop target does not consume its version resource'
Require ($workerCmake.Contains('precision_geometry_worker_version.rc')) 'geometry worker does not consume its version resource'
Require ($installerScript.Contains('--setupIcon $setupIcon')) 'Squirrel package creation does not bind the project setup icon'
Write-Output 'Validated native icon inventory and executable version-resource wiring.'
