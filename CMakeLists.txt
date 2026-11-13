cmake_minimum_required(VERSION 3.22)
# The release workflow rewrites the version on the next line from the tag, so menu, installer and release agree.
project(Apurrr2500 VERSION 0.1.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Core tests (no JUCE needed): ctest, or sh tests/run.sh
option(APURR_BUILD_TESTS "Build the core tests" OFF)
if(APURR_BUILD_TESTS)
    add_executable(apurr_core_tests tests/test_core.cpp)
    target_include_directories(apurr_core_tests PRIVATE src)
    enable_testing()
    add_test(NAME core COMMAND apurr_core_tests)
endif()

option(APURR_BUILD_PLUGIN "Build the VST3 plugin" ON)
if(APURR_BUILD_PLUGIN)
    # Pass -DAPURR_JUCE_DIR=/path/to/JUCE to use a local JUCE, otherwise JUCE is fetched.
    if(APURR_JUCE_DIR)
        add_subdirectory(${APURR_JUCE_DIR} juce)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    juce_add_binary_data(ApurrAssets SOURCES
        assets/fonts/BarlowCondensed-Medium.ttf
        assets/fonts/BarlowCondensed-SemiBold.ttf
        assets/fonts/BarlowCondensed-Bold.ttf
        assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf
        assets/fonts/JetBrainsMono-Regular.ttf)
    set_target_properties(ApurrAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(Apurrr2500
        COMPANY_NAME "Studio 87"
        PLUGIN_MANUFACTURER_CODE Stu8
        PLUGIN_CODE Apur
        FORMATS VST3
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx Dynamics
        COPY_PLUGIN_AFTER_BUILD FALSE
        PRODUCT_NAME "Apurrr2500")

    set(APURR_SOURCES
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginProcessor.h
        src/core/Apurrr2500.h
        src/editor/PluginEditor.cpp
        src/editor/PluginEditor.h
        src/editor/Presets.cpp
        src/editor/Presets.h
        src/editor/Dialogs.cpp
        src/editor/Dialogs.h
        src/editor/Theme.h
        src/editor/Layout.h
        src/editor/Widgets.h
        src/editor/Views.h
        src/editor/RenderGate.h)
    target_sources(Apurrr2500 PRIVATE ${APURR_SOURCES})

    target_compile_definitions(Apurrr2500 PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

    target_link_libraries(Apurrr2500
        PRIVATE ApurrAssets juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # Offscreen renderer of the real editor, plus self checks (not part of releases).
    option(APURR_BUILD_SNAPSHOT "Build the editor snapshot tool" OFF)
    if(APURR_BUILD_SNAPSHOT)
        juce_add_console_app(ApurrSnapshot PRODUCT_NAME "ApurrSnapshot")
        target_sources(ApurrSnapshot PRIVATE tools/snapshot.cpp ${APURR_SOURCES})
        target_compile_definitions(ApurrSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Apurrr2500" JucePlugin_VersionString="${PROJECT_VERSION}")
        target_link_libraries(ApurrSnapshot PRIVATE ApurrAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
    endif()
endif()
