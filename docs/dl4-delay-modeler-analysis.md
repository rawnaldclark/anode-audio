# Line 6 DL4 Delay Modeler -- Complete Technical Analysis

## Table of Contents
1. [Overall Architecture](#1-overall-architecture)
2. [Physical Layout & Controls](#2-physical-layout--controls)
3. [All 16 Delay Models -- Complete Reference](#3-all-16-delay-models)
4. [DSP Implementation Details per Model](#4-dsp-implementation-details)
5. [Key Sonic Characteristics](#5-key-sonic-characteristics)
6. [The Edge (U2) Settings & Famous Users](#6-the-edge-u2-settings)
7. [Loop Sampler Specifications](#7-loop-sampler-specifications)
8. [Factory Presets](#8-factory-presets)
9. [Implementation Plan for Our App](#9-implementation-plan-for-our-app)

---

## 1. Overall Architecture

### Hardware Platform
- **DSP Chip**: Motorola DSP56364 (56-bit accumulator, 24-bit data path)
- **Sample Rate**: 31.25 kHz (confirmed by US Patent 5,789,689: "31.2 kHz preferred")
- **Internal Processing**: 24-bit fixed-point (the DSP56364 is a 24-bit processor with 56-bit accumulators for intermediate calculations)
- **A/D Converter**: 24-bit sigma-delta
- **D/A Converter**: 24-bit sigma-delta
- **DRAM Buffer**: 64K words (1024 kbits) for delay lines -- enough for ~2.1 seconds at 31.25 kHz per channel. Later revisions may have expanded this for the 2.5s maximum delay time
- **Power**: 4x C batteries or 9VAC 1200mA power supply
- **Bypass**: True bypass via mechanical relays (input-to-output hardwire). Alternate "DSP bypass" mode available (delays trail off when bypassed)

### Signal Flow
```
Guitar Input --> Anti-alias LPF --> 24-bit ADC --> DSP56364 Processing --> 24-bit DAC --> Output LPF --> Output
                                                       |
                                              [Model Algorithm]
                                              [Delay Line Memory]
                                              [Feedback Path]
                                              [Mix Control]
```

### Key Architecture Notes
- **Stereo I/O**: Left/Right inputs and outputs (the DL4 is true stereo for stereo delay models)
- **Mono compatibility**: Left/Mono input and output for mono rigs
- **Expression pedal input**: Interpolates between two parameter snapshots (heel-down and toe-down positions stored per preset)
- **3 user presets + tap tempo**: Footswitches A, B, C for preset recall; D for tap tempo
- **Oversampling**: The Line 6 patent describes 8x oversampling specifically for nonlinear processing (distortion/saturation). For linear delay processing, the native 31.25 kHz rate is used directly

### Why 31.25 kHz?
This is exactly 1 MHz / 32 -- a convenient division for the DSP56364's clock architecture. The ~15.6 kHz Nyquist frequency is adequate for guitar (most guitar signal energy is below 6-8 kHz). The lower sample rate maximizes available delay time from the fixed DRAM buffer size.

---

## 2. Physical Layout & Controls

### Front Panel (Top to Bottom)
```
+================================================================+
|                                                                  |
|  [MODEL SELECTOR]  16-position rotary knob (leftmost)           |
|                                                                  |
|  [DELAY TIME]  [REPEATS]  [TWEAK]  [TWEEZ]  [MIX]             |
|     knob         knob      knob     knob    knob               |
|                                                                  |
|  (A)           (B)           (C)           (TAP TEMPO)          |
| RECORD/       PLAY/         PLAY          1/2 SPEED/            |
| OVERDUB       STOP          ONCE          REVERSE               |
|  [LED]         [LED]         [LED]         [LED]                |
|                                                                  |
+================================================================+
```

### Model Selector Positions (clockwise from top)
```
Position 1:  DIGITAL DELAY        Position 9:   PING PONG
Position 2:  DIGITAL W/ MOD       Position 10:  REVERSE
Position 3:  RHYTHMIC DELAY       Position 11:  DYNAMIC DELAY
Position 4:  STEREO DELAYS        Position 12:  AUTO-VOLUME ECHO
Position 5:  LO RES DELAY         Position 13:  TUBE ECHO
Position 6:  ANALOG W/ MOD        Position 14:  TAPE ECHO
Position 7:  ANALOG ECHO          Position 15:  MULTI-HEAD
Position 8:  SWEEP ECHO           Position 16:  LOOP SAMPLER
```

### Universal Controls (same knobs for all models)
| Knob | Function | Range |
|------|----------|-------|
| **DELAY TIME** | Sets delay time (or left channel delay for stereo models) | Model-dependent, up to 2.5s |
| **REPEATS** | Feedback amount (number of echoes) | 1 repeat to infinite/self-oscillation |
| **TWEAK** | Model-specific parameter (see per-model table below) | 0.0 to 1.0 (full knob range) |
| **TWEEZ** | Model-specific parameter (see per-model table below) | 0.0 to 1.0 (full knob range) |
| **MIX** | Wet/dry blend | Full dry (CCW) to full wet (CW) |

### Footswitches
| Switch | Normal Mode | Loop Sampler Mode |
|--------|------------|-------------------|
| **A** | Preset A recall/bypass | Record / Overdub |
| **B** | Preset B recall/bypass | Play / Stop |
| **C** | Preset C recall/bypass | Play Once |
| **TAP TEMPO** | Tap to set delay time | 1/2 Speed (single tap) / Reverse (double tap) |

### Physical Dimensions & Appearance
- **Color**: Signature Line 6 green ("Stompbox Green")
- **Size**: Approximately 10" x 7.5" x 2.5" (large-format stompbox)
- **Weight**: ~2.5 lbs with batteries
- **Construction**: Die-cast metal chassis
- **LED Colors**: Red LEDs for each preset/function

---

## 3. All 16 Delay Models -- Complete Reference

### Model 1: TUBE ECHO
**Based on**: Maestro Echoplex EP-1 (1963)

**History**: The EP-1 was the first Echoplex, made by Harris-Teller in Chicago. It used a vacuum tube (12AX7) preamp stage with a looped 1/4" tape cartridge passing separate record and playback heads. The tube electronics and tape saturation combined to produce a characteristically warm, harmonically rich echo that became the gold standard for guitar delay.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time (up to 2.5 seconds) |
| REPEATS | Number of echoes (feedback amount) |
| **TWEAK** | **Wow & Flutter** -- emulates the mechanical imperfections of the tape transport motor speed variations |
| **TWEEZ** | **Drive** -- amount of tube distortion and tape saturation on the echoes |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Warm, dark repeats that degrade progressively (treble loss + saturation increase per repeat)
- The tube preamp adds 2nd/3rd harmonics to the signal
- Tape wow/flutter creates subtle pitch modulation (typically 0.5-3 Hz, 5-15 cents depth)
- At high Drive settings, echoes become increasingly saturated and compressed
- Self-oscillation at max Repeats creates warm, organ-like feedback tones
- THIS IS THE MOST COVETED DL4 MODEL -- the combination of tube warmth and tape degradation is uniquely musical

---

### Model 2: TAPE ECHO
**Based on**: Maestro Echoplex EP-3 (solid-state)

**History**: The EP-3 replaced the EP-1's tube electronics with transistors (solid-state). Same mechanical tape loop design but cleaner, less distorted repeats. Famously used by Eddie Van Halen and Jimmy Page. The EP-3's JFET preamp (TIS58/2N5457) became legendary in its own right -- many guitarists ran it as an always-on "tone enhancer" for the preamp character alone.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time (up to 2.5 seconds) |
| REPEATS | Number of echoes (feedback amount) |
| **TWEAK** | **Bass** -- low-frequency tone control for the echoes |
| **TWEEZ** | **Treble** -- high-frequency tone control for the echoes |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Cleaner than Tube Echo but still has tape character (transport artifacts, gentle saturation)
- Each repeat loses high-frequency content (built-in treble roll-off in feedback path)
- Bass/Treble controls allow shaping the echo tone independently of the dry signal
- Slightly brighter and more "hi-fi" than the EP-1 model
- Tape transport artifacts present but less pronounced than the tube model

---

### Model 3: MULTI-HEAD
**Based on**: Roland RE-101 Space Echo

**History**: Roland's Space Echo series (RE-101, RE-201, RE-301, RE-501) used a loop of 1/4" magnetic tape with multiple stationary playback heads at fixed positions. Unlike the Echoplex's movable head, the Space Echo achieves different delay times by selecting which heads are active and varying the motor speed. The multi-head capability creates complex rhythmic patterns impossible with single-head designs.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Motor speed (scales ALL head times proportionally) |
| REPEATS | Feedback amount |
| **TWEAK** | **Heads 1 & 2 select** -- turns emulated tape heads 1 and 2 on/off |
| **TWEEZ** | **Heads 3 & 4 select** -- turns emulated tape heads 3 and 4 on/off |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Head time ratios approximately 1:2:3:4 (matching the physical head spacing of the RE-101)
- At nominal speed: Head 1 ~66ms, Head 2 ~132ms, Head 3 ~198ms, Head 4 ~264ms (scales with Delay Time)
- Multiple active heads create complex polyrhythmic echo patterns
- Tape saturation and treble loss per repeat (same as our RE-201 analysis)
- The interaction between multiple heads and feedback creates dense, evolving textures
- Tweak/Tweez function as quasi-switches: below ~25% = head off, above ~25% = head on (with crossfade zone)

**Note**: The manual says "based on RE-101" but the Helix model list says "RE-101 Space Echo." The RE-101 was actually the most basic Space Echo (no spring reverb, no chorus). Our existing RE-201 research (docs/re-201-space-echo-circuit-analysis.md) covers the full circuit in detail -- the RE-101 uses the same tape transport and head arrangement but without the reverb section.

---

### Model 4: SWEEP ECHO
**Based on**: Line 6 Original (EP-1 tape tone + sweeping filter)

**History**: A Line 6 original design that combines the basic tape echo character of the EP-1 with a sweeping bandpass/resonant filter applied to the delay repeats. This creates an auto-wah-like effect on the echoes while leaving the dry signal untouched.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time |
| REPEATS | Feedback amount |
| **TWEAK** | **Sweep Speed** -- rate of the filter sweep (LFO rate) |
| **TWEEZ** | **Sweep Depth** -- amount of filter frequency modulation |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Tape-colored echoes with a slowly sweeping resonant filter
- The filter is applied only in the feedback path, so each repeat is progressively more filtered
- At maximum depth, echoes morph dramatically in tone with each repeat
- At minimum depth (Tweez = 0), behaves like a standard tape echo
- Expression pedal recommended: sweep from no modulation (heel) to deep modulation (toe)
- The sweep is an LFO-driven bandpass/resonant filter, likely ~200 Hz to ~4 kHz range

---

### Model 5: ANALOG ECHO
**Based on**: Boss DM-2

**History**: The Boss DM-2 (1981) was one of the most popular analog delay pedals, using MN3005 BBD (bucket brigade device) chips. BBD delays pass the signal through a chain of capacitors clocked at a specific rate, introducing characteristic "warmth" through bandwidth limitation and mild distortion at each stage. The DM-2 offered 20-300ms delay time with a signature dark, warm tone.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time (up to 2.5 seconds -- far beyond a real DM-2's 300ms) |
| REPEATS | Feedback amount |
| **TWEAK** | **Bass** -- low-frequency tone control |
| **TWEEZ** | **Treble** -- high-frequency tone control |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Dark, warm repeats with significant treble roll-off per repeat cycle
- BBD clock noise simulation (subtle high-frequency artifacts, typically filtered but present)
- Bandwidth limitation: real DM-2 has ~3-5 kHz bandwidth that narrows further with longer delay times
- Gentle compression/saturation from the BBD charge transfer imperfections
- At max Repeats, self-oscillation is warm and smooth (not harsh like digital)
- The manual specifically mentions: spinning the Delay Time knob at max Repeats creates "space-aged speeding race car imploding" -- classic analog pitch-shifting feedback effect

---

### Model 6: ANALOG ECHO w/ MOD
**Based on**: Electro-Harmonix Deluxe Memory Man

**History**: The EHX Deluxe Memory Man (1978) combined a BBD delay (MN3005/MN3008) with an analog chorus/vibrato circuit. The chorus modulation is applied only to the delayed signal, creating lush, swimming echoes. The Memory Man was essential to U2's first album (Boy, 1980) and became synonymous with "shoegaze" and post-punk guitar tones. The original had ~500ms max delay; the DL4 extends this to 2.5 seconds.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time (up to 2.5 seconds) |
| REPEATS | Feedback amount |
| **TWEAK** | **Modulation Speed** -- chorus LFO rate applied to echoes |
| **TWEEZ** | **Modulation Depth** -- chorus pitch deviation depth |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Same warm, dark BBD character as Analog Echo, PLUS chorus modulation
- Modulation applied to wet/echo signal only (dry signal remains unaffected)
- The chorus creates pitch variation in the repeats (typically +/- 5-15 cents)
- Each successive repeat accumulates more modulation, creating increasingly "smeared" echoes
- At medium modulation, produces the classic "swimming" echo sound
- At high modulation depth, echoes become noticeably pitch-wobbled -- great for ambient textures
- The combination of BBD warmth + chorus is uniquely lush

---

### Model 7: LO RES DELAY
**Based on**: Early 1980s digital delay units (8-bit era)

**History**: The first digital delays (early 1980s) used primitive A/D converters with as few as 8 bits of resolution. This low bit depth created distinctive quantization noise, aliasing artifacts, and a "grungy" character. Some early digital samplers (Fairlight, Ensoniq Mirage) are prized for exactly this lo-fi quality. This model emulates that era's character.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time |
| REPEATS | Feedback amount |
| **TWEAK** | **Tone** -- overall frequency response/filtering |
| **TWEEZ** | **Resolution** -- digital bit depth from 24 bits (clean) down to 6 bits (crunchy) |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- At 24-bit (Tweez fully CW): clean, transparent delay
- At 6-bit (Tweez fully CCW): extreme quantization noise, staircase waveforms, digital grunge
- Quantization noise increases proportionally with signal level (unlike analog noise which is constant)
- Aliasing artifacts appear as metallic, inharmonic tones
- The Tone control helps tame or emphasize the harshness
- At intermediate bit depths (10-12 bit), produces a usable "vintage digital" character
- Self-oscillation with low bit depth creates increasingly distorted, chaotic feedback

---

### Model 8: DIGITAL DELAY
**Based on**: Line 6 Original (clean digital delay, comparable to Boss DD-series)

**History**: A pristine, high-quality digital delay using the full 24-bit resolution of the DL4's converters. This is the "transparent" option -- what you put in comes back out unchanged, except for the delay time and number of repeats. The bass/treble controls allow shaping the echo tone.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time (up to 2.5 seconds) |
| REPEATS | Feedback amount |
| **TWEAK** | **Bass** -- low-frequency tone control for echoes |
| **TWEEZ** | **Treble** -- high-frequency tone control for echoes |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Crystal-clear, uncolored repeats at neutral Tweak/Tweez settings
- No saturation, no modulation, no pitch artifacts -- pure digital delay
- Bass/Treble controls are in the feedback path, so each repeat is progressively shaped
- With Bass reduced and Treble reduced: echoes darken progressively (fake analog character)
- With Treble boosted: echoes remain bright and present (modern production sound)
- Self-oscillation is harsh and digital (unlike the warm self-oscillation of analog models)
- This is THE model for precise rhythmic delay playing (The Edge's dotted-eighth style)

---

### Model 9: DIGITAL DELAY w/ MOD
**Based on**: Line 6 Original

**History**: The clean Digital Delay model enhanced with a chorus/vibrato modulation circuit applied to the echo repeats only. Combines digital clarity with analog-style modulation warmth.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time |
| REPEATS | Feedback amount |
| **TWEAK** | **Modulation Speed** -- chorus LFO rate on echoes |
| **TWEEZ** | **Modulation Depth** -- chorus pitch deviation |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Clean digital repeats with added chorus modulation
- Unlike Analog w/ Mod, the base delay tone is bright and clear (no BBD coloration)
- Modulation creates pitch variation in repeats (wet signal only)
- At low depth: subtle thickening of echoes
- At high depth: obvious pitch wobble, approaching vibrato territory
- Useful for ambient/atmospheric textures where you want clarity plus movement
- Each successive repeat accumulates more pitch deviation

---

### Model 10: RHYTHMIC DELAY
**Based on**: Line 6 Original

**History**: A creative delay that synchronizes echo timing to note subdivisions relative to a tapped tempo. Instead of setting delay time directly, you tap quarter notes, then select the rhythmic subdivision for the echoes. This makes it trivial to get musically useful delay times (dotted eighths, triplets, etc.) without calculating milliseconds.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | **Rhythm subdivision selector** (6 positions): quarter, dotted eighth, eighth, eighth triplet, sixteenth, dotted sixteenth |
| REPEATS | Feedback amount |
| **TWEAK** | **Modulation Speed** -- chorus LFO rate on echoes |
| **TWEEZ** | **Modulation Depth** -- chorus pitch deviation |
| MIX | Wet/dry blend |

**Delay Time Knob Subdivisions** (relative to tapped quarter note):
| Knob Position | Subdivision | Ratio to Quarter |
|---------------|-------------|-----------------|
| Full CCW | Quarter note | 1.0x |
| ~10 o'clock | Dotted eighth | 0.75x |
| ~12 o'clock | Eighth note | 0.5x |
| ~2 o'clock | Eighth triplet | 0.333x |
| ~4 o'clock | Sixteenth note | 0.25x |
| Full CW | Dotted sixteenth | 0.1875x |

**Key Sonic Characteristics**:
- Rhythmically precise delays locked to tap tempo
- Modulation (via Tweak/Tweez) adds chorus character to the rhythmic echoes
- Ideal for The Edge-style dotted eighth note playing
- Expression pedal can morph between subdivisions in real time

---

### Model 11: STEREO DELAYS
**Based on**: Line 6 Original

**History**: Independent left and right channel delays with separate time and feedback controls. This is the secret behind many U2 guitar sounds (specifically cited in the manual: "How did The Edge get that groovy sound on Where the Streets Have No Name?"). Also the "Big L.A. Solo" sound of the late 1980s.

| Parameter | Function |
|-----------|----------|
| **DELAY TIME** | **Left channel delay time** |
| **REPEATS** | **Left channel repeats (feedback)** |
| **TWEAK** | **Right channel delay time** |
| **TWEEZ** | **Right channel repeats (feedback)** |
| MIX | Wet/dry blend (both channels) |

**Key Sonic Characteristics**:
- True stereo operation: L/R inputs processed independently through separate delay lines
- In mono hookup: both L and R delays are summed to mono output
- Different L/R delay times create wide spatial effects
- Different L/R repeat counts create asymmetric echo patterns
- Classic setting: left = dotted eighth, right = quarter note (or vice versa) for polyrhythmic shimmer
- No tone coloration -- clean digital delay character
- The most "U2" of all DL4 models

---

### Model 12: PING PONG
**Based on**: Line 6 Original

**History**: A stereo delay where the output of each channel feeds into the opposite channel, creating echoes that bounce back and forth between left and right speakers.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | **Left delay time** (the longer delay) |
| REPEATS | Feedback amount (both channels) |
| **TWEAK** | **Right delay time as percentage of left** -- at 12 o'clock, L and R are evenly spaced |
| **TWEEZ** | **Stereo Spread** -- from mono (minimum) to full stereo (maximum) |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Echoes alternate between left and right channels
- Tweak at 12 o'clock = equal L/R spacing (classic ping-pong)
- Tweak below 12 o'clock = right delay is shorter proportion of left
- Tweak above 12 o'clock = right delay is longer proportion of left
- At minimum Tweez: mono output (no stereo spread)
- At maximum Tweez: hard-panned L/R bouncing
- Cross-feedback creates increasingly complex stereo patterns with high Repeats
- Clean digital character

---

### Model 13: REVERSE
**Based on**: Line 6 Original (emulates Hendrix/Beatles backward tape effects)

**History**: Reverse delay captures a chunk of audio equal to the delay time, then plays it back reversed. This was originally achieved by flipping the tape reel on a multitrack recorder (Hendrix's "Are You Experienced," Beatles' "Rain"). The DL4 does it in real-time.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Reverse chunk length (up to 1.25 seconds max -- shorter than other models) |
| REPEATS | Feedback amount (reversed audio fed back into the reverse buffer) |
| **TWEAK** | **Modulation Speed** -- LFO rate applied to the reversed echoes |
| **TWEEZ** | **Modulation Depth** -- pitch modulation amount on reversed echoes |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Audio is captured in real-time and played back reversed with the set delay time
- At 100% wet (Mix full CW): pure reversed sound -- "instant backwards guitar solo"
- Reverse playback creates inherent swelling/crescendo effect (attack becomes decay)
- Legato playing translates best into usable reversed phrases
- Feedback at high settings creates cascading layers of reversed audio
- Modulation adds pitch wobble to the reversed echoes
- Maximum delay time is 1.25 seconds (shorter than other models -- requires buffer for capture + playback)

---

### Model 14: DYNAMIC DELAY
**Based on**: TC Electronic 2290 Dynamic Digital Delay

**History**: The TC 2290 (1985) was a studio-grade digital delay that introduced "dynamic delay" -- an envelope follower that ducks the echo volume while the player is active, then lets the echoes rise when the player stops or plays quietly. This prevents echoes from cluttering busy passages while maintaining ambient sustain during pauses. A studio essential.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time |
| REPEATS | Feedback amount |
| **TWEAK** | **Threshold** -- the level breakpoint where ducking engages/disengages. Below threshold, echoes are at full volume. Above threshold, echoes are ducked |
| **TWEEZ** | **Duck Amount** -- how much the echoes are reduced when ducking is active. Higher = more ducking |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Echoes are volume-ducked while the player is active (above threshold)
- When the player stops or plays below threshold, echoes fade in at full level
- Creates a very "professional" delay sound -- echoes fill the gaps without cluttering the performance
- The envelope follower has a smooth attack/release to avoid abrupt volume jumps
- At minimum Tweez: minimal ducking (echoes always audible)
- At maximum Tweez: deep ducking (echoes nearly silent during playing, full volume during pauses)
- Clean digital delay character in the delay path itself
- This is the model for "invisible" delay that adds depth without being heard as distinct echoes

---

### Model 15: AUTO-VOLUME ECHO
**Based on**: Line 6 Original

**History**: Combines two effects: (1) an auto-volume swell (fade-in envelope on each note, like turning the guitar volume knob up from zero after picking), and (2) a tape-style echo with wow/flutter modulation. The swell removes the pick attack, creating a bowed or pad-like quality.

| Parameter | Function |
|-----------|----------|
| DELAY TIME | Echo time |
| REPEATS | Feedback amount |
| **TWEAK** | **Modulation Depth** -- wow & flutter amount on the echo |
| **TWEEZ** | **Ramp Time** -- attack time for the auto-volume swell (how long the fade-in takes) |
| MIX | Wet/dry blend |

**Key Sonic Characteristics**:
- Each note/echo has its attack removed, creating a violin-like swell
- Short ramp time (Tweez low): quick swell, subtle effect
- Long ramp time (Tweez high): slow, dramatic fade-in on each note
- The echo portion has tape-style modulation (wow & flutter via Tweak)
- The auto-volume swell is applied BEFORE the delay, so both the dry signal and echoes fade in
- Creates ethereal, pad-like guitar textures
- At 100% wet with long ramp time and high repeats: ambient infinite pad generator
- Combined with reverb: instant ambient/shoegaze texture

---

### Model 16: LOOP SAMPLER
**Based on**: Line 6 Original (built-in looper)

**History**: A 14-second loop recorder integrated into the DL4. This was one of the first affordable loop pedals and became enormously popular with live performers. The loop sampler includes overdub capability, half-speed playback, and reverse -- plus a unique pre-loop echo unit.

| Parameter | Function |
|-----------|----------|
| **DELAY TIME** | Pre-loop echo time (echo applied BEFORE recording to the loop) |
| **REPEATS** | Pre-loop echo feedback |
| **TWEAK** | Pre-loop echo modulation depth |
| **TWEEZ** | **Pre-loop echo volume** (at minimum = echo off, only dry signal recorded) |
| **MIX** | **Loop playback volume** |

**Loop Sampler Specifications**:
- **Maximum loop length**: 14 seconds (at full speed)
- **Half-speed loop length**: 28 seconds (record at half speed, playback at half speed)
- **Overdub**: Each overdub pass slightly attenuates previous layers (natural fade-out)
- **Half speed**: Single tap of TAP TEMPO switch -- halves playback speed (drops pitch one octave)
- **Reverse**: Double-tap TAP TEMPO switch -- reverses playback direction
- **Both**: Half speed + reverse can be combined
- **Pre-loop echo**: Unique feature -- you can add echo to your signal as you record into the loop
- **True bypass disabled**: True bypass is automatically disabled in Loop Sampler mode

**Footswitch Functions in Loop Sampler Mode**:
| Switch | Function |
|--------|----------|
| A (Record/Overdub) | Start recording; press again to end recording and begin looping + overdub |
| B (Play/Stop) | Start/stop loop playback |
| C (Play Once) | One-shot playback (plays loop once and stops) |
| TAP TEMPO | Single tap = half speed; double tap = reverse |

---

## 4. DSP Implementation Details

### Common Architecture (All Delay Models)

All 15 delay models (excluding Loop Sampler) share this basic architecture:

```
Input --> [Pre-Processing] --> Delay Line Write --> Delay Line Read --> [Post-Processing] --> Wet Signal
                                                         |                                        |
                                                         +--- [Feedback Processing] <--- Feedback Tap

Wet Signal * Mix + Dry Signal * (1 - Mix) --> Output
```

The differences between models are in:
1. **Pre-processing** (before writing to delay line)
2. **Post-processing** (after reading from delay line)
3. **Feedback path processing** (what happens to the signal before it's fed back)
4. **Modulation** (LFO-driven pitch/time variation)
5. **Delay line read method** (fixed point, interpolated, reversed, multi-tap)

### Per-Model DSP Breakdown

#### Model 1: TUBE ECHO
```
Algorithm:
  Input --> Soft Saturation (tube model) --> Write to delay line
  Read delay line (with interpolated modulated read pointer for wow/flutter)
  Feedback path: LP filter (~3-5 kHz) + soft saturation (tape + tube)

  LFO for wow/flutter: composite of multiple sinusoids (0.3-3 Hz range)
    - Primary wow: ~1 Hz sinusoid
    - Secondary flutter: ~8-12 Hz sinusoid (lower amplitude)
    - Random component: lowpass-filtered noise

  Tube saturation model: asymmetric soft clipping
    - 2nd harmonic emphasis (tube characteristic)
    - Input drive controlled by TWEEZ

  Tape saturation: tanh() waveshaper with frequency-dependent pre-emphasis
    - High frequencies saturate first (tape head magnetization curve)

CPU estimate: ~45 FLOPS/sample (saturation + modulated read + LP filter)
Memory: delay_line[78125] for 2.5s at 31.25 kHz = ~312 KB (stereo)
```

#### Model 2: TAPE ECHO
```
Algorithm:
  Input --> Write to delay line
  Read delay line (with subtle wow/flutter modulation)
  Feedback path: shelving EQ (bass + treble controls) + mild saturation

  Tape transport simulation: similar to Tube Echo but less saturation
  Bass/Treble: 2-band shelving EQ in feedback path
    - Bass shelf: ~200 Hz, controlled by TWEAK
    - Treble shelf: ~3 kHz, controlled by TWEEZ

  Lighter saturation than Tube Echo (transistor, not tube)
  JFET preamp character: subtle 2nd/3rd harmonic addition

CPU estimate: ~30 FLOPS/sample (EQ + mild saturation + modulated read)
```

#### Model 3: MULTI-HEAD
```
Algorithm:
  Input --> Write to single delay line
  4 read taps at fixed ratios: 1x, 2x, 3x, 4x base delay time
  TWEAK controls: heads 1,2 on/off (crossfade)
  TWEEZ controls: heads 3,4 on/off (crossfade)

  Each head output:
    - LP filter in feedback path (~5 kHz, first-order)
    - Subtle tape saturation (tanh)

  Head mixing: sum of active heads --> feedback path

  Head enable logic (per pair):
    knob < 0.25: both heads off
    0.25 < knob < 0.50: first head on, second off
    0.50 < knob < 0.75: both heads on
    knob > 0.75: first head off, second on
    (with crossfade zones between states)

CPU estimate: ~50 FLOPS/sample (4 interpolated reads + mixing + LP + saturation)
Memory: Same delay line, 4 read pointers
```

#### Model 4: SWEEP ECHO
```
Algorithm:
  Input --> Tube Echo-style saturation --> Write to delay line
  Read delay line
  Feedback path: State-variable bandpass filter (LFO-modulated center frequency)

  Sweep filter:
    - Type: resonant bandpass (SVF or biquad)
    - Center frequency range: ~200 Hz to ~4 kHz
    - Q: moderate (~2-4)
    - LFO: triangle or sine wave
    - LFO rate: TWEAK (0.05 Hz to 5 Hz range)
    - LFO depth: TWEEZ (0% to 100% of frequency range)

  Each repeat passes through the sweeping filter, creating progressive tonal morphing

CPU estimate: ~40 FLOPS/sample (SVF + saturation + LFO)
```

#### Model 5: ANALOG ECHO
```
Algorithm:
  Input --> Write to delay line (with BBD bandwidth limiting)
  Read delay line
  Feedback path: LP filter + shelving EQ + mild saturation

  BBD emulation:
    - Bandwidth limiting: LP filter at ~3-5 kHz (shorter delay = wider BW, longer delay = narrower BW)
    - Clock noise: subtle high-frequency artifacts (filtered but perceptible)
    - Charge transfer loss: mild signal degradation per clock stage
    - Each repeat accumulates more LP filtering (progressive darkening)

  Shelving EQ (feedback path):
    - Bass: ~150 Hz shelf, controlled by TWEAK
    - Treble: ~3 kHz shelf, controlled by TWEEZ

  BBD saturation: soft, symmetric clipping (gentler than tape)

CPU estimate: ~35 FLOPS/sample (LP + EQ + saturation)
```

#### Model 6: ANALOG ECHO w/ MOD
```
Algorithm:
  Same as Analog Echo, PLUS:
  Chorus modulation applied to delay line read (modulated read pointer offset)

  Chorus LFO:
    - Waveform: triangle or sine
    - Rate: TWEAK (0.1 Hz to 8 Hz)
    - Depth: TWEEZ (0 to ~3 ms pitch deviation, corresponding to ~15 cents at 1 kHz)

  Modulation is applied per-repeat, so each echo accumulates more chorus
  First echo: slight pitch variation
  Fourth echo: significant pitch smearing

  BBD bandwidth limiting same as Analog Echo

CPU estimate: ~40 FLOPS/sample (BBD sim + chorus + LP + saturation)
```

#### Model 7: LO RES DELAY
```
Algorithm:
  Input --> Bit crusher (in feedback path or on wet signal)

  Bit reduction:
    - TWEEZ controls bit depth: 6-bit to 24-bit (continuous interpolation)
    - Implementation: quantize to N-bit range, then scale back to full range
    - At 6 bits: 64 amplitude levels, ~36 dB dynamic range
    - At 12 bits: 4096 levels, ~72 dB dynamic range
    - At 24 bits: 16M levels, ~144 dB dynamic range (transparent)

  Optional sample rate reduction (implied by "Lo Res"):
    - May also reduce effective sample rate for additional "vintage digital" character

  TWEAK: tone filter (LP, shelving, or parametric for taming harshness)

CPU estimate: ~20 FLOPS/sample (bit crush + tone filter) -- cheapest model
```

#### Model 8: DIGITAL DELAY
```
Algorithm:
  Input --> Write to delay line (no coloration)
  Read delay line (linear interpolation for non-integer sample delays)
  Feedback path: 2-band shelving EQ

  Shelving EQ:
    - Bass: ~200 Hz shelf, TWEAK
    - Treble: ~4 kHz shelf, TWEEZ

  No saturation, no modulation -- pure delay + tone shaping
  Linear interpolation for smooth delay time changes (no zipper noise)

CPU estimate: ~15 FLOPS/sample (interpolated read + EQ) -- second cheapest
```

#### Model 9: DIGITAL DELAY w/ MOD
```
Algorithm:
  Same as Digital Delay but with chorus modulation on delay read

  Chorus LFO:
    - Rate: TWEAK (0.1 Hz to 8 Hz)
    - Depth: TWEEZ (0 to ~3 ms)
    - Applied to wet signal only

  No saturation, no bandwidth limiting -- clean + modulated

CPU estimate: ~25 FLOPS/sample (interpolated read + modulation + EQ)
```

#### Model 10: RHYTHMIC DELAY
```
Algorithm:
  Same as Digital Delay w/ Mod

  DELAY TIME knob behavior changed:
    - Quantized to 6 rhythmic subdivisions of the tapped tempo
    - Internal tempo register set by TAP TEMPO switch
    - Subdivision applied as multiplier to quarter-note period

  Subdivision mapping:
    Quarter note:      delay_ms = tap_period_ms * 1.0
    Dotted eighth:     delay_ms = tap_period_ms * 0.75
    Eighth note:       delay_ms = tap_period_ms * 0.5
    Eighth triplet:    delay_ms = tap_period_ms * 0.333
    Sixteenth note:    delay_ms = tap_period_ms * 0.25
    Dotted sixteenth:  delay_ms = tap_period_ms * 0.1875

CPU estimate: ~25 FLOPS/sample (same as Digital w/ Mod)
```

#### Model 11: STEREO DELAYS
```
Algorithm:
  Two independent delay lines (left and right)

  Left channel:
    - Delay time: DELAY TIME knob
    - Feedback: REPEATS knob

  Right channel:
    - Delay time: TWEAK knob
    - Feedback: TWEEZ knob

  Each channel: independent delay line + linear interpolation
  No cross-feed between channels
  Clean digital character (no saturation, no modulation)

CPU estimate: ~20 FLOPS/sample (2x interpolated reads, no processing)
Memory: 2x delay line buffers
```

#### Model 12: PING PONG
```
Algorithm:
  Two delay lines with cross-feedback:

  Left_in = Input + Right_delay_out * feedback
  Right_in = Left_delay_out * feedback

  Left delay time: DELAY TIME
  Right delay time: DELAY TIME * TWEAK (ratio 0.0 to 2.0, centered at 1.0)

  Stereo spread: TWEEZ controls panning width
    - At 0%: both channels summed to center (mono)
    - At 100%: full L/R panning

  Clean digital, no saturation

CPU estimate: ~25 FLOPS/sample (2x interpolated reads + cross-feedback + panning)
```

#### Model 13: REVERSE
```
Algorithm:
  Capture buffer: stores incoming audio in a circular buffer
  Playback: reads the buffer backwards at the same sample rate

  Buffer length = DELAY TIME (max 1.25 seconds = ~39,000 samples at 31.25 kHz)

  Implementation:
    - Write pointer advances forward normally
    - Read pointer starts at write_position and moves backward
    - When read pointer reaches the start of the captured chunk, it resets
    - Crossfade at boundaries to avoid clicks (~5-10 ms)

  Feedback: reversed output is fed back into the capture buffer
  Modulation (TWEAK = speed, TWEEZ = depth): LFO on playback rate

  The shorter max delay (1.25s vs 2.5s) is because the algorithm needs
  double-buffering: one buffer capturing while the other plays back reversed

CPU estimate: ~25 FLOPS/sample (buffer management + crossfade + modulation)
Memory: 2x capture buffers for seamless switching
```

#### Model 14: DYNAMIC DELAY
```
Algorithm:
  Clean digital delay PLUS envelope-controlled VCA on wet signal

  Envelope follower on input signal:
    - RMS or peak detection with ~10-50 ms attack, ~100-500 ms release
    - Threshold: TWEAK (0 dBFS range mapped to useful guitar levels)

  VCA behavior:
    - When input > threshold: wet signal attenuated by TWEEZ amount (duck)
    - When input < threshold: wet signal at full level (echoes audible)
    - Smooth transition (avoid abrupt volume jumps)

  Duck amount: TWEEZ (0 dB to -30 dB or more attenuation)

CPU estimate: ~25 FLOPS/sample (envelope follower + VCA + delay)
```

#### Model 15: AUTO-VOLUME ECHO
```
Algorithm:
  Envelope generator (attack ramp) on input signal
  PLUS tape-style echo with wow/flutter

  Auto-volume swell:
    - Envelope detector on input: detects note onset (pick attack)
    - On each new note: volume ramps from 0 to 1.0 over TWEEZ-controlled time
    - Ramp time: TWEEZ (5 ms to ~2 seconds)
    - Creates bowing/fade-in effect on every note

  Echo section:
    - Tape-style delay with wow/flutter modulation
    - Modulation depth: TWEAK
    - LP filter in feedback path (tape character)

  The swell is applied BEFORE the delay, so both dry and wet signals swell

CPU estimate: ~35 FLOPS/sample (envelope detection + ramp + tape delay + modulation)
```

#### Model 16: LOOP SAMPLER
```
Algorithm:
  Phase 1 -- Pre-loop echo:
    Simple digital delay with modulation
    - Delay time: DELAY TIME knob
    - Feedback: REPEATS knob
    - Modulation: TWEAK
    - Echo volume: TWEEZ (0 = off, acts as echo on/off + level)

  Phase 2 -- Loop engine:
    - Circular buffer: 437,500 samples at 31.25 kHz = 14 seconds
    - Record: write input (post-echo) to buffer
    - Overdub: sum new input with existing buffer content (with slight attenuation of existing)
    - Play: read buffer at write speed
    - Half speed: read at half rate (with linear interpolation)
    - Reverse: read pointer moves backward
    - Playback volume: MIX knob

  Overdub attenuation: each pass multiplies existing content by ~0.95 (gradual fade)
  Half speed: effectively doubles loop length to 28 seconds

CPU estimate: ~15 FLOPS/sample (echo) + ~5 FLOPS/sample (loop read/write)
Memory: 437,500 samples * 3 bytes (24-bit) = ~1.3 MB for mono loop
```

---

## 5. Key Sonic Characteristics

### The DL4 "Sound"
The DL4 has a characteristic sonic signature that distinguishes it from both the original hardware it models AND from other digital delay units:

1. **The 31.25 kHz sample rate** imparts a subtle "darkness" to all models. The Nyquist frequency at 15.6 kHz means there's inherent treble roll-off compared to 44.1 kHz or 48 kHz units. This actually HELPS the tape and analog models sound more authentic.

2. **24-bit quantization with 56-bit accumulator math** means the DSP calculations themselves are very clean -- no 16-bit quantization artifacts in the processing. The "character" comes from the intentional modeling, not from DSP limitations.

3. **Progressive treble loss in feedback path**: Every model except the clean Digital Delay has some form of LP filtering in the feedback loop. This means each repeat is darker than the last -- exactly as happens with real tape and analog delays.

4. **Soft saturation in tape/analog models**: The saturation in the feedback path means repeats gradually get more compressed and harmonically enriched. This makes self-oscillation sound musical rather than harsh.

5. **Self-oscillation behavior**: At max Repeats, feedback >= 1.0 and the delay self-oscillates. The character depends on the model:
   - **Tape/Tube models**: warm, compressed, organ-like -- musically useful
   - **Analog models**: warm, dark, swirling (with modulation) -- very controllable
   - **Digital models**: harsh, bright, runaway -- harder to control musically
   - **Lo Res model**: chaotic, distorted, destructive -- intentionally experimental

6. **Tap Tempo interaction**: When tap tempo changes the delay time, the delay line read pointer must jump to a new position. The DL4 handles this by:
   - Immediate jump with brief crossfade (for most models)
   - This creates a momentary pitch artifact during the transition
   - Spinning the Delay Time knob at high feedback creates the famous "spaceship" effect (continuous pitch sweep as the read pointer speed changes)

### Why the Tube Echo is the Most Coveted Model

The Tube Echo (EP-1 emulation) is special because it combines THREE sources of harmonic enrichment:
1. **Tube preamp saturation**: adds musical 2nd and 3rd harmonics
2. **Tape saturation**: adds further harmonic complexity with frequency-dependent compression
3. **Wow and flutter**: adds pitch modulation that creates natural chorus-like thickening

No other DL4 model has all three. The Tape Echo has tape character but lacks tube drive. The Analog models have BBD warmth but lack tape transport artifacts. The Tube Echo is the only model where the echoes sound like a complete musical instrument, not just a delayed copy of the input.

---

## 6. The Edge (U2) Settings & Famous Users

### The Edge's DL4 Usage

The Edge (David Howell Evans) is the single most famous DL4 user. His signature sound relies on precise rhythmic delays -- particularly the **dotted eighth note delay** -- to create the shimmering, cascading guitar textures that define U2's sound.

#### The Edge's Primary DL4 Settings

**Model**: Digital Delay (Model 8) or Stereo Delays (Model 11)

**Why Digital Delay?** The Edge needs PRECISION. His delay technique relies on the echoes being rhythmically exact -- any wow/flutter or pitch modulation would destroy the rhythmic illusion. The clean Digital Delay gives him crystal-clear repeats that blend with his picking to create the impression of a more complex part than he's actually playing.

**Dotted Eighth Note Delay Calculation**:
```
Quarter note period = 60,000 / BPM (in milliseconds)
Dotted eighth = Quarter * 0.75

Example for "Where the Streets Have No Name" (BPM ~126):
  Quarter = 60,000 / 126 = 476 ms
  Dotted eighth = 476 * 0.75 = 357 ms

Example for "Pride (In the Name of Love)" (BPM ~106):
  Quarter = 60,000 / 106 = 566 ms
  Dotted eighth = 566 * 0.75 = 424 ms
```

**Typical Edge Settings**:
| Parameter | Setting | Notes |
|-----------|---------|-------|
| Model | Digital Delay or Stereo Delays | Clean, rhythmic |
| Delay Time | Dotted eighth note (set via tap tempo) | ~300-500 ms depending on song tempo |
| Repeats | 3-5 repeats (moderate) | Enough to create cascading texture, not so much as to become muddy |
| Tweak (Bass) | ~12 o'clock (neutral) | Keeps echoes clean |
| Tweez (Treble) | Slightly below 12 o'clock | Subtle darkening of repeats |
| Mix | ~40-50% | Echoes are prominent but not overwhelming |

#### The Edge's Approach
The Edge's technique is to play simple patterns (often single notes or simple arpeggios) and use the dotted eighth delay to fill in the rhythmic gaps, creating the illusion of a much more complex part. The listener hears a continuous stream of notes, but the guitarist is only playing half of them -- the delay provides the rest.

Key songs and their likely DL4 settings:

| Song | Model | Delay Time | Notes |
|------|-------|-----------|-------|
| Where the Streets Have No Name | Stereo Delays | ~357 ms (dotted 8th at 126 BPM) | L/R different times for width |
| Pride (In the Name of Love) | Digital Delay | ~424 ms (dotted 8th at 106 BPM) | Clean, precise |
| I Will Follow | Digital Delay | ~375 ms (dotted 8th at 120 BPM) | The original "Edge sound" |
| Sunday Bloody Sunday | Digital Delay | Eighth note at ~105 BPM (~286 ms) | Simpler delay pattern |
| Bad | Stereo Delays | Dotted 8th + quarter note | Complex stereo spread |

### Other Famous DL4 Users

| Artist | Primary Model(s) | Usage |
|--------|-----------------|-------|
| **Radiohead** (Jonny Greenwood, Ed O'Brien) | Tape Echo, Reverse, Lo Res | Experimental textures, ambient soundscapes |
| **John Mayer** | Tube Echo, Analog w/ Mod | Blues-inflected delay, warm echoes |
| **Nels Cline** (Wilco) | Reverse, Sweep Echo, Loop Sampler | Experimental, noise, looping |
| **Kevin Shields** (My Bloody Valentine) | Analog w/ Mod, Reverse | Dense layered textures |
| **Dave Grohl / Chris Shiflett** (Foo Fighters) | Tape Echo, Digital Delay | Standard rock delay |
| **St. Vincent** (Annie Clark) | Various | Creative delay textures |
| **Trey Anastasio** (Phish) | Tube Echo, Digital Delay | Improvisation, looping |
| **Sonic Youth** | Lo Res, Reverse, Loop Sampler | Noise, experimentation |

---

## 7. Loop Sampler Specifications

### Technical Details
| Spec | Value |
|------|-------|
| Maximum loop length | 14 seconds (full speed) |
| Half-speed loop length | 28 seconds |
| Sample rate | 31.25 kHz (same as delay models) |
| Bit depth | 24-bit internal processing |
| Audio quality | Mono (stereo inputs summed) |
| Overdub behavior | Previous layers attenuated ~5% per pass |
| Pre-loop echo | Adjustable delay time, feedback, modulation, and volume |
| Storage | Volatile (loop lost on power-off or model change) |

### Loop Sampler State Machine
```
IDLE --> [Record/Overdub pressed] --> RECORDING
RECORDING --> [Play/Stop pressed] --> PLAYING
RECORDING --> [Record/Overdub pressed] --> PLAYING + OVERDUBBING
RECORDING --> [Play Once pressed] --> PLAY_ONCE
PLAYING --> [Play/Stop pressed] --> STOPPED
PLAYING --> [Record/Overdub pressed] --> OVERDUBBING
PLAYING --> [Play Once pressed] --> PLAY_ONCE (plays to end, then stops)
OVERDUBBING --> [Record/Overdub pressed] --> PLAYING
OVERDUBBING --> [Play/Stop pressed] --> STOPPED
STOPPED --> [Play/Stop pressed] --> PLAYING (from loop start)
STOPPED --> [Play Once pressed] --> PLAY_ONCE

Any state --> [TAP TEMPO single tap] --> Toggle half-speed
Any state --> [TAP TEMPO double tap] --> Toggle reverse
```

### Overdub Decay
The DL4's overdub decay means that older layers gradually fade with each overdub pass. This is modeled as a multiplication factor applied to the existing loop content each time overdub recording cycles:
```
existing_sample *= decay_factor  (approximately 0.95)
new_sample = existing_sample + input_sample
```
This means after ~14 overdub passes, original content is reduced to ~50% (-6 dB). After ~46 passes, original is at ~10% (-20 dB).

---

## 8. Factory Presets

The DL4 ships with 3 factory presets (one per footswitch memory). Based on the factory presets document:

| Preset | Switch | Model | Description |
|--------|--------|-------|-------------|
| A | A (leftmost) | Tape Echo | Classic tape delay sound |
| B | B (middle) | Analog Echo w/ Mod | Warm modulated delay |
| C | C (rightmost) | Digital Delay | Clean rhythmic delay |

(Exact parameter values from the factory presets PDF were not extractable due to PDF encoding, but these are the typical factory defaults.)

---

## 9. Implementation Plan for Our App

### Mapping DL4 Architecture to Our Signal Chain

We already have a Delay effect (index 12) in our signal chain. To implement a full DL4 emulation, we have several options:

#### Option A: Single "DL4 Delay" Effect with Model Selector
Add a new effect slot (e.g., index 27) that implements all 16 DL4 models internally, with a `model` parameter (0-15) that selects the active algorithm. This matches the physical DL4's architecture.

**Parameters** (mapped to our EffectRegistry):
| Param Index | Name | Range | Default |
|-------------|------|-------|---------|
| 0 | model | 0-15 (int selector) | 0 (Digital Delay) |
| 1 | delayTime | 0.0-1.0 (maps to ms range per model) | 0.5 |
| 2 | repeats | 0.0-1.0 | 0.3 |
| 3 | tweak | 0.0-1.0 | 0.5 |
| 4 | tweez | 0.0-1.0 | 0.5 |
| 5 | mix | 0.0-1.0 | 0.5 |
| 6 | tapTempo | 0.0 (special -- set via tap gesture in UI) | 0.0 |

#### Option B: Replace Existing Delay with DL4 Engine
Replace our current generic delay (index 12) with the DL4 engine, making it the primary delay effect.

#### Recommended: Option A
Keep the existing Delay for backward compatibility; add DL4 as a new effect. This also lets users stack a DL4 delay with the existing simpler delay if desired.

### CPU Budget Estimates (at 48 kHz)
| Model | FLOPS/sample | Overhead vs. current Delay |
|-------|-------------|---------------------------|
| Digital Delay | ~15 | Comparable |
| Analog Echo | ~35 | ~2.3x |
| Tube Echo | ~45 | ~3x |
| Multi-Head | ~50 | ~3.3x |
| Loop Sampler | ~20 + loop buffer | ~1.3x + memory |

All models are well within our budget. The most expensive (Multi-Head at ~50 FLOPS/sample) is still far cheaper than our WDF pedals (~200-400 FLOPS/sample with oversampling).

### Memory Requirements
- **Delay line**: 2.5 seconds * 48,000 Hz * 4 bytes = 480 KB (mono) or 960 KB (stereo)
- **Loop sampler**: 14 seconds * 48,000 Hz * 4 bytes = 2.69 MB (mono)
- **Total worst case**: ~3.7 MB (well within mobile budget)

### Key Implementation Challenges
1. **Model-specific Tweak/Tweez behavior**: The UI must show different parameter labels based on the selected model
2. **Multi-Head head selection**: Tweak/Tweez act as quasi-switches, not smooth continuous parameters
3. **Stereo Delays**: All 4 main knobs are repurposed (L time, L repeats, R time, R repeats)
4. **Reverse delay**: Requires double-buffered capture/playback with crossfading
5. **Rhythmic Delay**: Needs tap tempo integration and quantized subdivision selection
6. **Loop Sampler**: Already have our Looper engine -- could integrate or offer as separate mode
7. **Self-oscillation**: Must handle gracefully -- soft limiter prevents runaway, but the musical character of self-oscillation differs per model
8. **Tap Tempo**: Already have tap tempo in our UI framework -- route to DL4 delay time

### UI Design for DL4 Mode
The DL4 has a distinctive layout that could be replicated as a special "DL4 pedal view":
- Green chassis background
- 5 knobs in a row (matching the physical layout)
- Model selector as a 16-position rotary
- 4 footswitch indicators across the bottom
- Expression pedal assignments
- Dynamic parameter labels that change based on model selection

---

## Appendix A: Complete Tweak/Tweez Quick Reference Table

| # | Model | TWEAK | TWEEZ |
|---|-------|-------|-------|
| 1 | Tube Echo | Wow & Flutter | Drive (tube/tape saturation) |
| 2 | Tape Echo | Bass (tone EQ) | Treble (tone EQ) |
| 3 | Multi-Head | Heads 1 & 2 on/off | Heads 3 & 4 on/off |
| 4 | Sweep Echo | Sweep Speed (LFO rate) | Sweep Depth (LFO amount) |
| 5 | Analog Echo | Bass (tone EQ) | Treble (tone EQ) |
| 6 | Analog Echo w/ Mod | Modulation Speed | Modulation Depth |
| 7 | Lo Res Delay | Tone | Resolution (6-24 bits) |
| 8 | Digital Delay | Bass (tone EQ) | Treble (tone EQ) |
| 9 | Digital Delay w/ Mod | Modulation Speed | Modulation Depth |
| 10 | Rhythmic Delay | Modulation Speed | Modulation Depth |
| 11 | Stereo Delays | Right delay time | Right repeats |
| 12 | Ping Pong | Right time (% of left) | Stereo Spread |
| 13 | Reverse | Modulation Speed | Modulation Depth |
| 14 | Dynamic Delay | Threshold (duck level) | Duck Amount (attenuation) |
| 15 | Auto-Volume Echo | Modulation Depth (wow/flutter) | Ramp Time (swell attack) |
| 16 | Loop Sampler | Pre-echo Modulation | Pre-echo Volume |

## Appendix B: Model Selector Position to Model Name Mapping

The physical DL4 has the model selector printed on the faceplate. Reading clockwise from the top:

```
Position  1 (12 o'clock):  DIGITAL DELAY
Position  2 (~1 o'clock):  DIGITAL W/ MOD
Position  3 (~2 o'clock):  RHYTHMIC DELAY
Position  4 (~3 o'clock):  STEREO DELAYS
Position  5 (~4 o'clock):  LO RES DELAY
Position  6 (~5 o'clock):  ANALOG W/ MOD
Position  7 (~6 o'clock):  ANALOG ECHO
Position  8 (~7 o'clock):  SWEEP ECHO
Position  9 (~8 o'clock):  PING PONG
Position 10 (~9 o'clock):  REVERSE
Position 11 (~10 o'clock): DYNAMIC DELAY
Position 12 (~11 o'clock): AUTO-VOLUME ECHO
Position 13 (~11:30):      TUBE ECHO
Position 14 (~12:30):      TAPE ECHO (but on left side of dial)
Position 15:               MULTI-HEAD
Position 16:               LOOP SAMPLER
```

Note: The exact positions on the dial group related models together:
- Digital models (1-4) are clustered at the top
- Analog/tape models (5-8) are on the right
- Stereo/spatial models (9-10) are at the bottom
- Special models (11-15) are on the left
- Loop Sampler (16) is at the bottom-left

## Appendix C: Comparison with Our Existing Delay Implementation

Our current Delay effect (index 12) is a basic digital delay with:
- Delay time (up to 2 seconds)
- Feedback
- Mix
- LP filter in feedback path
- Optional modulation

This roughly corresponds to the DL4's "Digital Delay w/ Mod" (Model 9), but without the model-switching architecture. Implementing the full DL4 engine as a separate effect would give us 15 additional delay types plus the loop sampler integration.

### What We Already Have That Overlaps
- **RE-201 Space Echo analysis**: Our docs/re-201-space-echo-circuit-analysis.md covers the tape transport, head delays, wow/flutter, and saturation in exhaustive detail. This directly informs Models 1-3 (Tube Echo, Tape Echo, Multi-Head).
- **Looper engine**: Our existing LooperEngine already implements record/overdub/play/stop with crossfading. The DL4 Loop Sampler would just need the pre-loop echo and half-speed/reverse additions.
- **BBD analysis**: Our Chorus CE-1 mode documents BBD behavior that applies to Models 5-6 (Analog Echo, Analog w/ Mod).

### What's New
- **Tube saturation in delay feedback path** (Model 1)
- **Bit-crushing in delay path** (Model 7)
- **Rhythmic subdivision quantization** (Model 10)
- **Stereo independent delay** (Model 11)
- **Cross-feedback ping-pong** (Model 12)
- **Reverse delay buffer management** (Model 13)
- **Envelope-ducked delay** (Model 14)
- **Auto-volume swell** (Model 15)

## Appendix D: DSP56364 Technical Notes

The Motorola (later Freescale, now NXP) DSP56364 is a member of the DSP563xx family:
- **Architecture**: 24-bit fixed-point DSP with 56-bit accumulator
- **Clock**: Up to 150 MHz
- **Multiply-accumulate**: Single-cycle 24x24-bit MAC producing 56-bit result
- **On-chip RAM**: 6K x 24-bit program RAM, 6K x 24-bit X data RAM, 6K x 24-bit Y data RAM
- **External memory interface**: For DRAM (delay line storage)
- **DMA**: 6 channels for efficient memory transfer
- **Audio interfaces**: ESSI (Enhanced Synchronous Serial Interface) for codec connection

The 31.25 kHz sample rate at this clock speed gives approximately 4,800 instruction cycles per sample -- more than enough for any individual delay model. The constraint is the external DRAM bandwidth for long delay lines.

### Fixed-Point Considerations for Our Implementation
Since the original runs at 24-bit fixed-point, our 32-bit float implementation will have MORE dynamic range and precision. However, to capture the "character" of the DL4, we should consider:
- The 31.25 kHz sample rate imparts a specific treble roll-off -- we could optionally downsample or apply a LP at ~14 kHz to match
- The 24-bit fixed point has ~144 dB dynamic range -- our 32-bit float has ~150 dB, so no loss
- Fixed-point saturation behavior (overflow wrapping) differs from float -- not relevant for delay models that use intentional soft clipping

---

*Document generated 2026-03-23. Sources: Line 6 Stompbox Pilot's Guide (Rev D, 2000), Line 6 DL4 Quick Start Pilot's Guide (Rev B), US Patent 5,789,689 (Line 6 amp modeling architecture), US Patent 6,504,935 (Line 6 distortion synthesis), Helix 3.80 model list from line6.com, Boss DM-2W specifications, effectsdatabase.com.*
