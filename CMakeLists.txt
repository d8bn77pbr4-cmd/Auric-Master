cmake_minimum_required(VERSION 3.22)
project(AuricMaster VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(AURIC_FORMATS VST3 AU)
else()
    set(AURIC_FORMATS VST3)
endif()

juce_add_plugin(AuricMaster
    COMPANY_NAME "AuricAudio"
    PLUGIN_MANUFACTURER_CODE Auri
    PLUGIN_CODE AuMs
    FORMATS ${AURIC_FORMATS}
    PRODUCT_NAME "Auric Master"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(AuricMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ResonanceSuppressor.cpp
    Source/AutoMatcher.cpp)

target_compile_definitions(AuricMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AuricMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(AURIC_BUILD_TESTS "Build the offline test runner" OFF)
if(AURIC_BUILD_TESTS)
    juce_add_console_app(AuricTests PRODUCT_NAME "AuricTests")
    target_sources(AuricTests PRIVATE tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/ResonanceSuppressor.cpp Source/AutoMatcher.cpp)
    target_compile_definitions(AuricTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Auric Master" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AuricTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_basics)
endif()
