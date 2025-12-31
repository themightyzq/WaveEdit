CLAUDE.md – AI Instructions & Architecture for WaveEdit by ZQ SFX

Project: WaveEdit – Professional Audio Editor
Developer: ZQ SFX
License: GPL v3
Tech Stack: JUCE 7.x, C++17, CMake

0. HOW CLAUDE MUST USE THIS FILE

NON-NEGOTIABLE RULE:
For every task, Claude Code must re-read this entire CLAUDE.md before doing any work. Do not rely on memory. Always refresh these instructions at the start of each request.

This file is the single source of truth for:
• How to reason about WaveEdit
• How to use the specialized agents
• How to structure and review code
• How to interact with the repository

If anything in a user prompt conflicts with this file, follow this file unless the user explicitly overrides it.

1. DOCUMENTATION STRUCTURE (SINGLE SOURCE OF TRUTH)

WaveEdit uses three core docs:

CLAUDE.md (this file)
AI instructions, multi-agent orchestration, architecture patterns, coding standards.

README.md
User guide, build instructions, keyboard shortcuts, feature overview.

TODO.md
Current status, roadmap, bug list, priorities.

Rules:
Do not duplicate content across these files.
If documentation is user-facing (how to use the app), update README.md.
If it is planning / backlog / status, update TODO.md.
If it is how AI should think or code, update CLAUDE.md.
When in doubt, link to other files rather than duplicating content.

2. PROJECT OVERVIEW

WaveEdit is a professional audio editor inspired by Sound Forge Pro, built with JUCE.  
Key characteristics:
• Sub-1 second startup
• Sample-accurate editing
• Keyboard-first workflow
• File-based (no project/session format)
• Mono/Stereo up to 8 channels
• WAV primary format, FLAC, MP3, OGG where implemented

Target users: audio engineers, podcasters, sound designers, game developers.

Core philosophy:
1. Speed – UI and operations must be fast and predictable.
2. Sample accuracy – All editing operates on audio units (samples, milliseconds), never pixels.
3. Keyboard-first – Every important action has a shortcut.
4. No bloat – No cloud features, no social features, no multi-track.
5. Professional quality – Behaves like a tool used to ship game assets.

3. TECH STACK AND CONSTRAINTS

Framework: JUCE 7.x+
Language: C++17+
Build System: CMake

Build script (source of truth):
./build-and-run.command
./build-and-run.command clean
./build-and-run.command debug

Audio I/O: CoreAudio / WASAPI / ALSA via JUCE
Data Model: file-based only

Never:
• Edit JUCE/ submodule.
• Add multi-track/MIDI/video/cloud features unless explicitly requested.

Refer to README.md for detailed build environment / platform notes.

4. MULTI-AGENT SYSTEM OVERVIEW

WaveEdit uses a multi-agent Claude Code system. Each agent lives in .claude/agents/.

Claude must:
1. Select correct agent(s) for the task.
2. Orchestrate agents in a pipeline.
3. Enforce global rules and safety protocols from this file.
4. Keep edits minimal, safe, and verifiable.

4.1 AGENT INDEX (SUMMARY OF ALL AGENTS)

system-architect
High-level architecture, separation of concerns, major structural changes, long-term design.

juce-editor-expert
Audio editor code: waveform rendering, AudioTransportSource, AudioThumbnail, file I/O, region systems, multi-document architecture.

juce-ui-designer
UI component implementation, layout, LookAndFeel, typography, interactions.

juce-build-infra
CMake, CI/CD, build scripts, presets, compiler flags.

keyboard-shortcut-architect
Design keyboard shortcuts and templates. Detect conflicts. Maintain Sound Forge / Pro Tools alignment.

code-reviewer
Review code for correctness, maintainability, performance, safety.

code-refactorer
Improve readability and structure of working code; remove duplication; maintain behavior.

test-generator
Generate unit, integration, E2E tests.

audio-qa-specialist
End-to-end QA for file editor workflows: open → edit → playback → save, regions, undo/redo, large files.

repo-organizer
Maintain repo cleanliness; detect dead code; ensure correct directory structure; clean test artifacts.

release-security-manager
Release prep: versioning, notices, crash reporting setup, installers, license compliance.

security-scanner
Security audits for sensitive codepaths.

4.2 HOW TO CHOOSE AGENTS

Default rule:
If the task touches multiple modules or is non-trivial, start with system-architect, then delegate to specialists.

