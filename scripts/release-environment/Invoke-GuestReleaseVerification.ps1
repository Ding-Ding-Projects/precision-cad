[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$VmName,
    [Parameter(Mandatory=$true)][pscredential]$Credential,
    [Parameter(Mandatory=$true)][string]$GuestScriptPath,
    [Parameter(Mandatory=$true)][string]$GuestArtifactDirectory,
    [Parameter(Mandatory=$true)][string]$GuestReceiptPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not (Get-VM -Name $VmName -ErrorAction SilentlyContinue)) { throw 'The explicitly named task-owned virtual machine does not exist.' }
if (-not (Test-Path -LiteralPath $GuestScriptPath -PathType Leaf)) { throw 'The guest verification script is missing.' }
if (-not (Test-Path -LiteralPath $GuestArtifactDirectory -PathType Container)) { throw 'The guest artifact directory is missing.' }
# The credential object is supplied by the caller from an approved protected route. This script never guesses, stores, or logs credentials.
$session=New-PSSession -VMName $VmName -Credential $Credential
try {
    Copy-Item -LiteralPath $GuestScriptPath -Destination 'C:\ReleaseVerification\Invoke-GuestVerification.ps1' -ToSession $session -Force
    Copy-Item -LiteralPath $GuestArtifactDirectory -Destination 'C:\ReleaseVerification\artifacts' -ToSession $session -Recurse -Force
    Invoke-Command -Session $session -ScriptBlock { & 'C:\ReleaseVerification\Invoke-GuestVerification.ps1' -ArtifactDirectory 'C:\ReleaseVerification\artifacts' -ReceiptPath 'C:\ReleaseVerification\runtime-receipt.json' }
    Copy-Item -FromSession $session -LiteralPath 'C:\ReleaseVerification\runtime-receipt.json' -Destination $GuestReceiptPath -Force
} finally { if ($session) { Remove-PSSession $session } }
