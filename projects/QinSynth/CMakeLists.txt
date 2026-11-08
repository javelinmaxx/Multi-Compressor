cmake_minimum_required(VERSION 3.22)
include("${CMAKE_CURRENT_LIST_DIR}/../PluginVersion.cmake")
project(QinSynth VERSION ${J_SONIC_PLUGIN_VERSION} LANGUAGES C CXX)
set(JUCE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "JUCE checkout")
add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
set(plugin_formats VST3 Standalone)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()
juce_add_plugin(QinSynth
    COMPANY_NAME "J-SONIC"
    BUNDLE_ID "audio.jsonic.qinsynth"
    PLUGIN_MANUFACTURER_CODE Chra
    PLUGIN_CODE QnS1
    FORMATS ${plugin_formats}
    PRODUCT_NAME "Qin Synth"
    DESCRIPTION "Sixteen-voice virtual-analog instrument with Zao drive and MIDI-steered Lan delay-reverb"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE
    VST3_CATEGORIES Instrument Synth)
target_sources(QinSynth PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp
    ../LanReverb/Source/ReverbEngine.cpp ../StagePluginStyle.cpp)
target_compile_features(QinSynth PUBLIC cxx_std_17)
target_compile_definitions(QinSynth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(QinSynth PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
option(QIN_SYNTH_BUILD_TESTS "Build Qin Synth tests" ON)
if(QIN_SYNTH_BUILD_TESTS)
    enable_testing()
    add_executable(QinSynthTests Tests/ProcessorTests.cpp)
    target_include_directories(QinSynthTests PRIVATE "${JUCE_SOURCE_DIR}/modules")
    target_compile_definitions(QinSynthTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_link_libraries(QinSynthTests PRIVATE QinSynth)
    add_test(NAME QinSynthProcessor COMMAND QinSynthTests)
endif()
