[CmdletBinding()]
param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($Root)) { $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw "Release environment test failed: $Message"}}
$scriptRoot=Join-Path $Root 'scripts/release-environment'
foreach($name in @('Test-ReleaseEnvironmentPreflight.ps1','New-ReleaseEnvironmentVm.ps1','Invoke-GuestReleaseVerification.ps1','Invoke-GuestVerification.ps1','Test-GuestReleaseReceipt.ps1')) { Require (Test-Path -LiteralPath (Join-Path $scriptRoot $name)) "$name is missing" }
$preflightPath=Join-Path $scriptRoot 'Test-ReleaseEnvironmentPreflight.ps1'
$preflightReceipt=Join-Path ([IO.Path]::GetTempPath()) ('precision-cad-release-preflight-'+[Guid]::NewGuid().ToString('N')+'.json')
$shell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$startInfo=[Diagnostics.ProcessStartInfo]::new(); $startInfo.FileName=$shell; $startInfo.Arguments=('-NoProfile -ExecutionPolicy Bypass -File "{0}" -VmName PrecisionCAD-ReleaseEnvironment-Test -OutputPath "{1}"' -f $preflightPath,$preflightReceipt); $startInfo.UseShellExecute=$false; $startInfo.CreateNoWindow=$true
$process=[Diagnostics.Process]::new(); $process.StartInfo=$startInfo; Require ($process.Start()) 'live preflight process could not start'; $process.WaitForExit()
try { Require ($process.ExitCode -eq 2) 'live preflight unexpectedly became provisionable without an approved image and accessible Hyper-V inventory'; $live=Get-Content -Raw -LiteralPath $preflightReceipt | ConvertFrom-Json; Require (-not $live.provisionable) 'preflight receipt falsely reports provisionable'; Require ($live.baseImage.supplied -eq $false) 'preflight treated an absent base image as supplied'; Require ($live.safety.mediaDiscovery -eq 'explicit-path-only') 'preflight searched for media outside the supplied path' } finally { Remove-Item -LiteralPath $preflightReceipt -Force -ErrorAction SilentlyContinue }
$validator=Join-Path $scriptRoot 'Test-GuestReleaseReceipt.ps1'
$temp=Join-Path ([IO.Path]::GetTempPath()) ('precision-cad-release-receipt-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
try {
  $blocked=Join-Path $temp 'blocked.json'
  @{version=1;kind='precision-cad-release-environment-runtime';observedAt=[DateTimeOffset]::UtcNow.ToString('o');setup=@{name='Setup.exe';sha256=('a'*64)};boundary=@{disposableGuest=$true;localAppDataRedirect=$false;hostUserProfileMutation=$false};processObservation=@{mode='blocked';exhaustive=$false;reason='Win32_ProcessStartTrace AccessDenied'};install=@{attempted=$false};launch=@{attempted=$false};verdict='blocked'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $blocked -Encoding utf8
  & $validator -ReceiptPath $blocked
  $failed=$false;try{& $validator -ReceiptPath $blocked -RequireComplete}catch{$failed=$true};Require $failed 'complete validator accepted a blocked process observer'
  $invalid=Join-Path $temp 'invalid.json'; @{version=1;kind='precision-cad-release-environment-runtime';observedAt=[DateTimeOffset]::UtcNow.ToString('o');setup=@{name='Setup.exe';sha256=('a'*64)};boundary=@{disposableGuest=$true;localAppDataRedirect=$false;hostUserProfileMutation=$false};processObservation=@{mode='blocked';exhaustive=$false;reason='Win32_ProcessStartTrace AccessDenied'};install=@{attempted=$true};launch=@{attempted=$false};verdict='blocked'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $invalid -Encoding utf8
  $failed=$false;try{& $validator -ReceiptPath $invalid}catch{$failed=$true};Require $failed 'validator accepted an install claim under blocked observation'
  $forged=Join-Path $temp 'forged.json'; @{version=1;kind='precision-cad-release-environment-runtime';observedAt=[DateTimeOffset]::UtcNow.ToString('o');setup=@{name='Setup.exe';sha256=('a'*64)};boundary=@{disposableGuest=$true;localAppDataRedirect=$false;hostUserProfileMutation=$false};processObservation=@{mode='Win32_ProcessStartTrace';exhaustive=$true};install=@{verified=$true};launch=@{verified=$true};updater=@{states=@('available','downloading','ready-to-restart')};verdict='complete'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $forged -Encoding utf8
  $failed=$false;try{& $validator -ReceiptPath $forged -RequireComplete}catch{$failed=$true};Require $failed 'validator accepted forged complete boolean fields as independent runtime evidence'
} finally { Remove-Item -LiteralPath $temp -Recurse -Force }
Write-Output 'Validated release environment preflight, provisioning boundaries, and blocked-receipt behavior.'
