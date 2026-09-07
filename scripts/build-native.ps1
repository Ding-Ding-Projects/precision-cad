param([switch]$Test, [switch]$ConfigureOnly, [string]$TestFilter, [string]$QtRoot, [string]$BuildToolsRoot)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content (Join-Path $root 'manifests/native-dependencies.json') -Raw | ConvertFrom-Json
$cache = Join-Path $env:LOCALAPPDATA 'precision-cad-toolchain'
$occtCache = Join-Path $cache ('occt-' + $manifest.occt.version)
New-Item -ItemType Directory -Force -Path $occtCache | Out-Null
function Get-VerifiedArchive($spec, $destination) {
    $zip = Join-Path $destination $spec.archiveName
    if (-not (Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $spec.sha256) {
        $stage = $zip + '.download-' + [Guid]::NewGuid().ToString('N')
        Invoke-WebRequest $spec.archiveUrl -OutFile $stage
        if ((Get-FileHash -LiteralPath $stage -Algorithm SHA256).Hash.ToLowerInvariant() -ne $spec.sha256) { throw 'Native dependency archive hash mismatch.' }
        Move-Item -LiteralPath $stage -Destination $zip -Force
    }
    return $zip
}
$occtZip = Get-VerifiedArchive $manifest.occt $occtCache
$occtRoot = Join-Path $occtCache $manifest.occt.root
if (-not (Test-Path (Join-Path $occtRoot 'cmake/OpenCASCADEConfig.cmake'))) {
    $outer = Join-Path $occtCache 'distribution'
    Expand-Archive -LiteralPath $occtZip -DestinationPath $outer -Force
    Expand-Archive -LiteralPath (Join-Path $outer $manifest.occt.innerArchive) -DestinationPath $occtCache -Force
}
$supportZip = Get-VerifiedArchive $manifest.occtSupport $occtCache
$support = Join-Path $occtCache 'supporting/unpacked'
if (-not (Test-Path (Join-Path $support $manifest.occtSupport.root))) {
    $outer = Join-Path $occtCache 'supporting'
    Expand-Archive -LiteralPath $supportZip -DestinationPath $outer -Force
    Expand-Archive -LiteralPath (Join-Path $outer $manifest.occtSupport.innerArchive) -DestinationPath $support -Force
}
if (-not $QtRoot) {
    $candidates = @(
        (Join-Path $cache 'Qt/6.8.3/msvc2022_64'),
        (Join-Path $env:LOCALAPPDATA 'material-virtualbox-toolchain/Qt/6.8.3/msvc2022_64')
    )
    $QtRoot = $candidates | Where-Object { Test-Path (Join-Path $_ 'bin/qtpaths.exe') } | Select-Object -First 1
}
if (-not $QtRoot) { throw 'Qt 6.8.3 msvc2022_64 is not available in the supported caches. Automatic Qt acquisition remains unimplemented.' }
if ((& (Join-Path $QtRoot 'bin/qtpaths.exe') --qt-version) -ne $manifest.qt.version) { throw 'Qt version does not match native manifest.' }
if (-not $BuildToolsRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path $vswhere) { $BuildToolsRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1) }
}
if (-not $BuildToolsRoot) { throw 'MSVC x64 build tools were not discovered. Automatic compiler acquisition remains unimplemented.' }
$vcvars = Join-Path $BuildToolsRoot 'VC/Auxiliary/Build/vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw 'Discovered compiler lacks vcvars64.bat.' }
$environmentLines = & $env:ComSpec /d /s /c "`"`"$vcvars`" >nul && set`""
if ($LASTEXITCODE -ne 0) { throw 'Could not activate the MSVC x64 environment.' }
foreach ($line in $environmentLines) {
    $index = $line.IndexOf('=')
    if ($index -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0,$index),$line.Substring($index+1),'Process') }
}
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$nativeBuild = Join-Path $root 'build/native'
$occtBin = Join-Path $occtRoot 'win64/vc14/bin'
$supportBins = @()
foreach ($requiredDll in $manifest.occtSupport.requiredRuntimeDlls) {
    $runtime = Join-Path (Join-Path $support $manifest.occtSupport.root) $requiredDll
    if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) { throw "Missing pinned supporting runtime: $requiredDll." }
    $supportBins += Split-Path -Parent $runtime
}
$supportBins = @($supportBins | Select-Object -Unique)
$env:PATH = ((@((Join-Path $QtRoot 'bin'),$occtBin) + $supportBins + @($env:PATH)) -join ';')
& $cmake -S $root -B $nativeBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DOpenCASCADE_DIR=$occtRoot/cmake" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'Native CMake configuration failed.' }
if ($ConfigureOnly) { return }
& $cmake --build $nativeBuild --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
if ($Test) {
    $testArguments = @('--test-dir',$nativeBuild,'--output-on-failure')
    if ($TestFilter) { $testArguments += @('-R',$TestFilter) }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') @testArguments
    if ($LASTEXITCODE -ne 0) { throw 'Native local tests failed.' }
}
Write-Output "Native build completed: $nativeBuild"
