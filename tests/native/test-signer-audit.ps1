[CmdletBinding()]
param([string]$AuditPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Validate([object[]]$Records) {
  $started=@($Records|Where-Object kind -eq 'started'); $ready=@($Records|Where-Object kind -eq 'ready'); $beats=@($Records|Where-Object kind -eq 'heartbeat'); $events=@($Records|Where-Object kind -eq 'process-start'); $terminal=@($Records|Where-Object kind -eq 'terminal')
  if($started.Count -ne 1 -or $ready.Count -ne 1 -or $beats.Count -lt 1 -or $events.Count -lt 1 -or $terminal.Count -ne 1 -or -not $terminal[0].healthy){throw 'audit coverage facts are incomplete'}
  $previous=[DateTimeOffset]::MinValue; foreach($record in $Records){$at=[DateTimeOffset]::Parse($record.at); if($at -lt $previous -or ($previous -ne [DateTimeOffset]::MinValue -and ($at-$previous).TotalSeconds -gt 5)){throw 'audit timeline is invalid'};$previous=$at}
}
$now=[DateTimeOffset]::UtcNow
$valid=@([pscustomobject]@{kind='started';at=$now.ToString('o')},[pscustomobject]@{kind='ready';at=$now.AddMilliseconds(1).ToString('o')},[pscustomobject]@{kind='heartbeat';at=$now.AddMilliseconds(2).ToString('o')},[pscustomobject]@{kind='process-start';at=$now.AddMilliseconds(3).ToString('o')},[pscustomobject]@{kind='terminal';at=$now.AddMilliseconds(4).ToString('o');healthy=$true})
Validate $valid
foreach($fixture in @(@($valid|Where-Object kind -ne 'ready'),@($valid|Where-Object kind -ne 'terminal'),@($valid|Where-Object kind -ne 'process-start'),@($valid[0],$valid[1],$valid[2],[pscustomobject]@{kind='terminal';at=$now.AddSeconds(6).ToString('o');healthy=$true}))){$failed=$false;try{Validate $fixture}catch{$failed=$true};if(-not $failed){throw 'negative audit fixture unexpectedly passed'}}
if($AuditPath){$records=@(Get-Content -LiteralPath $AuditPath|ForEach-Object{$_|ConvertFrom-Json});Validate $records}
Write-Output 'Validated signer-audit positive and negative fixtures.'
