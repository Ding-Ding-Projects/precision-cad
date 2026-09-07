param([string]$BinaryDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $root 'build/native'
if (-not $BinaryDirectory) { $BinaryDirectory = Join-Path $buildRoot 'bin' }
$destination = [IO.Path]::GetFullPath($BinaryDirectory)
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $root 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $destination.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Runtime destination must be inside this project build directory.' }
$cacheLines = Get-Content (Join-Path $buildRoot 'CMakeCache.txt')
function Read-CMakeValue([string]$key) {
    $line = $cacheLines | Where-Object { $_ -match ('^' + [regex]::Escape($key) + ':[^=]+=') } | Select-Object -First 1
    if (-not $line) { throw "Missing configured build value: $key" }
    return $line.Substring($line.IndexOf('=')+1)
}
$qtRoot = (Read-CMakeValue 'CMAKE_PREFIX_PATH').Split(';')[0]
$qtBin = Join-Path $qtRoot 'bin'
$compiler = Read-CMakeValue 'CMAKE_CXX_COMPILER'
$dumpbin = Join-Path (Split-Path -Parent $compiler) 'dumpbin.exe'
$app = Join-Path $destination 'precision_cad.exe'
$worker = Join-Path $destination 'precision_geometry_worker.exe'
foreach ($file in @($app,$worker,$dumpbin,(Join-Path $qtBin 'windeployqt.exe'))) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing native runtime input: $file" }
}
$provenance = Get-Content (Join-Path $buildRoot 'build-provenance.json') -Raw | ConvertFrom-Json
$head = (git -C $root rev-parse --verify HEAD).Trim()
if ($provenance.sourceCommit -ne $head -or (git -C $root status --porcelain)) { throw 'The native runtime must be staged from its unchanged committed source.' }
& (Join-Path $qtBin 'windeployqt.exe') --force --release --qmldir (Join-Path $root 'src/app') --dir $destination $app
if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed.' }
$manifest = Get-Content (Join-Path $root 'manifests/native-dependencies.json') -Raw | ConvertFrom-Json
$occtCache = Join-Path $env:LOCALAPPDATA ('precision-cad-toolchain/occt-' + $manifest.occt.version)
$searchDirectories = @((Join-Path $occtCache ($manifest.occt.root + '/win64/vc14/bin')),$qtBin)
foreach ($relative in $manifest.occtSupport.requiredRuntimeDlls) {
    $searchDirectories += Split-Path -Parent (Join-Path $occtCache ('supporting/unpacked/' + $manifest.occtSupport.root + '/' + $relative))
}
$searchDirectories += $destination
$queue = [Collections.Generic.Queue[string]]::new()
$queue.Enqueue($app); $queue.Enqueue($worker)
$visited = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
while ($queue.Count) {
    $binary = $queue.Dequeue()
    if (-not $visited.Add($binary)) { continue }
    $imports = & $dumpbin /NOLOGO /DEPENDENTS $binary
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the built binary imports.' }
    foreach ($line in $imports) {
        $name = $line.Trim()
        if ($name -notmatch '^[A-Za-z0-9_.-]+[.]dll$') { continue }
        if ($name -match '^(api-ms-|ext-ms-)') { continue }
        $resolved = $null
        foreach ($directory in $searchDirectories) {
            $candidate = Join-Path $directory $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $resolved=$candidate; break }
        }
        if (-not $resolved) {
            if (Test-Path -LiteralPath (Join-Path $env:SystemRoot ('System32/' + $name))) { continue }
            throw "Unresolved native runtime import: $name"
        }
        $target = Join-Path $destination $name
        if ([IO.Path]::GetFullPath($resolved) -ne [IO.Path]::GetFullPath($target)) {
            Copy-Item -LiteralPath $resolved -Destination $target -Force
            if ((Get-FileHash $resolved -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) { throw "Runtime copy hash mismatch: $name" }
        }
        $queue.Enqueue($target)
    }
}
foreach ($relative in @('Qt6Core.dll','platforms/qwindows.dll','jemalloc.dll','tbb12.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $destination $relative) -PathType Leaf)) { throw "Required staged runtime is absent: $relative" }
}
Copy-Item -LiteralPath (Join-Path $buildRoot 'build-provenance.json') -Destination (Join-Path $destination 'build-provenance.json') -Force
$receipt = [ordered]@{ schemaVersion=1; sourceCommit=$head; build=$provenance; applicationSha256=(Get-FileHash $app -Algorithm SHA256).Hash.ToLowerInvariant(); workerSha256=(Get-FileHash $worker -Algorithm SHA256).Hash.ToLowerInvariant(); scope='native-development-runtime'; installer=$false }
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $buildRoot 'runtime-receipt.json') -Encoding utf8
Write-Output "Native development runtime staged: $destination"
