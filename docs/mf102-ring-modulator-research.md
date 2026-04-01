# Moog MF-102 Moogerfooger Ring Modulator: Circuit Analysis & DSP Implementation Guide

## Part 1: Overview & Architecture

### 1.1 Product History

The MF-102 Ring Modulator was designed by Bob Moog and his engineering team at Moog Music, introduced in 1998 as part of the Moogerfooger line. It is a direct descendant of the original Moog modular synthesizer modules. The ring modulator concept in Moog's lineage traces back to Harald Bode's designs -- Moog licensed Bode's Ring Modulator design in 1966 (Models 6401 and 6402), which used diode rings and transformers. The MF-102 is a modernized, all-solid-state implementation using OTA (Operational Transconductance Amplifier) technology instead of diode rings.

### 1.2 Functional Blocks

The MF-102 contains three complete modular functions:

1. **Ring Modulator** -- OTA-based four-quadrant analog multiplier
2. **Carrier Oscillator** -- Voltage-controlled oscillator (VCO), sine-like waveform
3. **Low Frequency Oscillator (LFO)** -- Dual-waveform (sine/square), voltage-controlled rate

**IMPORTANT CORRECTION**: The MF-102 does **NOT** contain a Moog ladder filter (4-pole VCF). That is the MF-101 Lowpass Filter, a separate Moogerfooger pedal. The MF-102's signal path is:

```
Input -> Input Buffer/Preamp (DRIVE) -> Ring Modulator Core -> Dry/Wet Mixer (MIX) -> Output Buffer (OUTPUT)
                                              ^
                                              |
                                     Carrier Oscillator (FREQ)
                                              ^
                                              |
                                        LFO (RATE, AMOUNT)
```

The MF-102 has a "squelch" circuit that minimizes carrier bleed-through at low input levels, compensating for component mismatch in the balanced modulator.

### 1.3 Published Specifications

| Parameter | Value |
|---|---|
| Input Impedance | 1 Mohm |
| Output Impedance | 600 ohm |
| Power Supply | +9V to +15V DC, 100mA nominal |
| Carrier Freq (LO) | 0.6 Hz to 80 Hz |
| Carrier Freq (HI) | 30 Hz to 4,000 Hz |
| LFO Rate | 0.1 Hz to 25 Hz |
| LFO Waveforms | Sine-like (triangle), Square |
| LFO Modulation Depth | 0 to 3 octaves of carrier frequency |
| LFO Polarity | Bipolar (up and down from center freq) |
| Mix Control | Crossfade from 100% dry to 100% wet |
| CV Inputs | 4x (Freq, Rate, LFO Amt, Mix) |
| Expression Pedal Inputs | 4x (same parameters) |
| Bypass | True bypass (mechanical footswitch) |
| Polyphony | Mono |

### 1.4 What Makes It Sound "Moog"

The MF-102's character comes from several factors:
1. **OTA nonlinearity**: The tanh() transfer characteristic of the OTA differential pair creates soft saturation at signal extremes, adding subtle harmonic coloration that a pure mathematical multiply lacks
2. **Carrier purity**: The sine-like carrier waveform is not perfectly pure -- it has slight harmonic content (likely 2nd/3rd harmonics from the VCO circuit), which adds "warmth" compared to a pure sine ring mod
3. **Carrier bleed**: Imperfect balance in the OTA causes slight carrier leakage, adding a pitched undertone
4. **Input drive stage**: The DRIVE control allows pushing the input into soft clipping before the ring modulator, adding even harmonics
5. **Analog component drift**: Slight frequency instability in the carrier oscillator creates micro-detuning effects
6. **High input impedance** (1 Mohm): Preserves guitar pickup loading characteristics

---

## Part 2: Ring Modulator Core -- OTA-Based Four-Quadrant Multiplier

### 2.1 The OTA as Ring Modulator

The MF-102 uses an OTA-based ring modulation topology. Based on the Moogerfooger line's known component choices (LM13700 dual OTA in the MF-103 Phaser, similar architecture across the line), the MF-102 almost certainly uses either a **CA3080** single OTA or (more likely in later production) an **LM13700** dual OTA, which is effectively two CA3080s with optional linearization diodes and output buffers in a single 16-pin DIP package.

The CA3080 was discontinued; the LM13700 is its modern replacement and is pin-compatible for single-OTA applications with minor differences:
- CA3080: Simpler Widlar current mirror, no linearization diodes, no buffer
- LM13700: Wilson current mirror (two diode-drops above V-), linearization diodes available, Darlington buffer included
- Both have identical core differential pair topology

### 2.2 OTA Transfer Function (Critical for DSP)

The fundamental OTA equation is:

```
I_out = (I_ABC / 2) * tanh(V_in / (2 * V_T))
```

Where:
- **I_out** = output current (the OTA output is a current, not voltage)
- **I_ABC** = Amplifier Bias Current (the control input), range 0.1 uA to 2 mA
- **V_in** = V+ - V- = differential input voltage
- **V_T** = thermal voltage = kT/q ≈ **26 mV at 25C (room temperature)**
- **tanh** = hyperbolic tangent (the natural transfer function of a bipolar differential pair)

The small-signal transconductance (linear region):
```
g_m = I_ABC / (2 * V_T)
```

At V_T = 26 mV:
```
g_m = I_ABC / 0.052 = 19.23 * I_ABC (in Siemens, when I_ABC is in Amps)
```

**Linear input range**: The OTA is approximately linear for |V_in| < 20 mV (< V_T). Beyond this, the tanh saturates. At V_in = ±52 mV (±2*V_T), the output is already at ~76% of its maximum. At V_in = ±130 mV (±5*V_T), the output is at ~99.9% -- effectively hard-clipped.

