$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot '../Build-Local.ps1'

function Assert-True([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}

$studio = & $scriptPath -Step Build -Product Studio -DryRun
Assert-True ($studio.ConfigureArguments -contains '-DMRS_BUILD_ARA=ON') 'Studio must default to ARA ON.'
Assert-True ($studio.ConfigureArguments -contains '-DMRS_BUILD_LIVE=OFF') 'Studio must not build Live by default.'
Assert-True ($studio.Targets.Count -eq 1 -and $studio.Targets[0] -eq 'MoonRiverStudioJuce') 'Default Studio build must be scoped.'
Assert-True (($studio.ConfigureArguments | Where-Object { $_ -like '-DMRS_JUCE_SDK_ROOT=*' }) -match 'build[/\\]sdk[/\\]juce') 'Use the prepared SDK paths.'
Assert-True ($studio.ConfigureArguments -contains '-DFETCHCONTENT_FULLY_DISCONNECTED=ON') 'Configure must not download dependencies.'

$live = & $scriptPath -Step Build -Product Live -DryRun
Assert-True ($live.ConfigureArguments -contains '-DMRS_BUILD_ARA=OFF') 'Standalone Live must be ARA-free.'
Assert-True (!($live.ConfigureArguments | Where-Object { $_ -like '-DMRS_ARA_SDK_ROOT=*' })) 'Live must not require the ARA SDK.'
Assert-True ($live.Targets.Count -eq 1 -and $live.Targets[0] -eq 'MRLiveJuce') 'Live must not build Studio.'
Assert-True ($studio.BuildDirectory -ne $live.BuildDirectory) 'Profiles must use separate build trees.'

$core = & $scriptPath -Step Build -Product Core -DryRun
foreach ($option in @('ASIO', 'VST3', 'ARA', 'DESKTOP', 'JUCE_UI', 'LIVE')) {
    Assert-True ($core.ConfigureArguments -contains "-DMRS_BUILD_$option=OFF") "Core must disable $option."
}
Assert-True (!($core.ConfigureArguments | Where-Object { $_ -like '*SDK_ROOT=*' })) 'Core must not require product SDKs.'

$both = & $scriptPath -Step Build -Product Both -DryRun
Assert-True ($both.ConfigureArguments -contains '-DMRS_BUILD_ARA=ON') 'Joint build must retain Studio ARA.'
Assert-True ($both.ConfigureArguments -contains '-DMRS_BUILD_LIVE=ON') 'Joint build must enable Live.'
$diagnostic = & $scriptPath -Product Studio -NoAra -DryRun
Assert-True ($diagnostic.BuildDirectory -ne $studio.BuildDirectory) 'Diagnostic ARA OFF must use a separate tree.'

$fixture = & $scriptPath -Step Build -Product Studio -AraFixture -DryRun
Assert-True ($fixture.Commands[0].Role -eq 'FixtureConfigure') 'ARA fixture must be configured first.'
Assert-True ($fixture.Commands[1].Role -eq 'FixtureBuild') 'ARA fixture must be built before product configuration.'
Assert-True ($fixture.ConfigureArguments -contains '-DMRS_ARA_TEST_PLUGIN_GAIN=0.5') 'ARA fixture expectations must match its actual gain.'
$configureOnly = & $scriptPath -Step Configure -Product Studio -AraFixture -DryRun
Assert-True (!($configureOnly.Commands | Where-Object { $_.Role -like '*Build' })) 'Configure must not build a fixture or product.'
$scoped = & $scriptPath -Product Core -Target @('mrs_core_tests', 'mrs_core_check') -DryRun
Assert-True ($scoped.Commands[-1].Arguments -contains 'mrs_core_tests' -and $scoped.Commands[-1].Arguments -contains 'mrs_core_check') 'Preserve each explicitly selected target as a separate argument.'

$test = & $scriptPath -Step Test -Product Core -TestRegex '^core$' -BuildDir 'build/path with spaces' -DryRun
Assert-True ($test.Commands.Count -eq 1 -and $test.Commands[0].Role -eq 'Test') 'Test must not configure or build.'
Assert-True ($test.Commands[0].Arguments -contains '^core$') 'Keep the exact test regex.'
Assert-True ($test.Commands[0].Arguments -contains $test.BuildDirectory) 'Keep build paths with spaces as one argument.'

$failed = $false
try { & $scriptPath -Step Test -Product Core -DryRun | Out-Null } catch { $failed = $_.Exception.Message -like '*TestRegex*' }
Assert-True $failed 'An unscoped test invocation must fail.'
$failed = $false
try { & $scriptPath -Product Live -AraFixture -DryRun | Out-Null } catch { $failed = $_.Exception.Message -like '*ARA*' }
Assert-True $failed 'Live must reject the ARA fixture.'

Write-Output 'PASS: local workflow scope, ARA separation, SDK paths, fixture order and test selection.'
