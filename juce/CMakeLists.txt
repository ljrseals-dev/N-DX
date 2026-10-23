cmake_minimum_required(VERSION 3.22)
project(NDXJuce VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(NDX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NDX_FORMATS AU)
endif()

juce_add_plugin(NDXJuce
    COMPANY_NAME "Lewis Seals"
    PLUGIN_MANUFACTURER_CODE Lsls
    PLUGIN_CODE Ndx1
    FORMATS ${NDX_FORMATS}
    PRODUCT_NAME "N-DX"
    BUNDLE_ID com.lewisseals.ndx
    VST3_CATEGORIES Fx Dynamics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(NDXJuce PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# The engine, parameter table and presets are shared with the Steinberg-SDK build.
target_include_directories(NDXJuce PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_compile_definitions(NDXJuce PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(NDXJuce
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
