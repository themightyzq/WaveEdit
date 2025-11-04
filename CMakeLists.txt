cmake_minimum_required(VERSION 3.15)

# Project definition
project(WaveEdit VERSION 0.1.0 LANGUAGES C CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# If you want to use JUCE's bundled modules
juce_add_gui_app(WaveEdit
    COMPANY_NAME "WaveEdit"
    PRODUCT_NAME "WaveEdit"
    BUNDLE_ID "com.waveedit.app"

    # Icon files (optional, can add later)
    # ICON_BIG ...
    # ICON_SMALL ...
)

# Target sources
target_sources(WaveEdit
    PRIVATE
        Source/Main.cpp
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioBufferManager.cpp
        Source/Audio/AudioBufferManager.h
        Source/Audio/AudioFileManager.cpp
        Source/Audio/AudioFileManager.h
        Source/Audio/AudioProcessor.cpp
        Source/Audio/AudioProcessor.h
        Source/Commands/CommandIDs.h
        Source/Utils/Settings.cpp
        Source/Utils/Settings.h
        Source/Utils/BWFMetadata.cpp
        Source/Utils/BWFMetadata.h
        Source/Utils/iXMLMetadata.cpp
        Source/Utils/iXMLMetadata.h
        Source/Utils/AudioClipboard.cpp
        Source/Utils/AudioClipboard.h
        Source/Utils/AudioBufferInputSource.cpp
        Source/Utils/AudioBufferInputSource.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/TabComponent.cpp
        Source/UI/TabComponent.h
        Source/UI/ErrorDialog.cpp
        Source/UI/ErrorDialog.h
        Source/UI/SettingsPanel.cpp
        Source/UI/SettingsPanel.h
        Source/UI/FilePropertiesDialog.cpp
        Source/UI/FilePropertiesDialog.h
        Source/UI/BWFEditorDialog.cpp
        Source/UI/BWFEditorDialog.h
        Source/UI/iXMLEditorDialog.cpp
        Source/UI/iXMLEditorDialog.h
        Source/UI/GoToPositionDialog.cpp
        Source/UI/GoToPositionDialog.h
        Source/UI/KeyboardCheatSheetDialog.cpp
        Source/UI/KeyboardCheatSheetDialog.h
        Source/UI/EditRegionBoundariesDialog.cpp
        Source/UI/EditRegionBoundariesDialog.h
        Source/UI/StripSilenceDialog.cpp
        Source/UI/StripSilenceDialog.h
        Source/UI/BatchExportDialog.cpp
        Source/UI/BatchExportDialog.h
        Source/UI/ShortcutEditorPanel.cpp
        Source/UI/ShortcutEditorPanel.h
        Source/Utils/Document.cpp
        Source/Utils/Document.h
        Source/Utils/DocumentManager.cpp
        Source/Utils/DocumentManager.h
        Source/Utils/Region.cpp
        Source/Utils/Region.h
        Source/Utils/RegionManager.cpp
        Source/Utils/RegionManager.h
        Source/Utils/RegionExporter.cpp
        Source/Utils/RegionExporter.h
        Source/Utils/Marker.cpp
        Source/Utils/Marker.h
        Source/Utils/MarkerManager.cpp
        Source/Utils/MarkerManager.h
        Source/Utils/KeymapManager.cpp
        Source/Utils/KeymapManager.h
        Source/Utils/UCSCategorySuggester.cpp
        Source/Utils/UCSCategorySuggester.h
        Source/UI/RegionDisplay.cpp
        Source/UI/RegionDisplay.h
        Source/UI/RegionListPanel.cpp
        Source/UI/RegionListPanel.h
        Source/UI/MarkerDisplay.cpp
        Source/UI/MarkerDisplay.h
)

# JUCE modules required for audio editing application
target_link_libraries(WaveEdit
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Preprocessor definitions
target_compile_definitions(WaveEdit
    PRIVATE
        # JUCE settings
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_VERSION>"

        # Enable modal loops for dialogs (required for BatchExportDialog, FilePropertiesDialog, etc.)
        JUCE_MODAL_LOOPS_PERMITTED=1

        # VST3 support (for future phases)
        # JUCE_PLUGINHOST_VST3=1
)

# Include directories
target_include_directories(WaveEdit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_COREAUDIO_ENABLED=1
    )
    # macOS 10.13+ required
    set_target_properties(WaveEdit PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.waveedit.app"
        MACOSX_BUNDLE_BUNDLE_NAME "WaveEdit"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
elseif(WIN32)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_WASAPI_ENABLED=1
        JUCE_DIRECTSOUND_ENABLED=0
    )
elseif(UNIX)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_ALSA_ENABLED=1
        JUCE_JACK_ENABLED=1
    )
endif()

# Copy keyboard template resources to app bundle
if(APPLE)
    # On macOS, copy templates to app bundle Resources directory
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Keymaps"
        COMMENT "Copying keyboard templates to app bundle"
    )
