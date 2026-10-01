[CmdletBinding()]
param(
    [string]$BaseImagePath,
    [string]$VmName = 'PrecisionCAD-ReleaseVerification',
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ErrorKind([Exception]$Exception) {
    if ($Exception -is [System.UnauthorizedAccessException] -or $Exception.Message -match 'required permission|requires elevation|Access is denied') { return 'authorization' }
    return 'unavailable'
}

function Invoke-Probe([scriptblock]$Action) {
    try { return [ordered]@{ available=$true; value=& $Action; failure=$null } }
    catch { return [ordered]@{ available=$false; value=$null; failure=[ordered]@{ kind=(Get-ErrorKind $_.Exception); type=$_.Exception.GetType().FullName; message=$_.Exception.Message } } }
}

if ([string]::IsNullOrWhiteSpace($VmName) -or $VmName -notmatch '^[A-Za-z0-9][A-Za-z0-9 ._-]{0,63}$') { throw 'VmName must be a bounded local Hyper-V name.' }
$baseImage = if ([string]::IsNullOrWhiteSpace($BaseImagePath)) {
    [ordered]@{ supplied=$false; exists=$false; path=$null; sha256=$null; reason='No approved base image was supplied. The preflight does not search disks for media.' }
} else {
    $resolved = [IO.Path]::GetFullPath($BaseImagePath)
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { [ordered]@{ supplied=$true; exists=$false; path=$resolved; sha256=$null; reason='The explicitly supplied base image does not exist.' } }
    else { [ordered]@{ supplied=$true; exists=$true; path=$resolved; sha256=(Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.ToLowerInvariant(); reason=$null } }
}

$module = Get-Module -ListAvailable -Name Hyper-V | Select-Object -First 1
$vmProbe = Invoke-Probe { @(Get-VM | Select-Object -Property Name,State,Generation,Version,Path) }
$featureProbe = Invoke-Probe { Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All | Select-Object -Property FeatureName,State }
$capability = [ordered]@{
    hyperVModulePresent=($null -ne $module)
    newVmCommandPresent=($null -ne (Get-Command New-VM -ErrorAction SilentlyContinue))
    getVm=$vmProbe
    feature=$featureProbe
    existingTargetVm=$null
}
if ($vmProbe.available) {
    $existing=@($vmProbe.value | Where-Object Name -eq $VmName | Select-Object -First 1)
    if ($existing.Count -eq 1) { $capability.existingTargetVm=$existing[0] }
}

$blockers=[Collections.Generic.List[string]]::new()
if (-not $capability.hyperVModulePresent -or -not $capability.newVmCommandPresent) { $blockers.Add('Hyper-V management commands are unavailable.') }
if (-not $vmProbe.available) { $blockers.Add('Hyper-V inventory could not be read: ' + $vmProbe.failure.kind + '.') }
if (-not $featureProbe.available) { $blockers.Add('Hyper-V feature state could not be read: ' + $featureProbe.failure.kind + '.') }
if ($featureProbe.available -and $featureProbe.value.State -ne 'Enabled') { $blockers.Add('The Hyper-V feature is not enabled.') }
if (-not $baseImage.exists) { $blockers.Add('An explicit approved base image is required.') }
if ($null -ne $capability.existingTargetVm) { $blockers.Add('The requested task-owned virtual machine name already exists and will not be replaced.') }
$receipt=[ordered]@{
    version=1
    kind='precision-cad-release-environment-preflight'
    observedAt=[DateTimeOffset]::UtcNow.ToString('o')
    vmName=$VmName
    provisionable=($blockers.Count -eq 0)
    blockers=@($blockers)
    baseImage=$baseImage
    capability=$capability
    safety=[ordered]@{ existingVmsReadOnly=$true; existingVmMutation=$false; hostSecurityMutation=$false; localAppDataRedirect=$false; mediaDiscovery='explicit-path-only' }
}
$json=$receipt | ConvertTo-Json -Depth 12
if ($OutputPath) { New-Item -ItemType Directory -Force -Path (Split-Path -Parent ([IO.Path]::GetFullPath($OutputPath))) | Out-Null; Set-Content -LiteralPath $OutputPath -Value $json -Encoding utf8 }
$json
if (-not $receipt.provisionable) { exit 2 }
