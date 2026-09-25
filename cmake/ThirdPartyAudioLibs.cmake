# ThirdPartyAudioLibs.cmake -- macOS only.
#
# Builds SoundTouch and LAME from pinned upstream source as STATIC libraries
# so WaveEdit.app can be a single universal (arm64 + x86_64) binary targeting
# macOS 11.0 with no bundled third-party dylibs. Homebrew's bottles for both
# libraries are thin (host-architecture only) and dynamic-only, which is what
# blocked universal builds before (see the git history of CMakeLists.txt's
# top-of-file comment, and CHANGELOG.md). Windows and Linux are untouched by
# this file: CMakeLists.txt still finds SoundTouch/LAME there via vcpkg /
# distro packages as dynamic libraries, exactly as before.
#
# Included from CMakeLists.txt only inside `if(APPLE)`, after the WaveEdit
# target already exists (juce_add_gui_app ran) but before anything reads
# SOUNDTOUCH_*/LAME_* variables. On return, sets those variables exactly as
# the find_library()/find_path() calls it replaces would have:
#   SOUNDTOUCH_LIBRARY   -- a CMake target name ("SoundTouch"), not a path
#   SOUNDTOUCH_INCLUDE_DIR
#   LAME_LIBRARY         -- a CMake target name ("lame_enc"), not a path
#   LAME_INCLUDE_DIR
# target_link_libraries()/target_include_directories() accept a target name
# exactly like a library path or an include path, so the rest of
# CMakeLists.txt (and the WaveEditCore test target) needs no further changes.

include_guard(GLOBAL)
include(FetchContent)

# ------------------------------------------------------------------------
# SoundTouch 2.3.3 (LGPL-2.1-or-later) -- ships its own working CMakeLists.txt
# (read after fetching, 2026-09-25, to pick the options below).
# ------------------------------------------------------------------------
# SoundTouch's own cmake_minimum_required(VERSION 3.1) is rejected outright
# by CMake >= 4.0 (support for <3.5 configure-time compatibility was
# removed). This tells CMake to treat it as if it said 3.5. It is a silent
# no-op on older CMake that never had the problem (e.g. CI's pinned 3.22.x).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# add_library(SoundTouch ...) in SoundTouch's CMakeLists has no STATIC/SHARED
# keyword, so it follows BUILD_SHARED_LIBS, which is OFF by default -- i.e.
# static already. These three options just turn off the parts we don't need.
set(SOUNDSTRETCH OFF CACHE BOOL "Disable SoundTouch's soundstretch CLI utility (WaveEdit only needs the library)" FORCE)
set(SOUNDTOUCH_DLL OFF CACHE BOOL "Disable SoundTouch's C DLL wrapper (unused)" FORCE)
set(OPENMP OFF CACHE BOOL "Disable OpenMP in SoundTouch (not needed for WaveEdit's usage)" FORCE)

FetchContent_Declare(soundtouch_src
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG 2.3.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(soundtouch_src)

set(SOUNDTOUCH_LIBRARY SoundTouch)
set(SOUNDTOUCH_INCLUDE_DIR "${soundtouch_src_SOURCE_DIR}/include")

# ------------------------------------------------------------------------
# LAME 3.100 (LGPL-2.0-or-later) -- no CMake support upstream, so this
# compiles libmp3lame's encoder sources directly into a static library
# target. (ExternalProject-with-autotools-per-arch-plus-lipo was the
# fallback plan; it proved unnecessary -- the encoder-only source subset
# compiles cleanly and portably under a plain add_library(), verified for
# both arm64 and x86_64 with a standalone smoke test before wiring this in.)
# ------------------------------------------------------------------------
FetchContent_Declare(lame_src
    URL https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download
    URL_HASH SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lame_src)

set(_lame_dir "${lame_src_SOURCE_DIR}")

# Encoder sources only (libmp3lame_la_SOURCES from upstream's
# libmp3lame/Makefile.am), minus mpglib_interface.c -- see
# cmake/lame_config/config.h for why that one file is excluded (it is
# LAME's own MP3 *decoder* glue, unused and, with HAVE_MPGLIB left
# undefined, an empty translation unit).
add_library(lame_enc STATIC
    "${_lame_dir}/libmp3lame/VbrTag.c"
    "${_lame_dir}/libmp3lame/bitstream.c"
    "${_lame_dir}/libmp3lame/encoder.c"
    "${_lame_dir}/libmp3lame/fft.c"
    "${_lame_dir}/libmp3lame/gain_analysis.c"
    "${_lame_dir}/libmp3lame/id3tag.c"
    "${_lame_dir}/libmp3lame/lame.c"
    "${_lame_dir}/libmp3lame/newmdct.c"
    "${_lame_dir}/libmp3lame/presets.c"
    "${_lame_dir}/libmp3lame/psymodel.c"
    "${_lame_dir}/libmp3lame/quantize.c"
    "${_lame_dir}/libmp3lame/quantize_pvt.c"
    "${_lame_dir}/libmp3lame/reservoir.c"
    "${_lame_dir}/libmp3lame/set_get.c"
    "${_lame_dir}/libmp3lame/tables.c"
    "${_lame_dir}/libmp3lame/takehiro.c"
    "${_lame_dir}/libmp3lame/util.c"
    "${_lame_dir}/libmp3lame/vbrquantize.c"
    "${_lame_dir}/libmp3lame/version.c"
)

# LameMP3AudioFormat.cpp does `#include <lame/lame.h>` (matching Homebrew's
# install layout: <prefix>/include/lame/lame.h), but upstream's tarball has
# a flat include/lame.h with no "lame/" subdirectory. Rather than patch the
# fetched source tree, copy the one header WaveEdit actually includes into a
# small generated "lame/" shim directory. lame.h itself only includes
# <stddef.h>/<stdarg.h>/<stdio.h> (verified), so a single-file copy is
# sufficient -- no other headers are pulled in transitively.
set(_lame_shim_dir "${CMAKE_BINARY_DIR}/lame_include_shim")
file(MAKE_DIRECTORY "${_lame_shim_dir}/lame")
configure_file("${_lame_dir}/include/lame.h" "${_lame_shim_dir}/lame/lame.h" COPYONLY)

target_include_directories(lame_enc
    PRIVATE
        "${_lame_dir}/include"
        "${_lame_dir}/libmp3lame"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lame_config"
)
target_compile_definitions(lame_enc PRIVATE HAVE_CONFIG_H)

set(LAME_LIBRARY lame_enc)
set(LAME_INCLUDE_DIR "${_lame_shim_dir}")

unset(_lame_dir)
unset(_lame_shim_dir)
