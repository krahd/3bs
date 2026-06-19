# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

add_library(threebs_presentation STATIC
    MetalSceneComponent.mm
    ../ui/AdvancedStateEditor.cpp
    ../ui/ArtworkPanel.cpp
    ../ui/PresetCatalog.cpp)

target_include_directories(threebs_presentation PUBLIC ..)
target_compile_features(threebs_presentation PUBLIC cxx_std_20)
target_link_libraries(threebs_presentation
    PUBLIC
        threebs_core
        ThreeBSAssets
        juce::juce_gui_extra
        juce::juce_data_structures
    PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore")

if(THREEBS_BUILD_TESTS)
    add_executable(threebs_metal_smoke ../../tests/MetalSmoke.mm)
    target_include_directories(threebs_metal_smoke PRIVATE ..)
    target_link_libraries(threebs_metal_smoke PRIVATE threebs_presentation)
    add_test(NAME threebs_metal_smoke COMMAND threebs_metal_smoke)
    set_tests_properties(threebs_metal_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()
