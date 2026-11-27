// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#include "core/MusicEngine.h"
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
using namespace threebs;
EngineConfig single() {
    EngineConfig c;
    for (std::size_t b=0;b<bodyCount;++b) {c.voices[b].enabled=b==0;c.voices[b].channel=static_cast<std::uint8_t>(b+1);}
    return c;
}
ProcessContext context(std::uint32_t n, bool start=false, double bps=2.0/48000.0) {ProcessContext p;p.sampleCount=n;p.transportStarted=start;p.beatsPerSample=bps;return p;}
void constructor() {
    auto c=single();for(auto& v:c.voices)v.channel=1;c.voices[0].octave=2;
    MusicEngine e(makeInitialState(InitialSystem::FigureEight,1),c);
    std::cout<<"constructor requested octave=2 enabled=[1,0,0], got octave="<<int(e.config().voices[0].octave)<<" enabled=["<<e.config().voices[0].enabled<<","<<e.config().voices[1].enabled<<","<<e.config().voices[2].enabled<<"]\n";
}
void speedPeak() {
    SimulationState s;s.bodies[0].position={-1,0,0};s.bodies[1].position={1,0,0};s.bodies[2].position={0,2,0};
    auto c=single();c.voices[0].triggerMapping=TriggerMapping::SpeedPeak;
    MusicEngine e(s,c);MusicEngine::EventBuffer out;double last=e.measurements()[0].speed;int increases=0,decreases=0,notes=0;
    for(int i=0;i<4800;++i){e.process(context(1,i==0),out);auto v=e.measurements()[0].speed;increases+=v>last;decreases+=v<last;for(const auto& ev:out)if(ev.type==MidiEventType::NoteOn)++notes;last=v;}
    std::cout<<"speed_peak monotonic_window: increases="<<increases<<" decreases="<<decreases<<" note_ons="<<notes<<"\n";
}
void pendingDisabled() {
    auto c=single();for(auto& v:c.voices){v.enabled=true;v.clockDivisionBeats=16.0;}c.voicingMode=VoicingMode::Strum;c.chordStrumMilliseconds=100;
    MusicEngine e(makeInitialState(InitialSystem::FigureEight,1),c);MusicEngine::EventBuffer out;e.process(context(1,true),out);
    for(auto& v:c.voices)v.enabled=false;c.inputGateEnabled=true;c.inputGateOpen=false;e.setConfig(c);e.process(context(12000),out);
    int on=0;for(const auto& ev:out)if(ev.type==MidiEventType::NoteOn)++on;
    std::cout<<"pending_strum after_disable_all_and_close_gate note_ons="<<on<<"\n";
}
void channelChange() {
    auto c=single();c.voices[0].clockDivisionBeats=16;c.voices[0].minimumDurationBeats=8;c.voices[0].maximumDurationBeats=8;
    MusicEngine e(makeInitialState(InitialSystem::FigureEight,1),c);MusicEngine::EventBuffer out;e.process(context(1,true),out);
    for(const auto& ev:out)if(ev.type==MidiEventType::NoteOn)std::cout<<"channel_change note_on_channel="<<int(ev.channel)<<" ";
    c.voices[0].channel=7;e.setConfig(c);out.clear();e.allNotesOff(0,out);for(const auto& ev:out)if(ev.type==MidiEventType::NoteOff)std::cout<<"note_off_channel="<<int(ev.channel)<<"\n";
}
std::vector<MidiEvent> render(std::uint32_t block) {
    auto c=single();for(auto& v:c.voices){v.enabled=true;v.triggerMapping=TriggerMapping::Clock;v.clockDivisionBeats=1.0/128;v.minimumTriggerIntervalBeats=0;v.minimumDurationBeats=1.0/1024;v.maximumDurationBeats=1.0/1024;v.durationGrid=DurationGrid::ContinuousLegacy;}
    MusicEngine e(makeInitialState(InitialSystem::FigureEight,1),c);MusicEngine::EventBuffer out;std::vector<MidiEvent> events;
    for(std::uint32_t pos=0;pos<16384;pos+=block){auto p=context(std::min(block,16384-pos),pos==0,4.0/48000);e.process(p,out);for(auto ev:out){ev.sampleOffset+=pos;events.push_back(ev);}}
    e.process(context(1),out);for(auto ev:out){ev.sampleOffset+=16384;events.push_back(ev);}
    return events;
}
void overflow() {
    auto a=render(512),b=render(16384);std::array<std::array<int,128>,17> active{};int unmatched=0;
    for(const auto& ev:b){auto& n=active[ev.channel][ev.data1];if(ev.type==MidiEventType::NoteOn)++n;else if(ev.type==MidiEventType::NoteOff)--n;}
    for(const auto& channel:active)for(auto n:channel)if(n>0)unmatched+=n;
    std::cout<<"buffer_overflow @240bpm 16384_samples: partition_512_events="<<a.size()<<" single_block_events="<<b.size()<<" unmatched_note_ons="<<unmatched<<"\n";
}
int main(){constructor();speedPeak();pendingDisabled();channelChange();overflow();}