### 2.3 Four-Quadrant Multiplication via OTA

For ring modulation, the OTA implements four-quadrant multiplication by using the **carrier signal to control I_ABC** while the **audio signal drives the differential input**.

The key insight: since g_m is proportional to I_ABC, and I_out = g_m * V_in (in the linear region), we get:

```
I_out ≈ (I_ABC / (2*V_T)) * V_in  =  (1 / (2*V_T)) * I_ABC * V_in
```

This is a **multiplication** of I_ABC (carrier) and V_in (audio input). When I_ABC swings both positive and negative (or more precisely, the carrier signal modulates I_ABC around a bias point), we get true four-quadrant multiplication: both carrier and modulator can be positive or negative, and the output tracks the product.

**However**, in a single CA3080/LM13700, I_ABC must remain positive (it's a current into a current mirror). True bipolar carrier operation requires either:
1. Two OTAs in push-pull configuration (one for positive carrier, one for negative)
2. A DC bias on I_ABC with the carrier as AC modulation (two-quadrant, not true ring mod)
3. A Gilbert cell topology (cross-coupled differential pairs)

The MF-102 likely uses approach **#1** (dual OTA push-pull), which is why the LM13700 (dual OTA) is the natural choice. One half handles positive carrier excursions, the other handles negative, and their outputs are differentially combined.

### 2.4 Gilbert Cell Equivalent Analysis

Whether implemented as discrete differential pairs or OTAs, the ring modulator core functions as a Gilbert cell four-quadrant multiplier. The Gilbert cell output equation is:

```
delta_I = I_EE * tanh(V_carrier / (2*V_T)) * tanh(V_audio / (2*V_T))
```

Where:
- **I_EE** = tail current of the bottom differential pair
- **V_carrier** = carrier signal voltage
- **V_audio** = audio input signal voltage

For small signals (both << 2*V_T ≈ 52 mV), this approximates:

```
delta_I ≈ I_EE * (V_carrier / (2*V_T)) * (V_audio / (2*V_T))
         = I_EE * V_carrier * V_audio / (4 * V_T^2)
```

This is a true linear four-quadrant multiplier for small signals. The tanh nonlinearity only becomes significant when either input exceeds ~20-30 mV.

### 2.5 Ring Modulation Mathematics

For a carrier frequency f_c and input frequency f_a, ideal ring modulation produces:

```
cos(2*pi*f_c*t) * cos(2*pi*f_a*t) = 0.5 * [cos(2*pi*(f_c+f_a)*t) + cos(2*pi*(f_c-f_a)*t)]
```

Output contains ONLY sum (f_c + f_a) and difference (f_c - f_a) frequencies. Neither f_c nor f_a appear in the output of a perfect ring modulator.

In practice, the MF-102 exhibits:
- **Carrier leakage**: Slight f_c component in output due to OTA mismatch (the squelch circuit minimizes this)
- **Input feedthrough**: Slight f_a component due to imperfect balance
- **Harmonic intermodulation**: Because both tanh() functions generate odd harmonics (3rd, 5th, 7th...), the actual output contains sum/difference products of ALL harmonics, not just fundamentals
- **DC component**: If the carrier has any DC offset, the output will contain the input signal itself (amplitude-modulated)

### 2.6 The Carrier Leakage / Squelch Circuit

The MF-102 has a built-in squelch circuit that reduces carrier bleed-through at low input levels. This is essentially a noise gate or expander on the ring modulator output, controlled by the input signal envelope. Without this, you'd hear the carrier oscillator as a constant tone when no input signal is present (due to OTA mismatch causing imperfect carrier suppression).

The squelch likely uses another OTA as a VCA (voltage-controlled amplifier), with an envelope follower on the input signal controlling the VCA gain.

---

## Part 3: Carrier Oscillator

### 3.1 Frequency Ranges

The carrier oscillator has two ranges selected by a LO-HI rocker switch:

| Range | Minimum | Maximum | Ratio |
|---|---|---|---|
| LO | 0.6 Hz | 80 Hz | ~133:1 (~7 octaves) |
| HI | 30 Hz | 4,000 Hz | ~133:1 (~7 octaves) |

The overlap region (30-80 Hz) allows smooth frequency selection across the transition.

### 3.2 Waveform

The carrier produces a "sine-like" waveform. In Moog's classic VCO designs, this is typically generated by:

1. **Triangle core oscillator** (integrator with Schmitt trigger comparator) produces a triangle wave
2. **Triangle-to-sine waveshaper** (diode/resistor breakpoint network or OTA-based) converts to approximate sine

The resulting waveform is a sine wave with residual harmonic distortion, typically:
- THD (Total Harmonic Distortion): ~1-3%
- Dominant harmonics: 2nd and 3rd
- This impurity is part of the Moog character -- a mathematically pure sine would sound "sterile" by comparison

### 3.3 Frequency Control

The FREQUENCY knob has a **logarithmic** (exponential) taper, consistent with Moog's voltage-per-octave standard. The CV input follows the standard **1 volt per octave** convention:

```
f = f_base * 2^(V_control)
```

Where V_control is the sum of the knob position voltage and external CV.

The LFO AMOUNT control determines how many octaves the LFO sweeps the carrier frequency. At maximum, the LFO modulates the carrier across 3 octaves (a factor of 8:1 in frequency).

### 3.4 VCO Circuit Topology (Moog Standard)

Based on classic Moog VCO architecture:

**Exponential converter**: Matched transistor pair (likely from a transistor array like CA3046 or matched discretes) converts linear control voltage to exponential current. Temperature compensation via a tempco resistor (typically 3.3 kohm NTC).

**Integrator**: OTA or op-amp integrating the exponential current across a timing capacitor. The capacitor charges linearly, producing the triangle wave rising edge.

**Schmitt trigger / comparator**: Detects when the integrator output reaches upper/lower thresholds and reverses the integration direction, producing the triangle wave.

**Triangle-to-sine shaper**: Breakpoint waveshaper using diode pairs and resistors to progressively compress the triangle peaks into a sine approximation.

### 3.5 Analog Drift

The VCO exhibits slight frequency drift due to:
- Temperature changes affecting V_T (thermal voltage) in the exponential converter
- Capacitor dielectric absorption and leakage
- Power supply voltage variation
- Component aging

Typical drift: ~0.1-0.5% over several minutes after warm-up, ~1-2% over temperature extremes. This drift is musically significant and contributes to the "alive" quality of analog ring modulation.

---

## Part 4: LFO Section

### 4.1 Specifications

| Parameter | Value |
|---|---|
| Rate Range | 0.1 Hz (10 sec period) to 25 Hz |
| Waveforms | Sine-like (triangle-derived) and Square |
| Amount Range | 0 to 3 octaves of carrier frequency modulation |
| Polarity | Bipolar (modulates carrier both up and down from center frequency) |
| CV Control | Rate and Amount independently controllable via CV/expression pedal |

### 4.2 LFO Circuit

The LFO uses the same triangle-core oscillator topology as the carrier VCO but at much lower frequencies:

1. **Integrator** charges a capacitor (much larger value than VCO, likely 1-10 uF for sub-Hz rates)
2. **Comparator** reverses at thresholds, producing triangle wave
3. **Triangle-to-sine shaper** generates the sine-like output
4. **Comparator output** directly provides the square wave (hard switching at thresholds)

The LFO rate control likely uses a linear potentiometer with exponential conversion (same 1V/oct standard), though the MF-102S software version offers LFO sync to tempo.

### 4.3 LFO-to-Carrier Modulation Path

The LFO output (scaled by the AMOUNT knob) feeds directly into the carrier VCO's exponential converter CV summing node. Because the exponential converter naturally provides the log-to-linear conversion:

```
f_carrier = f_base * 2^(V_knob + V_LFO * amount)
```

At amount = 0, carrier frequency is static. At amount = max, the LFO sweeps the carrier over 3 octaves (e.g., 100 Hz to 800 Hz with a bipolar 1.5-octave-each-way sweep).

### 4.4 LFO Waveform Selection

- **Sine-like**: Smooth, gradual modulation. Creates vibrato-like ring modulation sweeps
- **Square**: Abrupt switching between two carrier frequencies. Creates trillo/trill-like alternating ring modulation

The software version (MF-102S) offers additional LFO polarity:
- **Bipolar**: Carrier modulates both higher AND lower than center frequency
- **Unipolar**: Carrier only modulates upward from the knob setting

---

## Part 5: Input/Output Stages & Gain Staging

### 5.1 Input Stage

- **Input Impedance**: 1 Mohm (high-Z, suitable for guitar pickups, synths, line level)
- **DRIVE Control**: Variable input gain, likely an op-amp-based preamp (TL072 or similar)
- **Level LED**: 4-state indicator (off / green / yellow / red) showing input level
- The DRIVE stage can be pushed into soft clipping for additional harmonic content before ring modulation

### 5.2 Output Stage

- **Output Impedance**: 600 ohm (low-Z, can drive long cables and mixer inputs)
- **OUTPUT Control**: Variable output gain (attenuation and boost)
- **LINK Feature** (MF-102S software): Maintains unity gain when adjusting DRIVE, automatically compensating OUTPUT

### 5.3 Mix Control

The MIX knob crossfades between:
- **Full CCW**: 100% dry (unmodulated) signal
- **Full CW**: 100% wet (ring modulated) signal
- **Center**: Equal blend of dry and wet

The mix circuit uses VCAs (likely OTA-based) for smooth, click-free crossfading. This is important because both the bypassed and ring-modulated signals go through VCA circuits, ensuring consistent signal quality and level matching.

### 5.4 Bypass

True bypass via mechanical footswitch. When bypassed, the signal passes directly from input to output jack with no active circuitry in the path.

### 5.5 Power Supply

The MF-102 accepts +9V to +15V DC. Internally, a charge pump or voltage doubler likely generates a bipolar supply (e.g., +/-7.5V from a 15V input, or +/-4.5V from 9V) to provide the bipolar rails needed for the OTAs and op-amps.

At 9V input, headroom is limited -- the OTA and op-amp stages can only swing approximately +/-3V before clipping (accounting for transistor saturation voltages). At 15V, this increases to approximately +/-6V, providing significantly more dynamic range.

Current draw of 100 mA is consistent with multiple op-amps, OTAs, and the VCO/LFO oscillator circuits.

---

## Part 6: Likely IC Complement (Based on Moogerfooger Family Analysis)

While the exact MF-102 schematic's IC list is not publicly documented in text form, analysis of the Moogerfooger family and the required circuit blocks suggests:

| Function | Likely IC | Qty | Notes |
|---|---|---|---|
| Ring Modulator Core | LM13700 (dual OTA) | 1 | Four-quadrant multiplier using both OTA halves |
| Input VCA / Squelch VCA | LM13700 (dual OTA) | 1 | Or share with above |
| Mix Crossfade VCAs | LM13700 (dual OTA) | 1 | Dry VCA + Wet VCA |
| Input/Output Buffers | TL072 (dual op-amp) | 1-2 | High input Z, low output Z |
| Carrier VCO | TL072 + transistors | 1 | Integrator + comparator |
| LFO | TL072 + transistors | 1 | Same topology as VCO |
| Voltage Regulators | LM78xx / LM79xx or equiv | 1-2 | Bipolar rail generation |
| Exponential Converter | Matched transistor pair | 1-2 | CA3046 array or discrete matched pair |

Total estimated IC count: 5-8 ICs plus discrete transistors and diodes.

The Moogerfooger MF-103 phaser is confirmed to use LM13700N OTAs with +/-5V or +/-9V supply rails. The MF-102 likely follows the same convention.

---

## Part 7: DSP Implementation Strategy

### 7.1 Ring Modulator Core Model

The ring modulator is the simplest block to model digitally. There are two approaches:

#### Approach A: Ideal Multiplication (Simplest, ~2 FLOPS/sample)

```
output = input * carrier
```

This produces perfect sum/difference frequencies with zero carrier leakage and zero harmonic distortion. It sounds "clean" but lacks the analog character. Suitable as a baseline.

#### Approach B: OTA-Accurate Model (~10-15 FLOPS/sample)

Model the Gilbert cell / dual-OTA transfer function:

```
// Per-sample processing
float Vt = 0.026f;       // Thermal voltage at 25C (26 mV)
float twoVt = 2.0f * Vt; // 52 mV
float Iee = 1.0f;        // Normalized tail current

// Scale input signals to realistic analog voltage ranges
// Guitar signal: ~100 mV to ~1V peak depending on DRIVE
// Carrier: ~200 mV peak (typical OTA driving level)
float audioScaled = input * inputGainFactor;  // e.g., input * 0.3 to 1.0
float carrierScaled = carrier * carrierLevel;  // e.g., carrier * 0.4

// Gilbert cell equation
float output = Iee * fast_tanh(audioScaled / twoVt) * fast_tanh(carrierScaled / twoVt);

// Scale back to normalized audio range
output *= outputNormalization;
```

The two tanh() functions are critical:
- **tanh(audio/2Vt)**: Provides soft clipping on the input, generating odd harmonics (3rd, 5th, 7th...) before multiplication. This is the DRIVE characteristic.
- **tanh(carrier/2Vt)**: The carrier is not a pure multiplier; its peaks are compressed, which reduces the higher-order sidebands and produces a "warmer" ring modulation compared to ideal multiplication.

**Scaling is critical**: The ratio of signal amplitude to V_T (26 mV) determines how much nonlinearity is present. At realistic analog signal levels (100-500 mV), both tanh functions are significantly into their nonlinear regions. At normalized DSP levels (0 to 1), the scaling factor must map to these analog-equivalent voltages.

#### Approach C: Full OTA Model with Carrier Leakage (~20-25 FLOPS/sample)

Add imperfections:

```
// Carrier leakage due to OTA mismatch (adjustable parameter)
float carrierLeak = 0.005f;  // -46 dB typical for well-matched OTA
float inputFeedthrough = 0.003f;  // -50 dB

// Squelch (noise gate on output, controlled by input envelope)
float envelope = envelopeFollower(fabsf(input));  // One-pole follower
float squelchGain = smoothStep(envelope, squelchThreshold, squelchThreshold * 2.0f);

// Ring mod output with imperfections
float ringOutput = Iee * fast_tanh(audioScaled / twoVt) * fast_tanh(carrierScaled / twoVt);
ringOutput += carrierLeak * carrier;        // Carrier bleed
ringOutput += inputFeedthrough * input;     // Input feedthrough
ringOutput *= squelchGain;                  // Squelch attenuates when no input
```

### 7.2 Carrier Oscillator Model

#### Sine Oscillator with Controllable Impurity

```
// Phase accumulator (standard digital oscillator)
carrierPhase += carrierFreq / sampleRate;
if (carrierPhase >= 1.0f) carrierPhase -= 1.0f;

// Pure sine
float carrier = sinf(2.0f * M_PI * carrierPhase);

// Add harmonic impurity (analog VCO character)
// 2nd harmonic: ~1-2% (-34 to -40 dB)
// 3rd harmonic: ~0.5-1% (-40 to -46 dB)
float carrier2 = sinf(4.0f * M_PI * carrierPhase);
float carrier3 = sinf(6.0f * M_PI * carrierPhase);
carrier += harmonicImpurity * (0.015f * carrier2 + 0.008f * carrier3);

// Optional: slight frequency drift (very subtle)
// Model as slow random walk on frequency, filtered to ~0.1 Hz bandwidth
```

**For efficiency**, use a wavetable or polynomial sine approximation rather than sinf().

#### Frequency Control with LFO Modulation

```
// Exponential frequency mapping (1V/oct equivalent)
float lfoValue = lfo.process();  // Returns -1 to +1
float modDepth = lfoAmount * 1.5f;  // 0 to 1.5 octaves each direction (3 octaves total)

// Exponential application (Moog standard V/oct)
float freqMod = exp2_approx(lfoValue * modDepth);
float actualFreq = baseCarrierFreq * freqMod;

// Clamp to valid range based on LO/HI switch
if (loRange)
    actualFreq = clamp(actualFreq, 0.6f, 80.0f);
else
    actualFreq = clamp(actualFreq, 30.0f, 4000.0f);
```

### 7.3 LFO Model

```
// Triangle-core LFO (matches analog topology)
lfoPhase += lfoRate / sampleRate;
if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

// Triangle wave: base waveform
float triangle = 4.0f * fabsf(lfoPhase - 0.5f) - 1.0f;  // -1 to +1

// Sine-like output: apply cubic soft-shaping to round the triangle peaks
// This matches the analog triangle-to-sine shaper behavior
float sine_like = triangle * (1.5f - 0.5f * triangle * triangle);  // Cubic approximation

// Square output: hard threshold of triangle
float square = (lfoPhase < 0.5f) ? 1.0f : -1.0f;

// Select waveform
float lfoOutput = (waveformSelect == SINE) ? sine_like : square;
```

### 7.4 Mix Control

```
// Crossfade with equal-power curve for smooth mix
float wetGain = sinf(mixAmount * M_PI * 0.5f);
float dryGain = cosf(mixAmount * M_PI * 0.5f);

output = dryGain * drySignal + wetGain * wetSignal;
```

Or for CPU efficiency, use a linear crossfade (the MF-102 hardware likely uses linear VCA crossfade):

```
output = (1.0f - mixAmount) * drySignal + mixAmount * wetSignal;
```

### 7.5 Anti-Aliasing Considerations

**Ring modulation creates new frequencies**: If the input contains frequencies up to f_max and the carrier is at f_c, the output contains frequencies at f_c + f_max and f_c - f_max. At 48 kHz sample rate, Nyquist is 24 kHz:

- Carrier at 4 kHz, input harmonic at 22 kHz: sum = 26 kHz (aliases to 22 kHz)
- Carrier at 4 kHz, input harmonic at 21 kHz: sum = 25 kHz (aliases to 23 kHz)

**Oversampling requirement**: For the MF-102's maximum carrier of 4 kHz, **2x oversampling** (96 kHz effective rate) is sufficient for guitar signals (fundamental content mostly below 8 kHz, harmonics attenuated). This ensures sum frequencies stay below Nyquist for the vast majority of the signal energy.

For "belt-and-suspenders" quality with high-gain distorted guitar input (harmonics extending to 15-20 kHz), **4x oversampling** eliminates aliasing entirely.

**The tanh nonlinearity also generates harmonics** that must be considered. Each tanh() on the audio input adds 3rd, 5th, 7th... harmonics before multiplication. These get multiplied with the carrier, creating even more sidebands. With heavy DRIVE settings, 4x oversampling is recommended.

**Anti-aliasing approach for this effect**:
1. **2x oversampling minimum** for all cases
2. **4x oversampling** when DRIVE > 0.5 (significant tanh nonlinearity)
3. Use half-band polyphase downsampling filters (same infrastructure as existing WDF pedals)

### 7.6 CPU Budget Estimate

Per-sample costs at 48 kHz (mono):

| Component | FLOPS | Notes |
|---|---|---|
| LFO (triangle + shaper) | ~8 | Phase accumulator + cubic shape |
| Carrier oscillator (sine approx) | ~12 | Wavetable or polynomial |
| Frequency modulation (exp2) | ~8 | exp2_approx for V/oct |
| Ring mod core (2x tanh) | ~14 | Pade [3/3] fast_tanh * 2 |
| Carrier leakage + squelch | ~10 | Envelope follower + smooth |
| Mix crossfade | ~4 | Linear or equal-power |
| **Subtotal (1x)** | **~56** | |
| **With 2x oversampling** | **~130** | + decimation filter |
| **With 4x oversampling** | **~250** | + decimation filter |

This is well within budget. Comparable to or cheaper than the Dattorro reverb (~280 FLOPS/sample).

---

## Part 8: OTA Mathematical Deep Dive (for DSP Accuracy)

### 8.1 Complete OTA Differential Pair Model

The CA3080/LM13700 OTA core is a bipolar differential pair. The exact transfer function, derived from Ebers-Moll transistor equations:

```
I_out = (I_ABC / 2) * tanh(V_diff / (2 * V_T))
```

Where:
- I_ABC = bias current (0.1 uA to 2 mA max for CA3080)
- V_diff = V+ - V- (differential input voltage)
- V_T = kT/q = 25.85 mV at 25C (often rounded to 26 mV)

The transconductance:
```
g_m = dI_out/dV_diff = (I_ABC / (2 * V_T)) * sech^2(V_diff / (2 * V_T))
```

At V_diff = 0 (quiescent point):
```
g_m_max = I_ABC / (2 * V_T)
```

For I_ABC = 500 uA (typical operating point):
```
g_m = 500e-6 / (2 * 26e-3) = 9.6 mA/V = 9.6 mS
```

### 8.2 Linearization Diodes (LM13700)

The LM13700 includes optional linearization diodes that extend the linear input range from ~20 mV to ~60 mV by pre-distorting the input signal. When used, the effective transfer function becomes:

```
I_out ≈ I_ABC * V_diff / (2 * V_T + R_lin * I_ABC)
```

Where R_lin depends on the linearization diode current. This trades off some gain for improved linearity. The MF-102 may or may not use the linearization diodes -- without them, the nonlinearity is more pronounced, which may be sonically desirable for a ring modulator.

**For DSP**: Model both options and compare. The linearized version sounds "cleaner" (closer to ideal multiplication). The non-linearized version sounds "fatter" with more harmonic content.

### 8.3 Temperature Dependence

V_T = kT/q where:
- k = 1.38065e-23 J/K (Boltzmann constant)
- T = absolute temperature in Kelvin
- q = 1.602e-19 C (electron charge)

At 25C (298 K): V_T = 25.69 mV (commonly rounded to 26 mV)
At 0C (273 K): V_T = 23.54 mV (sharper nonlinearity)
At 50C (323 K): V_T = 27.84 mV (softer nonlinearity)

The temperature affects the softness of the tanh clipping. Warmer temperatures = softer clipping = fewer harmonics. This is generally a subtle effect but contributes to analog "character."

### 8.4 Gilbert Cell vs Dual OTA

If the MF-102 uses a Gilbert cell (two cross-coupled differential pairs sharing a common tail current source), the complete output is:

```
delta_I = I_EE * tanh(V1 / (2*V_T)) * tanh(V2 / (2*V_T))
```

If it uses two separate OTA halves from an LM13700:

```
// OTA1: positive carrier half-cycle
I_out1 = (I_ABC1 / 2) * tanh(V_audio / (2*V_T))

// OTA2: negative carrier half-cycle
I_out2 = (I_ABC2 / 2) * tanh(V_audio / (2*V_T))

// Differential output
I_total = I_out1 - I_out2
```

Where I_ABC1 and I_ABC2 are derived from the carrier signal through opposing polarity bias circuits. When the carrier is positive, I_ABC1 increases and I_ABC2 decreases; when negative, vice versa.

Both topologies produce essentially the same tanh * tanh transfer function. The Gilbert cell is slightly more accurate because the transistors share the same thermal environment (on the same die), while separate OTAs may have slight parameter mismatches.

**For DSP**: Use the Gilbert cell equation directly:
```
output = tanh(audio / (2*Vt)) * tanh(carrier / (2*Vt))
```
This is simpler and captures the essential nonlinear character.

---

## Part 9: Moog Ladder Filter Reference (for future MF-101 implementation)

Although the MF-102 does NOT contain a Moog ladder filter, this section documents the Moog ladder for completeness and potential future use (e.g., running the MF-102 output into an MF-101-style filter).

### 9.1 Original Topology (US Patent 3,475,623)

- **Structure**: 4 identical RC stages in cascade, where the "R" is the dynamic base-emitter resistance of matched bipolar transistor pairs
- **Capacitors**: Each stage has a fixed capacitor between the emitters of a transistor pair. Values vary by implementation:
  - Minimoog: 68 nF (0.068 uF) with 150 ohm base resistors
  - Moog 904A (modular): Three settings: 1.2 uF, 0.3 uF, 0.075 uF
  - Polymoog/Opus 3: 10 nF (0.01 uF) with 150 ohm
  - Prodigy/Rogue: 27 nF (0.027 uF) with 1 kohm
- **Transistors**: Matched pairs (CA3046 transistor array commonly used in production). The original used discrete matched NPN pairs
- **Resonance**: Adjustable resistor from output back to input summing node. Self-oscillation possible at maximum resonance (feedback gain > unity at cutoff)
- **Control**: Exponential current source (matched transistor pair + tempco resistor) sets the tail current of all 4 stages simultaneously. Cutoff frequency is proportional to current: f_c = I_control / (2 * pi * C)
- **Frequency range**: ~1000:1 (approximately 10 octaves)
- **Rolloff**: 24 dB/octave (4-pole)
- **Response**: Approximately Butterworth when resonance is zero

### 9.2 DSP Implementation: Huovilainen Model (Recommended)

From DAFx 2004, Antti Huovilainen's non-linear digital implementation:

**Requires 2x oversampling minimum.**

**Coefficient calculation**:
```
fc = cutoffFreq / sampleRate;
f = fc * 0.5;  // Account for 2x oversampling
fcr = 1.8730*fc*fc*fc + 0.4955*fc*fc - 0.6490*fc + 0.9988;  // Tuning correction
acr = -3.9364*fc*fc + 1.8409*fc + 0.9968;  // Resonance compensation
tune = (1.0 - exp(-2.0*M_PI*f*fcr)) / thermal;  // thermal ≈ 0.000025
resQuad = 4.0 * resonance * acr;
```

**Per-sample processing (2 iterations per input sample for 2x OS)**:
```
for (int j = 0; j < 2; j++) {
    input = sample - resQuad * delay[5];  // Feedback subtraction

    stage[0] = delay[0] + tune * (tanh(input * thermal) - stageTanh[0]);
    delay[0] = stage[0];
    stageTanh[0] = tanh(stage[0] * thermal);

    stage[1] = delay[1] + tune * (stageTanh[0] - stageTanh[1]);
    delay[1] = stage[1];
    stageTanh[1] = tanh(stage[1] * thermal);

    stage[2] = delay[2] + tune * (stageTanh[1] - stageTanh[2]);
    delay[2] = stage[2];
    stageTanh[2] = tanh(stage[2] * thermal);

    stage[3] = delay[3] + tune * (stageTanh[2] - stageTanh[3]);
    delay[3] = stage[3];
    stageTanh[3] = tanh(stage[3] * thermal);

    delay[5] = (stage[3] + delay[4]) * 0.5;  // Half-sample delay compensation
    delay[4] = stage[3];
}
output = delay[5];
```

The `thermal = 0.000025` constant scales signals to the tanh nonlinearity range. The tanh per stage models the BJT differential pair saturation that gives the Moog ladder its characteristic "warmth" -- it compresses peaks before they reach the resonance feedback path, preventing the harsh resonance of a linear filter.

---

## Part 10: Complete Per-Sample DSP Algorithm

### 10.1 State Variables

```cpp
// Carrier oscillator
float carrierPhase_ = 0.0f;      // 0 to 1
float carrierFreqHz_ = 440.0f;   // Current frequency after modulation

// LFO
float lfoPhase_ = 0.0f;          // 0 to 1

// Squelch envelope follower
float squelchEnv_ = 0.0f;        // Smoothed input level

// Parameter smoothing
float smoothFreq_ = 440.0f;
float smoothMix_ = 0.5f;
float smoothDrive_ = 0.5f;
float smoothLfoAmount_ = 0.0f;
float smoothLfoRate_ = 1.0f;
```

### 10.2 Parameters

```cpp
// User controls (0 to 1 normalized)
float frequency;     // Carrier frequency knob position
float mix;           // Dry/wet mix (0=dry, 1=wet)
float drive;         // Input gain / distortion amount
float lfoRate;       // LFO speed
float lfoAmount;     // LFO depth (0 to 3 octaves)

// Discrete controls
enum CarrierRange { LO, HI };     // LO: 0.6-80 Hz, HI: 30-4000 Hz
enum LfoWaveform { SINE, SQUARE }; // Sine-like or square LFO
```

### 10.3 Per-Sample Algorithm

```cpp
void processOneSample(float input, float& output) {
    // 1. Input Drive Stage
    float driven = input * (1.0f + drive * 4.0f);  // 0 to 12 dB gain

    // 2. LFO
    lfoPhase_ += smoothLfoRate_ / sampleRate_;
    if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;

    float lfoVal;
    if (lfoWaveform_ == SINE) {
        // Triangle-derived sine approximation
        float tri = 4.0f * fabsf(lfoPhase_ - 0.5f) - 1.0f;
        lfoVal = tri * (1.5f - 0.5f * tri * tri);  // Cubic sine approx
    } else {
        lfoVal = (lfoPhase_ < 0.5f) ? 1.0f : -1.0f;
    }

    // 3. Carrier Frequency with LFO Modulation
    float baseFreq = mapFrequency(smoothFreq_, carrierRange_);
    float modOctaves = lfoVal * smoothLfoAmount_ * 1.5f;  // +/- 1.5 oct = 3 oct range
    float actualFreq = baseFreq * fast_exp2(modOctaves);

    // 4. Carrier Oscillator
    carrierPhase_ += actualFreq / sampleRate_;
    if (carrierPhase_ >= 1.0f) carrierPhase_ -= 1.0f;

    // Sine with harmonic impurity
    float carrier = fast_sin(carrierPhase_ * 2.0f * M_PI);
    float carrier2 = fast_sin(carrierPhase_ * 4.0f * M_PI);
    carrier += 0.015f * carrier2;  // ~1.5% 2nd harmonic

    // 5. Ring Modulation (OTA Gilbert cell model)
    const float Vt2 = 0.052f;  // 2 * 26 mV thermal voltage
    float audioVoltage = driven * 0.3f;   // Scale to ~300 mV peak equivalent
    float carrierVoltage = carrier * 0.4f; // Scale to ~400 mV peak equivalent

    float ringOut = fast_tanh(audioVoltage / Vt2) * fast_tanh(carrierVoltage / Vt2);

    // 6. Carrier Leakage (analog imperfection)
    ringOut += 0.005f * carrier;  // -46 dB carrier bleed

    // 7. Squelch (attenuate output when no input)
    float absIn = fabsf(input);
    squelchEnv_ = squelchEnv_ * 0.999f + absIn * 0.001f;  // ~3.3 ms attack/release at 48kHz
    float squelchGain = fminf(squelchEnv_ * 50.0f, 1.0f);  // Gate threshold ~-34 dBFS
    ringOut *= squelchGain;

    // 8. Output normalization
    ringOut *= 1.5f;  // Compensate for tanh peak reduction

    // 9. Dry/Wet Mix
    output = (1.0f - smoothMix_) * input + smoothMix_ * ringOut;
}

// Frequency mapping helper
float mapFrequency(float knobPos, CarrierRange range) {
    // Exponential mapping (log frequency)
    if (range == LO) {
        // 0.6 Hz to 80 Hz (log scale)
        return 0.6f * powf(80.0f / 0.6f, knobPos);  // ~7 octaves
    } else {
        // 30 Hz to 4000 Hz (log scale)
        return 30.0f * powf(4000.0f / 30.0f, knobPos);  // ~7 octaves
    }
}
```

### 10.4 Voltage Scaling Notes

The ratio of signal amplitude to V_T is the key "character" parameter:

| Signal/V_T ratio | Behavior | Character |
|---|---|---|
| < 0.5 (< 13 mV) | Nearly linear | Clean, transparent ring mod |
| 0.5 - 2.0 (13-52 mV) | Mild nonlinearity | Warm, slight harmonic enrichment |
| 2.0 - 5.0 (52-130 mV) | Strong nonlinearity | Rich harmonics, "Moog" character |
| > 5.0 (> 130 mV) | Near-saturation | Distorted, aggressive ring mod |

The MF-102 at moderate DRIVE settings operates in the 1.0-3.0 range (musically useful zone). With DRIVE cranked, it moves into the 5.0+ zone for aggressive, distorted ring modulation.

**For the DSP model**, expose the `audioVoltage` and `carrierVoltage` scaling factors as hidden parameters (or derive them from the DRIVE control) so users can tune the analog-equivalent nonlinearity.

---

## Part 11: Comparison with Other Ring Modulators

### 11.1 Diode Ring (Classic, e.g., Bode Ring Modulator)

- Uses 4 matched diodes in a ring/bridge configuration with two transformers
- Transfer function is a **sign() function**, not tanh(): output = input * sign(carrier)
- This produces MUCH harsher sidebands (effectively square-wave multiplication)
- Carrier leakage depends on diode matching (typically worse than OTA)
- The transformer coupling limits bandwidth and adds its own resonance
- Sounds "metallic" and "clangy" -- the classic ring mod sound

### 11.2 OTA-Based (MF-102)

- Smoother multiplication due to tanh() soft limiting
- Less harsh sidebands, especially with moderate drive levels
- Better carrier rejection (OTA can be more precisely balanced than diodes)
- Wider bandwidth (no transformer limitation)
- Sounds "warmer" and more "musical" at moderate settings

### 11.3 Digital Multiplication (Pure DSP)

- Perfect multiplication: output = input * carrier
- Zero carrier leakage, zero harmonic distortion from the multiplication itself
- Can sound "sterile" or "mathematical" without the analog imperfections
- No bandwidth limitations

**The MF-102's sonic identity sits between the diode ring's harshness and digital purity.** The tanh nonlinearity is the key ingredient.

---

## Part 12: Implementation Recommendations for Guitar Emulator App

### 12.1 Effect Parameters

| Parameter | Range | Default | Type |
|---|---|---|---|
| mix | 0.0 - 1.0 | 0.5 | Continuous |
| carrierFreq | 0.0 - 1.0 (mapped to Hz) | 0.5 | Continuous, log mapping |
| carrierRange | 0 (LO) or 1 (HI) | 1 (HI) | Discrete |
| drive | 0.0 - 1.0 | 0.3 | Continuous |
| lfoRate | 0.0 - 1.0 (mapped to Hz) | 0.3 | Continuous, log mapping |
| lfoAmount | 0.0 - 1.0 | 0.0 | Continuous |
| lfoWaveform | 0 (Sine) or 1 (Square) | 0 | Discrete |
| otaCharacter | 0.0 - 1.0 | 0.5 | Continuous (analog nonlinearity amount) |

### 12.2 Oversampling Strategy

- **Default**: 2x oversampling (sufficient for most guitar signals with carrier < 2 kHz)
- **When drive > 0.5 AND carrier > 1 kHz**: Consider 4x (the tanh harmonics + high carrier = aliasing risk)
- Use existing Oversampler infrastructure from WDF pedal effects

### 12.3 Signal Chain Position

Ring modulator should be placed:
- **After** fuzz/distortion (traditional Moog usage with synths)
- **Before** delay/reverb (ring mod into delay = classic ambient texture)
- Suggested index: **between Overdrive and AmpModel** (index position TBD based on current chain)

### 12.4 CPU-Critical Optimizations

1. Use `fast_math::fast_tanh` (Pade [3/3] already in codebase) for both tanh calls
2. Use `fast_math::exp2_approx` for LFO-to-frequency exponential mapping
3. Use wavetable sine (256-1024 entries with linear interpolation) instead of sinf()
4. Pre-compute frequency mapping constants in `setSampleRate()` / parameter update
5. Parameter smoothing: one-pole filter on all continuous params (existing pattern)

### 12.5 Unique Selling Points vs Existing Effects

The Ring Modulator complements existing effects:
- **Tremolo** (index 15): AM only, no frequency shifting. Ring mod IS tremolo when carrier < ~5 Hz
- **Octavia** (index 24): Octave-up via rectification. Ring mod can create sub-octave and arbitrary intervals
- **Chorus/Vibrato**: Delay-based modulation. Ring mod is frequency-domain modulation (fundamentally different)

The Ring Modulator is a versatile effect spanning tremolo, vibrato-like pitch shifting, metallic harmonics, synth-like tones, and extreme sound design.

---

## References & Sources

### Official Documentation
- [Moog MF-102 User Manual (ManualsLib)](https://www.manualslib.com/manual/673980/Moog-Moogerfooger-Mf-102.html)
- [MF-102S Software Ring Modulator (Moog Connect)](https://moogconnect.net/m/mfs/mf-102s.html)
- [MF-102S Ring Modulator Plugin (Moog Music)](https://software.moogmusic.com/store/mf-102s)
- [Moog MF-102 Schematics (SynthXL)](https://www.synthxl.com/moog-mf-102/)
- [Moogerfooger Schematics Thread (MadBeanPedals)](https://www.madbeanpedals.com/forum/index.php?topic=32775.0)

### OTA / Circuit Analysis
- [CA3080 Datasheet (MIT)](https://snebulos.mit.edu/projects/acis/file_cabinet/0/03001/03001_0101_r01m.pdf)
- [CA3080 Application Note AN6668 (Intersil)](https://www.experimentalistsanonymous.com/diy/Datasheets/AN6668.pdf)
- [Comparing OTAs: CA3080 vs LM13600 vs LM13700 (Electric Druid)](https://electricdruid.net/comparing-otas/)
- [OTA Transfer Function (EEEGuide)](https://www.eeeguide.com/operational-transconductance-amplifier-ota/)
- [OTA Wikipedia](https://en.wikipedia.org/wiki/Operational_transconductance_amplifier)

### Gilbert Cell / Ring Modulator Theory
- [An Analysis of Transistor-Based Analog Ring Modulators (DAFx 2009)](https://www.dafx.de/paper-archive/2009/papers/paper_24.pdf)
- [A Simple Digital Model of the Diode-Based Ring-Modulator (DAFx 2011)](http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf)
- [Gilbert Cell (Wikipedia)](https://en.wikipedia.org/wiki/Gilbert_cell)
- [Introduction to the Gilbert Multiplier (AllAboutCircuits)](https://www.allaboutcircuits.com/technical-articles/introduction-to-the-gilbert-multiplier/)

### Moog Ladder Filter (Reference)
- [Non-Linear Digital Implementation of the Moog Ladder Filter - Huovilainen (DAFx 2004)](https://dafx.de/paper-archive/2004/P_061.PDF)
- [MoogLadders Collection (GitHub)](https://github.com/ddiakopoulos/MoogLadders)
- [Moog Ladder Filter Patent US3475623 (Google Patents)](https://patents.google.com/patent/US3475623A/en)
- [Moog Ladder Filter Analysis (Tim Stinchcombe)](https://www.timstinchcombe.co.uk/synth/Moog_ladder_tf.pdf)
- [Moog Ladder Filter Introduction (North Coast Synthesis)](https://northcoastsynthesis.com/news/modular-synthesis-intro-part-7-the-moog-ladder-filter/)

### Historical Context
- [Bob Moog Schematics Release (Moog Foundation)](https://moogfoundation.org/bob-moog-schematics-release-1-for-our-8th-anniversary/)
- [Bode Ring Modulator Schematic (Moog Foundation)](https://moogfoundation.org/schematics/attachment/470/)
- [Moog Patents Overview (till.com)](https://till.com/articles/moog/patents.html)
- [Dr Robert & His Modular Moogs (Sound on Sound)](https://www.soundonsound.com/people/dr-robert-his-modular-moogs)
