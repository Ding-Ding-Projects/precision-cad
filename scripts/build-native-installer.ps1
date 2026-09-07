[CmdletBinding()]
param(
    [switch]$Silent,
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'manifests/native-dependencies.json') | ConvertFrom-Json
$head = (git -C $root rev-parse --verify HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve the installer candidate commit.' }
if (git -C $root status --porcelain) { throw 'Installer packaging requires an unchanged committed source tree.' }
if (-not $Version) { $Version = (Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern '^project\(PrecisionCAD VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1).Matches[0].Groups[1].Value }
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Installer version '$Version' must be numeric semantic version text." }
$buildStartedAt = [DateTimeOffset]::UtcNow.ToString('o')
$candidateShort = $head.Substring(0, 12)
$output = Join-Path $root ("artifacts/native/squirrel-windows/" + $candidateShort)
$buildLog = Join-Path $output 'installer-build.log'
$setupIcon = Join-Path $root 'assets/precision-cad.ico'
$iconBytes = [IO.File]::ReadAllBytes($setupIcon)
if ($iconBytes.Length -lt 6 -or [BitConverter]::ToUInt16($iconBytes, 0) -ne 0 -or [BitConverter]::ToUInt16($iconBytes, 2) -ne 1) { throw 'Squirrel setup icon must be a valid Precision CAD ICO source.' }
$signerNames = @('signtool.exe','azuresigntool.exe')
$observedSignerInvocations = [Collections.Generic.List[object]]::new()
function Start-SignerAudit {
    $auditPath = Join-Path $output 'signer-process-audit.jsonl'
    $readyPath = Join-Path $output 'signer-process-audit.ready'
    $stopPath = Join-Path $output 'signer-process-audit.stop'
    $sessionId = [Guid]::NewGuid().ToString('N')
    $observer = Join-Path $PSScriptRoot 'observe-signer-processes.ps1'
    $process = Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$observer,'-LogPath',$auditPath,'-ReadyPath',$readyPath,'-StopPath',$stopPath,'-SessionId',$sessionId) -PassThru -WindowStyle Hidden
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds(10)
    while ([DateTimeOffset]::UtcNow -lt $deadline -and -not (Test-Path -LiteralPath $readyPath)) {
        if ($process.HasExited) { throw 'Signer-process observer exited before reporting readiness.' }
        Start-Sleep -Milliseconds 50
    }
    if (-not (Test-Path -LiteralPath $readyPath) -or (Get-Content -LiteralPath $readyPath -Raw) -ne $sessionId) { throw 'Signer-process observer did not complete its readiness handshake.' }
    return [ordered]@{ process=$process; path=$auditPath; readyPath=$readyPath; stopPath=$stopPath; sessionId=$sessionId; startedAt=[DateTimeOffset]::UtcNow.ToString('o') }
}
function Stop-SignerAudit($audit) {
    Set-Content -LiteralPath $audit.stopPath -Value $audit.sessionId -Encoding ascii -NoNewline
    if (-not $audit.process.WaitForExit(10000)) { throw 'Signer-process observer did not terminate after the package command ended.' }
    if ($audit.process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $audit.path)) { throw 'Signer-process observer did not produce a healthy audit log.' }
    $records = @(Get-Content -LiteralPath $audit.path | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json })
    $started = @($records | Where-Object kind -eq 'started'); $ready = @($records | Where-Object kind -eq 'ready'); $heartbeats = @($records | Where-Object kind -eq 'heartbeat'); $events = @($records | Where-Object kind -eq 'process-start'); $terminal = @($records | Where-Object kind -eq 'terminal')
    if ($started.Count -ne 1 -or $ready.Count -ne 1 -or $heartbeats.Count -lt 1 -or $events.Count -lt 1 -or $terminal.Count -ne 1 -or -not $terminal[0].healthy) { throw 'Signer-process audit coverage is incomplete.' }
    $previous = [DateTimeOffset]::MinValue
    foreach ($record in $records) { $at=[DateTimeOffset]::Parse([string]$record.at); if ($at -lt $previous -or ($previous -ne [DateTimeOffset]::MinValue -and ($at-$previous).TotalSeconds -gt 5)) { throw 'Signer-process audit contains a timing gap.' }; $previous=$at }
    foreach ($record in $events | Where-Object { $signerNames -contains ([string]$_.processName).ToLowerInvariant() }) { $observedSignerInvocations.Add($record) }
    if ($observedSignerInvocations.Count -ne 0) { throw 'A signer process was observed during unsigned Squirrel packaging.' }
    Set-ItemProperty -LiteralPath $audit.path -Name IsReadOnly -Value $true
    return [ordered]@{ path='signer-process-audit.jsonl'; sha256=(Get-FileHash -LiteralPath $audit.path -Algorithm SHA256).Hash.ToLowerInvariant(); coverage=[ordered]@{ source='Win32_ProcessStartTrace'; startedAt=$started[0].at; readyAt=$ready[0].at; endedAt=$terminal[0].at; heartbeatCount=$heartbeats.Count; processStartEventCount=$events.Count; terminalHealthy=$true; gapsOverFiveSeconds=0 }; observerPid=$audit.process.Id }
}
function Get-VerifiedFile([string]$Uri, [string]$Path, [string]$ExpectedHash) {
    if (-not (Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) {
        $stage = "$Path.download-$([Guid]::NewGuid().ToString('N'))"
        Invoke-WebRequest -Uri $Uri -OutFile $stage
        if ((Get-FileHash -LiteralPath $stage -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) { Remove-Item -LiteralPath $stage -Force -ErrorAction SilentlyContinue; throw "SHA-256 mismatch for $Uri" }
        Move-Item -LiteralPath $stage -Destination $Path -Force
    }
    return $Path
}

$toolchain = Join-Path $env:LOCALAPPDATA 'precision-cad-toolchain'
$squirrelRoot = Join-Path $toolchain ("squirrel-" + $manifest.squirrel.version)
New-Item -ItemType Directory -Force -Path $squirrelRoot | Out-Null
$squirrelPackage = Get-VerifiedFile $manifest.squirrel.archiveUrl (Join-Path $squirrelRoot $manifest.squirrel.archiveName) $manifest.squirrel.sha256
$squirrelExtract = Join-Path $squirrelRoot 'package'
$squirrel = Join-Path $squirrelExtract $manifest.squirrel.toolPath
if (-not (Test-Path -LiteralPath $squirrel)) { Expand-Archive -LiteralPath $squirrelPackage -DestinationPath $squirrelExtract -Force }
if (-not (Test-Path -LiteralPath $squirrel)) { throw 'Pinned Squirrel.Windows package does not contain Squirrel.exe.' }
$nugetRoot = Join-Path $toolchain ("nuget-" + $manifest.nuget.version)
New-Item -ItemType Directory -Force -Path $nugetRoot | Out-Null
$nuget = Get-VerifiedFile $manifest.nuget.url (Join-Path $nugetRoot 'nuget.exe') $manifest.nuget.sha256

$artifactsRoot = [IO.Path]::GetFullPath((Join-Path $root 'artifacts/native/squirrel-windows')) + [IO.Path]::DirectorySeparatorChar
if (-not ([IO.Path]::GetFullPath($output) + [IO.Path]::DirectorySeparatorChar).StartsWith($artifactsRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer output must remain inside the task-owned artifacts directory.' }
if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Item -ItemType Directory -Force -Path $output | Out-Null
$transcribing = $false
try {
    Start-Transcript -LiteralPath $buildLog -Force | Out-Null; $transcribing = $true
    $env:SQUIRREL_ENABLE_SIGNING = $null
    $env:SQUIRREL_SIGNTOOL = $null
    $env:SQUIRREL_CERTIFICATE_PATH = $null
    & (Join-Path $PSScriptRoot 'build-native.ps1')
    if ($LASTEXITCODE -ne 0) { throw "Native build failed with exit code $LASTEXITCODE." }
    & (Join-Path $PSScriptRoot 'stage-native-runtime.ps1')
    if ($LASTEXITCODE -ne 0) { throw "Native runtime staging failed with exit code $LASTEXITCODE." }

$staged = Join-Path $root 'build/native/bin'
$packageRoot = Join-Path $root ("build/native/squirrel-package/" + $candidateShort)
$packageRootGuard = [IO.Path]::GetFullPath((Join-Path $root 'build/native/squirrel-package')) + [IO.Path]::DirectorySeparatorChar
if (-not ([IO.Path]::GetFullPath($packageRoot) + [IO.Path]::DirectorySeparatorChar).StartsWith($packageRootGuard, [StringComparison]::OrdinalIgnoreCase)) { throw 'Squirrel package workspace must remain inside the task-owned build directory.' }
if (Test-Path -LiteralPath $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
$packageLib = Join-Path $packageRoot 'lib/net45'
New-Item -ItemType Directory -Force -Path $packageLib | Out-Null
$stagedItems = @(Get-ChildItem -LiteralPath $staged -Force)
if ($stagedItems.Count -eq 0) { throw 'Native runtime staging produced no packageable files.' }
foreach ($item in $stagedItems) { Copy-Item -LiteralPath $item.FullName -Destination $packageLib -Recurse -Force }
$nuspec = Join-Path $packageRoot 'PrecisionCAD.nuspec'
@"
<?xml version="1.0"?>
<package>
  <metadata>
    <id>PrecisionCAD</id>
    <version>$Version</version>
    <title>Precision CAD</title>
    <authors>Precision CAD</authors>
    <owners>Precision CAD</owners>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>Precision CAD native development release.</description>
  </metadata>
</package>
"@ | Set-Content -LiteralPath $nuspec -Encoding utf8
& $nuget pack $nuspec -BasePath $packageRoot -OutputDirectory $packageRoot -NoPackageAnalysis -NonInteractive
if ($LASTEXITCODE -ne 0) { throw 'NuGet package construction failed.' }
$nupkg = Join-Path $packageRoot ("PrecisionCAD.$Version.nupkg")
if (-not (Test-Path -LiteralPath $nupkg)) { throw 'NuGet package construction did not produce the expected input package.' }
$signerAudit = Start-SignerAudit
Push-Location $output
try { & $squirrel ("--releasify=" + $nupkg) --releaseDir $output --no-msi --setupIcon $setupIcon } finally { Pop-Location; $signerAuditEvidence = Stop-SignerAudit $signerAudit }
if ($LASTEXITCODE -ne 0) { throw "Squirrel.Windows releasify failed with exit code $LASTEXITCODE." }
$setup = @(Get-ChildItem -LiteralPath $output -File -Filter 'Setup.exe')
$releases = @(Get-ChildItem -LiteralPath $output -File -Filter 'RELEASES')
$full = @(Get-ChildItem -LiteralPath $output -File -Filter '*-full.nupkg')
if ($setup.Count -ne 1 -or $releases.Count -ne 1 -or $full.Count -lt 1) { throw 'Squirrel.Windows did not produce exactly one Setup.exe, one RELEASES file, and at least one full nupkg.' }
if ((Get-AuthenticodeSignature -LiteralPath $setup[0].FullName).Status -ne 'NotSigned') { throw 'Setup.exe must be unsigned for this development release.' }
$releaseText = Get-Content -Raw -LiteralPath $releases[0].FullName
foreach ($package in $full) { if ($releaseText -notmatch [regex]::Escape($package.Name)) { throw "RELEASES does not reference $($package.Name)." } }
    if ($transcribing) { Stop-Transcript | Out-Null; $transcribing = $false }
    $runtimeReceiptPath = Join-Path $root 'build/native/runtime-receipt.json'
    if (-not (Test-Path -LiteralPath $runtimeReceiptPath)) { throw 'Native runtime receipt is missing.' }
    $runtimeReceipt = Get-Content -LiteralPath $runtimeReceiptPath -Raw | ConvertFrom-Json
    if ($runtimeReceipt.sourceCommit -ne $head -or $runtimeReceipt.packageVersion -ne $Version -or $runtimeReceipt.architecture -ne 'x64') { throw 'Native runtime receipt does not bind the staged runtime to this package candidate.' }
    $provenance = [ordered]@{ version=1; sourceCommit=$head; builtAt=[DateTimeOffset]::UtcNow.ToString('o'); buildStartedAt=$buildStartedAt; buildEndedAt=[DateTimeOffset]::UtcNow.ToString('o'); packagingCommand='build-installer.bat /s'; cleanSource=$true; cleanOutput=$true; package=[ordered]@{ id='PrecisionCAD'; version=$Version; architecture='x64' }; buildLog=[ordered]@{ path='installer-build.log'; sha256=(Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash.ToLowerInvariant() }; runtimePayload=$runtimeReceipt.payload; signing=[ordered]@{ mode='unsigned'; inputsCleared=$true; certificateAutoDiscoveryDisabled=$true; processAuditComplete=$true; signerInvocationCount=$observedSignerInvocations.Count; observedSignerInvocations=@($observedSignerInvocations); controls=[ordered]@{ signWithParamsPresent=$false; invocation=@('--releasify=<candidate>','--releaseDir','<output>','--no-msi','--setupIcon','<project-icon>'); knownToolBehavior='Squirrel.Windows signing requires explicit --signWithParams' }; processAudit=$signerAuditEvidence } }
    $provenance | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'squirrel-provenance.json') -Encoding utf8
    $receipt = [ordered]@{ version=1; sourceCommit=$head; packageVersion=$Version; architecture='x64'; provenance='squirrel-provenance.json'; artifacts=[ordered]@{ setup=[ordered]@{name=$setup[0].Name;bytes=$setup[0].Length;sha256=(Get-FileHash $setup[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()}; releases=[ordered]@{name=$releases[0].Name;bytes=$releases[0].Length;sha256=(Get-FileHash $releases[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()}; fullPackages=@($full | ForEach-Object { [ordered]@{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} }) } }
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'squirrel-artifact-receipt.json') -Encoding utf8
Get-ChildItem -LiteralPath $output -File | Sort-Object Name | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name } | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
Write-Output "Unsigned Squirrel.Windows installer artifacts: $output"
} finally {
    if ($transcribing) { Stop-Transcript | Out-Null }
}
