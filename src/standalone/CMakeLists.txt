# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

juce_add_gui_app(ThreeBSStandalone
    PRODUCT_NAME "The Three Body Solution"
    COMPANY_NAME "krahd"
    BUNDLE_ID "com.krahd.threebodysolution"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Tomas Laurenzo")

target_sources(ThreeBSStandalone PRIVATE Main.cpp)
target_include_directories(ThreeBSStandalone PRIVATE ..)
target_compile_features(ThreeBSStandalone PRIVATE cxx_std_20)
target_compile_definitions(ThreeBSStandalone PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ThreeBSStandalone
    PRIVATE
        threebs_core
        threebs_presentation
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
