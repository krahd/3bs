# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

add_library(threebs_presentation STATIC
    MetalSceneComponent.mm
    ../ui/AdvancedStateEditor.cpp
    ../ui/AdvancedConfigurationEditor.cpp
    ../ui/ArtworkPanel.cpp
    ../ui/PresetCatalog.cpp
    ../ui/UserConfiguration.cpp)

target_include_directories(threebs_presentation PUBLIC ..)
target_compile_features(threebs_presentation PUBLIC cxx_std_20)
target_link_libraries(threebs_presentation
    PUBLIC
        threebs_core
        threebs_presentation_model
        ThreeBSAssets
        juce::juce_gui_extra
        juce::juce_data_structures
    PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework CoreText"
        "-framework CoreGraphics")

if(THREEBS_BUILD_TESTS)
    juce_add_gui_app(threebs_ui_smoke PRODUCT_NAME "3bs UI Acceptance")
    target_sources(threebs_ui_smoke PRIVATE ../../tests/UiSmoke.cpp)
    target_include_directories(threebs_ui_smoke PRIVATE ..)
    target_compile_definitions(threebs_ui_smoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(threebs_ui_smoke PRIVATE threebs_presentation)
    add_test(NAME threebs_ui_smoke COMMAND $<TARGET_FILE:threebs_ui_smoke>)
    set_tests_properties(threebs_ui_smoke PROPERTIES TIMEOUT 120)
    add_executable(threebs_metal_smoke ../../tests/MetalSmoke.mm)
    target_include_directories(threebs_metal_smoke PRIVATE ..)
    target_link_libraries(threebs_metal_smoke PRIVATE threebs_presentation)
    add_test(NAME threebs_metal_smoke COMMAND threebs_metal_smoke)
    set_tests_properties(threebs_metal_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()
