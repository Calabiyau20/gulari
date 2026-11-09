cmake_minimum_required(VERSION 3.22)

# -----------------------------------------------------------------------------
# Synthgularity - name, vendor and IDs live here (change them in one place).
# -----------------------------------------------------------------------------
set(PG_PRODUCT_NAME   "Synthgularity")
set(PG_COMPANY_NAME   "Calabi Yau")
set(PG_BUNDLE_ID      "com.calabiyau.synthgularity")
set(PG_MANUFACTURER   CbYu)   # 4 chars, at least one upper-case letter
set(PG_PLUGIN_CODE    Sygy)   # 4 chars, unique per plugin
set(PG_VERSION        "0.4.1")

# Must be set before project(): lowest macOS the plugin runs on
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)

project(Synthgularity VERSION ${PG_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

option(PG_BUILD_TESTS "Build the offline render test" OFF)

add_subdirectory(libs/JUCE)
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------------
# Mutable Instruments DSP (MIT licence, (c) Emilie Gillet) - Plaits, Rings, Clouds
# -----------------------------------------------------------------------------
set(MI libs/eurorack)
file(GLOB_RECURSE PLAITS_SRC ${MI}/plaits/dsp/*.cc)
set(MUTABLE_SRC
    ${PLAITS_SRC}
    ${MI}/plaits/resources.cc
    ${MI}/rings/dsp/part.cc
    ${MI}/rings/dsp/string.cc
    ${MI}/rings/dsp/resonator.cc
    ${MI}/rings/dsp/fm_voice.cc
    ${MI}/rings/dsp/string_synth_part.cc
    ${MI}/rings/resources.cc
    ${MI}/clouds/dsp/granular_processor.cc
    ${MI}/clouds/dsp/correlator.cc
    ${MI}/clouds/dsp/mu_law.cc
    ${MI}/clouds/dsp/pvoc/frame_transformation.cc
    ${MI}/clouds/dsp/pvoc/phase_vocoder.cc
    ${MI}/clouds/dsp/pvoc/stft.cc
    ${MI}/clouds/resources.cc
    ${MI}/stmlib/dsp/units.cc
    ${MI}/stmlib/dsp/atan.cc
    ${MI}/stmlib/utils/random.cc)
add_library(mutable STATIC ${MUTABLE_SRC})
target_include_directories(mutable PUBLIC ${MI})
target_compile_definitions(mutable PUBLIC TEST=1)
set_target_properties(mutable PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(mutable PRIVATE /w /FIcstdio /FIcmath -Wno-everything)
    target_compile_definitions(mutable PUBLIC _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(mutable PRIVATE -w -O2 -include cstdio)
endif()

# -----------------------------------------------------------------------------
# Plugin
# -----------------------------------------------------------------------------
set(PG_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PG_FORMATS AU)
endif()

juce_add_plugin(Synthgularity
    PRODUCT_NAME "${PG_PRODUCT_NAME}"
    COMPANY_NAME "${PG_COMPANY_NAME}"
    BUNDLE_ID "${PG_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE ${PG_MANUFACTURER}
    PLUGIN_CODE ${PG_PLUGIN_CODE}
    VERSION ${PG_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${PG_FORMATS})

clap_juce_extensions_plugin(TARGET Synthgularity
    CLAP_ID "${PG_BUNDLE_ID}"
    CLAP_FEATURES instrument synthesizer stereo)

# Brand assets: Syncopate + Montserrat, and the official logo if Resources/logo-calabi-yau.png exists
set(PG_ASSETS
    Resources/fonts/Syncopate-Bold.ttf
    Resources/fonts/Montserrat-SemiBold.ttf
    Resources/fonts/Montserrat-Bold.ttf
    Resources/brand/isologo.png
    Resources/brand/bg-logogrid.jpg
    Resources/brand/banner-arch.jpg
    Resources/brand/orb-torus.jpg
    Resources/brand/art-serpent.jpg
    Resources/brand/about-machine.jpg)
set(PG_HAS_LIBRARY 0)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/presets/FactoryLibrary.zip)
    list(APPEND PG_ASSETS Resources/presets/FactoryLibrary.zip)
    set(PG_HAS_LIBRARY 1)
endif()
set(PG_HAS_LOGO 0)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo-calabi-yau.png)
    list(APPEND PG_ASSETS Resources/logo-calabi-yau.png)
    set(PG_HAS_LOGO 1)
endif()
juce_add_binary_data(SynthgularityAssets SOURCES ${PG_ASSETS})
set_target_properties(SynthgularityAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE PG_SOURCES Source/*.cpp Source/*.h)
target_sources(Synthgularity PRIVATE ${PG_SOURCES})
target_include_directories(Synthgularity PRIVATE Source)

target_compile_definitions(Synthgularity PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    PG_PRODUCT_NAME="${PG_PRODUCT_NAME}"
    PG_COMPANY_NAME="${PG_COMPANY_NAME}"
    PG_HAS_LOGO=${PG_HAS_LOGO} PG_HAS_LIBRARY=${PG_HAS_LIBRARY})

target_link_libraries(Synthgularity
    PRIVATE
        mutable
        SynthgularityAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
# Offline render test (Linux/Mac console app, no GUI) - renders a WAV
# -----------------------------------------------------------------------------
if(PG_BUILD_TESTS)
    juce_add_console_app(SynthgularityRenderTest PRODUCT_NAME "SynthgularityRenderTest")
    file(GLOB_RECURSE PG_DSP_SOURCES Source/dsp/*.cpp Source/dsp/*.h Source/PluginProcessor.cpp Source/Params.cpp)
    target_sources(SynthgularityRenderTest PRIVATE tests/RenderTest.cpp ${PG_DSP_SOURCES})
    target_include_directories(SynthgularityRenderTest PRIVATE Source)
    target_compile_definitions(SynthgularityRenderTest PRIVATE
        JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PG_HEADLESS=1
        PG_PRODUCT_NAME="${PG_PRODUCT_NAME}" PG_COMPANY_NAME="${PG_COMPANY_NAME}")
    target_link_libraries(SynthgularityRenderTest PRIVATE mutable juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(SynthgularityPresetGen PRODUCT_NAME "SynthgularityPresetGen")
    target_sources(SynthgularityPresetGen PRIVATE tests/PresetGen.cpp ${PG_DSP_SOURCES})
    target_include_directories(SynthgularityPresetGen PRIVATE Source)
    target_compile_definitions(SynthgularityPresetGen PRIVATE
        JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PG_HEADLESS=1
        PG_PRODUCT_NAME="${PG_PRODUCT_NAME}" PG_COMPANY_NAME="${PG_COMPANY_NAME}")
    target_link_libraries(SynthgularityPresetGen PRIVATE mutable juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(SynthgularityUiSnapshot PRODUCT_NAME "SynthgularityUiSnapshot")
    file(GLOB_RECURSE PG_ALL_SOURCES Source/*.cpp Source/*.h)
    target_sources(SynthgularityUiSnapshot PRIVATE tests/UiSnapshot.cpp ${PG_ALL_SOURCES})
    target_include_directories(SynthgularityUiSnapshot PRIVATE Source)
    target_compile_definitions(SynthgularityUiSnapshot PRIVATE
        JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PG_NO_PLUGIN_ENTRY=1
        PG_PRODUCT_NAME="${PG_PRODUCT_NAME}" PG_COMPANY_NAME="${PG_COMPANY_NAME}" PG_HAS_LOGO=${PG_HAS_LOGO} PG_HAS_LIBRARY=${PG_HAS_LIBRARY})
    target_link_libraries(SynthgularityUiSnapshot PRIVATE mutable SynthgularityAssets juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
