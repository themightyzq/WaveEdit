/*
  ==============================================================================

    AsyncApplyLifetimeTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression guard for C1: the plugin-chain / offline-plugin Apply paths run
    behind a non-modal ProgressDialog and commit their result on the message
    thread only if the document is still alive. They detect a mid-render close
    via a juce::Component::SafePointer<WaveformDisplay> lifeline.

    The full async race (worker thread + ProgressDialog + tab close) is not
    drivable in the console UnitTest harness. What IS testable -- and what the
    fix depends on -- is the PRECONDITION: closing a document synchronously
    destroys the WaveformDisplay the lifeline watches, so the SafePointer nulls.
    If a future refactor made document teardown deferred/async, the lifeline
    guard would silently stop protecting the commit, and this test would fail.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "UI/WaveformDisplay.h"

//==============================================================================
class AsyncApplyLifetimeTests : public juce::UnitTest
{
public:
    AsyncApplyLifetimeTests()
        : juce::UnitTest("Async Apply Lifetime (C1)", "Integration") {}

    void runTest() override
    {
        testCloseNullsLifeline();
        testCloseAtNullsLifeline();
        testSurvivingDocumentKeepsLifeline();
    }

private:
    void testCloseNullsLifeline()
    {
        beginTest("closeDocument() synchronously nulls the WaveformDisplay lifeline");

        DocumentManager docMgr;
        Document* doc = docMgr.createDocument();
        expect(doc != nullptr, "createDocument() returns a document");

        juce::Component::SafePointer<WaveformDisplay> lifeline(&doc->getWaveformDisplay());
        expect(lifeline.getComponent() != nullptr, "lifeline is live before close");

        const bool closed = docMgr.closeDocument(doc);
        expect(closed, "closeDocument() succeeds");
        expect(lifeline.getComponent() == nullptr,
               "lifeline nulls immediately after close -- the commit guard is effective");
    }

    void testCloseAtNullsLifeline()
    {
        beginTest("closeDocumentAt() nulls the lifeline for the right document");

        DocumentManager docMgr;
        Document* doc0 = docMgr.createDocument();
        Document* doc1 = docMgr.createDocument();

        juce::Component::SafePointer<WaveformDisplay> lifeline0(&doc0->getWaveformDisplay());
        juce::Component::SafePointer<WaveformDisplay> lifeline1(&doc1->getWaveformDisplay());

        expect(docMgr.closeDocumentAt(0), "closeDocumentAt(0) succeeds");
        expect(lifeline0.getComponent() == nullptr, "closed document's lifeline is null");
        expect(lifeline1.getComponent() != nullptr, "surviving document's lifeline stays live");
    }

    void testSurvivingDocumentKeepsLifeline()
    {
        beginTest("A live document keeps a non-null lifeline (commit proceeds)");

        DocumentManager docMgr;
        Document* doc = docMgr.createDocument();

        juce::Component::SafePointer<WaveformDisplay> lifeline(&doc->getWaveformDisplay());

        // No close: the async completion would see a live lifeline and commit.
        expect(lifeline.getComponent() == &doc->getWaveformDisplay(),
               "lifeline still points at the live display");

        docMgr.closeAllDocuments();
        expect(lifeline.getComponent() == nullptr, "lifeline nulls once the document is gone");
    }
};

static AsyncApplyLifetimeTests asyncApplyLifetimeTests;