elseif(WIN32)
    # On Windows, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMENT "Copying keyboard templates to executable directory"
    )
else()
    # On Linux, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMENT "Copying keyboard templates to executable directory"
    )
endif()

# Copy resources (for future phases - icons, fonts, etc.)
# file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets
#      DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Installation rules (optional)
install(TARGETS WaveEdit
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# ============================================================================
# Automated Testing Infrastructure
# ============================================================================

# Option to enable/disable test building
option(WAVEEDIT_BUILD_TESTS "Build WaveEdit automated tests" ON)

if(WAVEEDIT_BUILD_TESTS)
    message(STATUS "Building WaveEdit automated tests")

    # Create a static library from Source/ files for test linking
    # This avoids having Main.cpp's main() function conflict with test runner
    add_library(WaveEditCore STATIC
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioBufferManager.cpp
        Source/Audio/AudioBufferManager.h
        Source/Audio/AudioFileManager.cpp
        Source/Audio/AudioFileManager.h
        Source/Audio/AudioProcessor.cpp
        Source/Audio/AudioProcessor.h
        Source/Commands/CommandIDs.h
        Source/Utils/Settings.cpp
        Source/Utils/Settings.h
        Source/Utils/BWFMetadata.cpp
        Source/Utils/BWFMetadata.h
        Source/Utils/iXMLMetadata.cpp
        Source/Utils/iXMLMetadata.h
        Source/Utils/AudioClipboard.cpp
        Source/Utils/AudioClipboard.h
        Source/Utils/AudioBufferInputSource.cpp
        Source/Utils/AudioBufferInputSource.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/TabComponent.cpp
        Source/UI/TabComponent.h
        Source/UI/ErrorDialog.cpp
        Source/UI/ErrorDialog.h
        Source/UI/SettingsPanel.cpp
        Source/UI/SettingsPanel.h
        Source/UI/FilePropertiesDialog.cpp
        Source/UI/FilePropertiesDialog.h
        Source/UI/BWFEditorDialog.cpp
        Source/UI/BWFEditorDialog.h
        Source/UI/iXMLEditorDialog.cpp
        Source/UI/iXMLEditorDialog.h
        Source/UI/GoToPositionDialog.cpp
        Source/UI/GoToPositionDialog.h
        Source/UI/KeyboardCheatSheetDialog.cpp
        Source/UI/KeyboardCheatSheetDialog.h
        Source/UI/EditRegionBoundariesDialog.cpp
        Source/UI/EditRegionBoundariesDialog.h
        Source/UI/StripSilenceDialog.cpp
        Source/UI/StripSilenceDialog.h
        Source/Utils/Document.cpp
        Source/Utils/Document.h
        Source/Utils/DocumentManager.cpp
        Source/Utils/DocumentManager.h
        Source/Utils/Region.cpp
        Source/Utils/Region.h
        Source/Utils/RegionManager.cpp
        Source/Utils/RegionManager.h
        Source/UI/RegionDisplay.cpp
        Source/UI/RegionDisplay.h
        Source/Utils/Marker.cpp
        Source/Utils/Marker.h
        Source/Utils/MarkerManager.cpp
        Source/Utils/MarkerManager.h
        Source/Utils/UCSCategorySuggester.cpp
        Source/Utils/UCSCategorySuggester.h
        Source/UI/MarkerDisplay.cpp
        Source/UI/MarkerDisplay.h
    )

    # Link WaveEditCore to JUCE modules
    target_link_libraries(WaveEditCore
        PUBLIC
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_data_structures
            juce::juce_events
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    # Include directories for WaveEditCore
    target_include_directories(WaveEditCore
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
            ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestUtils
    )

    # Preprocessor definitions for WaveEditCore
    target_compile_definitions(WaveEditCore
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_APPLICATION_NAME_STRING="WaveEdit"
            JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    )

    # Platform-specific settings for WaveEditCore
    if(APPLE)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_COREAUDIO_ENABLED=1
        )
    elseif(WIN32)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_WASAPI_ENABLED=1
            JUCE_DIRECTSOUND_ENABLED=0
        )
    elseif(UNIX)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_ALSA_ENABLED=1
            JUCE_JACK_ENABLED=1
        )
    endif()

    # Create test console application using JUCE's console app macro
    juce_add_console_app(WaveEditTests
        COMPANY_NAME "ZQ SFX"
        PRODUCT_NAME "WaveEdit Tests"
        BUNDLE_ID "com.zqsfx.waveedittests"
    )

    # Test source files
    # Note: Add test files as they are created
    target_sources(WaveEditTests
        PRIVATE
            Tests/TestRunner.cpp                                # Main test runner
            Tests/TestUtils/TestAudioFiles.h
            Tests/TestUtils/AudioAssertions.h
            Tests/Unit/AudioEngineTests.cpp                     # Week 2 ✅
            Tests/Unit/AudioBufferManagerTests.cpp              # Week 2 ✅
            Tests/Unit/AudioProcessorTests.cpp                  # Week 2 ✅
            Tests/Unit/BWFMetadataTests.cpp                     # Phase 4 ✅ (BWF Metadata System)
            Tests/Unit/BWFEditorDialogTests.cpp                 # Phase 4 ✅ (BWF Editor Dialog)
            Tests/Integration/FileIOTests.cpp                   # Week 3 ✅ (File I/O Integration)
            Tests/Unit/UndoRedoTests.cpp                        # Week 3 ✅ (Undo/Redo Data Integrity)
            Tests/Integration/MultiDocumentTests.cpp            # Week 4 ✅ (Multi-file architecture)
            Tests/Integration/InterFileClipboardTests.cpp       # Week 4 ✅ (Inter-file clipboard)
            Tests/Integration/PlaybackEditingIntegrationTests.cpp # Week 4 ✅ (Playback + Editing)
            Tests/Unit/Phase35FeaturesTests.cpp                 # Week 4 ✅ (Phase 3.5: Time Format, Auto-Save)
            Tests/Integration/EditingToolsTests.cpp             # Week 4 ✅ (Silence, Trim, DC Offset)
            Tests/Unit/GoToPositionDialogTests.cpp              # Week 4 ✅ (Go To Position parsing)
            Tests/Integration/RegionListBatchRenameTests.cpp    # Week 4 ✅ (Batch Rename regions)
            # Tests/EndToEnd/CompleteWorkflowTests.cpp          # Week 5
            # Tests/EndToEnd/StressTests.cpp                    # Week 5
            # Tests/Performance/WaveformRenderingBenchmarks.cpp # Week 6
            # Tests/Performance/PlaybackLatencyBenchmarks.cpp   # Week 6
    )

    # Link test executable to WaveEditCore and JUCE test utilities
    target_link_libraries(WaveEditTests
        PRIVATE
            WaveEditCore
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    # Include test utilities
    target_include_directories(WaveEditTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestUtils
    )

    message(STATUS "WaveEdit automated tests configured successfully")
    message(STATUS "Build tests with: cmake --build . --target WaveEditTests")
    message(STATUS "Run tests with: ./WaveEdit_artefacts/Debug/WaveEditTests (or Release)")

endif()
