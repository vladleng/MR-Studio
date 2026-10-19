param([string]$Build='build/windows',[switch]$Ara)
$ErrorActionPreference='Stop'
$sdk=(Resolve-Path -LiteralPath 'build/sdk').Path
$asio=(Resolve-Path -LiteralPath 'build/private-sdk/asio.zip').Path
$args=@('-S','.', '-B',$Build,'-G','Visual Studio 17 2022','-A','x64','-DBUILD_TESTING=ON','-DMRS_BUILD_ASIO=ON','-DMRS_BUILD_DESKTOP=ON','-DMRS_BUILD_JUCE_UI=ON','-DMRS_BUILD_VST3=ON',"-DMRS_JUCE_SDK_ROOT=$sdk/juce","-DMRS_VST3_SDK_ROOT=$sdk/vst3","-DMRS_ASIO_SDK_ZIP=$asio","-DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$sdk/portaudio",'-DFETCHCONTENT_FULLY_DISCONNECTED=ON',"-DMRS_BUILD_ARA=$(if($Ara){'ON'}else{'OFF'})")
if($Ara){$args+="-DMRS_ARA_SDK_ROOT=$sdk/ara"}
& cmake @args;if($LASTEXITCODE){throw 'Windows configure failed'}
