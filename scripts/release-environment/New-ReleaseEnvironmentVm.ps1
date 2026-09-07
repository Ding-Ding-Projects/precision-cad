[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)][string]$BaseImagePath,
    [Parameter(Mandatory=$true)][string]$VmName,
    [Parameter(Mandatory=$true)][string]$VmPath,
    [Parameter(Mandatory=$true)][string]$SwitchName,
    [Parameter(Mandatory=$true)][switch]$AcceptBaseImageLicense,
    [Parameter(Mandatory=$true)][string]$PreflightPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
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
$root=[IO.Path]::GetFullPath($VmPath)
if (Test-Path -LiteralPath $root) { throw 'The task-owned virtual machine path already exists and will not be overwritten.' }
if (-not $PSCmdlet.ShouldProcess($VmName, 'Create a new task-owned Generation 2 Hyper-V virtual machine')) { return }
New-Item -ItemType Directory -Path $root | Out-Null
$vhd=Join-Path $root 'disk.vhdx'
Copy-Item -LiteralPath ([IO.Path]::GetFullPath($BaseImagePath)) -Destination $vhd
$createdVm=New-VM -Name $VmName -Generation 2 -VHDPath $vhd -Path $root -SwitchName $SwitchName
Set-VMProcessor -VMName $VmName -Count 4
Set-VMMemory -VMName $VmName -DynamicMemoryEnabled $true -StartupBytes 4GB
[ordered]@{version=1;vmName=$VmName;vmId=$createdVm.Id.Guid;vmPath=$root;vhdPath=$vhd;vhdSha256=(Get-FileHash -LiteralPath $vhd -Algorithm SHA256).Hash.ToLowerInvariant();generation=2;switch=[ordered]@{name=$switch.Name;id=$switch.Id};processorCount=4;startupBytes=4GB;createdAt=[DateTimeOffset]::UtcNow.ToString('o');preflightSha256=(Get-FileHash -LiteralPath $PreflightPath -Algorithm SHA256).Hash.ToLowerInvariant()} | ConvertTo-Json -Depth 5
