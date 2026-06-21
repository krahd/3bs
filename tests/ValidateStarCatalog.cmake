# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Tomas Laurenzo

if(NOT EXISTS "${STAR_FILE}")
    message(FATAL_ERROR "HYG star catalogue is missing: ${STAR_FILE}")
endif()

file(STRINGS "${STAR_FILE}" rows REGEX "^-?[0-9]+\\.[0-9]+,-?[0-9]+\\.[0-9]+,-?[0-9]+\\.[0-9]+,-?[0-9]+\\.[0-9]+$")
list(LENGTH rows row_count)
if(row_count LESS 8000)
    message(FATAL_ERROR "HYG star catalogue contains only ${row_count} valid rows")
endif()

message(STATUS "Validated ${row_count} HYG star catalogue rows")
