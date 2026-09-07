Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ($null -eq ('System.Security.Cryptography.ProtectedData' -as [type])) { try { Add-Type -AssemblyName System.Security.Cryptography.ProtectedData -ErrorAction Stop } catch { Add-Type -AssemblyName System.Security -ErrorAction Stop } }
if ($null -eq ('System.Security.Cryptography.ProtectedData' -as [type])) { throw 'CurrentUser DPAPI is unavailable; owner registry creation is refused.' }

function Get-ReleaseEnvironmentRegistryRoot([string]$RegistryRoot) {
    if ([string]::IsNullOrWhiteSpace($RegistryRoot)) { return (Join-Path $env:LOCALAPPDATA 'PrecisionCAD\release-environment\owned-vms') }
    return [IO.Path]::GetFullPath($RegistryRoot)
}
function Initialize-ReleaseEnvironmentRegistry([string]$RegistryRoot) {
    $root=Get-ReleaseEnvironmentRegistryRoot $RegistryRoot
    if (Test-Path -LiteralPath $root) { Assert-ReleaseEnvironmentRegistryAcl $root; return $root }
    New-Item -ItemType Directory -Path $root -ErrorAction Stop | Out-Null
    $current=[Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl=Get-Acl -LiteralPath $root
    $acl.SetAccessRuleProtection($true,$false)
    foreach($rule in @($acl.Access)){[void]$acl.RemoveAccessRule($rule)}
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($current,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))
    Set-Acl -LiteralPath $root -AclObject $acl
    Assert-ReleaseEnvironmentRegistryAcl $root
    return $root
}
function Assert-ReleaseEnvironmentRegistryAcl([string]$RegistryRoot) {
    $root=Get-ReleaseEnvironmentRegistryRoot $RegistryRoot
    $current=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $acl=Get-Acl -LiteralPath $root
    $ownerSid=if ([string]$acl.Owner -match '^S-') { [string]$acl.Owner } else { ([Security.Principal.NTAccount]::new([string]$acl.Owner)).Translate([Security.Principal.SecurityIdentifier]).Value }
    if ($acl.AreAccessRulesProtected -ne $true -or $ownerSid -ne $current) { throw 'Release environment registry ACL is not owner-only.' }
    $allowed=@($acl.Access | Where-Object { $_.AccessControlType -eq 'Allow' })
    $ruleSid=if ([string]$allowed[0].IdentityReference -match '^S-') { [string]$allowed[0].IdentityReference } else { ([Security.Principal.NTAccount]::new([string]$allowed[0].IdentityReference)).Translate([Security.Principal.SecurityIdentifier]).Value }
    if ($allowed.Count -ne 1 -or $ruleSid -ne $current -or ($allowed[0].FileSystemRights -band [Security.AccessControl.FileSystemRights]::FullControl) -ne [Security.AccessControl.FileSystemRights]::FullControl) { throw 'Release environment registry ACL grants access beyond the current owner.' }
}
function Protect-ReleaseEnvironmentRecord([hashtable]$Record,[string]$Path) {
    $plain=[Text.Encoding]::UTF8.GetBytes(($Record|ConvertTo-Json -Depth 16 -Compress))
    $cipher=[Security.Cryptography.ProtectedData]::Protect($plain,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    [IO.File]::WriteAllBytes($Path,$cipher)
}
function Write-ReleaseEnvironmentRecord([hashtable]$Record,[string]$RegistryRoot) {
    $root=Initialize-ReleaseEnvironmentRegistry $RegistryRoot
    if ($Record.operationId -notmatch '^[0-9a-f]{32}$') { throw 'Registry record needs a generated operation identifier.' }
    $target=Join-Path $root ($Record.operationId+'.dpapi')
    $stage=$target+'.staging-'+[Guid]::NewGuid().ToString('N')
    $Record.updatedAt=[DateTimeOffset]::UtcNow.ToString('o')
    Protect-ReleaseEnvironmentRecord $Record $stage
    Move-Item -LiteralPath $stage -Destination $target -Force
    return $target
}
function Read-ReleaseEnvironmentRecord([string]$VmId,[string]$RegistryRoot) {
    $root=Get-ReleaseEnvironmentRegistryRoot $RegistryRoot
    Assert-ReleaseEnvironmentRegistryAcl $root
    $matches=@(Get-ChildItem -LiteralPath $root -Filter '*.dpapi' -File | Where-Object {
        try { $bytes=[IO.File]::ReadAllBytes($_.FullName); $plain=[Security.Cryptography.ProtectedData]::Unprotect($bytes,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser); ([Text.Encoding]::UTF8.GetString($plain)|ConvertFrom-Json).vmId -eq $VmId } catch { $false }
    })
    if($matches.Count -ne 1){throw 'No unique owner-created provisioning record exists for this VM ID.'}
    $bytes=[IO.File]::ReadAllBytes($matches[0].FullName);$plain=[Security.Cryptography.ProtectedData]::Unprotect($bytes,$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    return ([Text.Encoding]::UTF8.GetString($plain)|ConvertFrom-Json)
}
Export-ModuleMember -Function Get-ReleaseEnvironmentRegistryRoot,Initialize-ReleaseEnvironmentRegistry,Assert-ReleaseEnvironmentRegistryAcl,Write-ReleaseEnvironmentRecord,Read-ReleaseEnvironmentRecord
