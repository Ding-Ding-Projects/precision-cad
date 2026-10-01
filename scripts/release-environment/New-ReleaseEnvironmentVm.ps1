[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)][string]$BaseImagePath,
    [Parameter(Mandatory=$true)][string]$VmName,
    [Parameter(Mandatory=$true)][string]$VmPath,
    [Parameter(Mandatory=$true)][string]$SwitchName,
    [Parameter(Mandatory=$true)][switch]$AcceptBaseImageLicense,
    [Parameter(Mandatory=$true)][string]$PreflightPath,
    [string]$RegistryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'ReleaseEnvironmentRegistry.psm1') -Force
if (-not $AcceptBaseImageLicense) { throw 'Provisioning requires explicit acceptance of the approved base image licence.' }
$preflightRunner=(Get-Process -Id $PID).Path
& $preflightRunner -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Test-ReleaseEnvironmentPreflight.ps1') -BaseImagePath $BaseImagePath -VmName $VmName -OutputPath $PreflightPath | Out-Null
$preflightExit=$LASTEXITCODE
if ($preflightExit -ne 0) { throw "Release environment preflight is blocked with exit code $preflightExit. No virtual machine was created." }
$preflight=Get-Content -Raw -LiteralPath $PreflightPath | ConvertFrom-Json
if (-not $preflight.provisionable) { throw 'Preflight receipt is not provisionable.' }
try { $existingVm=Get-VM -Name $VmName -ErrorAction Stop } catch { if ($_.Exception.Message -notmatch 'not found|cannot find') { throw 'The requested virtual machine could not be inventoried safely.' }; $existingVm=$null }
if ($existingVm) { throw 'The requested virtual machine already exists and will not be replaced.' }
$switch=Get-VMSwitch -Name $SwitchName -ErrorAction Stop
if ($switch.SwitchType -ne 'Private') { throw 'Initial guest provisioning requires an explicitly named Private virtual switch.' }
$root=[IO.Path]::GetFullPath($VmPath)
if (Test-Path -LiteralPath $root) { throw 'The task-owned virtual machine path already exists and will not be overwritten.' }
if (-not $PSCmdlet.ShouldProcess($VmName, 'Create a new task-owned Generation 2 Hyper-V virtual machine')) { return }
$record=[ordered]@{version=2;operationId=[Guid]::NewGuid().ToString('N');vmName=$VmName;vmId=$null;vmPath=$root;vhdPath=(Join-Path $root 'disk.vhdx');baseImage=[ordered]@{path=([IO.Path]::GetFullPath($BaseImagePath));sha256=$null};generation=2;switch=[ordered]@{name=$switch.Name;id=$switch.Id;type=$switch.SwitchType};processorCount=4;startupBytes=4GB;preflightSha256=(Get-FileHash -LiteralPath $PreflightPath -Algorithm SHA256).Hash.ToLowerInvariant();stage='planned';failure=$null}
Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null
try {
  New-Item -ItemType Directory -Path $root | Out-Null; $record.stage='directory-created'; Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null
  $record.baseImage.sha256=(Get-FileHash -LiteralPath $record.baseImage.path -Algorithm SHA256).Hash.ToLowerInvariant(); Copy-Item -LiteralPath $record.baseImage.path -Destination $record.vhdPath; $record.stage='disk-copied'; Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null
  $createdVm=New-VM -Name $VmName -Generation 2 -VHDPath $record.vhdPath -Path $root -SwitchName $SwitchName; $record.vmId=$createdVm.Id.Guid; $record.stage='vm-created'; Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null
  Set-VMProcessor -VMName $VmName -Count 4; Set-VMMemory -VMName $VmName -DynamicMemoryEnabled $true -StartupBytes 4GB; $record.stage='configured'; Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null
} catch { $record.stage='partial'; $record.failure=$_.Exception.Message; Write-ReleaseEnvironmentRecord $record $RegistryRoot | Out-Null; throw }
$record | ConvertTo-Json -Depth 8
