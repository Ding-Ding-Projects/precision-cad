[CmdletBinding()]
param([string]$AuditPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Validate([object[]]$Records) {
  $started=@($Records|Where-Object kind -eq 'started'); $ready=@($Records|Where-Object kind -eq 'ready'); $beats=@($Records|Where-Object kind -eq 'heartbeat'); $events=@($Records|Where-Object kind -eq 'process-start'); $samples=@($Records|Where-Object kind -eq 'sample'); $terminal=@($Records|Where-Object kind -eq 'terminal')
  if($started.Count -ne 1 -or $ready.Count -ne 1 -or $beats.Count -lt 1 -or (($events.Count + $samples.Count) -lt 1) -or $terminal.Count -ne 1 -or -not $terminal[0].healthy){throw 'audit coverage facts are incomplete'}
  $previous=[DateTimeOffset]::MinValue; foreach($record in $Records){$at=[DateTimeOffset]::Parse($record.at); if($at -lt $previous -or ($previous -ne [DateTimeOffset]::MinValue -and ($at-$previous).TotalSeconds -gt 5)){throw 'audit timeline is invalid'};$previous=$at}
}
$now=[DateTimeOffset]::UtcNow
$valid=@([pscustomobject]@{kind='started';at=$now.ToString('o')},[pscustomobject]@{kind='ready';at=$now.AddMilliseconds(1).ToString('o')},[pscustomobject]@{kind='heartbeat';at=$now.AddMilliseconds(2).ToString('o')},[pscustomobject]@{kind='process-start';at=$now.AddMilliseconds(3).ToString('o')},[pscustomobject]@{kind='terminal';at=$now.AddMilliseconds(4).ToString('o');healthy=$true})
Validate $valid
foreach($fixture in @(@($valid|Where-Object kind -ne 'ready'),@($valid|Where-Object kind -ne 'terminal'),@($valid|Where-Object kind -ne 'process-start'),@($valid[0],$valid[1],$valid[2],[pscustomobject]@{kind='terminal';at=$now.AddSeconds(6).ToString('o');healthy=$true}))){$failed=$false;try{Validate $fixture}catch{$failed=$true};if(-not $failed){throw 'negative audit fixture unexpectedly passed'}}
if($AuditPath){$records=@(Get-Content -LiteralPath $AuditPath|ForEach-Object{$_|ConvertFrom-Json});Validate $records}
$root=Split-Path -Parent (Split-Path $PSScriptRoot)
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('precision-cad-signer-observer-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
  $log=Join-Path $temporary 'audit.jsonl'; $ready=Join-Path $temporary 'ready'; $stop=Join-Path $temporary 'stop'; $session=[Guid]::NewGuid().ToString('N')
  $windowsPowerShell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
  $observer=Join-Path $root 'scripts\observe-signer-processes.ps1'
  $arguments=('-NoProfile -ExecutionPolicy Bypass -File "{0}" -LogPath "{1}" -ReadyPath "{2}" -StopPath "{3}" -SessionId "{4}"' -f $observer,$log,$ready,$stop,$session)
  $stderr=Join-Path $temporary 'observer.stderr.log'
  $startInfo=[Diagnostics.ProcessStartInfo]::new(); $startInfo.FileName=$windowsPowerShell; $startInfo.Arguments=$arguments; $startInfo.UseShellExecute=$false; $startInfo.CreateNoWindow=$true; $startInfo.RedirectStandardError=$true
  $process=[Diagnostics.Process]::new(); $process.StartInfo=$startInfo; if(-not $process.Start()){throw 'real observer could not start'}
  $deadline=[DateTimeOffset]::UtcNow.AddSeconds(10)
  while(-not (Test-Path -LiteralPath $ready) -and [DateTimeOffset]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 50}
  if(-not (Test-Path -LiteralPath $ready)){throw 'real observer did not become ready'}
  Set-Content -LiteralPath $stop -Value $session -Encoding ascii -NoNewline
  if(-not $process.WaitForExit(10000)){throw 'real observer did not stop'}
  $process.Refresh()
  Set-Content -LiteralPath $stderr -Value $process.StandardError.ReadToEnd() -Encoding utf8
  if($process.ExitCode -ne 0){throw "real observer exit code was $($process.ExitCode)"}
  $records=@(Get-Content -LiteralPath $log|ForEach-Object{$_|ConvertFrom-Json}); Validate $records
  $terminal=@($records|Where-Object kind -eq 'terminal')[0]
  if($terminal.mode -ne 'Get-Process-polling' -or $terminal.exhaustive){throw 'real observer did not record the expected non-exhaustive polling fallback'}
} finally { Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue }
Write-Output 'Validated signer-audit positive and negative fixtures.'
