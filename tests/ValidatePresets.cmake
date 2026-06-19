# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

if(NOT EXISTS "${PRESET_FILE}")
    message(FATAL_ERROR "Factory preset file is missing: ${PRESET_FILE}")
endif()

file(READ "${PRESET_FILE}" json)
string(JSON schema ERROR_VARIABLE error GET "${json}" schemaVersion)
if(error OR NOT schema EQUAL 1)
    message(FATAL_ERROR "Factory preset schemaVersion must be 1: ${error}")
endif()

string(JSON count ERROR_VARIABLE error LENGTH "${json}" presets)
if(error OR NOT count EQUAL 24)
    message(FATAL_ERROR "Expected 24 factory presets, found ${count}: ${error}")
endif()

math(EXPR last "${count} - 1")
foreach(index RANGE 0 ${last})
    foreach(field id name description system seed chaos physics voices visual camera)
        string(JSON value ERROR_VARIABLE field_error GET "${json}" presets ${index} ${field})
        if(field_error)
            message(FATAL_ERROR "Preset ${index} lacks ${field}: ${field_error}")
        endif()
    endforeach()
    string(JSON voice_count LENGTH "${json}" presets ${index} voices)
    if(NOT voice_count EQUAL 3)
        message(FATAL_ERROR "Preset ${index} must define exactly three voices")
    endif()
endforeach()

message(STATUS "Validated ${count} factory presets (schema v${schema})")
