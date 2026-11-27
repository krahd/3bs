// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#include "core/MusicEngine.h"
#include <limits>
int main() {
    auto state=threebs::makeInitialState(threebs::InitialSystem::FigureEight,1);
    state.bodies[0].position.x=std::numeric_limits<double>::quiet_NaN();
    threebs::MusicEngine engine(state);
    threebs::ProcessContext c;c.sampleCount=128;c.transportStarted=true;
    threebs::MusicEngine::EventBuffer out;engine.process(c,out);
}
