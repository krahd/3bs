// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#pragma once
#include <cstddef>
namespace threebs::allocationProbe {
struct Counts { std::size_t cppAllocations{}, cAllocations{}, frees{}; };
void begin() noexcept;
Counts end() noexcept;
void noteCppAllocation() noexcept;
void noteCppFree(void*) noexcept;
}
