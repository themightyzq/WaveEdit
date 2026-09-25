/* Minimal hand-written config.h for building libmp3lame's ENCODER sources
 * (see cmake/ThirdPartyAudioLibs.cmake) with CMake/clang on macOS, in place
 * of the config.h that LAME's own `configure` script would normally
 * generate from config.h.in (LAME has no CMake support upstream).
 *
 * Derivation method: every C source file under libmp3lame, and
 * include/lame.h, were grepped (2026-09-25, lame-3.100) for references to
 * HAVE_..., STDC_HEADERS, and similar autoconf-style macros. Only macros
 * actually tested by that code are defined here; everything else in
 * config.h.in is deliberately left out.
 *
 * Two macros are intentionally NEVER defined, both for the same reason --
 * this header is compiled twice by a single clang invocation to produce one
 * universal (arm64 + x86_64) object per source file, so it must not select
 * anything architecture-specific:
 *   - HAVE_XMMINTRIN_H / HAVE_NASM / MMX_choose_table: these gate x86 SSE
 *     intrinsics and hand-written x86 asm. Leaving them undefined makes
 *     every source file take LAME's portable C fallback path on both arches.
 *   - HAVE_MPGLIB: gates LAME's own MP3 *decoder*. WaveEdit only uses LAME
 *     as an ENCODER -- Source/Audio/LameMP3AudioFormat.cpp calls lame_init,
 *     lame_set_*, lame_init_params, lame_encode_buffer_ieee_float,
 *     lame_encode_flush, lame_close only (verified by grepping for lame_*
 *     call sites; no hip_decode* calls). MP3 *decoding* goes through JUCE's
 *     own built-in MP3AudioFormat. With HAVE_MPGLIB undefined,
 *     libmp3lame/mpglib_interface.c's entire body is `#ifdef`'d out (it
 *     compiles to an empty translation unit), so it is not even added to
 *     the CMake target and the mpglib/ decoder sources are never touched.
 */
#ifndef LAME_CONFIG_H
#define LAME_CONFIG_H

/* ANSI C headers (stdlib.h, string.h, etc.) are present -- true for
 * macOS/Linux libc. Read by libmp3lame/machine.h. */
#define STDC_HEADERS 1

/* Standard headers used by libmp3lame/machine.h. */
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1

/* string.h provides these; machine.h and lame.c probe them directly
 * (independently of STDC_HEADERS) in a couple of spots. */
#define HAVE_MEMCPY 1
#define HAVE_STRCHR 1

/* libmp3lame/util.h declares `extern ieee754_float32_t fast_log2(...)`
 * unconditionally (only the *definition* in util.c is gated on
 * USE_FAST_LOG, which this config.h does not define, but the declaration
 * still needs the type). Upstream's config.h.in is where this non-standard
 * type name gets defined when the platform doesn't already provide it --
 * it never does on macOS or Linux, so this is not conditional there
 * either. */
typedef float ieee754_float32_t;

#endif /* LAME_CONFIG_H */
