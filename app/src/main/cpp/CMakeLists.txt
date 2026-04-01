cmake_minimum_required(VERSION 3.22)
project(guitar_engine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use -O2 and -ffast-math for audio performance in release builds.
# Deliberately avoiding -O3 which can cause issues with some NDK versions
# and aggressive optimizations that break IEEE float compliance in undesirable ways.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -ffast-math")

# Fetch Oboe audio library from Google's GitHub repository.
# Oboe provides a high-performance, low-latency audio API that abstracts
# AAudio (API 27+) and OpenSL ES (API 26) behind a unified interface.
include(FetchContent)
FetchContent_Declare(
    oboe
    GIT_REPOSITORY https://github.com/google/oboe.git
    GIT_TAG        1.9.0
)
FetchContent_MakeAvailable(oboe)

# Fetch PFFFT (Pretty Fast FFT) for SIMD-optimized single-precision FFT.
# Used by the cabinet simulator for FFT-based IR convolution.
# Provides NEON on ARM and SSE on x86 from a single codebase.
# Ships in Android AOSP (platform/external/pffft).
FetchContent_Declare(
    pffft
    GIT_REPOSITORY https://github.com/marton78/pffft.git
    GIT_TAG        master
)
# Disable PFFFT extras we don't need
set(PFFFT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PFFFT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Enable NEON SIMD for ARM targets.
# The marton78 fork requires PFFFT_ENABLE_NEON to be explicitly set;
# its auto-detection via MIPP fails during NDK cross-compilation.
# SSE is auto-detected via __x86_64__ and needs no explicit flag.
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    add_compile_definitions(PFFFT_ENABLE_NEON=1)
endif()

FetchContent_MakeAvailable(pffft)

# =============================================================================
# chowdsp_wdf - Wave Digital Filter library for circuit-accurate pedal emulation
# =============================================================================
# Header-only C++14 library for building real-time WDF circuit models.
# Used by: BossDS1, BossDS2, ProCoRAT, BossHM2, Univibe effects.
# License: BSD-3-Clause (permissive).
FetchContent_Declare(
    chowdsp_wdf
    GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_wdf.git
    GIT_TAG        439ae0abcd0e6b881e9616b6a54883d1f4c96878  # Pin to main branch HEAD
)
# Disable tests/benchmarks (not needed for our build)
set(CHOWDSP_WDF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CHOWDSP_WDF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(chowdsp_wdf)

# =============================================================================
# NeuralAmpModelerCore (NAM) - Neural amp model inference
# =============================================================================
# Loads .nam model files and runs real-time inference via WaveNet/LSTM.
# NAMCore does NOT export a CMake library target, so we fetch its source
# and build a static library from its files. NAMCore bundles its own
# Eigen (linear algebra) and nlohmann/json (JSON parsing) as submodules.
# License: MIT (NAMCore), MPL-2.0 (Eigen), MIT (nlohmann/json).

FetchContent_Declare(
    NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        20a04fcf466dc4233730412b120e5bbad72402c3  # Pin to known-working commit
    GIT_SUBMODULES_RECURSE TRUE
)
# Populate without running NAMCore's CMake (it builds test executables,
# not a library, and has platform guards that would fail Android cross-builds).
FetchContent_GetProperties(NeuralAmpModelerCore)
if(NOT neuralampmodelercore_POPULATED)
    FetchContent_Populate(NeuralAmpModelerCore)
endif()

# Build NAMCore as a static library from its source files.
file(GLOB NAM_SOURCES "${neuralampmodelercore_SOURCE_DIR}/NAM/*.cpp")
add_library(nam_core STATIC ${NAM_SOURCES})
target_include_directories(nam_core PUBLIC
    "${neuralampmodelercore_SOURCE_DIR}"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann"
)
# NAMCore v0.4.0 uses C++20 features (Eigen::placeholders::lastN, etc.).
set_target_properties(nam_core PROPERTIES CXX_STANDARD 20)
target_compile_options(nam_core PRIVATE -O2 -ffast-math)

# =============================================================================
# FAUST DSP Build Pipeline
# =============================================================================
# FAUST is a functional audio DSP language that compiles to optimized C++.
# The pipeline supports two modes:
#   1. If the FAUST compiler is installed: .dsp files are compiled to C++
#      headers at build time via add_custom_command.
#   2. If FAUST is not installed: pre-generated headers from faust/generated/
#      are used instead. This allows building without the FAUST SDK.
#
# To install FAUST: https://faust.grame.fr/downloads/
# To regenerate a header manually:
#   faust -lang cpp -cn ClassName -i -single -inpl -nvi input.dsp -o Output.h

find_program(FAUST_EXECUTABLE faust)
if(FAUST_EXECUTABLE)
    message(STATUS "FAUST compiler found: ${FAUST_EXECUTABLE}")
    message(STATUS "  FAUST DSP files will be compiled to C++ at build time")
else()
    message(STATUS "FAUST compiler not found -- using pre-generated headers")
    message(STATUS "  Install FAUST to enable build-time DSP compilation")
endif()

# Directory for FAUST-generated C++ headers (both auto-generated and pre-generated)
set(FAUST_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/faust/generated")

# Function to compile a FAUST .dsp file to a C++ header.
# When FAUST is available, this creates a custom command that regenerates
# the header whenever the .dsp source changes. The output goes to the
# faust/generated/ directory alongside the pre-generated fallbacks.
#
# Arguments:
#   DSP_FILE   - Path to the .dsp file (relative to CMAKE_CURRENT_SOURCE_DIR)
#   CLASS_NAME - C++ class name for the generated DSP
function(faust_compile DSP_FILE CLASS_NAME)
    if(NOT FAUST_EXECUTABLE)
        return()  # Use pre-generated header
    endif()

    set(OUTPUT_FILE "${FAUST_GENERATED_DIR}/${CLASS_NAME}.h")
    set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${DSP_FILE}")

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${FAUST_EXECUTABLE}
            -lang cpp
            -cn ${CLASS_NAME}
            -i              # Inline architecture includes (self-contained header)
            -single         # Single-precision float (matches our audio pipeline)
            -inpl           # Allow in-place input/output buffers
            -nvi            # Remove virtual keyword (eliminates vtable overhead)
            ${INPUT_FILE}
            -o ${OUTPUT_FILE}
        DEPENDS ${INPUT_FILE}
        COMMENT "FAUST: compiling ${DSP_FILE} -> ${CLASS_NAME}.h"
        VERBATIM
    )

    # Add the generated file as a dependency of the main target
    target_sources(guitar_engine PRIVATE ${OUTPUT_FILE})
endfunction()

# Build the shared library that Kotlin loads via System.loadLibrary()
add_library(guitar_engine SHARED
    audio_engine.cpp
    signal_chain.cpp
    jni_bridge.cpp
    dsp/fft_convolver.cpp
)

# Audio processing MUST be optimized even in debug builds:
#   -O2: Without it, debug builds run 10-50x slower (no inlining, no SIMD
#         vectorization). With 96-frame callbacks (2ms budget @ 48kHz), the
#         unoptimized signal chain easily exceeds the deadline → underruns.
#   -ffast-math: Enables FTZ (flush-to-zero) which prevents denormalized
#         float values from causing 10-100x CPU stalls in feedback paths
#         (reverb damping, delay feedback, filter state decay).
# These flags apply to our audio code only; dependencies (Oboe, PFFFT, NAM)
# use their own build settings.
target_compile_options(guitar_engine PRIVATE -O2 -ffast-math)

# Include paths for our own headers and subdirectories
target_include_directories(guitar_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/faust
    ${CMAKE_CURRENT_SOURCE_DIR}/faust/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/looper
    ${CMAKE_CURRENT_SOURCE_DIR}/drums
    ${CMAKE_CURRENT_SOURCE_DIR}/drums/voices
)

# Register FAUST DSP files for compilation (must be after add_library).
# Each call pairs a .dsp source with its C++ class name.
# Pre-generated headers must exist in faust/generated/ with matching names.
faust_compile("faust/dsp/tremolo.dsp" "FaustTremolo")

# Link dependencies:
# - oboe: audio I/O abstraction over AAudio/OpenSL ES
# - PFFFT: SIMD-optimized FFT for cabinet IR convolution
# - nam_core: NeuralAmpModelerCore for neural amp model inference
# - log: Android __android_log_print for non-audio-thread logging
# - android: native Android APIs (ANativeWindow, etc.)
# - OpenSLES: fallback audio backend for API 26 devices
target_link_libraries(guitar_engine
    oboe
    PFFFT
    nam_core
    chowdsp_wdf
    log
    android
    OpenSLES
)
