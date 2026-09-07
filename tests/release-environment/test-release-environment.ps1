[CmdletBinding()]
param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($Root)) { $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw "Release environment test failed: $Message"}}
$scriptRoot=Join-Path $Root 'scripts/release-environment'
foreach($name in @('Test-ReleaseEnvironmentPreflight.ps1','New-ReleaseEnvironmentVm.ps1','Invoke-GuestReleaseVerification.ps1','Invoke-GuestVerification.ps1','Test-GuestReleaseReceipt.ps1')) { Require (Test-Path -LiteralPath (Join-Path $scriptRoot $name)) "$name is missing" }
$preflight=Get-Content -Raw -LiteralPath (Join-Path $scriptRoot 'Test-ReleaseEnvironmentPreflight.ps1')
Require ($preflight.Contains('Get-VM')) 'preflight does not inventory existing virtual machines'
Require ($preflight.Contains('mediaDiscovery')) 'preflight does not record explicit-only media discovery'
Require ($preflight.Contains('exit 2')) 'preflight does not produce a non-success blocked outcome'
$provision=Get-Content -Raw -LiteralPath (Join-Path $scriptRoot 'New-ReleaseEnvironmentVm.ps1')
Require ($provision.Contains('AcceptBaseImageLicense')) 'provisioning does not require explicit licence acceptance'
Require ($provision.Contains('already exists and will not be replaced')) 'provisioning can replace a named virtual machine'
$guest=Get-Content -Raw -LiteralPath (Join-Path $scriptRoot 'Invoke-GuestVerification.ps1')
Require ($guest.Contains('Win32_ProcessStartTrace AccessDenied')) 'guest receipt does not preserve the known observer blocker'
Require ($guest.Contains('polling is incomplete')) 'guest receipt treats polling as complete evidence'
$validator=Join-Path $scriptRoot 'Test-GuestReleaseReceipt.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('precision-cad-release-receipt-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
try {
  $blocked=Join-Path $temp 'blocked.json'
  @{version=1;kind='precision-cad-release-environment-runtime';boundary=@{disposableGuest=$true;localAppDataRedirect=$false;hostUserProfileMutation=$false};processObservation=@{mode='blocked';exhaustive=$false};verdict='blocked'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $blocked -Encoding utf8
  & $validator -ReceiptPath $blocked
  $failed=$false;try{& $validator -ReceiptPath $blocked -RequireComplete}catch{$failed=$true};Require $failed 'complete validator accepted a blocked process observer'
} finally { Remove-Item -LiteralPath $temp -Recurse -Force }
Write-Output 'Validated release environment preflight, provisioning boundaries, and blocked-receipt behavior.'
