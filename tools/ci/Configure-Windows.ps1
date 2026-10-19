param([string]$Build='build/windows',[switch]$Ara)
$ErrorActionPreference='Stop'
$sdk=(Resolve-Path -LiteralPath 'build/sdk').Path
$asio=(Resolve-Path -LiteralPath 'build/private-sdk/asio.zip').Path
$args=@('-S','.', '-B',$Build,'-G','Visual Studio 17 2022','-A','x64','-DBUILD_TESTING=ON','-DMRS_BUILD_ASIO=ON','-DMRS_BUILD_DESKTOP=ON','-DMRS_BUILD_JUCE_UI=ON','-DMRS_BUILD_VST3=ON',"-DMRS_JUCE_SDK_ROOT=$sdk/juce","-DMRS_VST3_SDK_ROOT=$sdk/vst3","-DMRS_ASIO_SDK_ZIP=$asio","-DFETCHCONTENT_SOURCE_DIR_PORTAUDIO=$sdk/portaudio",'-DFETCHCONTENT_FULLY_DISCONNECTED=ON',"-DMRS_BUILD_ARA=$(if($Ara){'ON'}else{'OFF'})")
if($Ara){
 & cmake -S tools/ara-render-fixture -B build/ara-render -G 'Visual Studio 17 2022' -A x64 "-DMRS_ARA_SDK_ROOT=$sdk/ara" "-DARA_VST3_SDK_DIR=$sdk/vst3" '-DSMTG_ENABLE_VSTGUI_SUPPORT=OFF' '-DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF' '-DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF' '-DSMTG_CREATE_PLUGIN_LINK=OFF'
 if($LASTEXITCODE){throw 'ARA fixture configure failed'}
 & cmake --build build/ara-render --config Release --target MRSAraRenderFixture --parallel 2
 if($LASTEXITCODE){throw 'ARA fixture build failed'}
 $fixture=(Resolve-Path -LiteralPath 'build/ara-render/Release/MRSAraRenderFixture.vst3').Path
 $args+=@("-DMRS_ARA_SDK_ROOT=$sdk/ara","-DMRS_ARA_RENDER_TEST_PLUGIN=$fixture","-DMRS_ARA_TEST_PLUGIN=$fixture")
}
& cmake @args;if($LASTEXITCODE){throw 'Windows configure failed'}