Examples:
Implementing new editing features: system-architect → juce-editor-expert → code-reviewer → test-generator → audio-qa-specialist.
Pure UI layout: juce-ui-designer → code-reviewer.
Keyboard shortcut work: keyboard-shortcut-architect → juce-editor-expert (for wiring).
Refactor only: code-refactorer → code-reviewer → test-generator.
Build/CI: juce-build-infra → code-reviewer → repo-organizer.
Security or release: release-security-manager → security-scanner → repo-organizer.

5. ORCHESTRATED WORKFLOWS

5.1 NEW FEATURE IMPLEMENTATION PIPELINE

1. Understand requirements.
2. Architecture plan (system-architect):
   - Identify affected modules and files.
   - Propose minimal, clean design.
   - Identify risks: threading, performance, undo/redo, regions, metadata.
3. Implementation (juce-editor-expert and/or juce-ui-designer):
   - Follow design exactly.
   - Use minimal diffs.
   - Wire commands.
   - Maintain multi-file architecture rules.
4. Code Review (code-reviewer).
5. Testing (test-generator).
6. Audio QA (audio-qa-specialist).
7. Repo hygiene (repo-organizer).
8. Completion only if all checks pass.

5.2 BUG FIX WORKFLOW

1. Reproduce the bug and restate it.
2. Diagnose:
   - system-architect for systemic issues
   - direct analysis for localized issues
3. Fix with minimal changes.
4. Add regression tests (test-generator).
5. Functional QA (audio-qa-specialist).
6. Review (code-reviewer).

5.3 REFACTOR WORKFLOW

1. Scope refactor (system-architect).
2. Refactor (code-refactorer).
3. Review (code-reviewer).
4. Add tests if safety is unclear (test-generator).

5.4 BUILD-INFRA WORKFLOW

1. juce-build-infra
2. code-reviewer
3. repo-organizer
4. release-security-manager (as needed)

5.5 RELEASE WORKFLOW

1. release-security-manager
2. security-scanner
3. repo-organizer
4. audio-qa-specialist

6. GLOBAL PROTOCOLS FOR ALL AGENTS

6.1 PREFLIGHT PROTOCOL (ALWAYS DO THIS FIRST)

1. Re-read CLAUDE.md.
2. Restate task.
3. Identify affected modules and risks.
4. Choose agents and justify their order.
5. Present a short plan before implementation.

6.2 MINIMAL-DIFF PROTOCOL

1. Prefer surgical edits.
2. Do not rewrite entire files unless explicitly needed.
3. Do not change unrelated code.
4. Do not rename public APIs unless requested.
5. Maintain formatting.

6.3 REVIEW AND TEST PROTOCOL

After any significant change:
1. Run code-reviewer.
2. Run test-generator.
3. Run audio-qa-specialist for editor features.

6.4 THREAD SAFETY PROTOCOL

Audio thread:
• Only DSP / getNextAudioBlock().
• No allocations, no locks, no file I/O, no logging.

Message thread:
• UI, file I/O, dialogs, logic.

Background threads:
• File loading, thumbnail generation, long DSP.

Never:  
Modify shared data across threads without synchronization.
Do heavy work in audio callback.

6.5 BUFFER UPDATE PROTOCOL

When audio buffer changes during playback, flush AudioTransportSource:

bool wasPlaying = isPlaying();
double position = getCurrentPosition();
m_transportSource.setSource(nullptr);
m_bufferSource->setBuffer(buffer, sampleRate, false);
m_transportSource.setSource(m_bufferSource.get(), 0, nullptr, sampleRate, numChannels);
if (wasPlaying) {
    m_transportSource.setPosition(position);
    m_transportSource.start();
}

Always prefer calling a helper like AudioEngine::reloadBufferPreservingPlayback().

6.6 JUCE IIR FILTER COEFFICIENT PROTOCOL

When implementing filter curve visualization or any code that reads JUCE IIR filter coefficients:

CRITICAL: JUCE normalizes biquad coefficients by a0 and stores only 5 elements, NOT 6.

Standard biquad form has 6 coefficients: [b0, b1, b2, a0, a1, a2]
JUCE's juce_IIRFilter_Impl.h assignImpl() normalizes by a0 and skips it during storage.

Storage format (5 elements):
  coeffs[0] = b0/a0
  coeffs[1] = b1/a0
  coeffs[2] = b2/a0
  coeffs[3] = a1/a0
  coeffs[4] = a2/a0

