param([string]$Source='build/native-ace-source',[string]$Build='build/native-ace')
$ErrorActionPreference='Stop'
$repo='https://github.com/timoncool/HOT-Step-CPP.git'
$commit='ee7286d14232b48ace3b6eed0fcf3048081ef0d1'
$ggml='0e23c505274225c09ba737519e02e6002ad8f3f8'
function Git-Checked([string[]]$Arguments){ & git @Arguments; if($LASTEXITCODE){throw 'ace-midi source operation failed'} }
if(!(Test-Path -LiteralPath "$Source/.git")){Git-Checked @('clone','--no-checkout','--filter=blob:none',$repo,$Source)}
$origin=& git -C $Source remote get-url origin;if($LASTEXITCODE -or $origin -ne $repo){throw 'Unexpected ace-midi origin'}
Git-Checked @('-C',$Source,'fetch','--depth','1','origin',$commit)
Git-Checked @('-C',$Source,'checkout','--detach',$commit)
Git-Checked @('-C',$Source,'submodule','update','--init','--depth','1','engine/ggml','engine/vendor/vst3sdk')
Git-Checked @('-C',"$Source/engine/vendor/vst3sdk",'submodule','update','--init','--depth','1','base','pluginterfaces','public.sdk')
if((& git -C $Source rev-parse HEAD) -ne $commit -or (& git -C "$Source/engine/ggml" rev-parse HEAD) -ne $ggml){throw 'ace-midi pin mismatch'}
Git-Checked @('-C',$Source,'diff','--exit-code','HEAD')
if(!(Get-Content -Raw core/include/mrs/audio_to_midi.hpp).Contains($commit)){throw 'Native pin differs from adapter'}
# Generic x64 CPU baseline, static CRT/ggml. No OpenMP/CUDA/Vulkan/runtime DLLs.
& cmake -S "$Source/engine" -B $Build -G 'Visual Studio 17 2022' -A x64 '-DHOT_STEP_STATIC_RUNTIME=ON' '-DBUILD_SHARED_LIBS=OFF' '-DGGML_STATIC=ON' '-DGGML_BACKEND_DL=OFF' '-DGGML_OPENMP=OFF' '-DGGML_CUDA=OFF' '-DGGML_VULKAN=OFF' '-DGGML_METAL=OFF' '-DGGML_NATIVE=OFF' '-DGGML_SSE42=OFF' '-DGGML_AVX=OFF' '-DGGML_AVX2=OFF' '-DGGML_BMI2=OFF' '-DGGML_F16C=OFF' '-DGGML_FMA=OFF'
if($LASTEXITCODE){throw 'ace-midi configure failed'}
& cmake --build $Build --config Release --target ace-midi --parallel 2
if($LASTEXITCODE){throw 'ace-midi build failed'}
$exe=Join-Path $Build 'Release/ace-midi.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Missing native executable'}
$usage=Start-Process -FilePath (Resolve-Path $exe).Path -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $Build 'usage.txt')
if($usage.ExitCode -ne 2){throw 'ace-midi invocation contract failed'}
$missing=Start-Process -FilePath (Resolve-Path $exe).Path -ArgumentList @('--model','__mrs_missing_model__','--jsonl') -Wait -PassThru -NoNewWindow -RedirectStandardError (Join-Path $Build 'missing-model.txt')
if($missing.ExitCode -ne 1){throw 'ace-midi missing-model contract failed'}
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
$dumpbin=Get-ChildItem -Path "$vs/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" | Sort-Object FullName -Descending | Select-Object -First 1
if(!$dumpbin){throw 'Cannot inspect native dependencies'}
$deps=& $dumpbin.FullName /DEPENDENTS $exe
if($LASTEXITCODE){throw 'Cannot inspect native dependencies'}
$imports=@($deps | ForEach-Object {if($_ -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$'){$Matches[1]}})
foreach($dll in $imports){if($dll -notmatch '^(KERNEL32|ADVAPI32|USER32|SHELL32|OLE32|WS2_32|BCRYPT|NTDLL|PSAPI|VERSION|api-ms-win-[A-Za-z0-9_.-]+|ext-ms-[A-Za-z0-9_.-]+)\.dll$'){throw "Non-system ace-midi dependency: $dll"}}
if(!$imports.Count){throw 'No native imports found'}
$notice=@("HOT-Step CPP $repo @ $commit", "GGML https://github.com/scragnog/HOT-ggml @ $ggml",'ace-midi CPU x64; static CRT/ggml, OpenMP/GPU backends OFF.',"Imports: $($imports -join ', ')",'', (Get-Content -Raw "$Source/LICENSE"),(Get-Content -Raw "$Source/engine/LICENSE"),(Get-Content -Raw "$Source/engine/ggml/LICENSE"))
$minimp3=Get-Content -Raw "$Source/engine/vendor/minimp3/minimp3.h"
$notice+=([regex]::Match($minimp3,'/\*([\s\S]*?)\*/').Groups[1].Value)
$notice | Set-Content -LiteralPath (Join-Path $Build 'ace-midi-NOTICES.txt') -Encoding utf8
Write-Host "Native ace-midi SHA256: $((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant())"
