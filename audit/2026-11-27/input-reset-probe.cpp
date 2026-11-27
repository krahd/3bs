// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#include "core/MusicEngine.h"
#include <iostream>
#include <algorithm>

threebs::EngineConfig makeConfig(threebs::MusicEngine& engine) {
    auto cfg=engine.config();
    for(auto& voice:cfg.voices) voice.enabled=false;
    auto& voice=cfg.voices[0];
    voice.enabled=true;
    voice.probability=1;
    voice.triggerMapping=threebs::TriggerMapping::Clock;
    voice.clockDivisionBeats=1.0/128.0;
    voice.minimumTriggerIntervalBeats=0;
    voice.minimumDurationBeats=voice.maximumDurationBeats=1;
    cfg.inputGateEnabled=true;
    return cfg;
}

int noteOnsWithAdapterInputBatching(unsigned blockSize) {
    threebs::MusicEngine engine;
    auto cfg=makeConfig(engine);
    int held=0, ons=0;
    for(unsigned start=0;start<1024;start+=blockSize) {
        // Mirror PluginProcessor.cpp 481-489: consume all MIDI input for a
        // block, ignoring each event's sample offset, then run whole block.
        if(100>=start && 100<start+blockSize) ++held;
        if(900>=start && 900<start+blockSize) held=std::max(0,held-1);
        cfg.inputGateOpen=held>0;
        engine.setConfig(cfg);
        threebs::ProcessContext ctx;
        ctx.sampleCount=blockSize;
        ctx.beatAtStart=double(start)*ctx.beatsPerSample;
        ctx.transportStarted=start==0;
        threebs::MusicEngine::EventBuffer events;
        engine.process(ctx,events);
        for(const auto& e:events) ons+=e.type==threebs::MidiEventType::NoteOn;
    }
    return ons;
}

int main() {
    threebs::MusicEngine engine;
    auto cfg=makeConfig(engine);
    cfg.inputGateEnabled=false;
    engine.setConfig(cfg);
    threebs::ProcessContext ctx;
    ctx.sampleCount=1;
    ctx.transportStarted=true;
    threebs::MusicEngine::EventBuffer events;
    engine.process(ctx,events);
    int ons=0;
    for(const auto& e:events) ons+=e.type==threebs::MidiEventType::NoteOn;
    // Same reset signature as standalone callbacks.
    engine.reset(engine.simulation().initialState());
    events.clear();
    engine.allNotesOff(0,events);
    std::cout<<"standalone-reset-path note_ons_before_reset="<<ons
             <<" note_offs_recoverable_after_reset="<<events.size()<<"\n";
    std::cout<<"adapter-input-batching note_ons_1024_block="<<noteOnsWithAdapterInputBatching(1024)
             <<" note_ons_128_blocks="<<noteOnsWithAdapterInputBatching(128)<<"\n";
}
