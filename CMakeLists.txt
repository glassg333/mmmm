cmake_minimum_required(VERSION 3.20)
project(MonomachineOne VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Source directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# 1. Standalone DSP & Architecture Verification Executables
add_executable(monomachine_suite_test tests/test_full_suite.cpp)
add_executable(monomachine_fm_test tests/test_fm_dyn.cpp)
add_executable(monomachine_bbox_test tests/test_bbox.cpp)
add_executable(monomachine_arp_test tests/test_arp.cpp)
add_executable(monomachine_matrix_test tests/test_mod_matrix.cpp)
add_executable(monomachine_conversions_test tests/test_conversions.cpp)

# 2. JUCE / VST3 Build Support (When JUCE is present)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)

    # Monomachine Synth VST3 (Instrument with MIDI Input)
    juce_add_plugin(MonomachineSynth
        COMPANY_NAME "MonomachineOne"
        PLUGIN_MANUFACTURER_CODE MnOn
        PLUGIN_CODE MnSy
        FORMATS VST3 Standalone
        PRODUCT_NAME "Monomachine One Synth"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
    )
    target_sources(MonomachineSynth PRIVATE
        dsp/monomachine_fm_dynamic.hpp
        dsp/monomachine_bbox.hpp
        dsp/monomachine_filter.hpp
        dsp/monomachine_chorus.hpp
        dsp/monomachine_effects.hpp
        dsp/monomachine_voice_chain.hpp
        models/machine_definitions.hpp
        models/track_pages.hpp
        models/arpeggiator.hpp
        models/modulation_matrix.hpp
    )

    # Monomachine FX VST3 (Audio Effect with MIDI Input for Envelope & LFO gating)
    juce_add_plugin(MonomachineFX
        COMPANY_NAME "MonomachineOne"
        PLUGIN_MANUFACTURER_CODE MnOn
        PLUGIN_CODE MnFx
        FORMATS VST3 Standalone
        PRODUCT_NAME "Monomachine One FX"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
    )
    target_sources(MonomachineFX PRIVATE
        dsp/monomachine_chorus.hpp
        dsp/monomachine_effects.hpp
        dsp/monomachine_filter.hpp
        dsp/monomachine_voice_chain.hpp
        models/track_pages.hpp
        models/modulation_matrix.hpp
    )
endif()
