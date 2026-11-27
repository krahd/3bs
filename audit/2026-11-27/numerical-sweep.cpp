// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#include "core/ThreeBodySimulation.h"
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace threebs;
struct Preset {const char* name;InitialSystem system;unsigned long long seed;double chaos,g,soft,speed;};
Preset ps[]={
{"eightfold-dawn",InitialSystem::FigureEight,33425301,0.02,1.0,0.02,1.0},
{"lagrange-glass",InitialSystem::Stable,33425302,0.05,0.72,0.05,0.72},
{"twin-moon-relay",InitialSystem::Hierarchical,33425303,0.12,0.65,0.04,1.15},
{"quiet-periapsis",InitialSystem::Hierarchical,33425304,0.08,0.52,0.06,0.46},
{"cyan-transit",InitialSystem::Stable,33425305,0.18,0.84,0.035,1.62},
{"ember-conjunction",InitialSystem::ControlledChaos,33425306,0.34,1.18,0.055,1.08},
{"violet-aphelion",InitialSystem::Stable,33425307,0.1,0.46,0.04,0.54},
{"red-shift-study",InitialSystem::ControlledChaos,33425308,0.52,1.3,0.07,1.35},
{"golden-barycenter",InitialSystem::FigureEight,33425309,0.06,0.96,0.025,0.88},
{"black-sun-threads",InitialSystem::Hierarchical,33425310,0.24,0.78,0.045,0.64},
{"orbital-ink",InitialSystem::Stable,33425311,0.14,0.68,0.03,0.82},
{"slingshot-cadence",InitialSystem::ControlledChaos,33425312,0.68,1.46,0.08,1.82},
{"pale-resonance",InitialSystem::FigureEight,33425313,0.03,0.88,0.03,0.38},
{"magenta-eclipse",InitialSystem::Hierarchical,33425314,0.3,1.05,0.05,1.24},
{"three-cold-lights",InitialSystem::Stable,33425315,0.07,0.6,0.035,0.76},
{"blue-giant-clock",InitialSystem::Hierarchical,33425316,0.04,0.92,0.025,0.96},
{"perihelion-dust",InitialSystem::ControlledChaos,33425317,0.46,1.12,0.09,1.48},
{"silent-conjunction",InitialSystem::FigureEight,33425318,0.09,0.74,0.04,0.5},
{"iron-triangle",InitialSystem::Stable,33425319,0.38,1.0,0.06,1.3},
{"solar-kite",InitialSystem::Hierarchical,33425320,0.2,0.7,0.045,0.9},
{"gravity-well",InitialSystem::ControlledChaos,33425321,0.28,1.72,0.12,0.58},
{"spiral-archive",InitialSystem::FigureEight,33425322,0.12,0.9,0.03,0.66},
{"unstable-crown",InitialSystem::ControlledChaos,33425323,0.82,1.38,0.1,1.56},
{"white-noon",InitialSystem::Stable,33425324,0.02,0.58,0.025,1.0},
};
int main(){for(const auto& p:ps){auto s=makeInitialState(p.system,p.seed,p.chaos);SimulationConfig c;c.gravitationalConstant=p.g;c.softening=p.soft;c.speed=p.speed;c.escapePolicy=EscapePolicy::Leave;ThreeBodySimulation sim(s,c);const auto e0=sim.totalEnergy();double maxdrift=0;bool finite=true;for(int i=0;i<6000;i++){sim.advance(0.01);const auto e=sim.totalEnergy();maxdrift=std::max(maxdrift,std::abs(e-e0)/std::max(1.0,std::abs(e0)));for(const auto& b:sim.state().bodies)finite=finite&&isFinite(b.position)&&isFinite(b.velocity);}std::cout<<p.name<<" max_relative_energy_drift="<<maxdrift<<" finite="<<finite<<"\n";}}
