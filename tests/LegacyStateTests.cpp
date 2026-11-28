// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo
#include "plugin/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
bool equivalent(const juce::var& a, const juce::var& b) {
    if (a.isInt() || a.isInt64() || a.isDouble()) {
        if (!(b.isInt() || b.isInt64() || b.isDouble())) return false;
        const auto x = static_cast<double>(a), y = static_cast<double>(b);
        return std::abs(x - y) <= 2.0e-5 * std::max(1.0, std::max(std::abs(x), std::abs(y)));
    }
    if (const auto* object = a.getDynamicObject()) {
        const auto* other = b.getDynamicObject();
        if (other == nullptr || object->getProperties().size() != other->getProperties().size()) return false;
        for (int i = 0; i < object->getProperties().size(); ++i) {
            const auto key = object->getProperties().getName(i);
            if (!other->hasProperty(key) || !equivalent(object->getProperty(key), other->getProperty(key))) {
                std::cerr << "Legacy mismatch at " << key.toString() << '\n'; return false;
            }
        }
        return true;
    }
    if (const auto* array = a.getArray()) {
        const auto* other = b.getArray();
        if (other == nullptr || array->size() != other->size()) return false;
        for (int i = 0; i < array->size(); ++i) if (!equivalent((*array)[i], (*other)[i])) return false;
        return true;
    }
    return a == b;
}
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    if (argc != 2) return EXIT_FAILURE;
    const juce::File directory(juce::String::fromUTF8(argv[1]));
    for (const auto name : {"default", "edited"}) {
        threebs::UserConfiguration expected; juce::String error;
        if (!threebs::deserializeUserConfiguration(directory.getChildFile(juce::String(name) + ".3bs").loadFileAsString(), expected, error)) {
            std::cerr << "Prior portable state rejected: " << error << '\n'; return EXIT_FAILURE;
        }
        if (juce::String(name) == "edited") {
            expected.timeSignatureSource = threebs::TimeSignatureSource::Manual;
            expected.timeSignatureNumerator = 7; expected.timeSignatureDenominator = 8;
        }
        juce::MemoryBlock oldBinary;
        if (!directory.getChildFile(juce::String(name) + ".bin").loadFileAsData(oldBinary)) return EXIT_FAILURE;
        threebs::ThreeBSProcessor restored;
        restored.setStateInformation(oldBinary.getData(), static_cast<int>(oldBinary.getSize()));
        if (restored.lastStateError().isNotEmpty()) {
            std::cerr << "Prior host state rejected: " << restored.lastStateError() << '\n'; return EXIT_FAILURE;
        }
        const auto compare = [&](const threebs::UserConfiguration& actual) {
            return equivalent(juce::JSON::parse(threebs::serializeUserConfiguration(expected)),
                              juce::JSON::parse(threebs::serializeUserConfiguration(actual)));
        };
        if (!compare(restored.currentUserConfiguration())) return EXIT_FAILURE;
        juce::MemoryBlock newBinary;
        restored.getStateInformation(newBinary);
        threebs::ThreeBSProcessor second;
        second.setStateInformation(newBinary.getData(), static_cast<int>(newBinary.getSize()));
        if (!compare(second.currentUserConfiguration())) return EXIT_FAILURE;
        std::cout << "PASS prior-executable host7 and portable3 " << name << " -> current immediate recall/resave\n";
    }
    return EXIT_SUCCESS;
}
