[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'All')]
    [string]$Step = 'Build',
    [string]$TestRegex,
    [switch]$Full,
    [ValidateRange(1, 32)]
    [int]$Jobs = 2,
    [string]$BuildDir = 'build/local-windows',
    [string[]]$Target,
    [switch]$NoAra,
    [switch]$NoLive,
    [switch]$PrepareSdk,
    [switch]$NoShortcut,
    [switch]$NoDeploy,
    [string]$DeployPath = 'C:\Users\Vladislav\MR Studio App',
    [string]$ShortcutName = 'MR Studio'
)

# Local Windows build for MR Studio / MR Live.
# Toolchain: Visual Studio 18 Community with bundled CMake/Ninja.
# SDK sources: build/sdk/{juce,vst3,portaudio,ara}; licensed ASIO archive: build/private-sdk/asio.zip.
# Run tools/ci/Prepare-Sdks.ps1 (with -Ara for ARA2) once to populate them.

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    # Native tools write progress to stderr; Windows PowerShell 5.1 would turn that into a
    # terminating error under ErrorActionPreference=Stop, so relax it for the call only.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Executable @Arguments
        $code = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "$Executable failed with exit code $code" }
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot $BuildDir
$sdkRoot = Join-Path $projectRoot 'build/sdk'
$asioArchive = Join-Path $projectRoot 'build/private-sdk/asio.zip'
$araSdk = Join-Path $sdkRoot 'ara'
$araFixtureDir = Join-Path $projectRoot 'build/ara-render'
$araFixture = Join-Path $araFixtureDir 'Release/MRSAraRenderFixture.vst3'

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakeExe = $cmakeCommand.Source
} else {
    $cmakeExe = 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
if (!(Test-Path -LiteralPath $cmakeExe)) { throw 'CMake is unavailable. Install the Visual Studio CMake component.' }
$ctestExe = Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe'

$requiredSdk = [ordered]@{
    'juce/CMakeLists.txt'      = 'build/sdk/juce'
    'vst3/CMakeLists.txt'      = 'build/sdk/vst3'
    'portaudio/CMakeLists.txt' = 'build/sdk/portaudio'
}
if (!$NoAra) { $requiredSdk['ara/ARA_API/CMakeLists.txt'] = 'build/sdk/ara' }

$missing = @()
foreach ($relative in $requiredSdk.Keys) {
    if (!(Test-Path -LiteralPath (Join-Path $sdkRoot $relative))) { $missing += (Join-Path 'build/sdk' $relative) }
}
if (!(Test-Path -LiteralPath $asioArchive)) { $missing += 'build/private-sdk/asio.zip' }

if ($missing.Count -gt 0) {
    if ($PrepareSdk) {
        $prepareArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'ci/Prepare-Sdks.ps1'))
        if (!$NoAra) { $prepareArgs += '-Ara' }
        Write-Host 'Preparing SDK caches (this downloads pinned SDK sources)...'
        Invoke-Checked 'powershell.exe' $prepareArgs
    } else {
        throw ("Missing SDK inputs:`n  {0}`nRun: powershell -ExecutionPolicy Bypass -File tools/ci/Prepare-Sdks.ps1{1}, or add -PrepareSdk." -f ($missing -join "`n  "), $(if ($NoAra) { '' } else { ' -Ara' }))
    }
}

if ($Step -eq 'Test') {
    if ($Full -and $TestRegex) { throw 'Choose either -Full or -TestRegex.' }
    if (!$Full -and !$TestRegex) { throw 'Specify -TestRegex for focused tests or -Full for an acceptance candidate.' }
    if (!(Test-Path -LiteralPath (Join-Path $buildRoot 'CTestTestfile.cmake'))) { throw 'Configure and build first.' }
    $testArgs = @('--test-dir', $buildRoot, '-C', 'Release', '--output-on-failure', '--no-tests=error')
    if ($TestRegex) { $testArgs += @('-R', $TestRegex) }
    if ($Full) { $testArgs += @('--timeout', '900') }
    Invoke-Checked $ctestExe $testArgs
    return
}

# --- ARA render fixture (a synthetic ARA-enabled VST3 used by the ARA test suites) ---
if (!$NoAra) {
    $fixtureArgs = @(
        '-S', (Join-Path $projectRoot 'tools/ara-render-fixture'), '-B', $araFixtureDir,
        '-G', 'Visual Studio 18 2026', '-A', 'x64',
        "-DMRS_ARA_SDK_ROOT=$araSdk",
        "-DARA_VST3_SDK_DIR=$(Join-Path $sdkRoot 'vst3')",
        '-DSMTG_ENABLE_VSTGUI_SUPPORT=OFF',
        '-DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF',
        '-DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF',
        '-DSMTG_CREATE_PLUGIN_LINK=OFF'
    )
    Invoke-Checked $cmakeExe $fixtureArgs
    Invoke-Checked $cmakeExe @('--build', $araFixtureDir, '--config', 'Release', '--target', 'MRSAraRenderFixture', '--parallel', '1')
    if (!(Test-Path -LiteralPath $araFixture)) { throw "ARA render fixture was not produced at $araFixture" }
}

