[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ArtifactDirectory,[Parameter(Mandatory=$true)][string]$ReceiptPath)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$setup=Get-ChildItem -LiteralPath $ArtifactDirectory -Filter Setup.exe -File | Select-Object -First 1
if ($null -eq $setup) { throw 'Guest verification requires Setup.exe in the staged artifact directory.' }
$processObservation=[ordered]@{ mode='blocked'; exhaustive=$false; reason='Win32_ProcessStartTrace AccessDenied; polling is incomplete and is not accepted as exhaustive verification.' }
$receipt=[ordered]@{
    version=1
    kind='precision-cad-release-environment-runtime'
    observedAt=[DateTimeOffset]::UtcNow.ToString('o')
    boundary=[ordered]@{ disposableGuest=$true; localAppDataRedirect=$false; hostUserProfileMutation=$false }
    setup=[ordered]@{ name=$setup.Name; sha256=(Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    install=[ordered]@{ attempted=$false; verified=$false; reason='Installation is not attempted until exhaustive process observation is available.' }
    launch=[ordered]@{ attempted=$false; verified=$false; reason='Installation was not verified.' }
    updater=[ordered]@{ priorCandidateFeed='unverified'; states=@(); reason='A deterministic prior/candidate feed is reserved for the updater implementation.' }
    processObservation=$processObservation
    verdict='blocked'
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent ([IO.Path]::GetFullPath($ReceiptPath))) | Out-Null
$receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $ReceiptPath -Encoding utf8
$receipt | ConvertTo-Json -Depth 12
exit 3