When calculating frequency response using the z-transform:
  H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)

Correct coefficient extraction:
  float b0 = coeffs[0];
  float b1 = coeffs[1];
  float b2 = coeffs[2];
  float a1 = coeffs[3];  // NOT coeffs[4]
  float a2 = coeffs[4];  // NOT coeffs[5]

Size check: if (coeffs.size() < 5) return identity;  // NOT < 6

Reference: JUCE/modules/juce_dsp/filter_design/juce_IIRFilter_Impl.h
  - assignImpl() at lines 41-58 shows skipping a0 index
  - process() at lines 138-161 shows reading indices 0,1,2,3,4

This applies to ALL JUCE IIR filters including:
  - juce::dsp::IIR::Filter
  - juce::dsp::IIR::Coefficients
  - Any custom filter using JUCE's coefficient storage

6.7 KEYBOARD SHORTCUT AND COMMAND SYSTEM PROTOCOL

Every new command must:
• Have unique CommandID.
• Be in getAllCommands().
• Have text in getCommandInfo().
• Have handler in perform().
• Have default keypress assigned.
• Have no conflicts with system or other commands.
• Be added to the default keymap template.

Never:
• Override system-reserved shortcuts.
• Create duplicates.
• Forget to update templates.

7. CODING STANDARDS

Indentation: 4 spaces.
Braces: Allman style.
Naming:
Classes: PascalCase
Methods: camelCase
Members: m_ prefix
Constants: UPPER_SNAKE_CASE or kPascalCase
Enums: PascalCase with UPPER_SNAKE_CASE values.

Comments:
Use Doxygen for public methods.
Explain WHY, not what.

Memory management:
Use RAII and smart pointers.
Avoid raw new/delete.

8. ARCHITECTURE PATTERNS

Component hierarchy:

MainComponent
  WaveformDisplay
  TransportControls
  Meters
  SettingsPanel

Rules:
Separate UI from audio logic.
Use message thread for UI.
Use ChangeBroadcaster / ChangeListener for communication.

Multi-file Document architecture:
Each open file = its own Document.
Each Document has independent AudioEngine, WaveformDisplay, UndoManager, RegionManager, playback state.

DocumentManager handles tab switching, save prompts, inter-file clipboard.

Region System:
Region: name, start, end, color.
RegionManager: create, rename, delete, merge, split, navigate.
Persistence: JSON sidecar .wav.regions.json
Batch export: each region -> its own WAV.

9. PERFORMANCE TARGETS

Startup < 1 second.
Load 10-min WAV < 2 seconds.
Waveform rendering: 60fps.
Playback latency < 10ms.
Save < 500ms.

Avoid:
Re-rendering full waveform.
Allocations in audio thread.
Blocking file I/O on message thread.

10. QUALITY ASSURANCE

10.1 QC BEFORE MARKING COMPLETE

1. Code Review.
2. Automated Tests all pass.
3. Functional end-to-end workflow tested.
4. Manual verification.
5. No P0/P1 issues left.

10.2 UI FEATURE CHECKLIST
UI shows up
Shortcut works
Menu works
Controls wired
Callbacks fire
No console errors
Processing dialogs must have Preview button

10.3 SHORTCUT CHECKLIST
CommandID added
Info added
Handler implemented
Template updated
No conflicts
Tested in app

10.4 DATA PERSISTENCE CHECKLIST
Data persists
Loads correctly
UI updates
Undo/redo correct
No crashes/corruption

11. WHAT TO AVOID

Do not add:
• Multi-track
• MIDI
• Video
• Cloud sync
• Social features
• Telemetry beyond crash reporting

Anti-patterns:
• Project/session files
• Global mutable state
• Duplicated behavior

12. BUILD INSTRUCTIONS (REFERENCE)

Use build script:
./build-and-run.command
./build-and-run.command clean
./build-and-run.command debug

Manual CMake only when requested.

13. RESOURCES WHEN STUCK

Re-read CLAUDE.md.
Check existing code patterns.
Use JUCE docs/forum.
Check TODO.md.
Ask user only if necessary.

14. FINAL NOTES

WaveEdit is a professional audio tool. Prioritize:
1. Speed
2. Accuracy
3. Usability
4. Simplicity

When unsure, ask:
Would a Sound Forge user find this intuitive and robust?

Always re-read this file before starting work.