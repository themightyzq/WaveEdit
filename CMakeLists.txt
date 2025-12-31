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
        Source/Audio/RecordingEngine.cpp
        Source/Audio/RecordingEngine.h
        Source/DSP/ParametricEQ.cpp
        Source/DSP/ParametricEQ.h
        Source/DSP/DynamicParametricEQ.cpp
        Source/DSP/DynamicParametricEQ.h
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
        Source/Utils/ToolbarConfig.cpp
        Source/Utils/ToolbarConfig.h
        Source/Utils/ToolbarManager.cpp
        Source/Utils/ToolbarManager.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/CompactTransport.cpp
        Source/UI/CompactTransport.h
        Source/UI/ToolbarButton.cpp
        Source/UI/ToolbarButton.h
        Source/UI/CustomizableToolbar.cpp
        Source/UI/CustomizableToolbar.h
        Source/UI/ToolbarCustomizationDialog.cpp
        Source/UI/ToolbarCustomizationDialog.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/NewFileDialog.cpp
        Source/UI/NewFileDialog.h
        Source/UI/NormalizeDialog.cpp
        Source/UI/NormalizeDialog.h
        Source/UI/FadeInDialog.cpp
        Source/UI/FadeInDialog.h
        Source/UI/FadeOutDialog.cpp
        Source/UI/FadeOutDialog.h
        Source/UI/FadeCurvePreview.cpp
        Source/UI/FadeCurvePreview.h
        Source/UI/DCOffsetDialog.cpp
        Source/UI/DCOffsetDialog.h
        Source/UI/RecordingDialog.cpp
        Source/UI/RecordingDialog.h
        Source/UI/ParametricEQDialog.cpp
        Source/UI/ParametricEQDialog.h
        Source/UI/GraphicalEQEditor.cpp
        Source/UI/GraphicalEQEditor.h
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
        Source/UI/SaveAsOptionsPanel.cpp
        Source/UI/SaveAsOptionsPanel.h
        Source/UI/ShortcutEditorPanel.cpp
        Source/UI/ShortcutEditorPanel.h
        Source/UI/ProgressDialog.cpp
        Source/UI/ProgressDialog.h
        Source/Utils/ProgressCallback.h
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
        Source/UI/MarkerListPanel.cpp
        Source/UI/MarkerListPanel.h
        Source/UI/MarkerDisplay.cpp
        Source/UI/MarkerDisplay.h
        Source/UI/SpectrumAnalyzer.cpp
        Source/UI/SpectrumAnalyzer.h
        Source/UI/PluginManagerDialog.cpp
        Source/UI/PluginManagerDialog.h
        Source/UI/PluginChainPanel.cpp
        Source/UI/PluginChainPanel.h
        Source/UI/PluginChainWindow.cpp
        Source/UI/PluginChainWindow.h
        Source/UI/PluginEditorWindow.cpp
        Source/UI/PluginEditorWindow.h
        Source/UI/OfflinePluginDialog.cpp
        Source/UI/OfflinePluginDialog.h
        Source/Plugins/PluginManager.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginChain.cpp
        Source/Plugins/PluginChain.h
        Source/Plugins/PluginChainNode.cpp
        Source/Plugins/PluginChainNode.h
        Source/Plugins/PluginPresetManager.cpp
        Source/Plugins/PluginPresetManager.h
        Source/Plugins/PluginScannerProtocol.h
        Source/Plugins/PluginScannerWorker.cpp
        Source/Plugins/PluginScannerWorker.h
        Source/Plugins/PluginScannerCoordinator.cpp
        Source/Plugins/PluginScannerCoordinator.h
        Source/Plugins/PluginScanState.h
        Source/Plugins/PluginScanDialogs.cpp
        Source/Plugins/PluginScanDialogs.h
        Source/Plugins/PluginPathsPanel.cpp
        Source/Plugins/PluginPathsPanel.h
        Source/Plugins/PluginChainRenderer.cpp
        Source/Plugins/PluginChainRenderer.h
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
        juce::juce_dsp
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
        JUCE_USE_MP3AUDIOFORMAT=1  # Enable MP3 encoding with LAME
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_VERSION>"

        # Enable modal loops for dialogs (required for BatchExportDialog, FilePropertiesDialog, etc.)
        JUCE_MODAL_LOOPS_PERMITTED=1

        # VST3 Plugin Hosting Support
        JUCE_PLUGINHOST_VST3=1
        # AudioUnit Plugin Hosting Support (macOS only)
        JUCE_PLUGINHOST_AU=1
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

