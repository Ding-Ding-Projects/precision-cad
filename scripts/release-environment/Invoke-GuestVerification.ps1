[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ArtifactDirectory,[Parameter(Mandatory=$true)][string]$ReceiptPath,[Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedSetupSha256)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$setup=Get-ChildItem -LiteralPath $ArtifactDirectory -Filter Setup.exe -File | Select-Object -First 1
if ($null -eq $setup) { throw 'Guest verification requires Setup.exe in the staged artifact directory.' }
if ((Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedSetupSha256) { throw 'Guest Setup.exe does not match the host-staged artifact hash.' }
$processObservation=[ordered]@{ mode='blocked'; exhaustive=$false; reason='Win32_ProcessStartTrace AccessDenied; polling is incomplete and is not accepted as exhaustive verification.' }
$receipt=[ordered]@{
    version=1
    kind='precision-cad-release-environment-runtime'
    observedAt=[DateTimeOffset]::UtcNow.ToString('o')
    boundary=[ordered]@{ disposableGuest=$true; localAppDataRedirect=$false; hostUserProfileMutation=$false }
    setup=[ordered]@{ name=$setup.Name; sha256=$ExpectedSetupSha256 }
    install=[ordered]@{ attempted=$false; verified=$false; reason='Installation is not attempted until exhaustive process observation is available.' }
    launch=[ordered]@{ attempted=$false; verified=$false; reason='Installation was not verified.' }
    updater=[ordered]@{ priorCandidateFeed='unverified'; states=@(); reason='A deterministic prior/candidate feed is reserved for the updater implementation.' }
    processObservation=$processObservation
    verdict='blocked'
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent ([IO.Path]::GetFullPath($ReceiptPath))) | Out-Null
$receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $ReceiptPath -Encoding utf8
$receipt | ConvertTo-Json -Depth 12
