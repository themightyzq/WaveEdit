/*
  ==============================================================================

    MarkerManagerTests.cpp
    WaveEdit - Professional Audio Editor

    Coverage for the marker subsystem (TODO acknowledged this gap).
    Exercises Marker basic operations, JSON round-trip, and
    MarkerManager lifecycle, sorting, navigation, selection, and
    sidecar persistence.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Utils/Marker.h"
#include "Utils/MarkerManager.h"

//==============================================================================
class MarkerBasicTests : public juce::UnitTest
{
public:
    MarkerBasicTests() : juce::UnitTest("Marker Basic", "MarkerSystem") {}

    void runTest() override
    {
        beginTest("Default marker is at sample 0");
        {
            Marker m;
            expectEquals(m.getPosition(), (int64_t) 0);
        }

        beginTest("Constructor sets fields");
        {
            Marker m("Chorus", 44100, juce::Colours::red);
            expectEquals(m.getName(), juce::String("Chorus"));
            expectEquals(m.getPosition(), (int64_t) 44100);
            expect(m.getColor() == juce::Colours::red, "color set");
        }

        beginTest("Setters update fields");
        {
            Marker m;
            m.setName("Verse");
            m.setPosition(22050);
            m.setColor(juce::Colours::blue);
            expectEquals(m.getName(), juce::String("Verse"));
            expectEquals(m.getPosition(), (int64_t) 22050);
            expect(m.getColor() == juce::Colours::blue, "color updated");
        }

        beginTest("Position cannot go negative");
        {
            Marker m;
            m.setPosition(-100);
            expect(m.getPosition() >= 0, "position clamped to non-negative");
        }

        beginTest("isNear honors tolerance");
        {
            Marker m("X", 1000);
            expect(m.isNear(1000, 0));
            expect(m.isNear(1005, 10));
            expect(! m.isNear(1015, 10));
        }

        beginTest("getPositionInSeconds converts using sample rate");
        {
            Marker m("X", 44100);
            expectWithinAbsoluteError(m.getPositionInSeconds(44100.0), 1.0, 1e-9);
            expectWithinAbsoluteError(m.getPositionInSeconds(48000.0),
                                      44100.0 / 48000.0, 1e-9);
        }

        beginTest("JSON round-trip preserves name/position/color");
        {
            Marker original("Bridge", 88200, juce::Colours::green);
            const auto json = original.toJSON();
            const Marker restored = Marker::fromJSON(json);
            expectEquals(restored.getName(), original.getName());
            expectEquals(restored.getPosition(), original.getPosition());
            expect(restored.getColor() == original.getColor(), "color round-trip");
        }
    }
};
static MarkerBasicTests markerBasicTests;

//==============================================================================
class MarkerManagerTests : public juce::UnitTest
{
public:
    MarkerManagerTests() : juce::UnitTest("MarkerManager", "MarkerSystem") {}

    void runTest() override
    {
        beginTest("Empty manager reports zero markers");
        {
            MarkerManager mgr;
            expectEquals(mgr.getNumMarkers(), 0);
            expect(mgr.getMarker(0) == nullptr);
            expectEquals(mgr.getSelectedMarkerIndex(), -1);
        }

        beginTest("Markers are kept sorted by position on add");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("late",  3000));
            mgr.addMarker(Marker("early", 1000));
            mgr.addMarker(Marker("mid",   2000));

            expectEquals(mgr.getNumMarkers(), 3);
            expectEquals(mgr.getMarker(0)->getName(), juce::String("early"));
            expectEquals(mgr.getMarker(1)->getName(), juce::String("mid"));
            expectEquals(mgr.getMarker(2)->getName(), juce::String("late"));
        }

        beginTest("Remove and removeAll");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("a", 100));
            mgr.addMarker(Marker("b", 200));
            mgr.addMarker(Marker("c", 300));
            expectEquals(mgr.getNumMarkers(), 3);

            mgr.removeMarker(1);
            expectEquals(mgr.getNumMarkers(), 2);
            expectEquals(mgr.getMarker(0)->getName(), juce::String("a"));
            expectEquals(mgr.getMarker(1)->getName(), juce::String("c"));

            mgr.removeAllMarkers();
            expectEquals(mgr.getNumMarkers(), 0);
        }

        beginTest("Out-of-range remove is a no-op");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("only", 100));
            mgr.removeMarker(-1);
            mgr.removeMarker(99);
            expectEquals(mgr.getNumMarkers(), 1);
        }

        beginTest("findMarkerAtSample uses tolerance");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("a", 1000));
            mgr.addMarker(Marker("b", 5000));

            expectEquals(mgr.findMarkerAtSample(1005, 10), 0);
            expectEquals(mgr.findMarkerAtSample(5000, 0),  1);
            expectEquals(mgr.findMarkerAtSample(2500, 10), -1);
        }

        beginTest("getNextMarkerIndex / getPreviousMarkerIndex");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("a", 1000));
            mgr.addMarker(Marker("b", 2000));
            mgr.addMarker(Marker("c", 3000));

            expectEquals(mgr.getNextMarkerIndex(0),    0);
            expectEquals(mgr.getNextMarkerIndex(1500), 1);
            expectEquals(mgr.getNextMarkerIndex(3000), -1); // nothing after last

            expectEquals(mgr.getPreviousMarkerIndex(2500), 1);
            expectEquals(mgr.getPreviousMarkerIndex(1000), -1); // nothing strictly before first
        }

        beginTest("Selection set/clear");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("a", 100));
            mgr.addMarker(Marker("b", 200));

            mgr.setSelectedMarkerIndex(1);
            expectEquals(mgr.getSelectedMarkerIndex(), 1);

            mgr.clearSelection();
            expectEquals(mgr.getSelectedMarkerIndex(), -1);

            mgr.setSelectedMarkerIndex(99); // invalid
            expect(mgr.getSelectedMarkerIndex() < mgr.getNumMarkers(),
                   "selected index never points past end");
        }

        beginTest("getAllMarkers returns a snapshot copy");
        {
            MarkerManager mgr;
            mgr.addMarker(Marker("a", 100));
            mgr.addMarker(Marker("b", 200));

            auto snapshot = mgr.getAllMarkers();
            expectEquals(snapshot.size(), 2);

            // Mutating the live manager should not affect the snapshot.
            mgr.removeAllMarkers();
            expectEquals(snapshot.size(), 2);
            expectEquals(mgr.getNumMarkers(), 0);
        }

        beginTest("Sidecar JSON round-trip");
        {
            // Use a unique temp file so concurrent tests don't collide.
            juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
            juce::File audioFile = tempDir.getNonexistentChildFile(
                "WaveEdit_MarkerTest", ".wav", false);

            // Touch the audio file so getMarkerFilePath produces a stable sidecar path.
            audioFile.create();

            {
                MarkerManager src;
                src.addMarker(Marker("intro", 1000, juce::Colours::red));
                src.addMarker(Marker("verse", 2000, juce::Colours::green));
                src.addMarker(Marker("outro", 3000, juce::Colours::blue));

                expect(src.saveToFile(audioFile), "save sidecar");
            }

            const auto sidecar = MarkerManager::getMarkerFilePath(audioFile);
            expect(sidecar.existsAsFile(), "sidecar file written");

            {
                MarkerManager dst;
                expect(dst.loadFromFile(audioFile), "load sidecar");
                expectEquals(dst.getNumMarkers(), 3);
                expectEquals(dst.getMarker(0)->getName(), juce::String("intro"));
                expectEquals(dst.getMarker(2)->getPosition(), (int64_t) 3000);
            }

            // Cleanup
            sidecar.deleteFile();
            audioFile.deleteFile();
        }
    }
};
static MarkerManagerTests markerManagerTests;
