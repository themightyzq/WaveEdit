/*
  ==============================================================================

    ThemeManagerTests.cpp
    WaveEdit - Professional Audio Editor

    Tests for the Phase 1 colour theme infrastructure: the built-in
    Dark and Light themes, theme switching, settings persistence, and
    listener notification.

    The tests touch the global Settings singleton and the global
    ThemeManager singleton — they reset both to a known state before
    each test and restore the original theme at the end so the runner's
    other tests don't see leaked state.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "UI/ThemeManager.h"
#include "UI/WaveEditTheme.h"
#include "Utils/Settings.h"

namespace
{
    /** RAII guard: snapshot the active theme on construction, restore
        on destruction so test side-effects don't leak. */
    class ThemeRestorer
    {
    public:
        ThemeRestorer()
            : m_savedId(waveedit::ThemeManager::getInstance().getCurrent().id) {}
        ~ThemeRestorer()
        {
            waveedit::ThemeManager::getInstance().setActiveById(m_savedId);
        }
    private:
        juce::String m_savedId;
    };

    /** Counts ChangeBroadcaster notifications. */
    class CountingListener : public juce::ChangeListener
    {
    public:
        void changeListenerCallback(juce::ChangeBroadcaster*) override { ++count; }
        int count = 0;
    };
}

class ThemeManagerTests : public juce::UnitTest
{
public:
    ThemeManagerTests() : juce::UnitTest("ThemeManager", "Unit") {}

    void runTest() override
    {
        beginTest("Built-in themes have all tokens populated");
        {
            const auto dark  = waveedit::BuiltInThemes::dark();
            const auto light = waveedit::BuiltInThemes::light();

            for (const auto* t : { &dark, &light })
            {
                expect(t->id.isNotEmpty(),
                       "theme id non-empty (" + t->name + ")");
                expect(t->name.isNotEmpty(),
                       "theme name non-empty (" + t->id + ")");
                // Sanity-check a handful of representative tokens are not
                // accidentally transparent black (the "uninitialised" sentinel
                // that would slip through).
                expect(t->background.getARGB() != 0u,
                       "background populated (" + t->id + ")");
                expect(t->waveformLine.getARGB() != 0u,
                       "waveformLine populated (" + t->id + ")");
                expect(t->text.getARGB() != 0u,
                       "text populated (" + t->id + ")");
            }
        }

        beginTest("getAvailableIds includes dark and light");
        {
            const auto ids = waveedit::ThemeManager::getInstance().getAvailableIds();
            expect(ids.contains("dark"),  "available ids include dark");
            expect(ids.contains("light"), "available ids include light");
        }

        beginTest("setActiveById switches theme + persists to settings");
        {
            ThemeRestorer guard;
            auto& tm = waveedit::ThemeManager::getInstance();

            tm.setActiveById("dark");
            expect(tm.getCurrent().id == "dark");

            tm.setActiveById("light");
            expect(tm.getCurrent().id == "light",
                   "switched to light");

            // Verify it was persisted.
            const auto persisted = Settings::getInstance()
                                       .getSetting("display.theme", "")
                                       .toString();
            expect(persisted == "light",
                   "settings.display.theme == light, was: " + persisted);
        }

        beginTest("setActiveById is a no-op when theme already active");
        {
            ThemeRestorer guard;
            auto& tm = waveedit::ThemeManager::getInstance();

            tm.setActiveById("dark");

            CountingListener listener;
            tm.addChangeListener(&listener);
            tm.setActiveById("dark");  // already active
            expectEquals(listener.count, 0,
                         "no broadcast when theme unchanged");
            tm.removeChangeListener(&listener);
        }

        beginTest("setActiveById ignores unknown theme ids");
        {
            ThemeRestorer guard;
            auto& tm = waveedit::ThemeManager::getInstance();
            tm.setActiveById("dark");
            const auto before = tm.getCurrent().id;

            tm.setActiveById("magenta-disco");  // bogus
            expect(tm.getCurrent().id == before,
                   "unknown id leaves theme unchanged");
        }

        beginTest("Listeners receive a notification on theme switch");
        {
            ThemeRestorer guard;
            auto& tm = waveedit::ThemeManager::getInstance();

            tm.setActiveById("dark");

            CountingListener listener;
            tm.addChangeListener(&listener);

            tm.setActiveById("light");
            expectEquals(listener.count, 1,
                         "exactly one notification per switch");

            tm.setActiveById("dark");
            expectEquals(listener.count, 2);

            tm.removeChangeListener(&listener);
        }

        beginTest("reloadFromSettings respects persisted theme id");
        {
            ThemeRestorer guard;
            auto& tm = waveedit::ThemeManager::getInstance();

            Settings::getInstance().setSetting("display.theme", "light");
            tm.reloadFromSettings();
            expect(tm.getCurrent().id == "light",
                   "reload pulled in light from settings");

            Settings::getInstance().setSetting("display.theme", "dark");
            tm.reloadFromSettings();
            expect(tm.getCurrent().id == "dark");
        }

        beginTest("reloadFromSettings falls back to dark on garbage value");
        {
            ThemeRestorer guard;
            Settings::getInstance().setSetting("display.theme", "potato");
            waveedit::ThemeManager::getInstance().reloadFromSettings();
            expect(waveedit::ThemeManager::getInstance().getCurrent().id == "dark",
                   "garbage settings value → dark fallback");
            // Restore the known good state.
            Settings::getInstance().setSetting("display.theme", "dark");
        }
    }
};

static ThemeManagerTests themeManagerTests;
