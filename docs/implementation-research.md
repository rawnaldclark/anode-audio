# Guitar Pedal Circuit Emulation: Implementation Research

This document contains comprehensive research into open-source C++ implementations
of guitar pedal circuit emulation, Wave Digital Filter libraries, FAUST DSP tools,
academic papers, and practical implementation patterns. All URLs and license
information are included.

---

## Table of Contents

1. [chowdsp_wdf Library](#1-chowdsp_wdf-library)
2. [ChowDSP Guitar Pedal Plugins](#2-chowdsp-guitar-pedal-plugins)
3. [Other Open-Source Pedal Emulations](#3-other-open-source-pedal-emulations)
4. [FAUST Guitar Effect Libraries](#4-faust-guitar-effect-libraries)
5. [DAFx Papers and Academic References](#5-dafx-papers-and-academic-references)
6. [Practical Implementation Patterns](#6-practical-implementation-patterns)
7. [Performance Data](#7-performance-data)
8. [Code Structure Recommendations](#8-code-structure-recommendations)
9. [Complete URL and License Index](#9-complete-url-and-license-index)

---

## 1. chowdsp_wdf Library

**Repository:** https://github.com/Chowdhury-DSP/chowdsp_wdf
**License:** BSD 3-Clause (permissive, suitable for commercial use)
**Paper:** https://arxiv.org/abs/2210.12554
**API Documentation:** https://ccrma.stanford.edu/~jatin/chowdsp/chowdsp_wdf/

### 1.1 Overview

chowdsp_wdf is a header-only C++ library for creating real-time Wave Digital Filter
(WDF) circuit models. It was created by Jatin Chowdhury and is the most mature and
performant open-source WDF library available. It enables developers to simulate
analog circuits using digital wave-based algorithms at sample rates suitable for
real-time audio processing.

### 1.2 Header-Only Status and Dependencies

- **Fully header-only.** Can be used by including `include/chowdsp_wdf/chowdsp_wdf.h`.
- **Language:** C++14 or higher required.
- **Build system:** CMake 3.1+ (optional but recommended).
- **Optional dependency:** XSIMD 8.0.0+ for SIMD parallelization (supports ARM NEON).
- **No other dependencies.**

CMake integration:

```cmake
add_subdirectory(chowdsp_wdf)
target_link_libraries(my_target PUBLIC chowdsp_wdf)
```

### 1.3 Android/ARM Compatibility

**Yes, chowdsp_wdf can be used on Android/ARM.** Key points:

- Header-only C++14 with no mandatory native dependencies makes cross-compilation
  straightforward with the Android NDK.
- XSIMD supports ARM NEON intrinsics natively.
- For Android NDK builds, set `CMAKE_ANDROID_ARM_NEON=ON` for armeabi-v7a targets.
- ChowCentaur has been successfully shipped as an iOS app (ARM), confirming ARM
  compatibility in production.
- The template-based API avoids virtual dispatch overhead, which is beneficial on
  mobile ARM processors with smaller caches and weaker branch predictors.

### 1.4 Complete Component List

The library provides two parallel API namespaces:

#### `wdft` Namespace (Compile-Time, Template-Based) -- RECOMMENDED

This namespace uses C++ templates to resolve the circuit topology at compile time,
enabling aggressive compiler optimization and eliminating virtual dispatch overhead.

**Passive Components:**

| Class | Description | Constructor |
|-------|-------------|-------------|
| `ResistorT<FloatType>` | Resistor | `ResistorT<double> r1 { 4700.0 };` (4.7K ohm) |
| `CapacitorT<FloatType>` | Capacitor (requires `prepare()`) | `CapacitorT<double> c1 { 47.0e-9 };` (47nF) |
| `CapacitorAlphaT<FloatType>` | Capacitor with alpha transform | Same as above |
| `InductorT<FloatType>` | Inductor (requires `prepare()`) | `InductorT<double> l1 { 100.0e-3 };` (100mH) |
| `InductorAlphaT<FloatType>` | Inductor with alpha transform | Same as above |
| `ResistorCapacitorSeriesT<FloatType>` | Combined RC series element | For efficiency |
| `ResistiveCapacitiveVoltageSourceT<FloatType>` | Combined RC voltage source | For efficiency |

**Sources:**

| Class | Description |
|-------|-------------|
| `IdealVoltageSourceT<FloatType, Port>` | Ideal voltage source (typically the input) |
| `IdealCurrentSourceT<FloatType, Port>` | Ideal current source |
| `ResistiveVoltageSourceT<FloatType, Port>` | Voltage source with series resistance |
| `ResistiveCurrentSourceT<FloatType, Port>` | Current source with parallel resistance |

**Adaptors (Connection Types):**

| Class | Description |
|-------|-------------|
| `WDFSeriesT<FloatType, Port1, Port2>` | Series connection of two elements |
| `WDFParallelT<FloatType, Port1, Port2>` | Parallel connection of two elements |
| `PolarityInverterT<FloatType, Port>` | Polarity inverter |
| `RtypeAdaptorT` | R-type adaptor for arbitrary topologies (with SIMD optimization) |

**Nonlinear Elements:**

| Class | Description |
|-------|-------------|
| `DiodeT<FloatType, Port>` | Single diode (non-adaptable, uses Wright Omega function) |
| `DiodePairT<FloatType, Port>` | Antiparallel diode pair (non-adaptable) |

**Utilities:**

| Class | Description |
|-------|-------------|
| `SwitchT<FloatType>` | Ideal switch (open/closed) |
| `OpenT<FloatType>` | Open circuit |
| `ShortT<FloatType>` | Short circuit |
| `ScopedDeferImpedancePropagation` | Batch impedance updates for parameter changes |

#### `wdf` Namespace (Runtime, Polymorphic)

Same components without the `T` suffix. Uses virtual dispatch for runtime circuit
construction. Useful when the circuit topology is not known at compile time or when
building a circuit editor/prototyper.

### 1.5 API Usage: Building a Circuit Model

The general pattern for building a WDF circuit model:

1. **Declare components** as member variables with their physical values.
2. **Connect components** using series/parallel adaptors, building a binary tree.
3. **Place nonlinear elements** (diodes) at the root of the tree.
4. **Implement `prepare(sampleRate)`** to initialize reactive elements.
5. **Implement `processSample(x)`** to process one sample through the circuit.

#### Example: RC Lowpass Filter

```cpp
#include <chowdsp_wdf/chowdsp_wdf.h>

struct RCLowpass {
    // Declare components with physical values
    wdft::ResistorT<double> r1 { 1.0e3 };          // 1 KOhm resistor
    wdft::CapacitorT<double> c1 { 1.0e-6 };        // 1 uF capacitor

    // Connect in series
    wdft::WDFSeriesT<double, decltype(r1), decltype(c1)> s1 { r1, c1 };

    // Polarity inverter (needed for proper wave direction)
    wdft::PolarityInverterT<double, decltype(s1)> inv { s1 };

    // Voltage source at the root (input signal)
    wdft::IdealVoltageSourceT<double, decltype(inv)> vs { inv };

    // Initialize reactive elements for the given sample rate
    void prepare(double sampleRate) {
        c1.prepare(sampleRate);
    }

    // Process one sample through the circuit
    inline double processSample(double x) {
        vs.setVoltage(x);              // Set input voltage
        vs.incident(inv.reflected());   // Propagate waves down the tree
        inv.incident(vs.reflected());   // Propagate waves back up
        return wdft::voltage<double>(c1); // Read output voltage across capacitor
    }
};
```

#### Example: Diode Clipper (Core of Most Distortion Pedals)

```cpp
struct DiodeClipper {
    // Input resistance and coupling capacitor
    wdft::ResistorT<double> r1 { 2.2e3 };         // 2.2K input resistor
    wdft::CapacitorT<double> c1 { 47.0e-9 };      // 47nF capacitor

    // Series connection of R and C
    wdft::WDFSeriesT<double, decltype(r1), decltype(c1)> s1 { r1, c1 };

    // Voltage source (input)
    wdft::IdealVoltageSourceT<double, decltype(s1)> vs { s1 };

    // Parallel connection for the diode pair
    wdft::WDFParallelT<double, decltype(vs), decltype(c1)> p1 { vs, c1 };

    // Antiparallel diode pair at the root (non-adaptable nonlinearity)
    // Parameters: Is (saturation current), Vt (thermal voltage)
    // For silicon diodes (1N4148): Is = 2.52e-9, Vt = 25.85e-3
    wdft::DiodePairT<double, decltype(p1)> dp { p1, 2.52e-9, 0.02585 };

    void prepare(double sampleRate) {
        c1.prepare(sampleRate);
    }

    inline double processSample(double x) {
        vs.setVoltage(x);
        dp.incident(p1.reflected());
        p1.incident(dp.reflected());
        return wdft::voltage<double>(c1);
    }
};
```

**Key technical detail:** The diode pair uses a numerical approximation of the
Wright Omega function via floating-point bit twiddling, rather than Newton-Raphson
iteration. This is dramatically faster (see Performance Data section).

### 1.6 Diode Parameters for Common Types

| Diode Type | Is (Saturation Current) | Vt (Thermal Voltage) | Used In |
|------------|------------------------|---------------------|---------|
| 1N4148 (silicon) | 2.52e-9 A | 25.85e-3 V | DS-1, RAT |
| 1N914 (silicon) | 2.52e-9 A | 25.85e-3 V | Tube Screamer |
| 1N34A (germanium) | 200e-9 A | 25.85e-3 V | Fuzz Face, Klon Centaur |
| MA150 (silicon) | 4.35e-9 A | 25.85e-3 V | Tube Screamer (original) |
| LED (red) | ~1e-18 A | 25.85e-3 V | Rat (some versions) |

---

## 2. ChowDSP Guitar Pedal Plugins

### 2.1 ChowCentaur (Klon Centaur Overdrive Emulation)

**Repository:** https://github.com/jatinchowdhury18/KlonCentaur
**License:** BSD-3-Clause
**Paper:** https://arxiv.org/abs/2009.02833

#### Circuit Decomposition

The Klon Centaur circuit is decomposed into these stages:

1. **Input buffer** -- TL072 op-amp buffer
2. **Feed-forward network FF-1** -- frequency shaping before clipping
3. **Feed-forward network FF-2** -- frequency shaping before clipping
4. **Clipping stage** -- germanium diodes for hard clipping
5. **Summing amplifier** -- combines clean and clipped signal (the "transparent" character)
6. **Tone control** -- treble/bass balance
7. **Output buffer** -- TL072 op-amp buffer

The Klon's signature "transparent" overdrive comes from step 5: the clean signal
is always blended with the clipped signal through the feed-forward networks,
preserving dynamics and low-end clarity.

#### Three Modeling Approaches Compared

The ChowCentaur project implements three different modeling approaches for
comparison:

1. **WDF Mode ("Traditional"):** Uses Wave Digital Filters for the nonlinear gain
   stage components (diodes, op-amp feedback) and biquad filters for linear
   filtering stages. This is the component-level circuit model.

2. **RNN Mode ("Neural"):** Uses a recurrent neural network (GRU/LSTM) trained on
   SPICE simulation output to model the entire gain stage. Uses the RTNeural
   library (https://github.com/jatinchowdhury18/RTNeural) for real-time inference.
   Trained on 4 minutes of guitar recordings processed through SPICE.

3. **Nodal Analysis:** Traditional circuit analysis approach using matrix methods.

#### Source Code Structure

```
KlonCentaur/
  ChowCentaur/             -- Main plugin code
    src/
      ChowCentaurPlugin.h/cpp   -- Plugin entry point
      GainStageProcessors/       -- WDF and neural gain stage models
        GainStageProc.h/cpp      -- Abstract gain stage interface
        GainStageWDF.h/cpp       -- WDF-based gain stage
        GainStageML.h/cpp        -- Neural network gain stage
  SubCircuits/              -- Individual WDF circuit models for each subcircuit
  GainStageTraining/        -- Python scripts for neural network training
    spice_models/           -- LTSpice schematics of the Klon
    training_data/          -- Input/output audio recordings
  TeensyCentaur/            -- Embedded Teensy microcontroller port
```

#### Accuracy

The WDF mode provides component-level accuracy for each subcircuit individually.
The neural network mode was trained on SPICE simulation output and achieves a
different quality of accuracy -- it captures the overall transfer function including
inter-stage interactions that WDF tree decomposition may not fully represent.
The comparison between approaches was the subject of the original research paper.

### 2.2 CHOW Tape Model (Tape Saturation)

**Repository:** https://github.com/jatinchowdhury18/AnalogTapeModel
**License:** GPLv3 (iOS version dual-licensed GPL + MPLv2)
**Paper:** Presented at DAFx 2019

This is NOT a WDF implementation. It uses physical modeling of magnetic tape
behavior directly.

**Techniques used:**
- **Hysteresis modeling** using the Jiles-Atherton model of magnetic hysteresis
- Playhead loss simulation (high-frequency rolloff from the tape head gap)
- Wow and flutter modeling (speed variations)
- Tape degradation simulation (aging effects)

The hysteresis model is the core innovation: it models the B-H curve of magnetic
tape material, where B (flux density) depends on the history of H (field intensity),
creating the characteristic tape saturation and compression.

### 2.3 BYOD (Build Your Own Distortion)

**Repository:** https://github.com/Chowdhury-DSP/BYOD
**License:** GPLv3 (dual-licensed)
**Language:** 95.5% C++

BYOD is the most comprehensive open-source collection of guitar pedal DSP models
available. It provides a modular signal chain where users can combine processors.

#### Complete Module Inventory

**Drive Modules (16+):**

| Module | Description | Modeling Technique |
|--------|-------------|-------------------|
| Diode Clipper | Basic diode clipper circuit | WDF |
| Diode Rectifier | Half-wave rectification distortion | WDF |
| Dirty Tube | Tube amplifier saturation | Waveshaping |
| Waveshaper | Configurable waveshaping curves | Mathematical |
| Flapjack | Unique distortion character | WDF/hybrid |
| Fuzz Machine | Fuzz pedal emulation | WDF |
| Mouse Drive | Big Muff-style distortion | WDF |
| Crying Child | Wah-based distortion | WDF |
| Krusher | Digital bit-crushing distortion | Digital |
| Centaur | Klon Centaur-derived overdrive | WDF |
| Screamer | Tube Screamer-derived overdrive | WDF |
| Distortion+ | MXR Distortion+-derived | WDF |
| BASS Face | Bass-focused Fuzz Face variant | WDF |
| King of Tone | Analog King overdrive variant | WDF |
| Metal Zone | Boss MT-2-inspired | WDF/filter |
| Hyper Drive | High-gain distortion | WDF/hybrid |

**Tone Modules (11+):**
- Various filter types (lowpass, highpass, bandpass, notch)
- Parametric EQ
- Graphic EQ
- Tilt EQ
- Ladder filter (Moog-style)
- State variable filter
- Baxandall EQ

**Modulation/Other (7+):**
- Chorus
- Compressor
- Delay
- Spring Reverb
- Solo Vibe (Uni-Vibe derived phaser/vibrato)
- Tremolo
- Noise gate

**Utilities (9+):**
- Mixer
- Frequency splitter (crossover)
- Oscilloscope (visual)
- Gain
- DC Blocker
- Buffer

#### Source Structure

```
BYOD/
  src/
    processors/
      drive/           -- All drive/distortion modules
        DiodeClipper.h/cpp
        Screamer.h/cpp
        Centaur.h/cpp
        FuzzMachine.h/cpp
        MouseDrive.h/cpp
        ...
      tone/            -- Tone/filter modules
      modulation/      -- Chorus, tremolo, etc.
      utility/         -- Mixers, gain, etc.
    PluginProcessor.h/cpp   -- Main audio processor
    ProcessorChain.h/cpp    -- Modular signal chain manager
  modules/             -- Git submodules (chowdsp_wdf, JUCE, etc.)
  sim/                 -- SPICE simulation scripts
```

### 2.4 Other ChowDSP Projects

- **ChowKick:** Bass drum synthesizer using WDF resonant circuits
  - https://github.com/Chowdhury-DSP/ChowKick
- **ChowPhaser:** Phaser effect using WDF all-pass networks
  - https://github.com/jatinchowdhury18/ChowPhaser
- **CHOW Matrix:** Delay network effect
  - https://github.com/Chowdhury-DSP/ChowMatrix

---

## 3. Other Open-Source Pedal Emulations

### 3.1 Boss DS-1 Implementations

| Project | URL | Technique | License | Language |
|---------|-----|-----------|---------|----------|
| DS1.lv2 | https://github.com/LiamLombard/DS1.lv2 | Circuit sim with LUTs | -- | C++ (66%), MATLAB (18%) |
| Kapitonov-Plugins-Pack | https://github.com/olegkapitonov/Kapitonov-Plugins-Pack | Models DS-1 and FuzzFace | -- | C++ |
| ATK SD1 | Via AudioTK ecosystem | x8 oversampling, Audio ToolKit | -- | C++ |

**DS1.lv2 Details:**
This project uses a hybrid approach: MATLAB scripts analyze the circuit and generate
lookup tables (LUTs), which are then loaded by the C++ real-time processing code.

```
DS1.lv2/
  CircuitSim/    -- MATLAB circuit analysis scripts
  LUT/           -- Pre-computed lookup tables for nonlinearities
  src/           -- C++ LV2 plugin source
```

**Academic Reference:**
"Simplified Modelling of the Boss DS-1 Distortion Pedal" -- available at:
https://www.academia.edu/65479550/Simplified_Modelling_of_the_Boss_DS_1_Distortion_Pedal

This paper identifies the diode clipper with embedded low-pass filter as the primary
distortion mechanism and proposes a computationally efficient digital model.

**Electrosmash Circuit Analysis:**
https://www.electrosmash.com/boss-ds1-analysis

Key circuit details for implementing a DS-1:
- **Clipping type:** Hard clipping (diodes to ground)
- **Clipping diodes:** Two antiparallel silicon diodes
- **Tone control:** Active high-pass/low-pass with variable frequency
- **Gain range:** Controlled by 100K potentiometer in op-amp feedback
- **High-pass cutoff before clipping:** ~72Hz

### 3.2 ProCo RAT Implementations

| Project | URL | Technique | License |
|---------|-----|-----------|---------|
| Proco-Rat (Rudro085) | https://github.com/Rudro085/Proco-Rat | Nodal analysis, JUCE | MIT |
| ProCoRat (mxmora) | https://github.com/mxmora/ProCoRat | -- | -- |
| PiRat | https://github.com/nezetic/pirat | Eurorack hardware | BSD-2-Clause |
| Rodent (SharpSoundPlugins) | https://github.com/ValdemarOrn/SharpSoundPlugins | C# VST | -- |

**Rudro085/Proco-Rat Details:**
The most instructive C++ implementation. Uses nodal analysis based on Electrosmash's
published circuit analysis and is built with JUCE. This is a good reference for
implementing the RAT's specific circuit topology.

**RAT Circuit Key Details** (from Electrosmash):
https://www.electrosmash.com/proco-rat

- **Clipping type:** Hard clipping (diodes to ground)
- **Clipping diodes:** 1N914 silicon diodes (some versions use LEDs or germanium)
- **Op-amp:** LM308 (single op-amp for gain stage)
- **Filter control:** Single "Filter" knob is a simple passive low-pass
- **Gain range:** Controlled by 1M potentiometer in op-amp feedback
- **Characteristic:** More aggressive, buzzier distortion than Tube Screamer

### 3.3 Boss MT-2 Metal Zone Implementations

**MetalTone:**
- **Repository:** https://github.com/brummer10/MetalTone
- **Language:** C (92%), with FAUST (1.4%) for DSP core
- **Format:** LV2 plugin with libxputty GUI
- **Description:** Modeled after the Boss MT-2 Metal Zone, a high-gain distortion
  pedal with an advanced EQ section featuring 15dB cut/boost and parametric mid
  (200Hz-5kHz sweep).

**ATK-MT2:**
- **Repository:** https://github.com/AudioTK/ATK-MT2
- **Description:** MT-2 emulation built on the Audio Toolkit library pipeline
  architecture.

### 3.4 Boss HM-2 Heavy Metal

No significant open-source implementations were found. The HM-2 is a specialized
pedal with a distinctive "chainsaw" distortion sound. The main commercial emulation
is Audiority "Heavy Pedal" (closed source). This represents an opportunity for
original implementation work.

Key HM-2 circuit characteristics that would need to be modeled:
- Two cascaded clipping stages (not just one)
- Active EQ with resonant peaks at specific frequencies
- Very high gain structure
- Infrared LED used for clipping in some stages

### 3.5 Uni-Vibe Implementations

**MusicDSP.org Univibe Emulator** (from Rakarrack):
- **URL:** https://www.musicdsp.org/en/latest/Effects/277-univox-univibe-emulator.html
- **License:** GPL (from Rakarrack project)
- **Original source:** https://sourceforge.net/projects/rakarrack

**Implementation details:**
- 4 cascaded all-pass filters using bilinear transform
- Vactrol (light-dependent resistor) model with temporal response characteristics
- The vactrol model simulates how the photoresistor resistance changes as the LFO
  modulates an LED brightness
- Dynamic coefficient recalculation per sample for each modulated all-pass stage
- The `out()` function processes audio through four filtering stages per channel

**Key technical insight:** The Univibe is fundamentally different from a standard
phaser. While both use all-pass filters, the Univibe's photocell stages have
asymmetric attack/decay times that create a characteristic "throbbing" quality.
The vactrol temporal response is critical to accuracy.

**Party Panda Univibe:**
- **Repository:** https://github.com/mattanikiej/party-panda-univibe
- **Format:** VST3/AU plugin

### 3.6 WDF-Specific Projects

**WaveDigitalFilters (Jatin Chowdhury's examples):**
- **Repository:** https://github.com/jatinchowdhury18/WaveDigitalFilters
- **License:** GPL-3.0

Includes implementations of:
- Diode Clipper
- Baxandall EQ
- Sallen-Key Filter
- TR-808 Bass Drum (Roland TR-808 kick circuit)
- BBD Delay (Bucket Brigade Device)
- LC Oscillator
- Passive LPF
- Voltage Divider
- Current Divider
- **WDFPrototyper:** Runtime circuit design tool

**WDF++:**
- **Repository:** https://github.com/AndrewBelt/WDFplusplus
- **License:** MIT (core WDF++.hpp library)
- **Example:** Fairchild 670 limiter tube stage model
- **Used in:** VCV Rack ecosystem
- **Core classes:** OnePort (base), IdealTransformer, NewtonRaphson
- **Author:** Maxime Coorevits (2013)

**RT-WDF:**
- **Repository:** https://github.com/RT-WDF/rt-wdf_lib
- **Depends on:** Armadillo (C++ linear algebra library)
- **Features:** Supports arbitrary topologies and multiple/multiport nonlinearities
- **Case studies:** Switchable attenuator, Fender Bassman tone stack,
  common-cathode triode amplifier stage
- **Origin:** CCRMA at Stanford University, funded research
- **Companion project:** wdfRenderer (working reference circuits)

**Differentiable WDFs:**
- **Repository:** https://github.com/jatinchowdhury18/differentiable-wdfs
- **Purpose:** Integrates WDFs with neural networks for automatic parameter
  optimization
- **Technique:** Exploits differentiability of WD simulations to enable
  gradient-based optimization of circuit component values
- **Validation:** Tested on fuzz guitar pedal with two potentiometers, achieving
  accuracy comparable to SPICE simulation
- **Application:** Can learn component values from audio recordings of real pedals

**pywdf (Python, for prototyping):**
- **Repository:** https://github.com/gusanthon/pywdf
- **Paper:** https://www.dafx.de/paper-archive/2023/DAFx23_paper_23.pdf
- **Components:** Voltage sources, capacitors, resistors, diodes, adaptors
- **Includes:** Frequency response and transient analysis tools
- **Validation:** Tube Screamer clipping stage compared to SPICE simulation

### 3.7 Fuzz Face WDF Model

**CCRMA Wiki Project:**
https://ccrma.stanford.edu/wiki/Wave_Digital_Filters_applied_to_the_Dunlop_%22Fuzz_Face%22_Distortion_Circuit

**Details:**
- Converts the Fuzz Face circuit into a Binary Connection Tree (BCT) WDF model
- Uses Ebers-Moll transistor model for the two AC128 germanium transistors
- Nonlinearities placed at the root of the tree for computability
- MATLAB implementation for simulation
- The Fuzz Face is a two-stage amplifier with feedback; the feedback network
  and matched germanium transistors are critical to the tone

### 3.8 Audio Toolkit (ATK)

**Repository:** https://github.com/AudioTK/AudioTK
**Organization:** https://github.com/AudioTK

A pipeline-based C++ DSP toolbox with modular components.

**Guitar pedal emulations built on ATK:**

| Project | Description |
|---------|-------------|
| ATK-SD1 | Boss SD-1 overdrive (x8 oversampling for the clipping stage) |
| ATK-MT2 | Boss MT-2 Metal Zone |
| ATKGuitarPreamp | Guitar preamp emulation |
| ATKBassPreamp | Bass preamp emulation |
| ATK-plugins | Collection of audio plugins built on ATK + JUCE |

**ATK Architecture:**
Uses a pipeline/workflow architecture where audio flows through connected
processing blocks. Each block has typed input and output ports.

**ATK Modelling Lite:**
- **Repository:** https://github.com/mbrucher/ATK-modelling-lite
- Simplified version of ATK's circuit modeling module

### 3.9 Neural Network Approaches

| Project | URL | Technique | License |
|---------|-----|-----------|---------|
| Neural Amp Modeler (NAM) | https://github.com/sdatkinson/neural-amp-modeler | Neural network amp/pedal | MIT |
| NAM Plugin | https://github.com/sdatkinson/NeuralAmpModelerPlugin | Real-time NAM player | GPL-3.0 |
| NeuralPi | https://github.com/GuitarML/NeuralPi | Raspberry Pi neural pedal | GPL-3.0 |
| GuitarLSTM | https://github.com/GuitarML/GuitarLSTM | LSTM amp/pedal modeling | MIT |
| PedalNetRT | https://github.com/GuitarML/PedalNetRT | WaveNet real-time pedal | MIT |
| SmartGuitarAmp | https://github.com/GuitarML/SmartGuitarAmp | JUCE + neural tube amp | GPL-3.0 |
| Mercury | https://github.com/GuitarML/Mercury | NAM on Daisy Seed hardware | -- |

**Neural Amp Modeler (NAM) Overview:**
- Website: https://www.neuralampmodeler.com/
- Train models from real hardware recordings (input/output signal pairs)
- Export trained models as `.nam` files
- Load into real-time plugin for playback
- Captures overall transfer function including all circuit interactions
- Effective for distortion/overdrive; less effective for time-based effects
  (delay, reverb)

**GuitarML Philosophy:**
GuitarML (https://guitarml.com/) believes in advancing music technology by releasing
it as free and open source. All projects are open source.

**PedalNetRT Details:**
- Uses WaveNet architecture for real-time guitar pedal emulation
- Effective at reproducing distortion/overdrive character
- NOT effective at reproducing reverb/delay effects (WaveNet has limited
  temporal receptive field)

### 3.10 LiveSPICE (Real-Time SPICE Simulator)

**Repository:** https://github.com/dsharlet/LiveSPICE
**Website:** https://www.livespice.org/
**License:** Open source
**Language:** C#

**Description:**
LiveSPICE is a SPICE-like circuit simulation tool for processing live audio signals.
You draw the circuit schematic in a visual editor and run live audio through it.

**Key Technical Features:**
- Custom Computer Algebra System (CAS) for circuit analysis
- JIT compilation of simulation programs for each specific circuit
- Most simulation logic is evaluated as a preprocessing step, not during real-time
  processing
- ASIO audio device support for ultra-low latency

**Component Library:**
- Op-amps
- Transistors (BJT, MOSFET)
- Vacuum tubes
- Transformers
- Diodes
- Standard passive components
- Custom components can be added

**Use Case:**
Excellent for rapid prototyping and validation. Draw a pedal circuit schematic,
play guitar through it in real-time to hear the result. Then implement the
verified circuit in C++ using WDF for production use.

---

## 4. FAUST Guitar Effect Libraries

### 4.1 FAUST wdmodels Library

**Documentation:** https://faustlibraries.grame.fr/libs/wdmodels/
**Source:** https://github.com/grame-cncm/faustlibraries/blob/master/wdmodels.lib
**Paper:** "A Wave-Digital Modeling Library for the Faust Programming Language" (SMC 2021)
**Paper PDF:** https://zenodo.org/records/5045088/files/SMC_2021_paper_83.pdf
**Prefix:** `wd.`

#### Complete Component Inventory

**Algebraic One Port Adaptors (Passive):**

| Function | Description |
|----------|-------------|
| `wd.resistor(R)` | Resistor with resistance R ohms |
| `wd.resistor_Vout(R)` | Resistor with voltage output |
| `wd.resistor_Iout(R)` | Resistor with current output |
| `wd.capacitor(C)` | Capacitor with capacitance C farads |
| `wd.capacitor_Vout(C)` | Capacitor with voltage output |
| `wd.capacitor_Iout(C)` | Capacitor with current output |
| `wd.inductor(L)` | Inductor with inductance L henrys |
| `wd.inductor_Vout(L)` | Inductor with voltage output |
| `wd.inductor_Iout(L)` | Inductor with current output |

**Sources:**

| Function | Description |
|----------|-------------|
| `wd.u_voltage` | Unadapted ideal voltage source |
| `wd.u_current` | Unadapted ideal current source |
| `wd.resVoltage(R)` | Resistive voltage source (series R + Vs) |
| `wd.resVoltage_Vout(R)` | Resistive voltage source with voltage output |
| `wd.u_resVoltage(R)` | Unadapted resistive voltage source |
| `wd.resCurrent(R)` | Resistive current source (parallel R + Is) |
| `wd.u_resCurrent(R)` | Unadapted resistive current source |
| `wd.u_switch` | Unadapted ideal switch |

**Nonlinear Elements:**

| Function | Description |
|----------|-------------|
| `wd.u_idealDiode` | Unadapted ideal diode |
| `wd.u_diodePair` | Antiparallel diode pair |
| `wd.u_diodeSingle` | Single diode |
| `wd.u_diodeAntiparallel(n, m)` | Multiple antiparallel diodes (n forward, m reverse) |
| `wd.u_diodeAntiparallel_omega` | Antiparallel diodes using Wright Omega function |
| `wd.u_chua` | Chua diode nonlinear resistor |
| `wd.lambert` | Lambert W function approximation |
| `wd.omega` | Wright Omega function approximation |

**Two Port Adaptors:**

| Function | Description |
|----------|-------------|
| `wd.parallel2Port` | Adapted 2-port parallel connection |
| `wd.u_parallel2Port` | Unadapted 2-port parallel connection |
| `wd.series2Port` | Adapted 2-port series connection |
| `wd.u_series2Port` | Unadapted 2-port series connection |
| `wd.parallelCurrent` | Parallel connection with ideal current source |
| `wd.seriesVoltage` | Series connection with ideal voltage source |
| `wd.transformer(N)` | Adapted ideal transformer (turns ratio N) |
| `wd.u_transformer(N)` | Unadapted ideal transformer |
| `wd.transformerActive` | Adapted active transformer |
| `wd.u_transformerActive` | Unadapted active transformer |

**Three Port Adaptors:**

| Function | Description |
|----------|-------------|
| `wd.parallel` | 3-port parallel connection |
| `wd.series` | 3-port series connection |

**R-Type Adaptors:**

| Function | Description |
|----------|-------------|
| `wd.u_sixportPassive` | Unadapted six-port rigid connection |

**Node and Tree Building Functions:**

| Function | Description |
|----------|-------------|
| `wd.buildtree` | Builds complete DSP model from connection tree |
| `wd.builddown` | Calculates downward-traveling waves |
| `wd.buildup` | Calculates upward-traveling waves |
| `wd.buildout` | Creates output matrix |
| `wd.getres` | Determines upward port resistance |
| `wd.parres` | Parallelized port resistance calculation |
| `wd.genericNode` | Generates adapted node from scattering matrix |
| `wd.genericNode_Vout` | Adapted node with voltage output |
| `wd.genericNode_Iout` | Adapted node with current output |
| `wd.u_genericNode` | Unadapted generic node |

#### Performance

FAUST wdmodels achieves 2-3x better performance than C++ libraries for **linear**
networks. However, chowdsp_wdf's C++ template implementation is significantly
faster for **nonlinear** circuits (diode clippers), because chowdsp_wdf uses a
fast Lambert W approximation via floating-point bit twiddling while wdmodels uses
Newton-Raphson iteration.

### 4.2 FAUST AANL Library (Antialiased Nonlinearities)

**Documentation:** https://faustlibraries.grame.fr/libs/aanl/

Provides ready-to-use antialiased versions of common distortion functions:

| Function | Description |
|----------|-------------|
| `aa.hardclip` | Hard clipper with 1st-order ADAA |
| `aa.hardclip2` | Hard clipper with 2nd-order ADAA |
| `aa.cubic` | Cubic soft clipper with ADAA |
| `aa.cubic2` | Cubic soft clipper with 2nd-order ADAA |
| `aa.tanh` | Hyperbolic tangent with ADAA |
| `aa.tanh2` | Hyperbolic tangent with 2nd-order ADAA |
| `aa.parabolic` | Parabolic soft clipper with ADAA |
| `aa.parabolic2` | Parabolic soft clipper with 2nd-order ADAA |

### 4.3 Guitarix FAUST Source Code

**Repository:** https://github.com/brummer10/guitarix
**Website:** https://guitarix.org/
**FAUST source files:** `trunk/src/faust/`
**License:** GPL

Guitarix was prototyped with FAUST and its core DSP code is written in FAUST,
then compiled to C++ for real-time use.

**FAUST example (from official FAUST repo):**
https://github.com/grame-cncm/faust/blob/master-dev/examples/misc/guitarix.dsp

This example includes:
- Tube preamp emulation (nonlinear waveshaping for each tube stage)
- Tubescreamer-style overdrive
- Tonestack (Fender/Marshall/VOX configurable)
- Cabinet simulation (impulse response convolution)

**Guitarix distortion.dsp:**
https://github.com/moddevices/guitarix/blob/master/trunk/src/faust/distortion.dsp

**GxPlugins (LV2 plugins derived from Guitarix):**
https://github.com/brummer10/GxPlugins.lv2

These include various fuzz and distortion models compiled as LV2 plugins.

### 4.4 Specific Pedal Models in FAUST

| Pedal Model | Source | Notes |
|-------------|--------|-------|
| MetalTone (Boss MT-2) | https://github.com/brummer10/MetalTone | FAUST for DSP core |
| CryBaby Wah | FAUST `effect.lib` | Digitized CryBaby model |
| Tube Screamer | `guitarix.dsp` example | Included in FAUST examples |
| Pultec EQP-1A | wdmodels documentation | WDF model example |
| Moog VCF | FAUST `effect.lib` | Voltage-controlled filter |

**Gap:** There are no direct DS-1, RAT, or HM-2 models in the standard FAUST
libraries. However, the building blocks (diode clippers, filters, op-amp models)
are all available, and these pedals could be constructed from them.

### 4.5 FAUST Accuracy vs. Circuit-Level Models

FAUST WDF models using the `wdmodels` library are mathematically equivalent to C++
WDF implementations -- they use the same underlying wave digital filter theory. The
difference is in the implementation efficiency:

- **Linear circuits:** FAUST wdmodels is 2-3x faster than C++ (FAUST compiler
  optimizes the signal flow graph aggressively)
- **Nonlinear circuits:** C++ (chowdsp_wdf) is much faster (7-8x for diode
  clippers) due to superior Lambert W approximation
- **Accuracy:** Both achieve the same numerical accuracy for equivalent circuit
  models

For pure waveshaping approaches (Guitarix tube preamp), the FAUST implementations
use polynomial or tanh-based waveshaping functions that approximate the tube
transfer curve. These are computationally cheaper but less physically accurate
than full circuit models.

---

## 5. DAFx Papers and Academic References

### 5.1 Essential Papers

#### David Yeh's PhD Thesis (2009) -- FOUNDATIONAL

**Title:** "Digital Implementation of Musical Distortion Circuits by Analysis and
Simulation"
**PDF:** https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf
**Institution:** CCRMA, Stanford University

**Contents:**
- Complete digital implementations derived from circuit analysis
- **Boss DS-1:** Full circuit decomposition and digital model
- **Ibanez Tube Screamer TS-9:** Full circuit decomposition and digital model
- **Fuzz Face:** Transistor-based fuzz with Ebers-Moll model
- Filter design from continuous-time prototypes (bilinear transform)
- Numerical methods for nonlinear ODEs (Newton-Raphson, etc.)
- Bandlimited signal exploitation for alias reduction

This thesis is the single most important reference for implementing guitar pedal
circuit emulations. It provides both theory and practical digital implementations.

#### David Yeh's Other Key Publications

**Publications page:** https://ccrma.stanford.edu/~dtyeh/papers/pubs.html

| Paper | Venue | Topic |
|-------|-------|-------|
| "Discretization of the '59 Fender Bassman Tone Stack" | DAFx-06 | Filter modeling |
| "Simplified, physically-informed models of distortion and overdrive guitar effects pedals" | DAFx-07 | DS-1, TS-9, Fuzz Face |
| "Numerical Methods for Simulation of Guitar Distortion Circuits" | DAFx-07 | ODE solvers |
| "Simulation of the diode limiter in guitar distortion circuits" | DAFx-07 | Diode modeling |
| "Automated Physical Modeling of Nonlinear Audio Circuits For Real-Time Audio Effects" | DAFx-10 | Automated modeling |

#### Kurt Werner's PhD Thesis (2016) -- FOUNDATIONAL FOR WDF THEORY

**Title:** "Virtual Analog Modeling of Audio Circuitry Using Wave Digital Filters"
**PDF:** https://stacks.stanford.edu/file/druid:jy057cz8322/KurtJamesWernerDissertation-augmented.pdf
**Institution:** CCRMA, Stanford University

**Contents:**
- Extended WDF theory to arbitrary topologies (R-type adaptors)
- Extended WDF theory to multiple/multiport nonlinearities
- Case study: TR-808 bass drum circuit (full WDF implementation)
- Case study: Fender Bassman tone stack
- Foundational for all modern WDF implementations (chowdsp_wdf, RT-WDF, wdmodels)

**Related paper:**
"Wave Digital Filter Adaptors for Arbitrary Topologies and Multiport" (DAFx-15)
https://dafx.de/paper-archive/2015/DAFx-15_submission_53.pdf

#### Jatin Chowdhury's Publications

| Paper | Venue/Source | Topic |
|-------|-------------|-------|
| "chowdsp_wdf: An Advanced C++ Library for Wave Digital Circuit Modelling" | arXiv:2210.12554 | Library design |
| "A Comparison of Virtual Analog Modelling Techniques for Desktop and Embedded Implementations" | arXiv:2009.02833 | Klon Centaur comparison |
| CHOW Tape Model | DAFx 2019 | Tape saturation |
| "Antiderivative Antialiasing in Nonlinear Wave Digital Filters" | DAFx 2020 | ADAA + WDF |
| "Practical Considerations for Antiderivative Anti-Aliasing" | Medium article | ADAA practical guide |

**Medium article (practical ADAA guide):**
https://jatinchowdhury18.medium.com/practical-considerations-for-antiderivative-anti-aliasing-d5847167f510

#### Werner et al. Diode Model Paper

**Title:** "An Improved and Generalized Diode Clipper Model for Wave Digital Filters"
**ResearchGate:** https://www.researchgate.net/publication/299514713_An_Improved_and_Generalized_Diode_Clipper_Model_for_Wave_Digital_Filters

This paper is the basis for chowdsp_wdf's `DiodeT` and `DiodePairT` classes.
It provides the mathematical framework for modeling diode clipping in WDF,
using the Lambert W function / Wright Omega function for the nonlinear solve.

#### Other Key Papers

| Paper | Authors | Venue | Topic |
|-------|---------|-------|-------|
| "RT-WDF: A Modular Wave Digital Filter Library..." | Werner, Dunkel, Rest | DAFx-16 | R-type adaptors, arbitrary topologies |
| "Wave Digital Filter Modeling of Circuits with Operational Amplifiers" | Werner, Smith, Abel | EUSIPCO 2016 | Op-amp in WDF |
| "Antiderivative Antialiasing for Stateful Systems" | Parker, Esqueda, Bilbao | MDPI Applied Sciences 2020 | Extended ADAA theory |
| "Emulating Diode Circuits with Differentiable Wave Digital Filters" | Chowdhury | DAFx 2022 | Neural + WDF hybrid |
| "Explicit Vector Wave Digital Filter Modeling..." | -- | DAFx 2023 | Advanced WDF techniques |
| "A Review of Digital Techniques for Modeling Vacuum-Tube Guitar Amplifiers" | Pakarinen, Yeh | Computer Music Journal | Comprehensive review |
| "Virtual Electric Guitars and Effects Using Faust and Octave" | Julius O. Smith III | LAC 2008 | FAUST guitar effects |
| "Simplifying Antiderivative Antialiasing with Lookup Tables" | -- | DAFx 2025 | LUT-based ADAA |
| "Note on Alias Suppression in Digital Distortion" | Martin Vicanek | -- | Alternative ADAA approach |

### 5.2 Key Technical Insights from the Literature

1. **Circuit Decomposition:** The signal path for distortion circuits can be
   decomposed into series/parallel stages which may be linear or nonlinear.
   Linear stages (filters, buffers) use standard IIR/biquad filters. Nonlinear
   stages (clipping) require WDF, numerical ODE solvers, or neural networks.

2. **Frequency-Selective Distortion:** Both the Tube Screamer and DS-1 apply
   different amounts of gain at different frequencies BEFORE clipping. The TS-9
   high-passes at ~720Hz in the feedback loop, causing less clipping of bass
   frequencies. The DS-1 high-passes at ~72Hz. This frequency shaping before
   nonlinearity is critical to the pedal's character and is what distinguishes
   one overdrive from another.

3. **Hard vs. Soft Clipping:**
   - **Hard clipping** (diodes to ground): DS-1, RAT, Fuzz Face.
     Creates sharp waveform limiting, more harmonics, "buzzy" or "fizzy" sound.
   - **Soft clipping** (diodes in op-amp feedback): Tube Screamer, Klon Centaur.
     Creates gradual gain reduction, fewer harmonics, "smoother" distortion.

4. **The "Transparent" Overdrive:** The Klon Centaur's unique character comes
   from feed-forward networks that blend the clean signal with the clipped signal,
   preserving dynamics and low-end clarity. The blend ratio changes with the
   gain knob position.

5. **Bilinear Transform Limitations:** The bilinear transform (used to discretize
   continuous-time circuits) introduces frequency warping. At low sample rates,
   this can shift filter cutoff frequencies significantly. Pre-warping critical
   frequencies helps. Higher sample rates (or oversampling) reduce this issue.

---

## 6. Practical Implementation Patterns

### 6.1 Analog Components Mapped to C++ Classes

The pattern is consistent across all WDF implementations:

```cpp
// Each analog component becomes a class instance with its physical value
wdft::ResistorT<double> r1 { 4700.0 };      // 4.7K resistor
wdft::CapacitorT<double> c1 { 47.0e-9 };    // 47nF capacitor
wdft::InductorT<double> l1 { 100.0e-3 };    // 100mH inductor

// Connections mirror the circuit schematic topology
wdft::WDFSeriesT<double, decltype(r1), decltype(c1)> series { r1, c1 };
wdft::WDFParallelT<double, decltype(r1), decltype(c1)> parallel { r1, c1 };

// Nonlinear elements (diodes) go at the root of the WDF tree
// Parameters: saturation current Is, thermal voltage Vt
wdft::DiodePairT<double, decltype(series)> dp {
    series,
    2.52e-9,    // Is for 1N4148 silicon diode
    0.02585     // Vt = kT/q at room temperature (25.85 mV)
};
```

For non-WDF approaches, analog components are modeled differently:

```cpp
// Standard biquad filter (for linear filter stages)
struct BiquadFilter {
    double b0, b1, b2, a1, a2;  // Coefficients
    double z1 = 0, z2 = 0;      // State (delay elements)

    double processSample(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Waveshaping (for simple nonlinear stages)
inline double softClip(double x) {
    return std::tanh(x);  // tanh soft clipper
}

inline double hardClip(double x, double threshold = 1.0) {
    return std::max(-threshold, std::min(threshold, x));
}
```

### 6.2 Signal Chain Structure (Per-Sample Processing Loop)

The standard pattern from JUCE-based plugins and standalone audio processing:

```cpp
class PedalEmulation {
    // Sub-circuits as member structs
    InputBuffer inputBuffer;
    ClippingStage clipper;
    ToneControl tone;
    OutputBuffer outputBuffer;

    // Oversampling processor (optional)
    juce::dsp::Oversampling<float> oversampling { 1, 2, ... };

    void prepare(double sampleRate, int samplesPerBlock) {
        // Reactive elements need the sample rate for discretization
        inputBuffer.prepare(sampleRate);
        clipper.prepare(sampleRate * oversamplingFactor);
        tone.prepare(sampleRate);
        outputBuffer.prepare(sampleRate);
        oversampling.initProcessing(samplesPerBlock);
    }

    void processBlock(float* buffer, int numSamples) {
        // Optional: upsample before nonlinear processing
        auto oversampledBlock = oversampling.processSamplesUp(inputBlock);

        for (int i = 0; i < oversampledBlock.getNumSamples(); ++i) {
            float x = oversampledBlock.getSample(0, i);

            // Process through each circuit stage in order
            x = inputBuffer.processSample(x);
            x = clipper.processSample(x);   // Nonlinear stage
            x = tone.processSample(x);
            x = outputBuffer.processSample(x);

            oversampledBlock.setSample(0, i, x);
        }

        // Optional: downsample back
        oversampling.processSamplesDown(outputBlock);
    }
};
```

**Key principle:** Each sub-circuit's `processSample()` function should be `inline`
and operate on a single sample. The per-sample loop is the innermost hot loop.

### 6.3 Parameter (Knob) Mapping to Component Values

Potentiometers in analog circuits act as variable resistors. In DSP code, the knob
position (0.0 to 1.0) must be mapped to the physical resistance value using the
appropriate taper curve.

#### Potentiometer Taper Types

```cpp
// LINEAR TAPER (B-taper): resistance varies linearly with position
// Used for: tone controls, blend knobs
float linearTaper(float position) {
    return position;  // Direct mapping
}

// AUDIO/LOGARITHMIC TAPER (A-taper): perceived loudness varies linearly
// Used for: volume controls, gain controls
// Simple parabolic approximation:
float audioTaper(float position) {
    return position * position;  // Parabolic approximation
}

// More accurate audio taper (from Orastron research):
// f(p) = a*p / (p + a - 1), where a controls the taper curve
float audioTaperAccurate(float position, float a = 0.15f) {
    if (position < 0.001f) return 0.0f;
    return (a * position) / (position + a - 1.0f);
}

// REVERSE AUDIO TAPER (C-taper):
float reverseAudioTaper(float position) {
    return 1.0f - (1.0f - position) * (1.0f - position);
}
```

#### Mapping Knob to Component Values

```cpp
// Example: Tube Screamer distortion knob
// P1 is a 500K logarithmic potentiometer controlling feedback gain
void setDistortion(float knobPosition) {
    float p = knobPosition;  // 0.0 to 1.0

    // Log taper approximation
    float logP = audioTaper(p);

    // The Tube Screamer feedback resistance = 51K fixed + up to 500K variable
    float resistance = 51.0e3 + logP * 500.0e3;  // 51K to 551K

    // Update the WDF resistor component
    r_distortion.setResistanceValue(resistance);
}

// Example: Tone control (variable cutoff frequency)
void setTone(float knobPosition) {
    float p = knobPosition;  // 0.0 to 1.0

    // DS-1 tone pot: 20K linear taper
    // Forms a variable voltage divider between high-pass and low-pass paths
    float r_hi = p * 20.0e3;         // Resistance to treble path
    float r_lo = (1.0f - p) * 20.0e3; // Resistance to bass path

    r_toneHi.setResistanceValue(r_hi + 100.0);   // Avoid zero
    r_toneLo.setResistanceValue(r_lo + 100.0);   // Avoid zero
}

// Example: Volume (output level)
void setVolume(float knobPosition) {
    float p = knobPosition;
    float gain = audioTaper(p);  // Audio taper for natural feel
    outputGain = gain;
}
```

#### Impedance Recalculation

When changing WDF component values at runtime, the port impedances must be
recalculated through the tree. chowdsp_wdf provides a utility for this:

```cpp
void updateParameters() {
    // Defer impedance propagation until all changes are made
    wdft::ScopedDeferImpedancePropagation defer { vs };

    r1.setResistanceValue(newR1Value);
    r2.setResistanceValue(newR2Value);
    c1.setCapacitanceValue(newC1Value);

    // Impedances propagate automatically when 'defer' goes out of scope
}
```

### 6.4 Anti-Aliasing: Oversampling vs. ADAA

Nonlinear processing (waveshaping, clipping) generates harmonics that can fold
back below the Nyquist frequency as aliasing artifacts. Two main approaches exist.

#### Oversampling

The traditional and straightforward approach:

```cpp
// Using JUCE's built-in oversampling
juce::dsp::Oversampling<float> oversampler {
    1,          // number of channels
    2,          // oversampling order (2^order factor, so 2 = 4x)
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
    true        // use maximum quality
};

void processBlock(juce::AudioBuffer<float>& buffer) {
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock = oversampler.processSamplesUp(block);

    // Process at higher sample rate
    for (int i = 0; i < oversampledBlock.getNumSamples(); ++i) {
        float x = oversampledBlock.getSample(0, i);
        x = nonlinearProcess(x);  // Clipping, waveshaping, etc.
        oversampledBlock.setSample(0, i, x);
    }

    oversampler.processSamplesDown(block);
}
```

**Typical oversampling factors:**
- 2x: Moderate distortion, light overdrive
- 4x: Standard distortion (good balance of quality vs. CPU)
- 8x: Heavy distortion, fuzz (ATK SD-1 uses this)
- 16x: Extreme precision (rarely needed in practice)

**CPU cost:** Scales linearly with oversampling factor.

#### ADAA (Antiderivative Anti-Aliasing)

The modern, more efficient approach:

**Repository:** https://github.com/jatinchowdhury18/ADAA

```cpp
// 1st-order ADAA for hard clipper
// Requires: the function f(x) and its first antiderivative F1(x)

// Hard clipper: f(x) = clamp(x, -1, 1)
inline double hardClip(double x) {
    return std::max(-1.0, std::min(1.0, x));
}

// First antiderivative of hard clipper:
// F1(x) = x^2/2 for |x| <= 1, |x| - 1/2 for |x| > 1
inline double hardClipAD1(double x) {
    double ax = std::abs(x);
    if (ax <= 1.0) return x * x / 2.0;
    return ax - 0.5;
}

// ADAA processing (1st order)
class ADAA1_HardClip {
    double x_prev = 0.0;

    double processSample(double x) {
        double diff = x - x_prev;

        double result;
        if (std::abs(diff) < 1e-7) {
            // Ill-conditioned case: fall back to direct evaluation
            result = hardClip((x + x_prev) / 2.0);
        } else {
            // ADAA formula: (F1(x) - F1(x_prev)) / (x - x_prev)
            result = (hardClipAD1(x) - hardClipAD1(x_prev)) / diff;
        }

        x_prev = x;
        return result;
    }
};
```

**For tanh soft clipping:**

```cpp
// tanh: f(x) = tanh(x)
// First antiderivative: F1(x) = log(cosh(x))
inline double tanhAD1(double x) {
    return std::log(std::cosh(x));
}
```

**Key findings on ADAA effectiveness:**
- 2x oversampling + 1st-order ADAA achieves similar aliasing suppression as
  6x oversampling without ADAA
- 2nd-order ADAA provides even better suppression
- ADAA can be applied within WDF circuits (paper: "Antiderivative Antialiasing
  in Nonlinear Wave Digital Filters", DAFx 2020)
- The ill-conditioned case (when consecutive samples are nearly equal) MUST be
  handled or the output will contain artifacts

**Recommendation for mobile/Android:** Use 2x oversampling combined with 1st-order
ADAA for the best balance of quality and CPU efficiency.

### 6.5 Tube Screamer Circuit Decomposition (Complete Reference)

The Tube Screamer (TS-9/TS-808) is the most thoroughly documented pedal circuit.
Here is the complete stage-by-stage implementation reference.

**Source:** https://www.electrosmash.com/tube-screamer-analysis

#### Stage 1: Input Buffer

```
Component values:
  R1 = 1K, R2 = 510K (bias), R3 = 10K (emitter)
  C1 = input coupling capacitor
  Q1 = 2SC1815 transistor (emitter follower)

Input impedance: ~446K ohm
Gain: unity (1.0)
```

**DSP implementation:** Can be modeled as a simple unity-gain buffer with a
high-pass filter (C1 coupling cap). The transistor buffer has negligible
nonlinearity in normal operation.

```cpp
// Simple RC high-pass for input coupling
BiquadFilter inputHighPass;  // Set cutoff around 30-50 Hz
```

#### Stage 2: Clipping Amplifier (Core Distortion)

```
Component values:
  Op-amp: JRC4558 (or equivalent)
  D1, D2: MA150 or 1N4148 (antiparallel clipping diodes)
  R4 = 4.7K (feedback resistor)
  R6 = 51K (fixed feedback)
  P1 = 500K logarithmic pot (distortion control)
  C3 = 0.047uF (high-pass in feedback, cutoff ~720Hz with R4)
  C4 = 51pF (low-pass across diodes, cutoff 5.6-61.2 kHz)
```

**How clipping works:**
- When output voltage exceeds diode forward voltage (~0.6V for silicon), diodes
  conduct, limiting gain from maximum (118) down to 1
- This is SOFT clipping because the diodes are in the feedback loop
- The high-pass in the feedback (C3 + R4, cutoff ~720Hz) means bass frequencies
  receive LESS clipping, creating the characteristic mid-boost

**DSP implementation:**
This is the stage that benefits most from WDF modeling with `DiodePairT`.
For a simpler implementation:

```cpp
// Simplified Tube Screamer clipping stage
struct TSClipping {
    double gain;           // Set by distortion knob (51K + up to 500K)
    BiquadFilter preHPF;   // 720 Hz high-pass (C3 + R4)
    BiquadFilter postLPF;  // Low-pass across diodes (C4)

    void setDistortion(float knobPos) {
        float logP = knobPos * knobPos;  // Log taper approximation
        gain = 1.0 + (51.0e3 + logP * 500.0e3) / 4.7e3;  // 11.8 to 118
    }

    double processSample(double x) {
        double filtered = preHPF.processSample(x);  // Bass bypass
        double amplified = filtered * gain;
        double clipped = softClipDiode(amplified);   // Diode pair model
        return postLPF.processSample(clipped);
    }

    // Simplified diode pair soft clipping
    double softClipDiode(double x) {
        // Approximate diode pair: output saturates at ~+/-0.6V
        const double Vd = 0.6;  // Forward voltage
        return Vd * std::tanh(x / Vd);
    }
};
```

#### Stage 3: Tone/Volume Control

```
Component values:
  R7 = 1K, C5 = 0.22uF (LPF cutoff ~723Hz)
  R8 = 220 ohm, C6 = 0.22uF
  P2 = 20K G-taper pot (tone)
  P3 = 100K audio pot (volume)
```

**DSP implementation:**

```cpp
struct TSToneControl {
    BiquadFilter toneFilter;

    void setTone(float knobPos) {
        // The tone control blends between low-pass and flat response
        // At minimum: strong LPF at ~723Hz
        // At maximum: relatively flat (slight HF boost)
        float cutoff = 723.0f + knobPos * 4000.0f;
        toneFilter.setLowpass(cutoff, 0.707f);
    }

    double processSample(double x) {
        return toneFilter.processSample(x);
    }
};
```

#### Stage 4: Output Buffer

```
Component values:
  Q3 = 2SC1815 (emitter follower)
  R12 = 510K (bias), R13 = 10K (emitter)
  RB = 100 ohm (series output), RC = 10K (shunt)
  C9 = 10uF (output coupling)

Output impedance: ~1.2K ohm
```

**DSP implementation:** Simple unity-gain buffer with high-pass for DC blocking.

### 6.6 DS-1 Key Differences from Tube Screamer

The DS-1 differs from the Tube Screamer in critical ways:

1. **Hard clipping vs. soft clipping:** DS-1 uses diodes to ground (hard clipping),
   while the TS uses diodes in the feedback loop (soft clipping)
2. **Pre-clipping high-pass:** DS-1 is ~72Hz (vs. TS ~720Hz), so more bass gets
   clipped
3. **Gain structure:** Single op-amp stage with transistor pre-gain
4. **Tone circuit:** Active filter with variable frequency response

### 6.7 Sample Rate Considerations

| Context | Typical Rate | Notes |
|---------|-------------|-------|
| Standard audio processing | 44.1/48 kHz | Base rate |
| WDF with 2x oversampling | 88.2/96 kHz | Good for mild overdrive |
| WDF with 4x oversampling | 176.4/192 kHz | Good for standard distortion |
| WDF with 8x oversampling | 352.8/384 kHz | Heavy distortion, fuzz |
| Benchmark standard | 192 kHz | Common for performance comparisons |

**Key insight from David Yeh's thesis:** "Typical digital implementations of
distortion upsample by a factor of eight or ten, process the nonlinearities,
and downsample back to typical audio rates." He also notes that "remaining
aliases at oversampling factors of eight or above tend to be masked by the
dense spectrum of guitar distortion."

For mobile/Android, 2x oversampling + ADAA is recommended as the starting point.

---

## 7. Performance Data

### 7.1 WDF Library Benchmarks (wdf-bakeoff)

**Source:** https://github.com/jatinchowdhury18/wdf-bakeoff

Processing 100 seconds of audio at 192 kHz sample rate:

| Circuit | chowdsp_wdf (Template C++) | chowdsp_wdf (Polymorphic C++) | FAUST wdmodels |
|---------|---------------------------|------------------------------|----------------|
| LPF | 0.21s (470x RT) | 0.81s (123x RT) | 0.47s (213x RT) |
| FF2 (Feed-forward) | 1.56s (64x RT) | 3.07s (33x RT) | 1.05s (95x RT) |
| Diode Clipper | 0.47s (214x RT) | 0.62s (161x RT) | 3.60s (27.8x RT) |

**RT = real-time ratio** (how many times faster than real-time)

**Key observations:**
- The template C++ API is consistently the fastest for nonlinear circuits
- For the diode clipper (the critical nonlinear stage), template C++ is **7.7x
  faster** than FAUST wdmodels (214x vs 27.8x real-time)
- The speed difference for nonlinear circuits comes from chowdsp_wdf's fast
  Wright Omega approximation vs. FAUST's Newton-Raphson iteration
- Even the slowest implementation (FAUST diode clipper at 27.8x real-time) is
  comfortably real-time

### 7.2 Oversampling CPU Impact

Oversampling multiplies CPU usage roughly linearly:

| Base rate | 2x OS | 4x OS | 8x OS |
|-----------|-------|-------|-------|
| 48 kHz (1x CPU) | 96 kHz (2x CPU) | 192 kHz (4x CPU) | 384 kHz (8x CPU) |

For a single WDF diode clipper at 48 kHz with 4x oversampling (192 kHz effective):
- chowdsp_wdf template: ~0.5% of one CPU core
- This means dozens of WDF circuit stages could run simultaneously

### 7.3 ADAA vs. Oversampling Trade-off

From Jatin Chowdhury's research:
- 2x oversampling + 1st-order ADAA achieves similar aliasing suppression as
  6x oversampling without ADAA
- This means ~3x CPU savings for equivalent quality
- 2nd-order ADAA provides even better results with minimal additional cost

### 7.4 Neural Network Performance

Neural approaches (NAM, GuitarLSTM) have different performance characteristics:
- NAM standard models: ~2-5% CPU per instance at 48 kHz
- GuitarLSTM: varies significantly with model size
- PedalNetRT (WaveNet): heavier, but optimized for real-time

Neural approaches are generally more CPU-intensive per instance than WDF but can
model circuits that are difficult to decompose into WDF trees.

---

## 8. Code Structure Recommendations

### 8.1 Recommended Architecture for Guitar Pedal Emulator App

```
app/src/main/cpp/
  dsp/
    core/
      AudioProcessor.h       -- Base class for all processors
      ParameterSmoothing.h   -- Parameter interpolation utilities
      Oversampling.h         -- Oversampling implementation
      ADAA.h                 -- Antiderivative anti-aliasing
      BiquadFilter.h         -- Standard biquad filter
    wdf/
      chowdsp_wdf.h          -- Include the chowdsp_wdf library (header-only)
    pedals/
      PedalBase.h            -- Base class for pedal emulations
      TubeScreamer.h/.cpp    -- TS-9/TS-808 emulation
      DS1.h/.cpp             -- Boss DS-1 emulation
      ProCoRat.h/.cpp        -- ProCo RAT emulation
      FuzzFace.h/.cpp        -- Dunlop Fuzz Face emulation
      KlonCentaur.h/.cpp     -- Klon Centaur emulation
      BigMuff.h/.cpp         -- EHX Big Muff emulation
    modulation/
      UniVibe.h/.cpp         -- Uni-Vibe emulation
      Chorus.h/.cpp          -- Chorus effect
      Tremolo.h/.cpp         -- Tremolo effect
    chain/
      SignalChain.h/.cpp     -- Manages ordered list of processors
    native-bridge.cpp        -- JNI bridge to Android/Kotlin
```

### 8.2 Base Processor Class

```cpp
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    // Called once when sample rate or block size changes
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    // Called per audio block
    virtual void processBlock(float* buffer, int numSamples) = 0;

    // Parameter interface
    virtual void setParameter(int paramId, float value) = 0;

    // Enable/disable (bypass)
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

protected:
    double sampleRate = 48000.0;
    bool enabled = true;
};
```

### 8.3 Pedal Base Class

```cpp
class PedalBase : public AudioProcessor {
public:
    void processBlock(float* buffer, int numSamples) override {
        if (!enabled) return;

        // Apply input gain
        for (int i = 0; i < numSamples; ++i)
            buffer[i] *= inputGain;

        // Upsample if needed
        if (oversamplingFactor > 1) {
            oversampler.upsample(buffer, numSamples, upsampledBuffer);
            int upsamples = numSamples * oversamplingFactor;
            processInternal(upsampledBuffer, upsamples);
            oversampler.downsample(upsampledBuffer, upsamples, buffer);
        } else {
            processInternal(buffer, numSamples);
        }

        // Apply output level
        for (int i = 0; i < numSamples; ++i)
            buffer[i] *= outputLevel;
    }

protected:
    // Subclasses implement per-sample processing
    virtual void processInternal(float* buffer, int numSamples) = 0;

    float inputGain = 1.0f;
    float outputLevel = 1.0f;
    int oversamplingFactor = 2;
    Oversampler oversampler;
};
```

### 8.4 Example: Simplified Tube Screamer Implementation

```cpp
class TubeScreamer : public PedalBase {
public:
    // Parameter IDs
    enum { DRIVE = 0, TONE = 1, LEVEL = 2 };

    void prepare(double sr, int maxBlock) override {
        sampleRate = sr;
        double effectiveSR = sr * oversamplingFactor;

        // Input coupling high-pass (~30 Hz)
        inputHPF.setHighPass(effectiveSR, 30.0, 0.707);

        // Feedback high-pass (~720 Hz) -- defines the mid-boost character
        feedbackHPF.setHighPass(effectiveSR, 720.0, 0.707);

        // Post-clipping low-pass (~5 kHz at min drive)
        postLPF.setLowPass(effectiveSR, 5000.0, 0.707);

        // Tone filter
        updateToneFilter();

        oversampler.prepare(maxBlock, oversamplingFactor);
    }

    void setParameter(int paramId, float value) override {
        switch (paramId) {
            case DRIVE: setDrive(value); break;
            case TONE:  setTone(value); break;
            case LEVEL: outputLevel = audioTaper(value); break;
        }
    }

protected:
    void processInternal(float* buffer, int numSamples) override {
        for (int i = 0; i < numSamples; ++i) {
            double x = buffer[i];

            // Stage 1: Input buffer (high-pass coupling)
            x = inputHPF.processSample(x);

            // Stage 2: Clipping amplifier
            // Apply frequency-selective gain (bass bypasses clipping)
            double highFreqContent = feedbackHPF.processSample(x);
            double clippedHigh = softClipDiode(highFreqContent * driveGain);
            double bassContent = x - highFreqContent;
            x = bassContent + clippedHigh;

            // Post-clipping filter
            x = postLPF.processSample(x);

            // Stage 3: Tone control
            x = toneFilter.processSample(x);

            buffer[i] = static_cast<float>(x);
        }
    }

private:
    double driveGain = 10.0;
    BiquadFilter inputHPF, feedbackHPF, postLPF, toneFilter;

    void setDrive(float knobPos) {
        float logP = knobPos * knobPos;  // Audio taper
        // Feedback resistance: 51K fixed + up to 500K variable
        double R_feedback = 51.0e3 + logP * 500.0e3;
        driveGain = R_feedback / 4.7e3;  // Gain = R_feedback / R4
    }

    void setTone(float knobPos) {
        float cutoff = 723.0f + knobPos * 4000.0f;
        toneFilter.setLowPass(sampleRate * oversamplingFactor, cutoff, 0.707);
    }

    void updateToneFilter() {
        setTone(0.5f);  // Default mid position
    }

    // Soft clipping model for antiparallel diode pair in feedback loop
    double softClipDiode(double x) {
        const double Vd = 0.6;  // Diode forward voltage
        return Vd * std::tanh(x / Vd);
    }

    float audioTaper(float p) {
        return p * p;
    }
};
```

### 8.5 Signal Chain Manager

```cpp
class SignalChain {
public:
    void addProcessor(std::unique_ptr<AudioProcessor> processor) {
        processors.push_back(std::move(processor));
    }

    void prepare(double sampleRate, int maxBlockSize) {
        for (auto& proc : processors) {
            proc->prepare(sampleRate, maxBlockSize);
        }
    }

    void processBlock(float* buffer, int numSamples) {
        for (auto& proc : processors) {
            if (proc->isEnabled()) {
                proc->processBlock(buffer, numSamples);
            }
        }
    }

    void setProcessorParameter(int processorIndex, int paramId, float value) {
        if (processorIndex < static_cast<int>(processors.size())) {
            processors[processorIndex]->setParameter(paramId, value);
        }
    }

private:
    std::vector<std::unique_ptr<AudioProcessor>> processors;
};
```

### 8.6 Integration with chowdsp_wdf (Full WDF Pedal Model)

For maximum accuracy, use WDF for the nonlinear clipping stages:

```cpp
class DS1_WDF : public PedalBase {
    // WDF Diode Clipper sub-circuit
    struct DiodeClipperCircuit {
        wdft::ResistorT<double> r_in { 2.2e3 };
        wdft::CapacitorT<double> c_couple { 47.0e-9 };
        wdft::WDFSeriesT<double, decltype(r_in), decltype(c_couple)> s1 {
            r_in, c_couple
        };

        wdft::ResistorT<double> r_load { 100.0e3 };
        wdft::WDFParallelT<double, decltype(s1), decltype(r_load)> p1 {
            s1, r_load
        };

        wdft::IdealVoltageSourceT<double, decltype(p1)> vs { p1 };

        // DS-1 uses 1N4148 silicon diodes for hard clipping
        wdft::DiodePairT<double, decltype(p1)> diodes {
            p1,
            2.52e-9,    // Is for 1N4148
            25.85e-3    // Vt at room temp
        };

        void prepare(double sampleRate) {
            c_couple.prepare(sampleRate);
        }

        double processSample(double x) {
            vs.setVoltage(x);
            diodes.incident(p1.reflected());
            p1.incident(diodes.reflected());
            return wdft::voltage<double>(c_couple);
        }
    };

    DiodeClipperCircuit clipper;
    BiquadFilter preGainFilter;  // Pre-clipping EQ
    BiquadFilter toneFilter;     // Post-clipping tone

    void prepare(double sr, int maxBlock) override {
        double effectiveSR = sr * oversamplingFactor;
        clipper.prepare(effectiveSR);
        preGainFilter.setHighPass(effectiveSR, 72.0, 0.707);
        // ... additional initialization
    }

    void processInternal(float* buffer, int numSamples) override {
        for (int i = 0; i < numSamples; ++i) {
            double x = buffer[i];
            x = preGainFilter.processSample(x) * driveGain;
            x = clipper.processSample(x);
            x = toneFilter.processSample(x);
            buffer[i] = static_cast<float>(x);
        }
    }

    double driveGain = 1.0;
};
```

---

## 9. Complete URL and License Index

### 9.1 WDF Libraries

| Project | URL | License |
|---------|-----|---------|
| chowdsp_wdf | https://github.com/Chowdhury-DSP/chowdsp_wdf | BSD 3-Clause |
| WDF++ | https://github.com/AndrewBelt/WDFplusplus | MIT |
| RT-WDF | https://github.com/RT-WDF/rt-wdf_lib | Check repo |
| WaveDigitalFilters (examples) | https://github.com/jatinchowdhury18/WaveDigitalFilters | GPL-3.0 |
| Differentiable WDFs | https://github.com/jatinchowdhury18/differentiable-wdfs | Check repo |
| pywdf (Python) | https://github.com/gusanthon/pywdf | Check repo |
| wdf-bakeoff (benchmarks) | https://github.com/jatinchowdhury18/wdf-bakeoff | Check repo |

### 9.2 ChowDSP Plugins

| Project | URL | License |
|---------|-----|---------|
| ChowCentaur (Klon) | https://github.com/jatinchowdhury18/KlonCentaur | BSD-3-Clause |
| CHOW Tape Model | https://github.com/jatinchowdhury18/AnalogTapeModel | GPL-3.0 |
| BYOD | https://github.com/Chowdhury-DSP/BYOD | GPL-3.0 |
| ChowKick | https://github.com/Chowdhury-DSP/ChowKick | Check repo |
| ChowPhaser | https://github.com/jatinchowdhury18/ChowPhaser | Check repo |
| ADAA Experiments | https://github.com/jatinchowdhury18/ADAA | Check repo |
| RTNeural | https://github.com/jatinchowdhury18/RTNeural | Check repo |

### 9.3 Pedal Emulation Projects

| Project | URL | License |
|---------|-----|---------|
| DS1.lv2 | https://github.com/LiamLombard/DS1.lv2 | Check repo |
| Kapitonov-Plugins-Pack | https://github.com/olegkapitonov/Kapitonov-Plugins-Pack | Check repo |
| Proco-Rat | https://github.com/Rudro085/Proco-Rat | MIT |
| PiRat | https://github.com/nezetic/pirat | BSD-2-Clause |
| MetalTone (MT-2) | https://github.com/brummer10/MetalTone | Check repo |
| ATK-MT2 | https://github.com/AudioTK/ATK-MT2 | Check repo |
| AudioTK | https://github.com/AudioTK/AudioTK | Check repo |
| ATK Plugins | https://github.com/mbrucher/ATK-plugins | Check repo |
| ATK Modelling Lite | https://github.com/mbrucher/ATK-modelling-lite | Check repo |
| cppAudioFX | https://github.com/maximoskp/cppAudioFX | Check repo |
| Party Panda Univibe | https://github.com/mattanikiej/party-panda-univibe | Check repo |

### 9.4 Neural Network Projects

| Project | URL | License |
|---------|-----|---------|
| Neural Amp Modeler | https://github.com/sdatkinson/neural-amp-modeler | MIT |
| NAM Plugin | https://github.com/sdatkinson/NeuralAmpModelerPlugin | GPL-3.0 |
| NeuralPi | https://github.com/GuitarML/NeuralPi | GPL-3.0 |
| GuitarLSTM | https://github.com/GuitarML/GuitarLSTM | MIT |
| PedalNetRT | https://github.com/GuitarML/PedalNetRT | MIT |
| SmartGuitarAmp | https://github.com/GuitarML/SmartGuitarAmp | GPL-3.0 |
| Mercury | https://github.com/GuitarML/Mercury | Check repo |

### 9.5 FAUST Resources

| Resource | URL | License |
|----------|-----|---------|
| FAUST wdmodels library docs | https://faustlibraries.grame.fr/libs/wdmodels/ | Check repo |
| FAUST wdmodels source | https://github.com/grame-cncm/faustlibraries/blob/master/wdmodels.lib | Check repo |
| FAUST AANL library docs | https://faustlibraries.grame.fr/libs/aanl/ | Check repo |
| FAUST demos | https://faustlibraries.grame.fr/libs/demos/ | Check repo |
| droosenb/faust-wdf-library | https://github.com/droosenb/faust-wdf-library | Check repo |
| Guitarix website | https://guitarix.org/ | GPL |
| Guitarix GitHub | https://github.com/brummer10/guitarix | GPL |
| GxPlugins.lv2 | https://github.com/brummer10/GxPlugins.lv2 | Check repo |
| guitarix.dsp (FAUST example) | https://github.com/grame-cncm/faust/blob/master-dev/examples/misc/guitarix.dsp | Check repo |
| thedrgreenthumb FAUST effects | https://github.com/thedrgreenthumb/faust | Check repo |

### 9.6 Circuit Analysis Resources

| Resource | URL |
|----------|-----|
| Electrosmash DS-1 Analysis | https://www.electrosmash.com/boss-ds1-analysis |
| Electrosmash Tube Screamer Analysis | https://www.electrosmash.com/tube-screamer-analysis |
| Electrosmash ProCo RAT Analysis | https://www.electrosmash.com/proco-rat |
| Electrosmash Homepage | https://www.electrosmash.com/ |
| LiveSPICE | https://github.com/dsharlet/LiveSPICE |
| LiveSPICE Website | https://www.livespice.org/ |
| Rakarrack (effects source) | https://sourceforge.net/projects/rakarrack |
| MusicDSP Univibe | https://www.musicdsp.org/en/latest/Effects/277-univox-univibe-emulator.html |
| CCRMA Fuzz Face WDF | https://ccrma.stanford.edu/wiki/Wave_Digital_Filters_applied_to_the_Dunlop_%22Fuzz_Face%22_Distortion_Circuit |
| Orastron Pot Mapping | https://www.orastron.com/blog/potentiometers-parameter-mapping-part-1 |
| GuitarML Website | https://guitarml.com/ |
| Neural Amp Modeler Website | https://www.neuralampmodeler.com/ |

### 9.7 Academic Papers and References

| Paper | Authors | URL |
|-------|---------|-----|
| Digital Implementation of Musical Distortion Circuits (PhD thesis) | David Yeh | https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf |
| David Yeh publications page | David Yeh | https://ccrma.stanford.edu/~dtyeh/papers/pubs.html |
| Virtual Analog Modeling Using WDF (PhD thesis) | Kurt Werner | https://stacks.stanford.edu/file/druid:jy057cz8322/KurtJamesWernerDissertation-augmented.pdf |
| chowdsp_wdf library paper | Jatin Chowdhury | https://arxiv.org/abs/2210.12554 |
| Klon Centaur comparison paper | Jatin Chowdhury | https://arxiv.org/abs/2009.02833 |
| Improved Diode Clipper Model for WDF | Werner et al. | https://www.researchgate.net/publication/299514713 |
| WDF Adaptors for Arbitrary Topologies | Werner, Smith, Abel | https://dafx.de/paper-archive/2015/DAFx-15_submission_53.pdf |
| WDF Modeling with Op Amps | Werner, Smith, Abel | https://eurasip.org/Proceedings/Eusipco/Eusipco2016/papers/1570255463.pdf |
| ADAA for Stateful Systems | Parker, Esqueda, Bilbao | https://www.mdpi.com/2076-3417/10/1/20 |
| ADAA in Nonlinear WDF | Chowdhury | https://dafx2020.mdw.ac.at/proceedings/papers/DAFx2020_paper_35.pdf |
| Emulating Diode Circuits with Differentiable WDF | Chowdhury | https://www.researchgate.net/publication/361416911 |
| Practical ADAA Considerations | Jatin Chowdhury | https://jatinchowdhury18.medium.com/practical-considerations-for-antiderivative-anti-aliasing-d5847167f510 |
| Alias Suppression in Digital Distortion | Martin Vicanek | https://vicanek.de/articles/AADistortion.pdf |
| pywdf paper | Antonacci | https://www.dafx.de/paper-archive/2023/DAFx23_paper_23.pdf |
| FAUST wdmodels paper | -- | https://zenodo.org/records/5045088/files/SMC_2021_paper_83.pdf |
| Virtual Electric Guitars in FAUST | Julius O. Smith III | https://ccrma.stanford.edu/~jos/pdf/Acoustics08.pdf |
| FAUST Guitar Effects (LAC 2008) | Julius O. Smith III | http://lac.linuxaudio.org/2008/download/papers/22.pdf |
| DS-1 Simplified Modelling | -- | https://www.academia.edu/65479550/Simplified_Modelling_of_the_Boss_DS_1_Distortion_Pedal |
| WDF Tutorial | David Yeh | https://ccrma.stanford.edu/~dtyeh/papers/wdftutorial.pdf |
| CCRMA WDF Lecture Notes | Julius O. Smith III | https://ccrma.stanford.edu/~jos/WaveDigitalFilters/WaveDigitalFilters_4up.pdf |
| Resolving WDF with Multiple Nonlinearities | -- | https://ccrma.stanford.edu/~jingjiez/portfolio/gtr-amp-sim/pdfs/Resolving%20Wave%20Digital%20Filters%20with%20MultipleMultiport%20Nonlinearities.pdf |
| WDF Theory and Practice (Fettweis) | Alfred Fettweis | https://ccrma.stanford.edu/~jingjiez/portfolio/gtr-amp-sim/pdfs/Wave%20Digital%20Filters%20Theory%20and%20Practice.pdf |
| Simplifying ADAA with Lookup Tables | -- | https://dafx25.dii.univpm.it/wp-content/uploads/2025/08/DAFx25_paper_30.pdf |
| Explicit Vector WDF Modeling | -- | https://www.dafx.de/paper-archive/2023/DAFx23_paper_60.pdf |

---

## Summary of Key Recommendations

1. **Primary WDF Library:** Use chowdsp_wdf (BSD-3-Clause) -- it is header-only,
   ARM/NEON compatible, and the fastest option for nonlinear circuits (214x
   real-time for diode clipper at 192 kHz).

2. **Circuit Decomposition:** Model each pedal as a chain of sub-circuits: input
   buffer, gain/clipping stage, tone control, output buffer. Use biquad filters
   for linear stages and WDF with DiodePairT for nonlinear clipping stages.

3. **Anti-Aliasing:** Use 2x oversampling + 1st-order ADAA for mobile targets.
   This achieves quality comparable to 6x oversampling with ~3x less CPU usage.

4. **Parameter Mapping:** Map knob positions (0.0-1.0) to component values using
   taper curves (linear, audio/log), then update WDF component values and
   recalculate impedances using ScopedDeferImpedancePropagation.

5. **Reference Materials:** Use Electrosmash for circuit schematics with component
   values. Use David Yeh's thesis for mathematical framework. Use BYOD source
   code for real-world implementation patterns.

6. **For prototyping:** Use pywdf (Python) or FAUST wdmodels to validate circuit
   models before implementing in C++ with chowdsp_wdf.

7. **Neural alternative:** For pedals that are difficult to decompose into WDF
   trees, consider Neural Amp Modeler (NAM) for training black-box models from
   audio recordings of real hardware.
