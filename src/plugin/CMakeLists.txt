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
        PRIVATE threebs_presentation juce::juce_audio_utils juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(ThreeBSAU
    COMPANY_NAME "Tomas Laurenzo"
    COMPANY_WEBSITE "https://github.com/krahd/3bs"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo"
    BUNDLE_ID "com.tomaslaurenzo.threebodysolution.au"
    PLUGIN_MANUFACTURER_CODE Tmlz
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
    COMPANY_NAME "Tomas Laurenzo"
    COMPANY_WEBSITE "https://github.com/krahd/3bs"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo"
    BUNDLE_ID "com.tomaslaurenzo.threebodysolution.vst3"
    PLUGIN_MANUFACTURER_CODE Tmlz
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
    add_executable(threebs_legacy_state_tests ../../tests/LegacyStateTests.cpp)
    target_include_directories(threebs_legacy_state_tests PRIVATE ..)
    target_compile_definitions(threebs_legacy_state_tests PRIVATE THREEBS_AU_MIDI_EFFECT=0)
    target_link_libraries(threebs_legacy_state_tests PRIVATE
        ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
    add_executable(threebs_plugin_tests ../../tests/PluginTests.cpp ../../tests/PluginRegressionTests.cpp)
    target_include_directories(threebs_plugin_tests PRIVATE ..)
    target_compile_definitions(threebs_plugin_tests PRIVATE THREEBS_AU_MIDI_EFFECT=0)
    target_link_libraries(threebs_plugin_tests PRIVATE
        ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME threebs_plugin_tests COMMAND threebs_plugin_tests)

    set(THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS 20 CACHE STRING
        "Processor concurrency test deadline; instrumented builds may need a larger bound")
    if(NOT THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS must be a positive integer")
    endif()
    add_executable(threebs_processor_concurrency_tests ../../tests/ProcessorConcurrencyTests.cpp)
    target_include_directories(threebs_processor_concurrency_tests PRIVATE ..)
    target_compile_definitions(threebs_processor_concurrency_tests PRIVATE
        THREEBS_AU_MIDI_EFFECT=0
        THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS=${THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS})
    target_link_libraries(threebs_processor_concurrency_tests PRIVATE
        ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME threebs_processor_concurrency_tests COMMAND threebs_processor_concurrency_tests)
    math(EXPR threebs_processor_ctest_timeout "${THREEBS_PROCESSOR_TEST_TIMEOUT_SECONDS} + 10")
    set_tests_properties(threebs_processor_concurrency_tests PROPERTIES TIMEOUT ${threebs_processor_ctest_timeout})

    add_library(threebs_allocation_probe SHARED ../../tests/AllocationProbe.cpp)
    target_compile_features(threebs_allocation_probe PRIVATE cxx_std_20)
    set_target_properties(threebs_allocation_probe PROPERTIES CXX_VISIBILITY_PRESET default)

    add_executable(threebs_realtime_profile ../../tests/RealtimeProfile.cpp)
    target_include_directories(threebs_realtime_profile PRIVATE ..)
    target_compile_definitions(threebs_realtime_profile PRIVATE THREEBS_AU_MIDI_EFFECT=0)
    target_link_libraries(threebs_realtime_profile PRIVATE
        threebs_allocation_probe ThreeBSVST3 threebs_presentation juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME threebs_realtime_profile COMMAND threebs_realtime_profile)
endif()
