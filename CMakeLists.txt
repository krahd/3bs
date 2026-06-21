# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

cmake_minimum_required(VERSION 3.22)

project(ThreeBodySolution VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(THREEBS_BUILD_PLUGINS "Build AU, VST3, and standalone targets" ON)
option(THREEBS_BUILD_TESTS "Build deterministic core tests" ON)
option(THREEBS_ADHOC_SIGN "Ad-hoc sign local development bundles" ON)

add_library(threebs_core STATIC
    src/core/MusicEngine.cpp
    src/core/Scale.cpp
    src/core/ThreeBodySimulation.cpp)

add_library(threebs_presentation_model STATIC
    src/render/CameraController.cpp
    src/render/PresentationState.cpp)

target_include_directories(threebs_core PUBLIC src)
target_compile_features(threebs_core PUBLIC cxx_std_20)
target_compile_options(threebs_core PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>)

target_include_directories(threebs_presentation_model PUBLIC src)
target_compile_features(threebs_presentation_model PUBLIC cxx_std_20)
target_compile_options(threebs_presentation_model PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>)

if(THREEBS_BUILD_TESTS)
    enable_testing()
    add_executable(threebs_core_tests tests/CoreTests.cpp)
    target_link_libraries(threebs_core_tests PRIVATE threebs_core threebs_presentation_model)
    add_test(NAME threebs_core_tests COMMAND threebs_core_tests)
    add_test(
        NAME threebs_factory_presets
        COMMAND ${CMAKE_COMMAND}
            -DPRESET_FILE=${CMAKE_CURRENT_SOURCE_DIR}/resources/presets/factory-presets.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidatePresets.cmake)
    add_test(
        NAME threebs_voicing_presets
        COMMAND ${CMAKE_COMMAND}
            -DPRESET_FILE=${CMAKE_CURRENT_SOURCE_DIR}/resources/presets/voicing-presets.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateVoicingPresets.cmake)
    add_test(
        NAME threebs_star_catalog
        COMMAND ${CMAKE_COMMAND}
            -DSTAR_FILE=${CMAKE_CURRENT_SOURCE_DIR}/resources/stars/hyg-v41-mag65.csv
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateStarCatalog.cmake)
endif()

if(THREEBS_BUILD_PLUGINS)
    if(NOT APPLE)
        message(FATAL_ERROR "The initial 3bs plugin targets require macOS")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
        message(FATAL_ERROR "JUCE is missing; run git submodule update --init --recursive")
    endif()

    enable_language(OBJC)
    enable_language(OBJCXX)
    add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)
    juce_add_binary_data(ThreeBSAssets
        NAMESPACE ThreeBSAssets
        SOURCES
            resources/presets/factory-presets.json
            resources/presets/voicing-presets.json
            resources/metal/SceneShaders.metal
            resources/stars/bright-stars.csv
            resources/stars/hyg-v41-mag65.csv)
    add_subdirectory(src/render)
    add_subdirectory(src/plugin)
    add_subdirectory(src/standalone)
endif()