$configureArgs = @(
    '-S', $projectRoot, '-B', $buildRoot,
    '-G', 'Visual Studio 18 2026', '-A', 'x64',
    '-DBUILD_TESTING=ON', '-DMRS_BUILD_ASIO=ON', '-DMRS_BUILD_DESKTOP=ON',
    '-DMRS_BUILD_JUCE_UI=ON', '-DMRS_BUILD_VST3=ON',
    "-DMRS_JUCE_SDK_ROOT=$(Join-Path $sdkRoot 'juce')",
    "-DMRS_VST3_SDK_ROOT=$(Join-Path $sdkRoot 'vst3')",
    "-DMRS_ASIO_SDK_ZIP=$asioArchive",
    "-DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$(Join-Path $sdkRoot 'portaudio')",
    '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',
    "-DMRS_BUILD_LIVE=$(if ($NoLive) { 'OFF' } else { 'ON' })"
)
if ($NoAra) {
    $configureArgs += '-DMRS_BUILD_ARA=OFF'
} else {
    $configureArgs += @(
        '-DMRS_BUILD_ARA=ON',
        "-DMRS_ARA_SDK_ROOT=$araSdk",
        "-DMRS_ARA_RENDER_TEST_PLUGIN=$araFixture",
        "-DMRS_ARA_TEST_PLUGIN=$araFixture",
        '-DMRS_ARA_TEST_PLUGIN_GAIN:STRING=0.5'
    )
}
Invoke-Checked $cmakeExe $configureArgs
if ($Step -eq 'Configure') { return }

$buildArgs = @('--build', $buildRoot, '--config', 'Release', '--parallel', "$Jobs")
if ($Target -and $Target.Count -gt 0) { $buildArgs += @('--target') + $Target }
Write-Host ("Building {0} with {1} parallel job(s)..." -f $(if ($Target) { "target(s): $($Target -join ', ')" } else { 'all targets' }), $Jobs)
Invoke-Checked $cmakeExe $buildArgs

# --- Deploy + desktop shortcut -------------------------------------------------
# Tooling on this machine restricts binaries located inside the checkout so that they may only
# write inside it (a workspace-scoped file policy). An app started from the workspace therefore
# cannot write project data or diagnostics into Documents. Copying the runtime next to the user
# profile keeps normal save/diagnostics behaviour, so the shortcut points at the deployed copy.
$builtExe = Join-Path $buildRoot 'MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE.exe'
$launchExe = $builtExe

if (!$NoDeploy -and (Test-Path -LiteralPath $builtExe)) {
    New-Item -ItemType Directory -Force -Path $DeployPath | Out-Null
    $artefacts = Split-Path -Parent $builtExe
    & robocopy $artefacts $DeployPath /MIR /NFL /NDL /NJH /NJS /NP /XF *.pdb | Out-Null
    $robocopyExit = $LASTEXITCODE
    if ($robocopyExit -ge 8) { throw "Deploy to $DeployPath failed (robocopy exit $robocopyExit)" }
    $launchExe = Join-Path $DeployPath 'Moon River Studio JUCE.exe'
    Write-Host "Deployed runtime: $DeployPath"
} elseif (!$NoDeploy) {
    Write-Host 'Studio executable not found after build; deployment skipped.'
}

if (!$NoShortcut -and (Test-Path -LiteralPath $launchExe)) {
    $shortcutPath = Join-Path ([Environment]::GetFolderPath('Desktop')) "$ShortcutName.lnk"
    $version = (Get-Item -LiteralPath $launchExe).VersionInfo.ProductVersion
    $shell = New-Object -ComObject WScript.Shell
    try {
        $shortcut = $shell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = $launchExe
        $shortcut.WorkingDirectory = Split-Path -Parent $launchExe
        $shortcut.IconLocation = "$launchExe,0"
        $shortcut.Description = if ($version) { "MR Studio $version (local build)" } else { 'MR Studio (local build)' }
        $shortcut.Save()
    }
    finally {
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($shell)
    }
    Write-Host "Desktop shortcut updated: $shortcutPath -> $launchExe"
}
elseif (!$NoShortcut) {
    Write-Host 'Studio executable not found after build; desktop shortcut left unchanged.'
}
