// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
// Audit reproduction against the unchanged 2026-11-27 baseline; not a production repair.
#include "core/SnapshotQueue.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>

int main(int argc, char**) {
    threebs::SpscQueue<threebs::RenderSnapshot, 64> q;
    std::atomic<bool> done{false};
    const bool steal = argc == 1;
    std::uint64_t reads=0, corrupt=0, regressions=0, dropped=0;
    std::thread producer([&] {
        for (std::uint64_t i=1; i<=2000000; ++i) {
            threebs::RenderSnapshot value;
            value.sequence=i;
            value.trajectoryRevision=i;
            for(auto& b:value.bodies) {
                b.mass=double(i);
                b.position={double(i),double(i),double(i)};
                b.velocity={double(i),double(i),double(i)};
            }
            if(!q.push(value)) {
                ++dropped;
                if(steal) {
                    threebs::RenderSnapshot discarded;
                    q.pop(discarded);
                    q.push(value);
                }
            }
        }
        done.store(true,std::memory_order_release);
    });
    std::thread consumer([&] {
        std::uint64_t prev=0;
        for(;;) {
            threebs::RenderSnapshot value;
            if(!q.pop(value)) {
                if(done.load(std::memory_order_acquire)) break;
                std::this_thread::yield();
                continue;
            }
            ++reads;
            if(value.sequence<=prev) ++regressions;
            prev=value.sequence;
            bool damaged=value.trajectoryRevision!=value.sequence;
            for(const auto& b:value.bodies)
                damaged=damaged || b.mass!=double(value.sequence) || b.position.x!=double(value.sequence)
                    || b.position.y!=double(value.sequence) || b.position.z!=double(value.sequence)
                    || b.velocity.x!=double(value.sequence) || b.velocity.y!=double(value.sequence)
                    || b.velocity.z!=double(value.sequence);
            corrupt+=damaged;
            if(reads%128==0) std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    producer.join();consumer.join();
    std::cout<<"mode="<<(steal?"current-producer-pop":"control-drop-new")<<" reads="<<reads<<" full="<<dropped
             <<" corrupt="<<corrupt<<" duplicate_or_regressed="<<regressions<<"\n";
}
