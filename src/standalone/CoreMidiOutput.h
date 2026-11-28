// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#pragma once

#include "core/MusicEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>

#include <charconv>
#include <cstdint>
#include <memory>
#include <system_error>

#if JUCE_MAC
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace threebs {

// CoreMIDI owns the system endpoint registry. This class deliberately does not
// use JUCE's message-thread-only device maps or shared MIDI Session. A runtime
// delivery thread creates, sends through, and destroys its own client/port;
// the UI may independently enumerate immutable device descriptions.
class CoreMidiOutput final {
public:
    ~CoreMidiOutput() {
#if JUCE_MAC
        if (virtualSource_ && endpoint_ != 0) (void)MIDIEndpointDispose(endpoint_);
        if (port_ != 0) (void)MIDIPortDispose(port_);
        if (client_ != 0) (void)MIDIClientDispose(client_);
#endif
    }
    CoreMidiOutput(const CoreMidiOutput&) = delete;
    CoreMidiOutput& operator=(const CoreMidiOutput&) = delete;

    static juce::Array<juce::MidiDeviceInfo> availableDevices() {
        juce::Array<juce::MidiDeviceInfo> result;
#if JUCE_MAC
        const auto count = MIDIGetNumberOfDestinations();
        for (ItemCount index = 0; index < count; ++index) {
            const auto endpoint = MIDIGetDestination(index);
            SInt32 identifier{};
            if (!endpointAvailable(endpoint) || MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID,
                                                              &identifier) != noErr) continue;
            auto name = stringProperty(endpoint, kMIDIPropertyDisplayName);
            if (name.isEmpty()) name = stringProperty(endpoint, kMIDIPropertyName);
            if (name.isEmpty()) name = "MIDI destination " + juce::String(identifier);
            result.add(juce::MidiDeviceInfo{name, juce::String(identifier)});
        }
#endif
        return result;
    }

    static bool isAvailable(const juce::String& identifier) {
#if JUCE_MAC
        return findDestination(identifier) != 0;
#else
        juce::ignoreUnused(identifier);
        return false;
#endif
    }

    static std::unique_ptr<CoreMidiOutput> createVirtual(const juce::String& name) {
#if JUCE_MAC
        auto result = std::unique_ptr<CoreMidiOutput>(new CoreMidiOutput);
        if (!result->createClient()) return {};
        const auto sourceName = name.toCFString();
        if (sourceName == nullptr) return {};
        // The application sends MIDI 1.0 channel-voice messages through the
        // modern protocol-explicit CoreMIDI API (macOS 11+, product minimum 13).
        const auto status = MIDISourceCreateWithProtocol(result->client_, sourceName,
                                                         kMIDIProtocol_1_0, &result->endpoint_);
        CFRelease(sourceName);
        if (status != noErr || result->endpoint_ == 0) return {};
        result->virtualSource_ = true;
        // Match the previous JUCE virtual-port ID calculation for the published
        // standalone bundle. This preserves clients' persistent source routes.
        // Another running instance may already own that ID; CoreMIDI then keeps
        // this instance's system-assigned unique ID without disturbing it.
        const auto persistentName = juce::String("com.tomaslaurenzo.threebodysolution.")
            + name + ".output";
        (void)MIDIObjectSetIntegerProperty(result->endpoint_, kMIDIPropertyUniqueID,
                                           static_cast<SInt32>(persistentName.hashCode()));
        return result;
#else
        juce::ignoreUnused(name);
        return {};
#endif
    }

    static std::unique_ptr<CoreMidiOutput> openDevice(const juce::String& identifier) {
#if JUCE_MAC
        const auto destination = findDestination(identifier);
        if (destination == 0) return {};
        auto result = std::unique_ptr<CoreMidiOutput>(new CoreMidiOutput);
        if (!result->createClient()) return {};
        if (MIDIOutputPortCreate(result->client_, CFSTR("ThreeBS delivery"), &result->port_) != noErr
            || result->port_ == 0) return {};
        // A disconnect/reconnect during construction must not retarget a stale
        // reference; the caller can retry the same stable destination ID later.
        if (findDestination(identifier) != destination) return {};
        result->endpoint_ = destination;
        return result;
#else
        juce::ignoreUnused(identifier);
        return {};
#endif
    }

    bool send(const MidiEvent& event) noexcept {
#if JUCE_MAC
        if (endpoint_ == 0 || event.channel < 1U || event.channel > 16U
            || event.data1 > 127U || event.data2 > 127U) return false;
        std::uint32_t status{};
        switch (event.type) {
            case MidiEventType::NoteOn: status = 0x90U; break;
            case MidiEventType::NoteOff: status = 0x80U; break;
            case MidiEventType::ControlChange: status = 0xb0U; break;
            default: return false;
        }
        status |= static_cast<std::uint32_t>(event.channel - 1U);
        // UMP message type 2 is one MIDI 1.0 channel-voice word. Group is zero.
        // All storage is on the stack; there is no JUCE message construction,
        // heap growth, endpoint enumeration or application lock on this path.
        const UInt32 word = 0x20000000U | (status << 16U)
            | (static_cast<std::uint32_t>(event.data1) << 8U) | event.data2;
        MIDIEventList packetList{};
        auto* packet = MIDIEventListInit(&packetList, kMIDIProtocol_1_0);
        if (MIDIEventListAdd(&packetList, sizeof(packetList), packet, 0, 1, &word) == nullptr)
            return false;
        const auto result = virtualSource_ ? MIDIReceivedEventList(endpoint_, &packetList)
                                          : MIDISendEventList(port_, endpoint_, &packetList);
        return result == noErr;
#else
        juce::ignoreUnused(event);
        return false;
#endif
    }

private:
    CoreMidiOutput() = default;
#if JUCE_MAC
    static juce::String stringProperty(MIDIObjectRef object, CFStringRef property) {
        CFStringRef value{};
        if (MIDIObjectGetStringProperty(object, property, &value) != noErr || value == nullptr)
            return {};
        const auto result = juce::String::fromCFString(value);
        CFRelease(value);
        return result;
    }

    static MIDIEndpointRef findDestination(const juce::String& identifier) {
        const auto* begin = identifier.toRawUTF8();
        const auto* end = begin + identifier.getNumBytesAsUTF8();
        SInt32 wanted{};
        const auto parsed = std::from_chars(begin, end, wanted);
        if (begin == end || parsed.ec != std::errc{} || parsed.ptr != end) return 0;
        const auto count = MIDIGetNumberOfDestinations();
        for (ItemCount index = 0; index < count; ++index) {
            const auto endpoint = MIDIGetDestination(index);
            SInt32 actual{};
            if (endpoint != 0 && MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID,
                                                             &actual) == noErr && actual == wanted)
                return endpointAvailable(endpoint) ? endpoint : 0;
        }
        return 0;
    }

    static bool offline(MIDIObjectRef object) noexcept {
        SInt32 value{};
        return object != 0 && MIDIObjectGetIntegerProperty(object, kMIDIPropertyOffline, &value) == noErr
            && value != 0;
    }
    static bool endpointAvailable(MIDIEndpointRef endpoint) noexcept {
        if (endpoint == 0 || offline(endpoint)) return false;
        // CoreMIDI can retain a disconnected driver's objects in the registry
        // with an offline flag instead of removing the destination altogether.
        MIDIEntityRef entity{};
        if (MIDIEndpointGetEntity(endpoint, &entity) == noErr && entity != 0) {
            if (offline(entity)) return false;
            MIDIDeviceRef device{};
            if (MIDIEntityGetDevice(entity, &device) == noErr && offline(device)) return false;
        }
        return true;
    }

    bool createClient() {
        // No notify callback or run-loop dependency: the delivery owner polls
        // destination availability outside its scheduled send operation.
        return MIDIClientCreate(CFSTR("The Three Body Solution"), nullptr, nullptr, &client_) == noErr
            && client_ != 0;
    }
    MIDIClientRef client_{};
    MIDIPortRef port_{};
    MIDIEndpointRef endpoint_{};
    bool virtualSource_{};
#endif
};

} // namespace threebs
