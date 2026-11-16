[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'All')]
    [string]$Step = 'Build',
    [ValidateSet('Core', 'Studio', 'Live', 'Both')]
    [string]$Product = 'Studio',
    [string]$BuildDir,
    [string[]]$Target,
    [string]$TestRegex,
    [switch]$Full,
    [switch]$NoAra,
    [switch]$AraFixture,
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [ValidateRange(1, 32)]
    [int]$Jobs = 2,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$useSdk = $Product -ne 'Core'
$useAra = $Product -in @('Studio', 'Both') -and !$NoAra
if ($NoAra -and $Product -notin @('Studio', 'Both')) { throw '-NoAra is only for a diagnostic Studio/Both build.' }
if ($AraFixture -and !$useAra) { throw 'The ARA fixture requires Studio/Both with ARA enabled.' }
if ($Full -and $TestRegex) { throw 'Choose either -Full or -TestRegex.' }
if ($Step -in @('Test', 'All')) {
    if (!$Full -and !$TestRegex) { throw 'Specify -TestRegex for focused tests or -Full for an acceptance candidate.' }
    if ($Full -and $useAra -and !$AraFixture) { throw 'Full ARA checks require -AraFixture so render tests are registered.' }
}
if (!$BuildDir) {
    $profile = $Product.ToLowerInvariant()
    if ($NoAra) { $profile += '-noara' }
    $BuildDir = "build/local-windows/$profile"
}
if (![IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $projectRoot $BuildDir }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
if ($BuildDir.TrimEnd('\', '/') -eq $projectRoot.TrimEnd('\', '/')) { throw 'BuildDir must not be the source root.' }
$sdk = Join-Path $projectRoot 'build/sdk'
$asio = Join-Path $projectRoot 'build/private-sdk/asio.zip'
$fixtureBuild = Join-Path $BuildDir 'ara-render'
$fixture = Join-Path $fixtureBuild "$Configuration/MRSAraRenderFixture.vst3"

$configureArguments = @(
    '-S', $projectRoot, '-B', $BuildDir, '-G', 'Visual Studio 18 2026', '-A', 'x64',
    '-DBUILD_TESTING=ON', '-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
)
$expected = [ordered]@{
    CMAKE_HOME_DIRECTORY = $projectRoot
    CMAKE_GENERATOR = 'Visual Studio 18 2026'
    CMAKE_GENERATOR_PLATFORM = 'x64'
    BUILD_TESTING = 'ON'
    MRS_BUILD_ASIO = $(if ($useSdk) { 'ON' } else { 'OFF' })
    MRS_BUILD_DESKTOP = $(if ($useSdk) { 'ON' } else { 'OFF' })
    MRS_BUILD_JUCE_UI = $(if ($useSdk) { 'ON' } else { 'OFF' })
    MRS_BUILD_VST3 = $(if ($useSdk) { 'ON' } else { 'OFF' })
    MRS_BUILD_LIVE = $(if ($Product -in @('Live', 'Both')) { 'ON' } else { 'OFF' })
    MRS_BUILD_ARA = $(if ($useAra) { 'ON' } else { 'OFF' })
}
foreach ($key in @($expected.Keys | Where-Object { $_ -like 'MRS_*' })) {
    $configureArguments += "-D$key=$($expected[$key])"
}
if ($useSdk) {
    $configureArguments += @(
        "-DMRS_JUCE_SDK_ROOT=$sdk/juce", "-DMRS_VST3_SDK_ROOT=$sdk/vst3",
        "-DMRS_ASIO_SDK_ZIP=$asio", "-DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$sdk/portaudio"
    )
}
if ($useAra) { $configureArguments += "-DMRS_ARA_SDK_ROOT=$sdk/ara" }
# Explicit empty values remove fixture registrations left by an earlier configure.
$expected.MRS_ARA_RENDER_TEST_PLUGIN = $(if ($AraFixture) { $fixture } else { '' })
$expected.MRS_ARA_TEST_PLUGIN = $expected.MRS_ARA_RENDER_TEST_PLUGIN
foreach ($key in @('MRS_ARA_RENDER_TEST_PLUGIN', 'MRS_ARA_TEST_PLUGIN')) {
    $configureArguments += "-D$key=$($expected[$key])"
}
$configureArguments += "-DMRS_ARA_TEST_PLUGIN_GAIN=$(if ($AraFixture) { '0.5' } else { '1' })"

if (!$Target) {
    $Target = switch ($Product) {
        'Core' { @('mrs_core_tests') }
        'Studio' { @('MoonRiverStudioJuce') }
        'Live' { @('MRLiveJuce') }
        'Both' { @('MoonRiverStudioJuce') } # Studio already depends on MRLiveJuce in this configuration.
    }
}
$commands = @()
if ($Step -in @('Configure', 'Build', 'All')) {
    if ($AraFixture) {
        $commands += [pscustomobject]@{ Role = 'FixtureConfigure'; Tool = 'cmake.exe'; Arguments = @(
            '-S', (Join-Path $projectRoot 'tools/ara-render-fixture'), '-B', $fixtureBuild,
            '-G', 'Visual Studio 18 2026', '-A', 'x64', "-DMRS_ARA_SDK_ROOT=$sdk/ara",
            "-DARA_VST3_SDK_DIR=$sdk/vst3", '-DSMTG_ENABLE_VSTGUI_SUPPORT=OFF',
            '-DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF', '-DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF',
            '-DSMTG_CREATE_PLUGIN_LINK=OFF', '-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
        ) }
        if ($Step -ne 'Configure') {
            $commands += [pscustomobject]@{ Role = 'FixtureBuild'; Tool = 'cmake.exe'; Arguments = @(
                '--build', $fixtureBuild, '--config', $Configuration, '--target', 'MRSAraRenderFixture', '--parallel', "$Jobs"
            ) }
        }
    }
    $commands += [pscustomobject]@{ Role = 'Configure'; Tool = 'cmake.exe'; Arguments = $configureArguments }
}
if ($Step -in @('Build', 'All')) {
    $commands += [pscustomobject]@{ Role = 'Build'; Tool = 'cmake.exe'; Arguments = @(
        '--build', $BuildDir, '--config', $Configuration, '--target'
    ) + $Target + @('--parallel', "$Jobs") }
}
if ($Step -in @('Test', 'All')) {
    $testArguments = @('--test-dir', $BuildDir, '-C', $Configuration, '--output-on-failure', '--no-tests=error')
    if ($TestRegex) { $testArguments += @('-R', $TestRegex) }
    $commands += [pscustomobject]@{ Role = 'Test'; Tool = 'ctest.exe'; Arguments = $testArguments }
}
$plan = [pscustomobject]@{
    Product = $Product; BuildDirectory = $BuildDir; Targets = @($Target)
    ConfigureArguments = $configureArguments; Commands = @($commands)
}
if ($DryRun) { return $plan }

& (Join-Path $PSScriptRoot 'Enter-DevEnvironment.ps1')
if ($useSdk) {
    foreach ($required in @('juce/CMakeLists.txt', 'vst3/public.sdk/source/vst/hosting/module.h', 'portaudio/CMakeLists.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $sdk $required))) { throw "Missing prepared SDK file: $required" }
    }
    if (!(Test-Path -LiteralPath $asio)) { throw "Missing prepared ASIO archive: $asio" }
    $lock = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'ci/sdk-lock.json') | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $asio -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.asio.sha256) {
        throw 'ASIO checksum mismatch. Restore the pinned archive; do not change the hash to bypass validation.'
    }
}
if ($useAra -and !(Test-Path -LiteralPath "$sdk/ara/ARA_API/ARAInterface.h")) { throw 'Missing prepared ARA SDK.' }

# Test never silently reconfigures an unrelated or differently profiled build tree.
if ($Step -eq 'Test') {
    $cachePath = Join-Path $BuildDir 'CMakeCache.txt'
    if (!(Test-Path -LiteralPath $cachePath)) { throw 'Configure and build this profile first.' }
    $cache = @{}
    foreach ($line in Get-Content -LiteralPath $cachePath) {
        if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') { $cache[$Matches[1]] = $Matches[2] }
    }
    foreach ($key in $expected.Keys) {
        $actual = ([string]$cache[$key]).Replace('\', '/').TrimEnd('/')
        $wanted = ([string]$expected[$key]).Replace('\', '/').TrimEnd('/')
        if (!$cache.ContainsKey($key) -or $actual -ne $wanted) { throw "Build profile mismatch ($key); configure/build with the same options first." }
    }
    if (!(Test-Path -LiteralPath (Join-Path $BuildDir 'CTestTestfile.cmake'))) { throw 'This profile has no configured tests.' }
}
foreach ($command in $commands) {
    Write-Host "[$($command.Role)] $($command.Tool) $($command.Arguments | ConvertTo-Json -Compress)"
    $commandArguments = $command.Arguments
    $isTest = $command.Role -eq 'Test'
    $previousTemp = $env:TEMP
    $previousTmp = $env:TMP
    try {
        if ($isTest) {
            # Test fixtures must not depend on a writable machine-wide C:\Temp.
            $testTemp = Join-Path $BuildDir 'Testing/Temp'
            New-Item -ItemType Directory -Path $testTemp -Force | Out-Null
            $env:TEMP = $testTemp
            $env:TMP = $testTemp
        }
        & $command.Tool @commandArguments
        if ($LASTEXITCODE -ne 0) { throw "$($command.Role) failed with exit code $LASTEXITCODE" }
    } finally {
        if ($isTest) {
            $env:TEMP = $previousTemp
            $env:TMP = $previousTmp
        }
    }
}
