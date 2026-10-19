cmake_minimum_required(VERSION 3.22)
project(MRSAraRenderFixture VERSION 1.0.0 LANGUAGES C CXX)
set(MRS_ARA_SDK_ROOT "" CACHE PATH "Local ARA SDK releases/2.3.0")
set(ARA_SETUP_DEBUGGING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MRS_ARA_SDK_ROOT}/ARA_Examples" examples EXCLUDE_FROM_ALL)
add_library(MRSAraRenderFixture SHARED fixture.cpp
  "${MRS_ARA_SDK_ROOT}/ARA_Examples/TestPlugIn/VST3/TestVST3Processor.cpp")
target_include_directories(MRSAraRenderFixture PRIVATE
  "${MRS_ARA_SDK_ROOT}/ARA_Examples/TestPlugIn/VST3"
  "${MRS_ARA_SDK_ROOT}/ARA_Examples/TestPlugIn")
target_compile_features(MRSAraRenderFixture PRIVATE cxx_std_20)
target_compile_definitions(MRSAraRenderFixture PRIVATE NOMINMAX=1 ARA_MAJOR_VERSION=2 ARA_MINOR_VERSION=3 ARA_PATCH_VERSION=0)
target_link_libraries(MRSAraRenderFixture PRIVATE ARATestPlugInCommon sdk)
set_target_properties(MRSAraRenderFixture PROPERTIES PREFIX "" SUFFIX ".vst3")
ara_add_vst3_main(MRSAraRenderFixture)
