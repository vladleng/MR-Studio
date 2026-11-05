param([string]$Source='build/native-ace-source',[string]$Build='build/native-ace')
$ErrorActionPreference='Stop'
$repo='https://github.com/timoncool/HOT-Step-CPP.git'
$commit='ee7286d14232b48ace3b6eed0fcf3048081ef0d1'
$ggml='0e23c505274225c09ba737519e02e6002ad8f3f8'
function Git-Checked([string[]]$Arguments){ & git @Arguments; if($LASTEXITCODE){throw 'ace-midi source operation failed'} }
if(!(Test-Path -LiteralPath "$Source/.git")){Git-Checked @('clone','--no-checkout','--filter=blob:none',$repo,$Source)}
Git-Checked @('-C',$Source,'config','core.autocrlf','false')
$origin=& git -C $Source remote get-url origin;if($LASTEXITCODE -or $origin -ne $repo){throw 'Unexpected ace-midi origin'}
Git-Checked @('-C',$Source,'fetch','--depth','1','origin',$commit)
Git-Checked @('-C',$Source,'checkout','--detach',$commit)
Git-Checked @('-C',$Source,'submodule','update','--init','--depth','1','engine/ggml','engine/vendor/vst3sdk')
Git-Checked @('-C',"$Source/engine/vendor/vst3sdk",'submodule','update','--init','--depth','1','base','pluginterfaces','public.sdk')
if((& git -C $Source rev-parse HEAD) -ne $commit -or (& git -C "$Source/engine/ggml" rev-parse HEAD) -ne $ggml){throw 'ace-midi pin mismatch'}
$patchPin=Get-Content -Raw 'tools/native/ace-midi-stem.json' | ConvertFrom-Json
if($patchPin.base_commit -ne $commit){throw 'Native patch base mismatch'}
$nativeSource=Join-Path $Source 'engine/tools/ace-midi.cpp'
if((Get-FileHash -LiteralPath $nativeSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $patchPin.source_sha256){
 Git-Checked @('-C',$Source,'diff','--exit-code','HEAD')
 New-Item -ItemType Directory -Force -Path $Build | Out-Null
 $patchInput=Join-Path $Build 'ace-midi-stem.patch'
 $patchText=(Get-Content -Raw 'tools/native/ace-midi-stem.patch').Replace("`r`n","`n")
 [IO.File]::WriteAllText((Join-Path (Get-Location) $patchInput),$patchText,[Text.UTF8Encoding]::new($false))
 if((Get-FileHash -LiteralPath $patchInput -Algorithm SHA256).Hash.ToLowerInvariant() -ne $patchPin.patch_sha256){throw 'Native patch hash mismatch'}
 Git-Checked @('-C',$Source,'apply','--check',(Resolve-Path $patchInput).Path)
 Git-Checked @('-C',$Source,'apply',(Resolve-Path $patchInput).Path)
}
if((Get-FileHash -LiteralPath $nativeSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $patchPin.source_sha256){throw 'Patched native source mismatch'}
$changed=@(& git -C $Source diff --name-only HEAD)
if($changed.Count -ne 1 -or $changed[0] -ne 'engine/tools/ace-midi.cpp'){throw 'Unexpected native source changes'}
if(!(Get-Content -Raw core/include/mrs/audio_to_midi.hpp).Contains($commit)){throw 'Native pin differs from adapter'}
# Both engines use the same source+patch and static CRT/ggml; no OpenMP/GPU DLLs.
function Build-Native([string]$VariantBuild,[bool]$Optimized){
 $cpu=if($Optimized){'ON'}else{'OFF'}
 & cmake -S "$Source/engine" -B $VariantBuild -G 'Visual Studio 17 2022' -A x64 '-DHOT_STEP_STATIC_RUNTIME=ON' '-DBUILD_SHARED_LIBS=OFF' '-DGGML_STATIC=ON' '-DGGML_BACKEND_DL=OFF' '-DGGML_OPENMP=OFF' '-DGGML_CUDA=OFF' '-DGGML_VULKAN=OFF' '-DGGML_METAL=OFF' '-DGGML_NATIVE=OFF' '-DGGML_AVX512=OFF' "-DGGML_SSE42=$cpu" "-DGGML_AVX=$cpu" "-DGGML_AVX2=$cpu" "-DGGML_BMI2=$cpu" "-DGGML_F16C=$cpu" "-DGGML_FMA=$cpu"
 if($LASTEXITCODE){throw 'ace-midi configure failed'}
 & cmake --build $VariantBuild --config Release --target ace-midi --parallel 2
 if($LASTEXITCODE){throw 'ace-midi build failed'}
 $binary=Join-Path $VariantBuild 'Release/ace-midi.exe'
 if(!(Test-Path -LiteralPath $binary)){throw 'Missing native executable'}
 $self=Start-Process -FilePath (Resolve-Path $binary).Path -ArgumentList @("--mrs-self-test") -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $VariantBuild "stem-self-test.txt")
 if($self.ExitCode -ne 0){throw "ace-midi decoder self-test failed"}
 $usage=Start-Process -FilePath (Resolve-Path $binary).Path -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $VariantBuild 'usage.txt')
 if($usage.ExitCode -ne 2){throw 'ace-midi invocation contract failed'}
 $invalid=Start-Process -FilePath (Resolve-Path $binary).Path -ArgumentList @('--instrument','__invalid__') -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $VariantBuild 'invalid-instrument.txt')
 if($invalid.ExitCode -ne 2){throw 'ace-midi stem contract failed'}
 $missing=Start-Process -FilePath (Resolve-Path $binary).Path -ArgumentList @('--model','__mrs_missing_model__','--jsonl') -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $VariantBuild 'missing-model.txt')
 if($missing.ExitCode -ne 1){throw 'ace-midi missing-model contract failed'}
}
Build-Native $Build $false
Build-Native "$Build-avx2" $true
$exe=Join-Path $Build 'Release/ace-midi.exe'
Copy-Item -LiteralPath "$Build-avx2/Release/ace-midi.exe" -Destination (Join-Path $Build 'Release/ace-midi-avx2.exe')
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
$dumpbin=Get-ChildItem -Path "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" | Sort-Object FullName -Descending | Select-Object -First 1
if(!$dumpbin){throw 'Cannot inspect native dependencies'}
$deps=@(& $dumpbin.FullName /DEPENDENTS $exe; & $dumpbin.FullName /DEPENDENTS (Join-Path $Build 'Release/ace-midi-avx2.exe'))
if($LASTEXITCODE){throw 'Cannot inspect native dependencies'}
$imports=@($deps | ForEach-Object {if($_ -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$'){$Matches[1]}})
foreach($dll in $imports){if($dll -notmatch '^(KERNEL32|ADVAPI32|USER32|SHELL32|OLE32|WS2_32|BCRYPT|NTDLL|PSAPI|VERSION|api-ms-win-[A-Za-z0-9_.-]+|ext-ms-[A-Za-z0-9_.-]+)\.dll$'){throw "Non-system ace-midi dependency: $dll"}}
if(!$imports.Count){throw 'No native imports found'}
$notice=@("HOT-Step CPP $repo @ $commit", "GGML https://github.com/scragnog/HOT-ggml @ $ggml",'ace-midi CPU x64; generic SSE2 + AVX2/FMA/F16C/BMI2; static CRT/ggml, OpenMP/GPU OFF.',"MR Studio stem decoder patch SHA256: $($patchPin.patch_sha256)","Patch source: https://github.com/vladleng/MR-Studio/blob/$env:GITHUB_SHA/tools/native/ace-midi-stem.patch","Imports: $($imports -join ', ')",'', (Get-Content -Raw "$Source/LICENSE"),(Get-Content -Raw "$Source/engine/LICENSE"),(Get-Content -Raw "$Source/engine/ggml/LICENSE"))
$minimp3=Get-Content -Raw "$Source/engine/vendor/minimp3/minimp3.h"
$notice+=([regex]::Match($minimp3,'/\*([\s\S]*?)\*/').Groups[1].Value)
$notice | Set-Content -LiteralPath (Join-Path $Build 'ace-midi-NOTICES.txt') -Encoding utf8
Write-Host "Native ace-midi SHA256: $((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant())"

Write-Host "Native ace-midi AVX2 SHA256: $((Get-FileHash -LiteralPath (Join-Path $Build 'Release/ace-midi-avx2.exe') -Algorithm SHA256).Hash.ToLowerInvariant())"
