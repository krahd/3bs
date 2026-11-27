// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit probe only: exercises the unmodified project TrailHistory implementation.
#include "render/TrailHistory.h"
#include <cmath>
#include <iostream>
int main() {
    threebs::TrailHistory<4096> trail;
    constexpr double framesPerSecond = 60.0;
    constexpr double requestedSeconds = 30.0;
    for (int frame = 0; frame <= 3600; ++frame) {
        const double time = frame / framesPerSecond;
        trail.append({10.0 * std::cos(time), 10.0 * std::sin(time), 0.0}, time, 1);
        trail.prune(time, requestedSeconds);
    }
    std::cout << "requested_seconds=" << requestedSeconds
              << " retained_seconds=" << 60.0 - trail[0].time
              << " samples=" << trail.size() << '\n';
}
