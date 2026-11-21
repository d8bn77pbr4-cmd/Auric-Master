cmake_minimum_required(VERSION 3.22)
project(AuricMaster2 VERSION 2.0.0)

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
    set(AM2_FORMATS VST3 AU)
else()
    set(AM2_FORMATS VST3)
endif()

set(AM2_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LoudnessMeter.cpp
    Source/Limiter.cpp
    Source/ResonanceSuppressor.cpp
    Source/AutoMatcher.cpp)

juce_add_plugin(AuricMaster2
    COMPANY_NAME "AuricAudio"
    PLUGIN_MANUFACTURER_CODE Auri
    PLUGIN_CODE AuM2
    FORMATS ${AM2_FORMATS}
    PRODUCT_NAME "Auric Master 2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(AuricMaster2 PRIVATE ${AM2_SOURCES})
target_compile_definitions(AuricMaster2 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(AuricMaster2
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(AM2_BUILD_TESTS "Build the offline test runner" OFF)
if(AM2_BUILD_TESTS)
    juce_add_console_app(AM2Tests PRODUCT_NAME "AM2Tests")
    target_sources(AM2Tests PRIVATE tests/TestMain.cpp ${AM2_SOURCES})
    target_compile_definitions(AM2Tests PRIVATE JucePlugin_Name="Auric Master 2" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(AM2Tests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
