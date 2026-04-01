# Design Spec: Tube Screamer (TS-808/TS9), BD-2 Upgrade, MXR 10-Band EQ

**Date**: 2026-03-30
**Status**: Approved
**Scope**: 2 workstreams — new Tube Screamer effect (#31), BD-2 accuracy upgrade (#30)
**Note**: Graphic EQ redesign (#7) was completed during research phase (7-band Q=1.5 -> 10-band MXR M108S Q=0.9, +/-12dB). See Section 3 for documentation of what was done.

---

## 1. Tube Screamer (TS-808 / TS9) — New Effect #31

### 1.1 Overview

Circuit-accurate emulation of the Ibanez TS-808 Tube Screamer (1979) with a TS9 mode switch. The only circuit difference between the two is the output stage resistor network (output impedance: 133 ohm TS-808 vs 503 ohm TS9).

### 1.2 Signal Flow

```
Input -> Q1 2SC1815GR emitter follower buffer (510K input impedance)
      -> 1uF coupling cap
      -> JRC4558D non-inverting clipping amplifier
         (feedback: 4.7K + 500K Drive pot + 47nF bass rolloff + 51pF treble rolloff)
         (two 1N914 diodes anti-parallel IN the feedback loop)
      -> 0.22uF coupling cap
      -> Passive tone control (220nF + 1K + 20K Tone pot)
      -> IC1b unity buffer
      -> 1uF coupling cap
      -> Output buffer + Volume pot (100K)
      -> Output network (TS-808: 100R series + 10K shunt / TS9: 470R series + 100K shunt)
      -> Output
```

### 1.3 Implementation: Yeh Decomposition

Based on David Yeh's Stanford CCRMA PhD thesis (2009) on digitizing the Tube Screamer. The implicit diode-in-feedback loop is decomposed into sequential explicit stages:

**Stage A — Input HPF (the mid-hump)**
- The 47nF cap in the feedback path creates a shelving high-pass at 720Hz
- `f_bass = 1 / (2 * pi * 4700 * 47e-9) = 720.8 Hz`
- Below 720Hz: gain drops toward unity (bass stays clean)
- Above 720Hz: full gain applied
- Implementation: 1st-order high-shelf filter at 720Hz

**Stage B — Frequency-dependent gain**
- Gain equation: `G(f) = 1 + Z_feedback(f) / 4700`
- At DC: `G = 1 + 4700/4700 = 2` (6dB, diodes irrelevant)
- At 1kHz+: `G = 1 + (4700 + R_drive) / 4700`
  - Drive min: `G = 1 + (4700 + 0) / 4700 = 2.0` (6dB)
  - Drive max: `G = 1 + (4700 + 500000) / 4700 = 108.4` (40.7dB)
- Treble rolloff from 51pF: `f_treble = 1 / (2 * pi * R_drive * 51e-12)`
  - Drive max: 5.66kHz (audible treble rolloff)
  - Drive min: 61.2kHz (inaudible)

**Stage C — Diode soft clipper (Newton-Raphson)**
- 1N914 silicon diodes: Is = 2.52nA, n = 1.752
- Shockley equation: `I = Is * (exp(V / (n * Vt)) - 1)`, Vt = 25.85mV at 25C
- The feedback topology means clipping is **compressive** (gain smoothly decreases as diodes conduct), NOT hard clipping
- Implicit equation (delay-free feedback loop):
  ```
  f(Vout) = Vin - Vout - R_feedback * 2 * Is * sinh(Vout / (n * Vt)) = 0
  ```
  where R_feedback = 4700 + R_drive (0 to 500K), Is = 2.52nA, n = 1.752, Vt = 25.85mV
- Newton-Raphson iteration (4 steps, warm start from previous sample's Vout):
  ```
  Jacobian: f'(Vout) = -1 - R_feedback * 2 * Is / (n * Vt) * cosh(Vout / (n * Vt))
  Update:   Vout_new = Vout - f(Vout) / f'(Vout)
  ```
- Soft clipping threshold: ~0.55V (onset of conduction), ~0.7V (full limiting)
- 2x oversampling around this nonlinearity

**Stage D — Tone control**
- Variable low-pass: `fc = 1 / (2 * pi * R_tone * 220e-9)`
- Tone = 0 (darkest): R = 1K+220R = 1.22K -> fc = 592 Hz
- Tone = 10 (brightest): R = 20K+1K+220R = 21.22K -> fc = 34 Hz (everything passes)
- Wait — this is a **mixing topology**: filtered signal (through 220nF+Tone pot) blends with unfiltered signal (through 1K resistor). More precisely:
  - `output = (unfiltered * 1K_path + filtered * tone_pot_path) / total_impedance`
  - At tone=0: mostly filtered (dark). At tone=10: mostly unfiltered (bright).
- Implementation: 1-pole LP at variable cutoff, crossfaded with dry signal by tone parameter
  - **Crossfade direction**: tone=0 (dark) -> output = mostly LP-filtered signal; tone=10 (bright) -> output = mostly unfiltered dry signal. The LP cutoff of 34Hz at tone=10 is irrelevant because the crossfade weight is entirely on the dry path. The LP filter effectively drops out of the circuit at high tone settings (its reactance becomes too high to conduct audio).

**Stage E — Output network (TS-808 vs TS9 mode)**
- TS-808: 100R series, 10K to ground. Output Z = ~133 ohm. Flat response.
- TS9: 470R series, 100K to ground. Output Z = ~503 ohm. Slight treble loss with capacitive loads (long cables).
- Implementation: simple gain/impedance difference. Model the TS9's output LP if driving a cable model, otherwise just the gain difference.

### 1.4 Parameters

| ID | Name | Range | Default | Notes |
|----|------|-------|---------|-------|
| 0 | Drive | 0.0-1.0 | 0.5 | Maps to 0-500K pot (audio taper) |
| 1 | Tone | 0.0-1.0 | 0.5 | Variable LP crossfade (dark to bright) |
| 2 | Level | 0.0-1.0 | 0.5 | Output volume (100K pot) |
| 3 | Mode | 0.0 or 1.0 | 0.0 | 0=TS-808, 1=TS9 |

### 1.5 Component BOM (Reference)

| Component | Value | Role |
|-----------|-------|------|
| R4 | 4.7K | Fixed feedback resistor |
| VR1 (Drive) | 500K audio | Variable feedback resistance |
| C3 | 47nF (0.047uF) | Feedback bass rolloff (720Hz) |
| C4 | 51pF | Feedback treble rolloff |
| D3, D4 | 1N914 (or MA150) | Anti-parallel feedback clipping diodes |
| IC1 | JRC4558D (TS-808) / TA75558P (some TS9) | Dual op-amp |
| C5 | 220nF | Tone filter cap |
| R_tone_fixed | 1K + 220R | Tone mixing resistors |
| VR2 (Tone) | 20K linear | Tone pot |
| VR3 (Level) | 100K audio | Volume pot |
| R_out_808 | 100R series, 10K shunt | TS-808 output network |
| R_out_TS9 | 470R series, 100K shunt | TS9 output network |
| Q1 | 2SC1815GR | Input buffer (emitter follower) |
| Bias | 2x 10K + 10uF | 4.5V virtual ground |

### 1.6 Factory Presets

| Name | Drive | Tone | Level | Mode | Character |
|------|-------|------|-------|------|-----------|
| Classic Green | 0.45 | 0.55 | 0.50 | 808 | The iconic TS tone |
| Edge of Breakup | 0.20 | 0.60 | 0.65 | 808 | Clean boost into amp |
| Texas Flood | 0.70 | 0.45 | 0.45 | 808 | SRV-style thick blues |
| Mid Boost | 0.30 | 0.70 | 0.70 | 808 | Rhythm crunch, mids forward |
| TS9 Bright | 0.50 | 0.65 | 0.50 | TS9 | Slightly more open top end |
| TS9 Lead | 0.75 | 0.50 | 0.55 | TS9 | Compressed singing lead |
| Stack (into OD) | 0.60 | 0.40 | 0.40 | 808 | For stacking before another drive |
| Warm Neck Pickup | 0.35 | 0.35 | 0.55 | 808 | Dark and smooth for humbuckers |
| Frusciante Lead | 0.55 | 0.50 | 0.50 | 808 | Strat single coil singing sustain |

### 1.7 Anti-Aliasing

2x oversampling around the diode clipper (Stage C). The soft-clipping-in-feedback topology generates significantly fewer high-order harmonics than hard clipping (DS-1, RAT), so 2x is sufficient. Pre-clipping bandwidth is limited to ~5.66kHz at max drive by the 51pF cap, further reducing alias-generating content.

### 1.8 Signal Chain Position

Index #31 in the signal chain array. Disabled by default. The default `effectOrder` list in `AudioViewModel` must be updated to insert index 31 between Boost (#3) and Overdrive (#4) in the ordering permutation. The index is a slot identifier; the user-visible position is determined by `effectOrder`.

---

## 2. BD-2 Blues Driver — Accuracy Upgrade

### 2.1 Overview

5 targeted upgrades to the existing BD-2 implementation (`boss_bd2.h`, slot #30), prioritized by sonic impact. No architectural changes — same signal flow, same parameters, same API.

### 2.2 Upgrade #1: Inter-Stage Tone Stack (CRITICAL)

**Current**: Single one-pole LP at 3kHz with 0.316x (-10dB) insertion loss.

**Problem**: The real circuit is a multi-component passive Fender-style network (fixed Treble=0, Bass=10, Mid~6). The current model passes 2x too much treble into Stage 2, causing wrong clipping character and incorrect gain knob response at every setting.

**Fix**: Replace with 2-biquad cascade:
- **Biquad 1**: Low shelf, fc=200Hz, gain=-3dB (mild bass rolloff below 100Hz)
- **Biquad 2**: 2nd-order Butterworth LP, fc=1.8kHz, Q=0.707 (treble rolloff matching measured response)
- **Fixed attenuation**: 0.25x (-12dB insertion loss)

**Measured frequency response of real tone stack**:
- Below 100Hz: -3dB (mild bass rolloff, R18=1M lets most bass pass)
- 100-500Hz: Flat (0dB reference after insertion loss)
- 500Hz-1kHz: Beginning to roll off
- Above 1kHz: -12dB/octave (2nd-order treble reduction)

### 2.3 Upgrade #2: JFET Clipping Transfer Function (HIGH)

**Current**: `fast_tanh(x * 1.2)` positive, `fast_tanh(x * 0.85) * 1.05` negative. Asymmetry ratio ~1.41:1.

**Problem**: Real circuit has fundamentally different curve shapes on each half. PNP (2SA1048) saturates with a narrow ~0.3V knee. JFET (2SK184-GR) pinches off with a wide ~0.8V knee via square-law. Real asymmetry ratio is ~2.5:1 in transition width.

**2SK184-GR specs**: Idss = 2.6-6.5mA (typ. 4.0mA GR grade), Vp = -0.8V typ.

**Fix**: Replace `jfetClip()` with:
- **Negative swing (JFET pinchoff)**: Cubic soft-clip approximating square-law:
  ```cpp
  // Normalized input t = |x| / (2 * Vp), clamped to [0, 1]
  // Cubic S-curve: y = Vminus * (1.5*t - 0.5*t^3)  -- matches JFET (1-Vgs/Vp)^2 shape
  // Wide knee (~0.8V transition width)
  ```
  Generates 2nd-harmonic-dominant even-order distortion.
- **Positive swing (PNP saturation)**: `fast_tanh(x / (headroom * 0.4))` where headroom = 3.85V. The 0.4 divisor gives a ~0.3V transition width, modeling abrupt PNP Vce_sat.
  ```cpp
  // headroom = Vplus - Vce_sat = 4.0 - 0.15 = 3.85V
  // output = headroom * fast_tanh(x / (headroom * 0.4))
  ```
- The soft-to-hard knee ratio (0.8V / 0.3V = 2.67:1) produces more even harmonics, giving the BD-2 its tube-like warmth.

### 2.4 Upgrade #3: Tone Control (HIGH)

**Current**: Tilt EQ — LP and HP at 1kHz crossfaded by tone parameter. This simultaneously boosts bass AND cuts treble (or vice versa).

**Problem**: Real circuit is a **treble-cut-only** variable low-pass filter. It never boosts bass. The tilt EQ is topologically wrong.

**Actual circuit**: C100=18nF series cap, TONE pot=250KA to output stage.
- Cutoff: `fc = 1 / (2 * pi * R_tone * 18e-9)`
- Tone=0 (darkest): R~1K -> fc = 8.8kHz. Seems high, but this is the LP cutoff of the *series* RC path. At low R, the cap's reactance dominates and the signal is attenuated across the board. The net effect is dark.
- Tone=5 (noon): R~50K -> fc = 177Hz. Treble above 177Hz is progressively rolled off.
- Tone=10 (brightest): R~250K -> fc = 35Hz. At this resistance, the RC filter's impedance is so high at audio frequencies that it acts as an open circuit -- signal passes through the direct path unfiltered. This is NOT a 35Hz LP filter on the output; the filter drops out of the circuit entirely.

**Fix**: Replace tilt EQ with single variable-cutoff one-pole LP:
- Audio taper pot mapping: `R = 1000 + tone^2 * 249000` (1K to 250K)
- Cutoff: `fc = 1 / (2*pi*R*18e-9)`
- One-pole LP per sample: `y[n] = (1-a)*x[n] + a*y[n-1]`

### 2.5 Upgrade #4: 1SS133 Diode Clippers (MODERATE)

**Current**: Completely ignored.

**Problem**: At moderate gain (20-40%, the BD-2's sweet spot for blues), these diodes shape the clipping character significantly. D3/D4 (1 pair) clip after Stage 1. D7-D10 (2 parallel pairs) clip after Stage 2.

**1SS133 specs**: Is = 2.5nA, n = 1.0, Vf = 0.55V at 1mA. Equivalent to 1N4148.

**Fix**: Add shunt diode clippers after each stage's JFET clip:
- D3/D4: single pair, onset at ~0.3V, full limiting at ~0.55V
- D7-D10: doubled pairs (softer knee due to parallel current sharing)
- Below ~0.3V: pass-through (diodes not conducting)
- Above ~0.55V: soft limit
- At high gain: rail saturation dominates, diodes irrelevant (already clipped below threshold)

### 2.6 Upgrade #5: Gyrator Q Refinement (LOW)

**Current**: Biquad peaking at 120Hz, +6dB, Q=1.5.

**Fix**: Adjust Q to ~2.0-2.5 based on gyrator damping analysis. The real gyrator's series resistance (~1.2K from R25) gives a Q of approximately 2.2. This creates a slightly tighter, more resonant bass peak.

No other changes needed — frequency and gain are already correct.

### 2.7 Implementation Order

1. Inter-stage tone stack (affects everything downstream)
2. JFET clipping (harmonic content)
3. Tone control (user-facing knob accuracy)
4. 1SS133 diodes (sweet spot refinement)
5. Gyrator Q (subtle low-end tightening)

---

## 3. Graphic EQ Redesign — MXR M108S 10-Band (COMPLETED)

> **Status**: Already implemented during research phase. This section documents what was done for reference.

### 3.1 Overview

Replaced the 7-band Boss GE-7 style EQ with an MXR M108S 10-Band Graphic EQ.

### 3.2 Root Cause: Why Current EQ Feels Weak

Three compounding factors:

1. **Q=1.5 too narrow**: Each band covers 0.92 octaves, but bands are spaced 1 octave apart. Leaves gaps where no slider has influence. Moving a fader barely affects frequencies between bands.
2. **Only 7 bands (100-6400Hz)**: Missing sub-bass (guitar low E = 82Hz), presence (4kHz), brilliance (8kHz), air (16kHz).
3. **+/-15dB too wide**: First 50% of slider travel is subtle, last 25% is harsh.

### 3.3 New Specification

| Parameter | Old (GE-7) | New (MXR M108S) |
|-----------|-----------|-----------------|
| Bands | 7 | 10 |
| Frequencies | 100-6400Hz | 31.25Hz-16kHz (ISO) |
| Q | 1.5 | 0.9 |
| Bandwidth per band | 0.92 octaves | 1.55 octaves |
| Band overlap | -0.08 oct (gap!) | +0.55 oct (smooth) |
| Gain range | +/-15dB | +/-12dB |
| Combined ripple | ~4dB (audible valleys) | <0.7dB (meets ISO spec) |

### 3.4 Band Frequencies

| Param ID | Frequency | Guitar Context |
|----------|-----------|----------------|
| 0 | 31.25 Hz | Sub-bass rumble, usually cut |
| 1 | 62.5 Hz | Low E fundamental (82Hz nearby) |
| 2 | 125 Hz | Bass body, warmth |
| 3 | 250 Hz | Low-mid mud or fullness |
| 4 | 500 Hz | Midrange body |
| 5 | 1 kHz | Upper-mid presence/honk |
| 6 | 2 kHz | Attack, cut-through |
| 7 | 4 kHz | Presence, pick definition |
| 8 | 8 kHz | Brilliance, sparkle |
| 9 | 16 kHz | Air (subtle on guitar) |
| 10 | — | Output Level (+/-12dB) |

### 3.5 Filter Topology

Keep SVF (Cytomic/Andrew Simper) peaking EQ — same topology as current code, proven stable and efficient. All 10 bands in series.

- `g = tan(pi * freq / sampleRate)` — pre-computed per band in `setSampleRate()`
- `k = 1/Q = 1/0.9 = 1.111` — uniform across all bands
- Peaking: `output = input + (gain^2 - 1) * bandpass`
- 64-bit double state variables for low-frequency stability (critical for 31.25Hz band)

### 3.6 Parameters

11 total parameters (10 band gains + 1 output level), all in dB:

| ID | Name | Range | Default |
|----|------|-------|---------|
| 0-9 | Band gains | -12.0 to +12.0 dB | 0.0 |
| 10 | Level | -12.0 to +12.0 dB | 0.0 |

### 3.7 UI Changes Required

- EqFaderStrip: update from 7 to 10 faders
- Fader width: narrower to fit 10 (42dp -> 33dp)
- Labels: update frequency labels for 10 bands
- Range: update slider range from +/-15 to +/-12

### 3.8 Preset Migration (COMPLETED)

Old 8 parameters (7 bands + level, indices 0-7) were migrated to 11 parameters (10 bands + level, indices 0-10). Old band indices 0-6 (100-6400Hz) mapped to new indices 2-8 (125-8000Hz) — approximate octave matches (~25% frequency shift, acceptable for EQ presets). Bands 0-1 (31.25, 62.5) and 9 (16kHz) defaulted to 0dB for migrated presets.

---

## 4. Technical Constraints

- **CPU budget**: Each new/upgraded effect should stay under ~200 FLOPS/sample at 1x rate (before oversampling). The TS-808 with Newton-Raphson at 2x OS will be ~60-100 FLOPS/sample effective. BD-2 upgrades add ~35-40 FLOPS/sample. Total per-effect budgets are well within mobile real-time constraints.
- **Memory**: No dynamic allocation in `process()`. All buffers pre-allocated in `setSampleRate()`
- **Thread safety**: Atomic parameters (relaxed ordering), lock-free design
- **Oversampling**: 2x for Tube Screamer diode clipper. BD-2 already has 2x on Stage 2.
- **Precision**: 64-bit double for all filter state variables. 32-bit float for audio I/O.

## 5. Files to Modify/Create

| File | Action | Description |
|------|--------|-------------|
| **C++ (app/src/main/cpp/)** | | |
| `effects/tube_screamer.h` | CREATE | New TS-808/TS9 effect |
| `effects/boss_bd2.h` | MODIFY | 5 accuracy upgrades |
| `signal_chain.h` | MODIFY | Add slot #31, update EFFECT_COUNT to 32 |
| `signal_chain.cpp` | MODIFY | Include and instantiate TubeScreamer |
| `native-lib.cpp` (or equivalent JNI) | MODIFY | Wire TubeScreamer parameter routing |
| **Kotlin (app/src/main/java/.../data/)** | | |
| `Preset.kt` | MODIFY | Add TubeScreamer to EffectRegistry, EFFECT_COUNT 31->32 |
| `FactoryPresets.kt` | MODIFY | Add TS factory presets |
| **Kotlin (app/src/main/java/.../viewmodel/)** | | |
| `AudioViewModel.kt` | VERIFY | Uses EFFECT_COUNT dynamically, should auto-adapt |
| `PresetDelegate.kt` | VERIFY | Uses EFFECT_COUNT dynamically, should auto-adapt |
| **Kotlin (app/src/main/java/.../ui/)** | | |
| `SettingsScreen.kt` | VERIFY | Displays EFFECT_COUNT, will auto-update |
| **Tests** | | |
| `EffectRegistryTest.kt` | MODIFY | Update `assertEquals(31, EFFECT_COUNT)` to 32 |
| `AudioViewModelTest.kt` | VERIFY | Uses EFFECT_COUNT dynamically |
| `PresetSerializationTest.kt` | VERIFY | Uses EFFECT_COUNT dynamically |
| `PresetManagerIntegrationTest.kt` | VERIFY | Uses EFFECT_COUNT dynamically |

**Note**: Most Kotlin files reference `EffectRegistry.EFFECT_COUNT` dynamically and will auto-adapt when the constant changes. Only `EffectRegistryTest.kt` hardcodes `31` and needs manual update. The `parametric_eq.h` and `EqFaderStrip.kt` changes were already completed during research.

## 6. Testing Strategy

- **Unit**: Verify each filter stage in isolation (HPF at 720Hz, diode clipper threshold, EQ band response)
- **Frequency sweep**: Compare TS and BD-2 frequency response curves against published Electrosmash measurements
- **A/B listening**: Compare against real pedal recordings (YouTube isolated DI comparisons)
- **Regression**: Ensure existing presets still sound correct after EQ parameter migration
- **Performance**: Profile on target device to verify CPU budget compliance
