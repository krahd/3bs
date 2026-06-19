# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

set(THREEBS_PLUGIN_SOURCES PluginEditor.cpp PluginProcessor.cpp)

function(threebs_configure_plugin target)
    target_sources(${target} PRIVATE ${THREEBS_PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE ..)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target}
        PRIVATE threebs_core threebs_presentation juce::juce_audio_utils juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(ThreeBSAU
    COMPANY_NAME "krahd"
    COMPANY_WEBSITE "https://github.com/krahd/3bs"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo"
    BUNDLE_ID "com.krahd.threebodysolution.au"
    PLUGIN_MANUFACTURER_CODE Krhd
    PLUGIN_CODE Tbs1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS AU
    PRODUCT_NAME "The Three Body Solution")
target_compile_definitions(ThreeBSAU PRIVATE THREEBS_AU_MIDI_EFFECT=1)
threebs_configure_plugin(ThreeBSAU)

juce_add_plugin(ThreeBSVST3
    COMPANY_NAME "krahd"
    COMPANY_WEBSITE "https://github.com/krahd/3bs"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo"
    BUNDLE_ID "com.krahd.threebodysolution.vst3"
    PLUGIN_MANUFACTURER_CODE Krhd
    PLUGIN_CODE Tbs2
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    PRODUCT_NAME "The Three Body Solution")
target_compile_definitions(ThreeBSVST3 PRIVATE THREEBS_AU_MIDI_EFFECT=0)
threebs_configure_plugin(ThreeBSVST3)

if(THREEBS_ADHOC_SIGN)
    add_custom_command(TARGET ThreeBSAU_AU POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:ThreeBSAU_AU>"
        VERBATIM)
    add_custom_command(TARGET ThreeBSVST3_VST3 POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:ThreeBSVST3_VST3>"
        VERBATIM)
endif()

if(THREEBS_BUILD_TESTS)
    add_executable(threebs_plugin_tests ../../tests/PluginTests.cpp)
    target_include_directories(threebs_plugin_tests PRIVATE ..)
    target_compile_definitions(threebs_plugin_tests PRIVATE THREEBS_AU_MIDI_EFFECT=0)
    target_link_libraries(threebs_plugin_tests PRIVATE
        ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME threebs_plugin_tests COMMAND threebs_plugin_tests)
endif()
