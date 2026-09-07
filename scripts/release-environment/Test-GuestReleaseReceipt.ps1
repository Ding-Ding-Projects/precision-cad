[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ReceiptPath,[switch]$RequireComplete)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$receipt=Get-Content -Raw -LiteralPath $ReceiptPath | ConvertFrom-Json
if ($receipt.version -ne 1 -or $receipt.kind -ne 'precision-cad-release-environment-runtime') { throw 'Release receipt schema is unsupported.' }
if (-not $receipt.boundary.disposableGuest -or $receipt.boundary.localAppDataRedirect -or $receipt.boundary.hostUserProfileMutation) { throw 'Release receipt does not prove the required disposable guest boundary.' }
if ($receipt.processObservation.exhaustive -ne $true) {
    if ($RequireComplete) { throw 'Release receipt cannot be complete without exhaustive Win32_ProcessStartTrace observation.' }
    if ($receipt.verdict -ne 'blocked' -or $receipt.processObservation.mode -ne 'blocked') { throw 'An incomplete process observer must remain explicitly blocked.' }
    Write-Output 'Validated blocked release receipt; exhaustive process observation remains unavailable.'
    exit 0
}
if ($receipt.install.verified -ne $true -or $receipt.launch.verified -ne $true -or @($receipt.updater.states).Count -lt 3) { throw 'Complete release receipt lacks installation, launch, or updater evidence.' }
Write-Output 'Validated complete disposable-guest release receipt.'
