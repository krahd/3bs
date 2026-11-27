// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#define main original_test_main
#include "../../tests/CoreTests.cpp"
#undef main
int main() {
    const auto run = [](const char* label, auto fn) { const auto before = failures; fn(); std::cout << label << ": " << failures - before << " failed assertions\n"; };
    run("testOctaveTranspose", testOctaveTranspose);
    run("testDurationMapping", testDurationMapping);
    run("testGlobalChordFrame", testGlobalChordFrame);
    run("testStrumBeatUnit", testStrumBeatUnit);
    run("testBarAutoReset", testBarAutoReset);
    return failures ? 1 : 0;
}
