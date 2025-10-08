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
        Source/Commands/CommandIDs.h
        Source/Utils/Settings.cpp
        Source/Utils/Settings.h
        Source/Utils/AudioClipboard.cpp
        Source/Utils/AudioClipboard.h
        Source/Utils/AudioBufferInputSource.cpp
        Source/Utils/AudioBufferInputSource.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
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

# Copy resources (for future phases - icons, fonts, etc.)
# file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets
#      DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Installation rules (optional)
install(TARGETS WaveEdit
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
