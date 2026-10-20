param([string]$Root='build/sdk', [switch]$Ara)
$ErrorActionPreference='Stop'
$lock=Get-Content -Raw -LiteralPath "$PSScriptRoot/sdk-lock.json" | ConvertFrom-Json
New-Item -ItemType Directory -Path $Root -Force | Out-Null
$Root=(Resolve-Path -LiteralPath $Root).Path
function Git-Checked([string[]]$Arguments){ & git @Arguments; if($LASTEXITCODE){throw "Git SDK operation failed"} }
$names=@('juce','vst3','portaudio');if($Ara){$names+='ara'}
foreach($name in $names){
 $sdk=$lock.$name;$path=Join-Path $Root $name
 if(!(Test-Path -LiteralPath "$path/.git")){Git-Checked @('clone','--no-checkout','--filter=blob:none',$sdk.url,$path)}
 $remote=& git -C $path remote get-url origin;if($LASTEXITCODE -or $remote -ne $sdk.url){throw "Unexpected SDK origin: $name"}
 Git-Checked @('-C',$path,'fetch','--depth','1','origin',$sdk.commit)
 Git-Checked @('-C',$path,'checkout','--detach',$sdk.commit)
 if($name -eq 'vst3'){Git-Checked @('-C',$path,'submodule','update','--init','--depth','1','base','cmake','pluginterfaces','public.sdk')}
 if($name -eq 'ara'){Git-Checked @('-C',$path,'submodule','update','--init','--recursive','--depth','1')}
 $sha=& git -C $path rev-parse HEAD;if($sha -ne $sdk.commit){throw "SDK pin mismatch: $name"}
 Git-Checked @('-C',$path,'diff','--exit-code','HEAD')
 Git-Checked @('-C',$path,'fsck','--no-dangling')
 $extra=& git -C $path status --porcelain --untracked-files=all
 if($extra){throw "SDK cache contains modified or untracked content: $name"}
 $modules=& git -C $path submodule status --recursive
 if($LASTEXITCODE){throw "Cannot validate SDK submodules: $name"}
 if(@($modules | Where-Object {$_ -match '^[+U]'}).Count){throw "Modified SDK submodule: $name"}
}
# Licensed ASIO archive is deliberately outside the public-source cache root.
$asio=Join-Path (Split-Path -Parent $Root) 'private-sdk/asio.zip'
New-Item -ItemType Directory -Path (Split-Path -Parent $asio) -Force | Out-Null
Invoke-WebRequest -Uri $lock.asio.url -OutFile $asio
if((Get-FileHash -LiteralPath $asio -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.asio.sha256){throw 'ASIO SDK checksum changed; explicit version/license review required'}
Write-Output 'SDK pins and ASIO integrity verified. No SDK files are packaged.'
