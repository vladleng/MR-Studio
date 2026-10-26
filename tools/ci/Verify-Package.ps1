param([Parameter(Mandatory)][string]$Package,[string]$ExpectedSource)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Package).Path
$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'SHA256.json') | ConvertFrom-Json
if($ExpectedSource -and $manifest.source -ne $ExpectedSource){throw 'Artifact source mismatch'}
if($manifest.version -ne '0.8.4'){throw 'Artifact version mismatch'}
$names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in $manifest.files){
 if($file.name -ne [IO.Path]::GetFileName($file.name) -or $file.name -in @('.','..') -or !$names.Add($file.name)){throw 'Invalid/duplicate artifact entry'}
 $path=Join-Path $root $file.name
 $actual=Get-Item -LiteralPath $path
 if($actual.Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256){throw "Artifact integrity mismatch: $($file.name)"}
}
foreach($file in (Get-ChildItem -LiteralPath $root -Recurse -File)){
 if($file.DirectoryName -ne $root -or ($file.Name -ne 'SHA256.json' -and !$names.Contains($file.Name))){throw 'Unmanifested artifact content'}
 if($file.Extension -in @('.pdb','.vst3','.wav','.nam','.zip','.lib','.h','.cpp')){throw 'Non-runtime content in artifact'}
}
foreach($required in @('Moon River Studio JUCE.exe','MR Live.exe','mrs_vst3_scan.exe','mrs_crash_reporter.exe','ASIO-SDK-LICENSE.txt','JUCE-LICENSE.md','CI-TEST-RESULT.txt','Signalsmith-Stretch-LICENSE.txt','Signalsmith-Linear-LICENSE.txt')){if(!$names.Contains($required)){throw "Required package entry missing: $required"}}
if($manifest.ara -and !$names.Contains('mrs_ara_scan.exe')){throw 'ARA helper missing'}
foreach($application in @('Moon River Studio JUCE.exe','MR Live.exe')){if((Get-Item -LiteralPath (Join-Path $root $application)).VersionInfo.FileVersion -ne '0.8.4'){throw "PE version mismatch: $application"}}
[pscustomobject]@{source=$manifest.source;version=$manifest.version;files=$names.Count;tests=$manifest.tests;ara=$manifest.ara;run=$manifest.run;integrity='PASS'}