# ============================================================================
# Bundle LAME MP3 Encoder Library
# ============================================================================
# For end-user releases, bundle LAME library in app bundle/installation
# This provides zero-friction MP3 encoding support without requiring users
# to manually install LAME. LGPL compliance: dynamically linked, source
# code available at https://lame.sourceforge.io/

if(APPLE)
    # Find LAME library - check both Homebrew locations
    find_library(LAME_LIBRARY
        NAMES mp3lame libmp3lame
        PATHS
            /opt/homebrew/lib          # Apple Silicon Homebrew
            /usr/local/lib             # Intel Homebrew
            /opt/local/lib             # MacPorts
        NO_DEFAULT_PATH
    )

    if(LAME_LIBRARY)
        message(STATUS "Found LAME library: ${LAME_LIBRARY}")

        # For Release builds, bundle LAME into app Frameworks
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks"
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_LIBRARY}"
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libmp3lame.dylib"
                COMMAND install_name_tool -id
                    "@rpath/libmp3lame.dylib"
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libmp3lame.dylib"
                COMMENT "Bundling LAME MP3 encoder library"
            )
            message(STATUS "LAME will be bundled in Release builds")
        else()
            message(STATUS "Debug build - LAME not bundled (use system library)")
        endif()
    else()
        message(WARNING "LAME library not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Install with: brew install lame")
    endif()

    # Set rpath to find bundled libraries
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

elseif(WIN32)
    # Windows: Look for LAME DLL
    find_file(LAME_DLL
        NAMES lame_enc.dll libmp3lame.dll
        PATHS
            "C:/Program Files/LAME"
            "C:/Program Files (x86)/LAME"
            "${CMAKE_SOURCE_DIR}/ThirdParty/lame/bin"
    )

    if(LAME_DLL)
        message(STATUS "Found LAME DLL: ${LAME_DLL}")

        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_DLL}"
                    "$<TARGET_FILE_DIR:WaveEdit>"
                COMMENT "Bundling LAME MP3 encoder DLL"
            )
            message(STATUS "LAME DLL will be bundled in Release builds")
        endif()
    else()
        message(WARNING "LAME DLL not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Download from: https://lame.sourceforge.io/")
    endif()

elseif(UNIX)
    # Linux: Look for LAME shared object
    find_library(LAME_LIBRARY
        NAMES mp3lame libmp3lame
        PATHS
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
    )

    if(LAME_LIBRARY)
        message(STATUS "Found LAME library: ${LAME_LIBRARY}")

        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_LIBRARY}"
                    "$<TARGET_FILE_DIR:WaveEdit>"
                COMMENT "Bundling LAME MP3 encoder library"
            )
            message(STATUS "LAME will be bundled in Release builds")
        endif()
    else()
        message(WARNING "LAME library not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Install with: sudo apt-get install libmp3lame0")
    endif()

    # Set rpath to find bundled libraries
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# ============================================================================
# Copy keyboard template resources to app bundle
# ============================================================================
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

# ============================================================================
# Copy toolbar template resources to app bundle
# ============================================================================
if(APPLE)
    # On macOS, copy templates to app bundle Resources directory
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Toolbars"
        COMMENT "Copying toolbar templates to app bundle"
    )
elseif(WIN32)
    # On Windows, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMENT "Copying toolbar templates to executable directory"
    )
