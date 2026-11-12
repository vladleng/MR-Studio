$ErrorActionPreference='Stop'
# Exercise the real artifact verifier, with integrity-valid manifests containing
# one removed native resource. No application or model files are needed.
$root=Join-Path ([IO.Path]::GetTempPath()) ('mrs-removed-native-'+[guid]::NewGuid())
try{
 foreach($name in @('ace-midi.exe','ace-midi-avx2.exe','ace-midi-NOTICES.txt')){
  $folder=Join-Path $root ([guid]::NewGuid())
  New-Item -ItemType Directory -Path $folder -Force | Out-Null
  $file=Join-Path $folder $name
  [IO.File]::WriteAllBytes($file,[byte[]](1,2,3))
  $manifest=@{application='MR Studio';version='0.10.2-fix4';release_label='0.10.2-fix4';versions=@{studio='0.10.2-fix4';live='0.8.13-fix1'};files=@(@{name=$name;bytes=3;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash})}
  # The verifier checks Studio resources before rejecting removed native files.
  foreach($resource in @('mr_transcription_worker.py','mr_transcription_requirements.txt','mr_separation_worker.py','mr_separation_requirements.txt','mr_separation_profiles.json','mr_separation_scnet.py','SCNet-MIT-LICENSE.txt')){
   $entry=Join-Path $folder $resource;[IO.File]::WriteAllBytes($entry,[byte[]](4,5,6))
   $manifest.files+=@{name=$resource;bytes=3;sha256=(Get-FileHash -LiteralPath $entry -Algorithm SHA256).Hash}
  }
  $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $folder 'SHA256.json')
  $rejected=$false
  try{ & (Join-Path $PSScriptRoot 'Verify-Package.ps1') -Package $folder }
  catch{if($_.Exception.Message -like 'Removed HOT-Step runtime must not be packaged:*'){$rejected=$true}else{throw}}
  if(!$rejected){throw "Removed resource accepted: $name"}
 }
 Write-Host 'PASS package rejects all removed native resources'
}finally{if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
