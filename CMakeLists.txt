cmake_minimum_required(VERSION 3.22)
project(Doblecupp VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# AU solo existe en macOS; en Windows se compila únicamente VST3
if(APPLE)
    set(DOBLE_FORMATS VST3 AU)
else()
    set(DOBLE_FORMATS VST3)
endif()

juce_add_plugin(Doblecupp
    COMPANY_NAME "2far"
    PLUGIN_MANUFACTURER_CODE Twfr
    PLUGIN_CODE Dbcp
    FORMATS ${DOBLE_FORMATS}
    PRODUCT_NAME "2far doblecupp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(DoblecuppAssets SOURCES Assets/character.png)
juce_generate_juce_header(Doblecupp)

target_sources(Doblecupp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Doblecupp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Doblecupp
    PRIVATE DoblecuppAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
