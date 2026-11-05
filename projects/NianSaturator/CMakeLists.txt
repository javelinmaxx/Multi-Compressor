cmake_minimum_required(VERSION 3.22)
include("${CMAKE_CURRENT_LIST_DIR}/../PluginVersion.cmake")
project(NianSaturator VERSION ${J_SONIC_PLUGIN_VERSION} LANGUAGES C CXX)
set(JUCE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "JUCE checkout")
add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
set(plugin_formats VST3 Standalone)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()
juce_add_plugin(NianSaturator
    COMPANY_NAME "J-SONIC"
    BUNDLE_ID "audio.jsonic.niansaturator"
    PLUGIN_MANUFACTURER_CODE Chra
    PLUGIN_CODE NiS1
    FORMATS ${plugin_formats}
    PRODUCT_NAME "Nian Saturator"
    DESCRIPTION "Two-band adaptive saturator"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE
    VST3_CATEGORIES Fx Distortion)
target_sources(NianSaturator PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp ../StagePluginStyle.cpp)
target_compile_features(NianSaturator PUBLIC cxx_std_17)
target_compile_definitions(NianSaturator PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NianSaturator PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
option(NIAN_SATURATOR_BUILD_TESTS "Build Nian Saturator tests" ON)
if(NIAN_SATURATOR_BUILD_TESTS)
    enable_testing()
    add_executable(NianSaturatorTests Tests/ProcessorTests.cpp)
    target_include_directories(NianSaturatorTests PRIVATE "${JUCE_SOURCE_DIR}/modules")
    target_compile_definitions(NianSaturatorTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_link_libraries(NianSaturatorTests PRIVATE NianSaturator)
    add_test(NAME NianSaturatorProcessor COMMAND NianSaturatorTests)
    include("${CMAKE_CURRENT_LIST_DIR}/../PluginGpuSmoke.cmake")
    add_plugin_gpu_smoke(NianSaturator)
endif()
