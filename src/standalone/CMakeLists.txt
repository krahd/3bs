# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

find_package(Threads REQUIRED)

juce_add_gui_app(ThreeBSStandalone
    PRODUCT_NAME "The Three Body Solution"
    COMPANY_NAME "Tomas Laurenzo"
    BUNDLE_ID "com.tomaslaurenzo.threebodysolution"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo")

target_sources(ThreeBSStandalone PRIVATE Main.cpp)
target_include_directories(ThreeBSStandalone PRIVATE ..)
target_compile_features(ThreeBSStandalone PRIVATE cxx_std_20)
target_compile_definitions(ThreeBSStandalone PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ThreeBSStandalone
    PRIVATE
        threebs_presentation
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(THREEBS_ADHOC_SIGN)
    add_custom_command(TARGET ThreeBSStandalone POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:ThreeBSStandalone>"
        VERBATIM)
endif()

if(THREEBS_BUILD_TESTS)
    add_executable(threebs_standalone_runtime_tests ../../tests/StandaloneRuntimeTests.cpp)
    target_include_directories(threebs_standalone_runtime_tests PRIVATE ..)
    target_compile_definitions(threebs_standalone_runtime_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(threebs_standalone_runtime_tests PRIVATE threebs_core Threads::Threads juce::juce_audio_devices juce::juce_events)
    add_test(NAME threebs_standalone_runtime_tests COMMAND threebs_standalone_runtime_tests)
    set_tests_properties(threebs_standalone_runtime_tests PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()