else()
    # On Linux, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMENT "Copying toolbar templates to executable directory"
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
        Source/DSP/ParametricEQ.cpp
        Source/DSP/ParametricEQ.h
        Source/DSP/DynamicParametricEQ.cpp
        Source/DSP/DynamicParametricEQ.h
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
        Source/Utils/ToolbarConfig.cpp
        Source/Utils/ToolbarConfig.h
        Source/Utils/ToolbarManager.cpp
        Source/Utils/ToolbarManager.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/CompactTransport.cpp
        Source/UI/CompactTransport.h
        Source/UI/ToolbarButton.cpp
        Source/UI/ToolbarButton.h
        Source/UI/CustomizableToolbar.cpp
        Source/UI/CustomizableToolbar.h
        Source/UI/ToolbarCustomizationDialog.cpp
        Source/UI/ToolbarCustomizationDialog.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/NewFileDialog.cpp
        Source/UI/NewFileDialog.h
        Source/UI/NormalizeDialog.cpp
        Source/UI/NormalizeDialog.h
        Source/UI/FadeInDialog.cpp
        Source/UI/FadeInDialog.h
        Source/UI/FadeOutDialog.cpp
        Source/UI/FadeOutDialog.h
        Source/UI/FadeCurvePreview.cpp
        Source/UI/FadeCurvePreview.h
        Source/UI/DCOffsetDialog.cpp
        Source/UI/DCOffsetDialog.h
        Source/UI/RecordingDialog.cpp
        Source/UI/RecordingDialog.h
        Source/UI/ParametricEQDialog.cpp
        Source/UI/ParametricEQDialog.h
        Source/UI/GraphicalEQEditor.cpp
        Source/UI/GraphicalEQEditor.h
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
        Source/UI/SpectrumAnalyzer.cpp
        Source/UI/SpectrumAnalyzer.h
        Source/UI/PluginManagerDialog.cpp
        Source/UI/PluginManagerDialog.h
        Source/UI/PluginChainPanel.cpp
        Source/UI/PluginChainPanel.h
        Source/UI/PluginChainWindow.cpp
        Source/UI/PluginChainWindow.h
        Source/UI/PluginEditorWindow.cpp
        Source/UI/PluginEditorWindow.h
        Source/UI/OfflinePluginDialog.cpp
        Source/UI/OfflinePluginDialog.h
        Source/Plugins/PluginManager.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginChain.cpp
        Source/Plugins/PluginChain.h
        Source/Plugins/PluginChainNode.cpp
        Source/Plugins/PluginChainNode.h
        Source/Plugins/PluginScannerProtocol.h
        Source/Plugins/PluginScannerWorker.cpp
        Source/Plugins/PluginScannerWorker.h
        Source/Plugins/PluginScannerCoordinator.cpp
        Source/Plugins/PluginScannerCoordinator.h
        Source/Plugins/PluginScanState.h
        Source/Plugins/PluginScanDialogs.cpp
        Source/Plugins/PluginScanDialogs.h
        Source/Plugins/PluginPathsPanel.cpp
        Source/Plugins/PluginPathsPanel.h
        Source/Plugins/PluginChainRenderer.cpp
        Source/Plugins/PluginChainRenderer.h
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
            juce::juce_dsp
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
            JUCE_USE_MP3AUDIOFORMAT=1  # Enable MP3 encoding with LAME
            JUCE_APPLICATION_NAME_STRING="WaveEdit"
            JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
            # VST3 Plugin Hosting Support
            JUCE_PLUGINHOST_VST3=1
            # AudioUnit Plugin Hosting Support (macOS only)
            JUCE_PLUGINHOST_AU=1
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

    # ==============================================================================
    # OPTIONAL: Automated Tests (only if Tests/ directory exists)
    # ==============================================================================
    if(EXISTS "${CMAKE_SOURCE_DIR}/Tests/TestRunner.cpp")
        message(STATUS "Building WaveEdit automated tests")

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
            Tests/Unit/FadeCurveTypesTests.cpp                  # Phase 4 ✅ (Fade Curve Types)
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
            Tests/Integration/SaveAsFormatsTests.cpp            # Phase 4 ✅ (Save As all formats)
            Tests/Integration/SaveAs8BitTests.cpp               # Phase 4 ✅ (8-bit Save Tests)
            # Tests/Integration/RegionListPanelTests.cpp        # Week 4 ⚠️ (Needs migration to Catch2)
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
    else()
        message(STATUS "Tests/ directory not found - skipping test build")
        message(STATUS "Tests are excluded from repository per .gitignore")
    endif()

endif() # WAVEEDIT_BUILD_TESTS
