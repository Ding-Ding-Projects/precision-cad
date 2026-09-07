param([switch]$Silent)
$ErrorActionPreference = 'Stop'
$commit = '27b6a080c8b669421bd4d444650c3b8eddec5687'
$root = Join-Path $env:LOCALAPPDATA 'material-virtualbox-toolchain'
$destination = Join-Path $root 'solvespace-v3.2'
New-Item -ItemType Directory -Force -Path $root | Out-Null
if (-not (Test-Path (Join-Path $destination '.git'))) {
    git clone --filter=blob:none --no-checkout https://github.com/solvespace/solvespace.git $destination
    if ($LASTEXITCODE -ne 0) { throw 'SolveSpace clone failed.' }
} else {
    # A newly acquired --no-checkout clone has no populated index yet. Only an
    # existing checkout can hold user changes that must be preserved here.
    $existingStatus = @(git -C $destination status --porcelain --untracked-files=all --ignore-submodules=none)
    if ($LASTEXITCODE -ne 0 -or $existingStatus.Count -ne 0) { throw 'SolveSpace source or submodules contain local changes; preserving them and refusing bootstrap.' }
}
git -C $destination fetch --depth 1 origin $commit; if ($LASTEXITCODE -ne 0) { throw 'SolveSpace source fetch failed.' }
git -C $destination checkout --detach $commit; if ($LASTEXITCODE -ne 0) { throw 'SolveSpace source checkout failed.' }
git -C $destination submodule update --init --depth 1 extlib/eigen extlib/mimalloc; if ($LASTEXITCODE -ne 0) { throw 'SolveSpace solver submodule acquisition failed.' }
$actual = (git -C $destination rev-parse HEAD).Trim()
if ($actual -ne $commit) { throw "SolveSpace commit mismatch: expected $commit, got $actual" }
$status = @(git -C $destination status --porcelain --untracked-files=all --ignore-submodules=none)
if ($LASTEXITCODE -ne 0 -or $status.Count -ne 0) { throw 'SolveSpace source or submodules are not clean.' }
$submodules = @(git -C $destination submodule status --recursive extlib/eigen extlib/mimalloc)
if ($LASTEXITCODE -ne 0 -or ($submodules | Where-Object { $_ -match '^[+U-]' })) { throw 'SolveSpace solver submodules differ from their pinned commits.' }
$header = Join-Path $destination 'include/slvs.h'
if (-not (Test-Path $header)) { throw 'SolveSpace libslvs header is absent.' }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $header).Hash.ToLowerInvariant()
if ($hash -ne 'b342bfeab4bd64b747ef3722f3218b64927939eaabfc34afed53819bd248552e') { throw "SolveSpace 3.2 libslvs header hash mismatch: $hash" }
if (-not $Silent) { Write-Output "SolveSpace 3.2 libslvs is ready at $destination ($actual)." }
