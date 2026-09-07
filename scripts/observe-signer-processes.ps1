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
try {
    $watcher.Start()
    Write-Audit 'ready' @{ watcherStarted=$true }
    Set-Content -LiteralPath $ReadyPath -Value $SessionId -Encoding ascii -NoNewline
    $lastHeartbeat = [DateTimeOffset]::MinValue
    while (-not (Test-Path -LiteralPath $StopPath)) {
        if (([DateTimeOffset]::UtcNow - $lastHeartbeat).TotalMilliseconds -ge 250) {
            Write-Audit 'heartbeat' @{}
            $lastHeartbeat = [DateTimeOffset]::UtcNow
        }
        try {
            $event = $watcher.WaitForNextEvent(250)
            if ($null -ne $event) {
                Write-Audit 'process-start' @{ processName=[string]$event['ProcessName']; processId=[uint32]$event['ProcessID']; parentProcessId=[uint32]$event['ParentProcessID'] }
            }
        } catch [System.Management.ManagementException] {
            if ($_.Exception.Message -notmatch 'timed out') { throw }
        }
    }
    Write-Audit 'terminal' @{ healthy=$true; stopRequested=$true }
} catch {
    Write-Audit 'terminal' @{ healthy=$false; error=$_.Exception.GetType().FullName }
    throw
} finally {
    if ($null -ne $watcher) { $watcher.Stop(); $watcher.Dispose() }
}
