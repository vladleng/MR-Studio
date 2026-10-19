[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test')]
    [string]$Step = 'Build',
    [string]$TestRegex,
    [switch]$Full,
    [ValidateRange(1, 32)]
    [int]$Jobs = 2
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'build/asio-local'
$depsRoot = Join-Path $projectRoot 'build/_deps'
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakeExe = $cmakeCommand.Source
} else {
    $cmakeExe = 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
if (!(Test-Path -LiteralPath $cmakeExe)) { throw 'CMake is unavailable. Install the Visual Studio CMake component.' }
$ctestExe = Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe'

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}

if ($Step -eq 'Test') {
    if ($Full -and $TestRegex) { throw 'Choose either -Full or -TestRegex.' }
    if (!$Full -and !$TestRegex) { throw 'Specify -TestRegex for focused tests or -Full for an acceptance candidate.' }
    if (!(Test-Path -LiteralPath (Join-Path $buildRoot 'CTestTestfile.cmake'))) { throw 'Configure and build first.' }
    $testArgs = @('--test-dir', $buildRoot, '-C', 'Release', '--output-on-failure', '--no-tests=error')
    if ($TestRegex) { $testArgs += @('-R', $TestRegex) }
    Invoke-Checked $ctestExe $testArgs
    return
}

foreach ($required in @('JUCE-9.0.3/CMakeLists.txt', 'vst3sdk/CMakeLists.txt', 'portaudio-src/CMakeLists.txt', 'mrs-asiosdk.zip')) {
    if (!(Test-Path -LiteralPath (Join-Path $depsRoot $required))) { throw "Missing cached dependency: $required. Restore it from the verified migration backup; this script does not download SDKs." }
}
$configureArgs = @(
    '-S', $projectRoot, '-B', $buildRoot,
    '-G', 'Visual Studio 18 2026', '-A', 'x64',
    '-DBUILD_TESTING=ON', '-DMRS_BUILD_ASIO=ON', '-DMRS_BUILD_DESKTOP=ON',
    '-DMRS_BUILD_JUCE_UI=ON', '-DMRS_BUILD_VST3=ON',
    "-DMRS_JUCE_SDK_ROOT=$depsRoot/JUCE-9.0.3",
    "-DMRS_VST3_SDK_ROOT=$depsRoot/vst3sdk",
    "-DMRS_ASIO_SDK_ZIP=$depsRoot/mrs-asiosdk.zip",
    "-DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$depsRoot/portaudio-src",
    '-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
)
Invoke-Checked $cmakeExe $configureArgs
if ($Step -eq 'Build') {
    Invoke-Checked $cmakeExe @('--build', $buildRoot, '--config', 'Release', '--parallel', "$Jobs")
}
