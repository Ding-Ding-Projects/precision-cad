param([Parameter(Mandatory = $true)][string]$Verifier, [Parameter(Mandatory = $true)][string]$FixtureParent)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path $FixtureParent ('provenance-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$fixture = (Resolve-Path -LiteralPath $fixture).Path
$Verifier = (Resolve-Path -LiteralPath $Verifier).Path
$source = Join-Path $fixture 'source'
$child = Join-Path $fixture 'child'
$script:checks = 0
function Invoke-Git([string]$Directory, [string[]]$GitArgs) {
    $output = & git -C $Directory @GitArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Fixture Git command failed: $($GitArgs[0]): $output" }
    return $output
}
function New-Source([string]$Path) {
    New-Item -ItemType Directory -Path $Path | Out-Null
    Invoke-Git $Path @('init', '--quiet') | Out-Null
    Invoke-Git $Path @('config', 'user.name', 'Claude Fable 5.1') | Out-Null
    Invoke-Git $Path @('config', 'user.email', 'noreply@anthropic.com') | Out-Null
    Invoke-Git $Path @('config', 'commit.gpgsign', 'false') | Out-Null
    Set-Content -LiteralPath (Join-Path $Path 'source.cpp') -Value 'int fixture = 1;'
    Invoke-Git $Path @('add', 'source.cpp') | Out-Null
    Invoke-Git $Path @('commit', '--quiet', '-m', "Create provenance fixture`n`n建立來源驗證樣本，假資料都要有真履歷。`n`nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>") | Out-Null
}
New-Source $child
New-Source $source
foreach ($name in @('eigen', 'mimalloc')) {
    Invoke-Git $source @('-c', 'protocol.file.allow=always', 'submodule', 'add', '--quiet', $child, "extlib/$name") | Out-Null
}
Invoke-Git $source @('commit', '--quiet', '-am', "Pin provenance fixture sources`n`n鎖定驗證樣本來源，避免版本玩捉迷藏。`n`nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>") | Out-Null
$expected = (Invoke-Git $source @('rev-parse', 'HEAD')).Trim()
function Assert-Verifier([bool]$Pass, [string]$Label, [string]$Commit = $expected) {
    $output = & cmake "-DPRECISION_SOLVESPACE_SOURCE=$source" "-DPRECISION_SOLVESPACE_COMMIT=$Commit" -P $Verifier 2>&1
    $passed = $LASTEXITCODE -eq 0
    if ($passed -ne $Pass) { throw "Unexpected provenance result for ${Label}: $output" }
    $script:checks++
}
Assert-Verifier $true 'symbolic HEAD with exact commit'
$tracked = Join-Path $source 'source.cpp'
$original = [IO.File]::ReadAllBytes($tracked)
Add-Content -LiteralPath $tracked -Value 'int altered = 1;'
Assert-Verifier $false 'tracked solver mutation'
[IO.File]::WriteAllBytes($tracked, $original)
Assert-Verifier $true 'restored tracked solver'
Invoke-Git $source @('checkout', '--detach', '--quiet', $expected) | Out-Null
Assert-Verifier $true 'detached exact HEAD'
$extra = Join-Path $source 'unexpected.hpp'
Set-Content -LiteralPath $extra -Value 'untracked source'
Assert-Verifier $false 'untracked source'
# This exact file belongs to this disposable fixture, never to a user checkout.
Remove-Item -LiteralPath $extra
Assert-Verifier $true 'restored untracked source'
$moduleFile = Join-Path $source 'extlib/eigen/source.cpp'
$originalModule = [IO.File]::ReadAllBytes($moduleFile)
Add-Content -LiteralPath $moduleFile -Value 'int altered = 1;'
Assert-Verifier $false 'dirty compiled submodule'
[IO.File]::WriteAllBytes($moduleFile, $originalModule)
Assert-Verifier $true 'restored compiled submodule'
Assert-Verifier $false 'wrong expected revision' ('0' * 40)
Assert-Verifier $true 'restored expected revision'
# A submodule checkout uses a .git file, exercising Git-aware provenance lookup.
$outerSource = $source
$source = Join-Path $outerSource 'extlib/eigen'
$childCommit = (Invoke-Git $source @('rev-parse', 'HEAD')).Trim()
# The generic source check must first pass the .git-file revision and clean checks;
# required module paths remain absent and therefore the full check must fail closed.
Assert-Verifier $false 'missing required solver submodules' $childCommit
$source = $outerSource
Invoke-Git $source @('submodule', 'deinit', '-f', 'extlib/eigen') | Out-Null
Assert-Verifier $false 'uninitialized solver submodule'
Invoke-Git $source @('-c', 'protocol.file.allow=always', 'submodule', 'update', '--init', 'extlib/eigen') | Out-Null
Assert-Verifier $true 'restored solver submodule'
Write-Output "PASS: $script:checks provenance assertions; fixtures retained under $fixture"
