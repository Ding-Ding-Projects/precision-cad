[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ReceiptPath,[switch]$RequireComplete)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$receipt=Get-Content -Raw -LiteralPath $ReceiptPath | ConvertFrom-Json
if ($receipt.version -ne 1 -or $receipt.kind -ne 'precision-cad-release-environment-runtime' -or [string]::IsNullOrWhiteSpace($receipt.observedAt)) { throw 'Release receipt schema is unsupported.' }
if (-not $receipt.boundary.disposableGuest -or $receipt.boundary.localAppDataRedirect -or $receipt.boundary.hostUserProfileMutation) { throw 'Release receipt does not prove the required disposable guest boundary.' }
if ($receipt.setup.name -ne 'Setup.exe' -or $receipt.setup.sha256 -notmatch '^[0-9a-f]{64}$') { throw 'Release receipt has no verified Setup.exe identity.' }
if ($receipt.processObservation.exhaustive -ne $true) {
    if ($RequireComplete) { throw 'Release receipt cannot be complete without exhaustive Win32_ProcessStartTrace observation.' }
    if ($receipt.verdict -ne 'blocked' -or $receipt.processObservation.mode -ne 'blocked' -or $receipt.processObservation.reason -notmatch 'AccessDenied' -or $receipt.install.attempted -or $receipt.launch.attempted) { throw 'An incomplete process observer must remain explicitly blocked.' }
    Write-Output 'Validated blocked release receipt; exhaustive process observation remains unavailable.'
    exit 0
}
throw 'No complete-runtime verifier is implemented. Boolean receipt fields cannot substitute for independent installed-file, process-observer, Lowlevel, and updater-feed evidence.'
