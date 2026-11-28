// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#include "AllocationProbe.h"
#include <cstdlib>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif
namespace {
thread_local bool tracking{};
thread_local threebs::allocationProbe::Counts counts{};
#if defined(__APPLE__)
void* tracedMalloc(std::size_t size) {
    if (tracking) ++counts.cAllocations;
    return malloc_zone_malloc(malloc_default_zone(), size);
}
void* tracedCalloc(std::size_t count, std::size_t size) {
    if (tracking) ++counts.cAllocations;
    return malloc_zone_calloc(malloc_default_zone(), count, size);
}
void* tracedRealloc(void* pointer, std::size_t size) {
    if (tracking) ++counts.cAllocations;
    auto* zone = pointer == nullptr ? malloc_default_zone() : malloc_zone_from_ptr(pointer);
    return malloc_zone_realloc(zone == nullptr ? malloc_default_zone() : zone, pointer, size);
}
void tracedFree(void* pointer) {
    if (pointer == nullptr) return;
    if (tracking) ++counts.frees;
    auto* zone = malloc_zone_from_ptr(pointer);
    malloc_zone_free(zone == nullptr ? malloc_default_zone() : zone, pointer);
}
struct Interposition { const void* replacement; const void* original; };
// This lives in its own linked dylib, so calls in the test executable (including
// statically linked JUCE) are interposed. Zone APIs avoid recursive interception.
__attribute__((used, section("__DATA,__interpose"))) const Interposition hooks[]{
    {reinterpret_cast<const void*>(&tracedMalloc), reinterpret_cast<const void*>(&std::malloc)},
    {reinterpret_cast<const void*>(&tracedCalloc), reinterpret_cast<const void*>(&std::calloc)},
    {reinterpret_cast<const void*>(&tracedRealloc), reinterpret_cast<const void*>(&std::realloc)},
    {reinterpret_cast<const void*>(&tracedFree), reinterpret_cast<const void*>(&std::free)}
};
#endif
}
namespace threebs::allocationProbe {
void begin() noexcept { counts = {}; tracking = true; }
Counts end() noexcept { tracking = false; return counts; }
void noteCppAllocation() noexcept { if (tracking) ++counts.cppAllocations; }
void noteCppFree(void* pointer) noexcept { if (tracking && pointer != nullptr) ++counts.frees; }
}
