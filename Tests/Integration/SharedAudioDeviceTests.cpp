/*
  ==============================================================================

    SharedAudioDeviceTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression test for the v0.9.1 follow-up "each tab opens its own audio
    device": every Document's AudioEngine initialised a private
    AudioDeviceManager, so a second tab opened a second device, which
    exclusive-mode / ASIO drivers on Windows refuse. Documents created by a
    DocumentManager with a shared device manager now play through that one
    manager, and only the current tab's engine is attached to it.

    CI-safe: the shared manager is never initialised, so no device opens.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "../../Source/Utils/DocumentManager.h"
#include "../../Source/Utils/Document.h"

class SharedAudioDeviceTests : public juce::UnitTest
{
public:
    SharedAudioDeviceTests() : juce::UnitTest("Tabs Share One Audio Device", "MultiDocument") {}

    void runTest() override
    {
        beginTest("Every tab uses the shared device manager; only the current tab is attached");

        juce::AudioDeviceManager shared;   // never initialised: no device is opened
        DocumentManager docs;
        docs.setSharedDeviceManager(&shared);

        Document* a = docs.createDocument();
        Document* b = docs.createDocument();
        Document* c = docs.createDocument();

        for (auto* doc : { a, b, c })
            expect(&doc->getAudioEngine().getDeviceManager() == &shared,
                   "document plays through the shared manager, not a device of its own");

        auto attached = [&]() -> juce::Array<Document*>
        {
            juce::Array<Document*> result;
            for (auto* doc : { a, b, c })
                if (doc != nullptr && docs.getDocumentIndex(doc) >= 0
                    && doc->getAudioEngine().isAudioCallbackActive())
                    result.add(doc);
            return result;
        };

        expect(attached() == juce::Array<Document*> { a }, "first tab is current and attached");

        docs.setCurrentDocument(c);
        expect(attached() == juce::Array<Document*> { c }, "switching moves the device to the new tab");

        docs.closeDocument(c);       // closing the current tab makes another current
        c = nullptr;
        expectEquals(docs.getNumDocuments(), 2);
        auto now = attached();
        expectEquals(now.size(), 1, "exactly one tab attached after closing the current one");
        expect(now.size() == 1 && now[0] == docs.getCurrentDocument(),
               "the new current tab is the attached one");

    }
};

static SharedAudioDeviceTests sharedAudioDeviceTests;
