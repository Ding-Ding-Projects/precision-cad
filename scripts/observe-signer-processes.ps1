[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [Parameter(Mandatory=$true)][string]$ReadyPath,
    [Parameter(Mandatory=$true)][string]$StopPath,
    [Parameter(Mandatory=$true)][string]$SessionId
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$sequence = 0
function Write-Audit([string]$Kind, [hashtable]$Values = @{}) {
    $script:sequence++
    $entry = [ordered]@{ sequence=$script:sequence; at=[DateTimeOffset]::UtcNow.ToString('o'); kind=$Kind; sessionId=$SessionId; observerPid=$PID }
    foreach ($pair in $Values.GetEnumerator()) { $entry[$pair.Key] = $pair.Value }
    $entry | ConvertTo-Json -Compress | Add-Content -LiteralPath $LogPath -Encoding utf8
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null
Write-Audit 'started' @{ source='Win32_ProcessStartTrace'; coverage='all process-start events observed after ready and before terminal' }
$watcher = [System.Management.ManagementEventWatcher]::new('SELECT * FROM Win32_ProcessStartTrace')
$mode = 'Win32_ProcessStartTrace'
$watcherStarted = $false
$failure = $null
try {
    try { $watcher.Start(); $watcherStarted = $true } catch { $mode = 'Get-Process-polling'; Write-Audit 'observation-limit' @{ reason='Win32_ProcessStartTrace unavailable'; limitation='short-lived processes can escape sampling' } }
    Write-Audit 'ready' @{ watcherStarted=($mode -eq 'Win32_ProcessStartTrace'); mode=$mode }
    Set-Content -LiteralPath $ReadyPath -Value $SessionId -Encoding ascii -NoNewline
    $lastHeartbeat = [DateTimeOffset]::MinValue
    while (-not (Test-Path -LiteralPath $StopPath)) {
        if (([DateTimeOffset]::UtcNow - $lastHeartbeat).TotalMilliseconds -ge 250) {
            Write-Audit 'heartbeat' @{}
            $lastHeartbeat = [DateTimeOffset]::UtcNow
        }
        if ($mode -eq 'Win32_ProcessStartTrace') {
            try {
                $watcher.Options.Timeout = [TimeSpan]::FromMilliseconds(250)
                $event = $watcher.WaitForNextEvent()
                if ($null -ne $event) {
                    Write-Audit 'process-start' @{ processName=[string]$event['ProcessName']; processId=[uint32]$event['ProcessID']; parentProcessId=[uint32]$event['ParentProcessID'] }
                }
            } catch [System.Management.ManagementException] {
                if ($_.Exception.Message -notmatch 'timed out') { throw }
            }
        } else {
            $processes = @(Get-Process -ErrorAction Stop)
            Write-Audit 'sample' @{ processCount=$processes.Count }
            foreach ($process in $processes) { Write-Audit 'process-sample' @{ processName=$process.ProcessName; processId=$process.Id } }
            Start-Sleep -Milliseconds 250
        }
    }
} catch {
    $failure = $_
}
$cleanupFailure = $null
if ($null -ne $watcher) {
    if ($watcherStarted) {
        try { $watcher.Stop() } catch { $cleanupFailure = $_ }
    }
    try { $watcher.Dispose() } catch { if ($null -eq $cleanupFailure) { $cleanupFailure = $_ } }
}
if ($null -ne $failure -or $null -ne $cleanupFailure) {
    $errorType = if ($null -ne $failure) { $failure.Exception.GetType().FullName } else { $cleanupFailure.Exception.GetType().FullName }
    Write-Audit 'terminal' @{ healthy=$false; error=$errorType; mode=$mode; exhaustive=($mode -eq 'Win32_ProcessStartTrace') }
    if ($null -ne $failure) { throw $failure }
    throw $cleanupFailure
}
Write-Audit 'terminal' @{ healthy=$true; stopRequested=$true; mode=$mode; exhaustive=($mode -eq 'Win32_ProcessStartTrace') }
