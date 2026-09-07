[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$VmName,
    [Parameter(Mandatory=$true)][pscredential]$Credential,
    [Parameter(Mandatory=$true)][string]$ProvisioningReceiptPath,
    [Parameter(Mandatory=$true)][string]$GuestScriptPath,
    [Parameter(Mandatory=$true)][string]$GuestArtifactDirectory,
    [Parameter(Mandatory=$true)][string]$GuestReceiptPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not (Test-Path -LiteralPath $ProvisioningReceiptPath -PathType Leaf)) { throw 'The provisioning receipt is missing.' }
$provisioning=Get-Content -Raw -LiteralPath $ProvisioningReceiptPath | ConvertFrom-Json
if ($provisioning.version -ne 1 -or $provisioning.vmName -ne $VmName -or $provisioning.vmId -notmatch '^[0-9a-f-]{36}$' -or $provisioning.generation -ne 2 -or $provisioning.baseImage.sha256 -notmatch '^[0-9a-f]{64}$' -or [string]::IsNullOrWhiteSpace($provisioning.switch.id)) { throw 'The provisioning receipt does not bind this task-owned Generation 2 guest.' }
$vm=Get-VM -Name $VmName -ErrorAction Stop
if ($vm.Id.Guid -ne $provisioning.vmId -or $vm.Generation -ne 2 -or $vm.State -ne 'Running' -or $vm.Path -ne $provisioning.vmPath) { throw 'The task-owned virtual machine does not match its provisioning receipt.' }
if (-not (Test-Path -LiteralPath $provisioning.vhdPath -PathType Leaf)) { throw 'The guest disk path recorded by the provisioning receipt is absent.' }
$adapter=Get-VMNetworkAdapter -VMName $VmName -ErrorAction Stop | Select-Object -First 1
$processor=Get-VMProcessor -VMName $VmName -ErrorAction Stop
if ($adapter.SwitchId -ne $provisioning.switch.id -or $processor.Count -ne $provisioning.processorCount) { throw 'The guest network or processor configuration does not match its provisioning receipt.' }
if (-not (Test-Path -LiteralPath $GuestScriptPath -PathType Leaf)) { throw 'The guest verification script is missing.' }
if (-not (Test-Path -LiteralPath $GuestArtifactDirectory -PathType Container)) { throw 'The guest artifact directory is missing.' }
# The credential object is supplied by the caller from an approved protected route. This script never guesses, stores, or logs credentials.
$setup=Get-ChildItem -LiteralPath $GuestArtifactDirectory -Filter Setup.exe -File | Select-Object -First 1
if ($null -eq $setup) { throw 'The staged artifact directory lacks Setup.exe.' }
$setupHash=(Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$session=New-PSSession -VMName $VmName -Credential $Credential
try {
    $guestRoot='C:\ReleaseVerification\'+[Guid]::NewGuid().ToString('N')
    Invoke-Command -Session $session -ScriptBlock { param($Path) New-Item -ItemType Directory -Path $Path -ErrorAction Stop | Out-Null } -ArgumentList $guestRoot
    Copy-Item -LiteralPath $GuestScriptPath -Destination ($guestRoot+'\Invoke-GuestVerification.ps1') -ToSession $session -ErrorAction Stop
    Copy-Item -LiteralPath $GuestArtifactDirectory -Destination ($guestRoot+'\artifacts') -ToSession $session -Recurse -ErrorAction Stop
    Invoke-Command -Session $session -ScriptBlock { param($Root,$Hash) & ($Root+'\Invoke-GuestVerification.ps1') -ArtifactDirectory ($Root+'\artifacts') -ReceiptPath ($Root+'\runtime-receipt.json') -ExpectedSetupSha256 $Hash } -ArgumentList $guestRoot,$setupHash
    Copy-Item -FromSession $session -LiteralPath ($guestRoot+'\runtime-receipt.json') -Destination $GuestReceiptPath -ErrorAction Stop
    $receipt=Get-Content -Raw -LiteralPath $GuestReceiptPath | ConvertFrom-Json
    if ($receipt.verdict -ne 'complete') { throw 'Guest receipt is blocked; installer, launch, and updater verification remain incomplete.' }
} finally { if ($session) { Remove-PSSession $session } }
