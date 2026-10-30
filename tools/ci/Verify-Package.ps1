param([Parameter(Mandatory)][string]$Package,[string]$ExpectedSource,[string]$ExpectedVersion)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Package).Path
$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'SHA256.json') | ConvertFrom-Json
if($ExpectedSource -and $manifest.source -ne $ExpectedSource){throw 'Artifact source mismatch'}
$versions=$manifest.versions
if(!$versions -or $versions.studio -notmatch '^0\.[0-9]+\.[0-9]+(?:-(?:fix|Upd)[0-9]+)?
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
if($manifest.application -notin @('MR Live','MR Studio + MR Live')){throw 'Unknown package application'}
if($manifest.application -eq 'MR Live' -and $manifest.ara){throw 'MR Live-only package must be ARA-free'}
[string[]]$applications=if($manifest.application -eq 'MR Live'){@('MR Live.exe')}else{@('Moon River Studio JUCE.exe','MR Live.exe')}
if($manifest.application -eq 'MR Live' -and $names.Contains('Moon River Studio JUCE.exe')){throw 'Studio executable is not part of a Live-only package'}
foreach($required in ($applications + @('mrs_vst3_scan.exe','mrs_crash_reporter.exe','ASIO-SDK-LICENSE.txt','JUCE-LICENSE.md','CI-TEST-RESULT.txt','Signalsmith-Stretch-LICENSE.txt','Signalsmith-Linear-LICENSE.txt'))){if(!$names.Contains($required)){throw "Required package entry missing: $required"}}
$araFiles=@('mrs_ara_scan.exe','ARA-API-LICENSE.txt','ARA-Library-LICENSE.txt')
foreach($name in $araFiles){
 if($manifest.ara -and !$names.Contains($name)){throw "ARA runtime/notice missing: $name"}
 if(!$manifest.ara -and $names.Contains($name)){throw "ARA content forbidden in ARA-free package: $name"}
}
foreach($application in $applications){
 $expectedPe=if($application -eq 'MR Live.exe'){($versions.live -split '-')[0]}else{($versions.studio -split '-')[0]}
 if((Get-Item -LiteralPath (Join-Path $root $application)).VersionInfo.FileVersion -ne $expectedPe){throw "PE version mismatch: $application"}
}
[pscustomobject]@{source=$manifest.source;version=$manifest.version;files=$names.Count;tests=$manifest.tests;ara=$manifest.ara;run=$manifest.run;integrity='PASS'}
 -or
   $versions.live -notmatch '^0\.[0-9]+\.[0-9]+(?:-(?:fix|Upd)[0-9]+)?
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
if($manifest.application -notin @('MR Live','MR Studio + MR Live')){throw 'Unknown package application'}
if($manifest.application -eq 'MR Live' -and $manifest.ara){throw 'MR Live-only package must be ARA-free'}
[string[]]$applications=if($manifest.application -eq 'MR Live'){@('MR Live.exe')}else{@('Moon River Studio JUCE.exe','MR Live.exe')}
if($manifest.application -eq 'MR Live' -and $names.Contains('Moon River Studio JUCE.exe')){throw 'Studio executable is not part of a Live-only package'}
foreach($required in ($applications + @('mrs_vst3_scan.exe','mrs_crash_reporter.exe','ASIO-SDK-LICENSE.txt','JUCE-LICENSE.md','CI-TEST-RESULT.txt','Signalsmith-Stretch-LICENSE.txt','Signalsmith-Linear-LICENSE.txt'))){if(!$names.Contains($required)){throw "Required package entry missing: $required"}}
$araFiles=@('mrs_ara_scan.exe','ARA-API-LICENSE.txt','ARA-Library-LICENSE.txt')
foreach($name in $araFiles){
 if($manifest.ara -and !$names.Contains($name)){throw "ARA runtime/notice missing: $name"}
 if(!$manifest.ara -and $names.Contains($name)){throw "ARA content forbidden in ARA-free package: $name"}
}
foreach($application in $applications){
 $expectedPe=if($application -eq 'MR Live.exe'){'0.8.12'}else{'0.8.10'}
 if((Get-Item -LiteralPath (Join-Path $root $application)).VersionInfo.FileVersion -ne $expectedPe){throw "PE version mismatch: $application"}
}
[pscustomobject]@{source=$manifest.source;version=$manifest.version;files=$names.Count;tests=$manifest.tests;ara=$manifest.ara;run=$manifest.run;integrity='PASS'}
){
 throw 'Missing or invalid component stage versions'
}
$expectedVersion=if($manifest.application -eq 'MR Live'){$versions.live}else{$versions.studio}
if($manifest.version -ne $expectedVersion -or $manifest.release_label -ne $expectedVersion){throw 'Artifact version mismatch'}
if($ExpectedVersion -and $manifest.version -ne $ExpectedVersion){throw 'Artifact version differs from Actions run'}
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
if($manifest.application -notin @('MR Live','MR Studio + MR Live')){throw 'Unknown package application'}
if($manifest.application -eq 'MR Live' -and $manifest.ara){throw 'MR Live-only package must be ARA-free'}
[string[]]$applications=if($manifest.application -eq 'MR Live'){@('MR Live.exe')}else{@('Moon River Studio JUCE.exe','MR Live.exe')}
if($manifest.application -eq 'MR Live' -and $names.Contains('Moon River Studio JUCE.exe')){throw 'Studio executable is not part of a Live-only package'}
foreach($required in ($applications + @('mrs_vst3_scan.exe','mrs_crash_reporter.exe','ASIO-SDK-LICENSE.txt','JUCE-LICENSE.md','CI-TEST-RESULT.txt','Signalsmith-Stretch-LICENSE.txt','Signalsmith-Linear-LICENSE.txt'))){if(!$names.Contains($required)){throw "Required package entry missing: $required"}}
$araFiles=@('mrs_ara_scan.exe','ARA-API-LICENSE.txt','ARA-Library-LICENSE.txt')
foreach($name in $araFiles){
 if($manifest.ara -and !$names.Contains($name)){throw "ARA runtime/notice missing: $name"}
 if(!$manifest.ara -and $names.Contains($name)){throw "ARA content forbidden in ARA-free package: $name"}
}
foreach($application in $applications){
 $expectedPe=if($application -eq 'MR Live.exe'){'0.8.12'}else{'0.8.10'}
 if((Get-Item -LiteralPath (Join-Path $root $application)).VersionInfo.FileVersion -ne $expectedPe){throw "PE version mismatch: $application"}
}
[pscustomobject]@{source=$manifest.source;version=$manifest.version;files=$names.Count;tests=$manifest.tests;ara=$manifest.ara;run=$manifest.run;integrity='PASS'}
