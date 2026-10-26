param([string]$Build='build/windows',[string]$Output='build/package',[switch]$Ara)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Package output already exists; use a new directory'}
New-Item -ItemType Directory -Path $Output | Out-Null
$gui=Join-Path $Build 'MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE.exe'
Copy-Item -LiteralPath $gui -Destination $Output
Copy-Item -LiteralPath (Join-Path $Build 'MRLiveJuce_artefacts/Release/MR Live.exe') -Destination $Output
foreach($name in @('mrs_vst3_scan.exe','mrs_crash_reporter.exe')){Copy-Item -LiteralPath "$Build/Release/$name" -Destination $Output}
if($Ara){Copy-Item -LiteralPath "$Build/Release/mrs_ara_scan.exe" -Destination $Output}
$licenses=@{'build/sdk/juce/LICENSE.md'='JUCE-LICENSE.md';'build/sdk/portaudio/LICENSE.txt'='PortAudio-LICENSE.txt';'build/sdk/vst3/LICENSE.txt'='VST3-LICENSE.txt';'build/sdk/vst3/base/LICENSE.txt'='VST3-base-LICENSE.txt';'build/sdk/vst3/pluginterfaces/LICENSE.txt'='VST3-pluginterfaces-LICENSE.txt';'build/sdk/vst3/public.sdk/LICENSE.txt'='VST3-public-sdk-LICENSE.txt';'apps/studio-juce/assets/OFL.txt'='OFL.txt';'third_party/signalsmith-stretch/LICENSE.txt'='Signalsmith-Stretch-LICENSE.txt';'third_party/signalsmith-linear/LICENSE.txt'='Signalsmith-Linear-LICENSE.txt'}
if($Ara){$licenses['build/sdk/ara/ARA_API/LICENSE.txt']='ARA-API-LICENSE.txt';$licenses['build/sdk/ara/ARA_Library/LICENSE.txt']='ARA-Library-LICENSE.txt'}
foreach($entry in $licenses.GetEnumerator()){Copy-Item -LiteralPath $entry.Key -Destination (Join-Path $Output $entry.Value)}
$asioLicenses=@(Get-ChildItem -LiteralPath "$Build/_deps/portaudio-build" -Recurse -File -Filter LICENSE.txt | Where-Object {$_.FullName -match 'asiosdk|ASIOSDK'})
if(!$asioLicenses.Count){throw 'ASIO license notice missing'}
Copy-Item -LiteralPath $asioLicenses[0].FullName -Destination "$Output/ASIO-SDK-LICENSE.txt"
$version=(Get-Item -LiteralPath $gui).VersionInfo.FileVersion
if($version -ne '0.8.2'){throw "Unexpected Windows version $version"}
$sha=& git rev-parse HEAD
@("MR Studio 0.8.2; source $sha; Windows Release; ARA=$Ara",'Launch Moon River Studio JUCE.exe. Home launches the separate MR Live.exe application. SDKs and third-party plugins are not included.','Development acceptance artifact. See the included license notices; commercial distribution is not certified by CI.','CI results do not certify physical ASIO/MIDI/DPI performance. See run test logs for integration status.') | Set-Content -LiteralPath "$Output/README.txt" -Encoding UTF8
$env:CI_TEST_RESULT | Set-Content -LiteralPath "$Output/CI-TEST-RESULT.txt" -Encoding UTF8
$files=@(Get-ChildItem -LiteralPath $Output -File | Sort-Object Name | ForEach-Object {@{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
@{source=$sha;version=$version;ara=[bool]$Ara;run=$env:GITHUB_RUN_ID;tests=$env:CI_TEST_RESULT;runner_image=$env:ImageVersion;sdk_lock_sha256=(Get-FileHash -LiteralPath "$PSScriptRoot/sdk-lock.json").Hash;files=$files} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$Output/SHA256.json" -Encoding UTF8
foreach($file in $files){if((Get-FileHash -LiteralPath (Join-Path $Output $file.name)).Hash -ne $file.sha256){throw 'Package integrity failure'}}

& (Join-Path $PSScriptRoot 'Verify-Package.ps1') -Package $Output -ExpectedSource $sha
