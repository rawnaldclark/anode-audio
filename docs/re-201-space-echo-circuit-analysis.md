# Roland RE-201 Space Echo: Exhaustive Circuit Analysis for DSP Implementation

## 1. Historical Context and Overview

The Roland RE-201 Space Echo was introduced in 1974 and remained in continuous production until 1990 -- an extraordinary 16-year production run. It combines a multi-head tape echo with an independent spring reverb in a single desktop unit. The RE-201 replaced the earlier RE-200 and became the most successful tape echo ever manufactured.

The unit was designed by Roland's engineering team (led by Ikutaro Kakehashi's philosophy of accessible effects) and uses entirely discrete transistor circuitry -- no op-amps anywhere in the signal path. This all-transistor design, combined with the tape mechanism's inherent nonlinearities, gives the RE-201 its distinctive warm, musical character that has proven extremely difficult to replicate digitally.

---

## 2. Overall Architecture and Signal Flow

### 2.1 Block Diagram

```
                                         TAPE LOOP (continuous loop)
                                    ┌──────────────────────────────────┐
                                    │                                  │
   INPUT ──► Input Preamp ──► Tone ──► Record Amp ──► RECORD HEAD ──► │
   (Inst/     (2SC828)        Stack    (2SC828)                        │
    Mic)                      (Bass/                    ┌──────────────┘
                               Treble)                  │
                                                        ▼
                                            ┌───── TAPE PATH ─────┐
                                            │                     │
                                    ┌───────┤  HEAD 1  HEAD 2  HEAD 3
                                    │       │  (~50mm) (~100mm)(~150mm)
                                    │       │  from record head
                                    │       └─────────────────────┘
                                    │              │    │    │
                                    │              ▼    ▼    ▼
                                    │         Playback Preamps
                                    │         (3x 2SC828 stages)
                                    │              │    │    │
                                    │              ▼    ▼    ▼
                                    │         MODE SELECTOR (12 pos)
                                    │              │
                                    │              ▼
                     FEEDBACK ◄────────── Feedback Mix
                     (INTENSITY)           │
                                           ▼
                                      Echo Volume ──► Output Mixer
                                                          │
                                                          ▼
                        Spring Reverb ──► Reverb Vol ──► Sum ──► OUTPUT
                        (Accutronics                         (Line Out)
                         Type 4)
```

### 2.2 Signal Path Summary

1. **Input stage**: Selectable Mic (10K ohm) or Instrument (470K ohm) input
2. **Input preamp**: Common-emitter transistor amp with ~20dB gain
3. **Tone controls**: Active bass/treble (Baxandall-type, transistor-based)
4. **Record amplifier**: Drives the record head with bias oscillator
5. **Tape loop**: Continuous loop, 3 playback heads at fixed physical positions
6. **Playback preamps**: One per head, high-gain recovery amplifiers
7. **Mode selector**: 12-position rotary switch selecting head/reverb combinations
8. **Feedback path**: Selected playback output(s) summed back to record input
9. **Spring reverb**: Independent Accutronics tank, fed from input preamp
10. **Output mixing**: Echo volume + Reverb volume + direct signal to output

---

## 3. Tape Transport System

### 3.1 Tape Specifications

| Parameter | Value |
|-----------|-------|
| Tape width | 1/4 inch (6.35mm) standard |
| Tape type | Standard oxide, similar to low-bias Type I cassette formulation |
| Tape loop length | Approximately 53.5 cm (21 inches) |
| Tape speed | 19 cm/s (7.5 ips) nominal at center of Repeat Rate range |
| Speed range | Approximately 9.5-38 cm/s (3.75-15 ips) via Repeat Rate knob |
| Tape tension | Maintained by felt pads and guide rollers |
| Erase method | DC erase head (located before record head) |

### 3.2 Motor and Speed Control

The RE-201 uses a **DC motor** with an electronic speed control circuit. The REPEAT RATE knob varies the motor voltage, which changes tape speed and therefore all delay times proportionally.

**Motor control circuit:**
- Transistor-regulated DC supply to motor
- Feedback via back-EMF sensing for speed stability
- 2SC828 transistor as series regulator
- Speed range: approximately 2:1 ratio from min to max Repeat Rate
- Motor voltage range: approximately 6V to 12V DC

**Critical insight for DSP**: The Repeat Rate knob does NOT change delay time independently per head. It changes tape speed, so ALL three head delay times scale proportionally. The ratio between heads remains constant regardless of Repeat Rate setting.

### 3.3 Wow and Flutter

The RE-201's wow and flutter characteristics are a DEFINING part of its sound -- not defects to be minimized.

**Published specification**: Wow and flutter < 0.35% WRMS (typical for consumer tape machines of the era)

**Measured characteristics on well-maintained units:**
- Total wow + flutter: 0.15-0.35% WRMS (varies with unit condition)
- Aged/worn units: can exceed 0.5-1.0% WRMS

**Spectral breakdown of speed variations:**

| Component | Frequency | Cause | Typical Amplitude |
|-----------|-----------|-------|-------------------|
| Primary wow | 0.5-1.5 Hz | Capstan eccentricity + loop tension variation | 0.08-0.15% peak |
| Secondary wow | 2-4 Hz | Tape splice passing over heads, guide roller eccentricity | 0.04-0.08% peak |
| Primary flutter | 8-12 Hz | Motor cogging, capstan bearing irregularity | 0.03-0.07% peak |
| Secondary flutter | 15-25 Hz | Tape-to-head friction variation, felt pad interaction | 0.01-0.03% peak |
| Broadband noise | 0.5-50 Hz | Random mechanical variations | 0.01-0.02% RMS |

**Wow/flutter model (sum of sinusoids + noise):**
```
speed_mod(t) = A1*sin(2*pi*f1*t + phi1)     // primary wow, ~1.0 Hz
             + A2*sin(2*pi*f2*t + phi2)     // secondary wow, ~3.0 Hz
             + A3*sin(2*pi*f3*t + phi3)     // primary flutter, ~10 Hz
             + A4*sin(2*pi*f4*t + phi4)     // secondary flutter, ~20 Hz
             + noise(t) * A_noise           // filtered random component

where:
  f1 = 0.5-1.5 Hz (slowly drifts over time)
  f2 = 2.5-4.0 Hz
  f3 = 8-12 Hz
  f4 = 15-25 Hz
  A1 = 0.0010 (0.10% peak, dominant)
  A2 = 0.0005 (0.05%)
  A3 = 0.0004 (0.04%)
  A4 = 0.0001 (0.01%)
  A_noise = 0.00015
  noise(t) = white noise filtered through 2nd-order bandpass 0.5-30 Hz
  phi1-4 = random initial phases
```

**CRITICAL**: The frequencies f1-f4 are NOT perfectly stable. They drift slowly over time (on the order of +/-10% variation over 10-30 seconds). The primary wow frequency is particularly variable because the tape loop length changes slightly as the tape stretches with use.

**Tape splice bump**: The RE-201 tape loop has a splice joint. Each time the splice passes a head, there is a brief (~5-15ms) transient disturbance -- a tiny pitch glitch and volume dip. This occurs once per tape revolution (every 2.8 seconds at nominal speed, i.e., ~0.36 Hz). This is NOT modeled in most emulations but is audible on real units.

---

## 4. Three Playback Heads

### 4.1 Physical Head Spacing

The three playback heads are mounted on a fixed head block. Their positions relative to the record head determine the delay times.

| Head | Distance from Record Head | Delay at Nominal Speed (19 cm/s) |
|------|--------------------------|----------------------------------|
| Head 1 | ~50 mm (closest) | ~263 ms |
| Head 2 | ~100 mm (middle) | ~526 ms |
| Head 3 | ~150 mm (farthest) | ~789 ms |

**Delay time calculation:**
```
delay_ms = (head_distance_mm / tape_speed_mm_per_sec) * 1000

At nominal 19 cm/s = 190 mm/s:
  Head 1: 50/190 * 1000 = 263 ms
  Head 2: 100/190 * 1000 = 526 ms
  Head 3: 150/190 * 1000 = 789 ms
```

**Note on exact measurements**: The exact head-to-head distances vary slightly between units due to manufacturing tolerances. The values above are typical. Some sources report slightly different ratios (e.g., Head 2 not exactly 2x Head 1). Actual measured values on specific units range from:
- Head 1: 240-290 ms
- Head 2: 480-560 ms
- Head 3: 720-840 ms

**The ratio between heads is approximately 1:2:3**, which creates musical rhythmic relationships between the taps.

### 4.2 Repeat Rate Knob and Delay Time Range

The Repeat Rate knob varies tape speed, scaling all delay times proportionally:

| Repeat Rate Setting | Tape Speed (approx) | Head 1 | Head 2 | Head 3 |
|---------------------|---------------------|--------|--------|--------|
| Minimum (slow tape) | ~9.5 cm/s | ~526 ms | ~1053 ms | ~1579 ms |
| Center (nominal) | ~19 cm/s | ~263 ms | ~526 ms | ~789 ms |
| Maximum (fast tape) | ~38 cm/s | ~132 ms | ~263 ms | ~395 ms |

Total delay range per head: approximately 2:1 above and below nominal, giving a 4:1 total range.

### 4.3 Head Frequency Response

The playback heads have inherent bandwidth limitations that are fundamental to the RE-201 sound. Each pass through the record/playback cycle acts as a low-pass filter.

**Single-pass frequency response (record + playback):**
- Low end: -3dB at approximately 40-60 Hz (limited by head size and tape speed)
- High end: -3dB at approximately 4-6 kHz at nominal speed (head gap loss + tape formulation)
- Peak response: slight emphasis around 1-3 kHz from head bump
- Roll-off slope: approximately 6 dB/octave above the -3dB point

**Speed-dependent HF response:**
At lower tape speeds, high-frequency response degrades further:
- At half speed: -3dB drops to approximately 2-3 kHz
- At double speed: -3dB extends to approximately 8-10 kHz

This can be modeled as:
```
f_3dB_Hz = base_cutoff * (tape_speed / nominal_speed)

where base_cutoff ~= 5000 Hz at nominal speed
```

### 4.4 Head Gap Loss (Detailed)

The playback head gap creates a sinc-like frequency response null:
```
H_gap(f) = sin(pi * f * g / v) / (pi * f * g / v)

where:
  g = head gap width (~2.5 um for the RE-201 playback heads)
  v = tape speed (m/s)
  f = frequency (Hz)
```

At nominal speed (0.19 m/s) with gap = 2.5 um:
- First null: f_null = v/g = 0.19/2.5e-6 = 76,000 Hz (far above audio band)

This means gap loss per se is NOT the dominant HF limitation. The dominant factors are:
1. **Spacing loss**: Air gap between tape and head surface
2. **Tape oxide thickness**: Self-demagnetization at short wavelengths
3. **Record head fringing**: Limited by record head gap width (~10 um)

### 4.5 Cumulative HF Loss in Feedback

**This is the single most important sonic characteristic of the RE-201.**

Each time the signal passes through the tape (record -> play -> feedback -> record), it loses high-frequency content. After N passes:

```
H_total(f) = H_single(f)^N

where H_single is the single-pass frequency response
```

If H_single is a 1st-order LPF at 5kHz:
- After 1 pass: -3dB at 5kHz
- After 2 passes: -6dB at 5kHz, -3dB at ~2.5kHz
- After 4 passes: -12dB at 5kHz, -3dB at ~1.25kHz
- After 8 passes: -24dB at 5kHz, signal is predominantly sub-1kHz

This progressive darkening of repeats is what makes the RE-201 "sit" in a mix so well -- later repeats become warm, ambient washes that don't compete with the source signal.

**DSP model**: Apply a 1st-order (or 2nd-order for accuracy) LPF in the feedback loop. Each recirculation naturally applies the filter again.

```
Recommended filter: 1-pole LPF at ~4500-5500 Hz (adjustable)
Additional: gentle HPF at ~60-80 Hz to model bass rolloff per cycle
Combined: bandpass character 60Hz-5kHz per pass
```

---

## 5. Tape Saturation and Nonlinearity

### 5.1 Record Bias

The RE-201 uses **AC bias** for the record head, generated by a bias oscillator:
- Bias frequency: approximately 55-65 kHz (well above audio band)
- Bias level: adjusted at the factory for optimal signal-to-noise vs distortion tradeoff
- The bias oscillator uses a transistor (2SC828) in a Colpitts or Hartley oscillator configuration

The bias level sets the operating point on the tape's magnetization curve. Under-bias gives more distortion (and a brighter, harsher sound). Over-bias gives cleaner sound but reduced high-frequency response and lower output level.

### 5.2 Tape Saturation Transfer Function

The tape's magnetic recording process is inherently nonlinear. The transfer function from input signal to recorded magnetization follows a sigmoid/hysteresis curve.

**Simplified model (no hysteresis):**
```
For DSP purposes, a good first approximation is soft clipping via tanh:

y(x) = tanh(drive * x)

where drive controls the input level relative to tape saturation
```

**More accurate model (with asymmetry and hysteresis):**

The Jiles-Atherton hysteresis model provides physically-based tape saturation. The key equations:

```
Anhysteretic magnetization (Langevin function):
  M_an = M_s * [coth(H_e/a) - a/H_e]

where:
  M_s = saturation magnetization
  H_e = H + alpha*M (effective field, includes inter-domain coupling)
  a = shape parameter (controls curve slope)
  alpha = mean-field coupling constant

Differential equation for magnetization:
  dM/dH = (M_an - M) / (k*delta - alpha*(M_an - M)) + c * dM_an/dH

where:
  k = domain wall pinning constant (controls coercivity/loop width)
  c = reversibility coefficient (0 = fully irreversible, 1 = fully reversible)
  delta = sign(dH/dt) (+1 or -1, direction of field change)
```

**Tape-specific Jiles-Atherton parameters (from Chowdhury's research):**

| Parameter | Symbol | Typical Value | Range |
|-----------|--------|---------------|-------|
| Saturation magnetization | M_s | 350,000 A/m | 200K-500K |
| Shape parameter | a | 22,000 A/m | 10K-50K |
| Mean-field coupling | alpha | 1.6e-3 | 1e-4 to 1e-2 |
| Domain wall pinning | k | 27,000 A/m | 10K-50K |
| Reversibility | c | 0.17 | 0.1-0.3 |

**Practical recommendation for our project**: Full Jiles-Atherton is CPU-expensive (requires oversampling and iterative ODE solver). For a mobile DSP context, use a **simplified model**:

```cpp
// Tape saturation: asymmetric soft clip with frequency-dependent drive
float tapeSaturate(float x, float drive, float bias) {
    // bias adds subtle asymmetry (real tape has slight DC offset)
    float biased = x * drive + bias * 0.01f;
    // Soft clip with slight even-harmonic asymmetry
    float y = std::tanh(biased) + 0.05f * biased / (1.0f + biased * biased);
    return y;
}
```

For higher fidelity, use the lookup-table approach from our existing fast_math infrastructure.

### 5.3 Frequency-Dependent Saturation

Real tape saturates high frequencies BEFORE low frequencies. This is because:
1. Short wavelengths (high frequencies) have more self-demagnetization
2. The bias level is optimized for mid-range frequencies
3. Head losses accumulate more at HF

**Model**: Apply a pre-emphasis filter before saturation, then de-emphasis after:
```
Pre-emphasis: +6dB/octave shelf above ~2kHz (1st-order)
Saturation: tanh or J-A model
De-emphasis: -6dB/octave shelf above ~2kHz (inverse of pre-emphasis)
```

This makes highs hit the saturation curve harder, naturally compressing them more -- exactly what real tape does.

### 5.4 Tape Noise Floor

| Characteristic | Value |
|---------------|-------|
| Signal-to-noise ratio | ~50 dB (weighted), ~45 dB (unweighted) |
| Noise spectrum | Pink-ish, -3dB/octave slope above ~1kHz |
| Hiss level | Increases with each feedback pass |
| DC offset | Small residual from bias, removed by output coupling caps |

**For DSP**: Add low-level filtered noise (pink spectrum, approximately -50dB below full scale) that accumulates with each feedback pass. This is subtle but contributes to the "alive" quality of the repeats.

---

## 6. Mode Selector (12 Positions)

### 6.1 Complete Mode Table

The 12-position mode selector controls which playback heads and the reverb are active.

| Mode | Head 1 | Head 2 | Head 3 | Reverb | Rhythmic Character |
|------|--------|--------|--------|--------|--------------------|
| 1 | ON | - | - | - | Single slapback |
| 2 | - | ON | - | - | Quarter note echo |
| 3 | ON | ON | - | - | Dotted eighth feel |
| 4 | - | - | ON | - | Half note echo |
| 5 | ON | - | ON | - | Dotted quarter feel |
| 6 | - | ON | ON | - | 2-beat pattern |
| 7 | ON | ON | ON | - | Triple cascade |
| 8 | ON | - | - | ON | Slapback + reverb |
| 9 | - | ON | - | ON | Echo + reverb |
| 10 | ON | ON | - | ON | Double + reverb |
| 11 | - | - | ON | ON | Long echo + reverb |
| 12 | ON | ON | ON | ON | Full cascade + reverb |

**Note**: Mode 12 (all three heads + reverb) is the most iconic and most commonly used setting. The overlapping echoes with progressive darkening create a dense, ambient wash.

### 6.2 Reverb-Only

The RE-201 does NOT have a reverb-only mode. The reverb is always combined with at least one tape head in modes 8-12. However, by turning the Echo Volume to zero and Reverb Volume up in modes 8-12, you can effectively get reverb-only operation.

### 6.3 Head Mixing

When multiple heads are active, their outputs are summed with approximately equal gain. There is no level control per individual head (unlike the Strymon Volante). All active heads feed equally into the output mix and the feedback path.

---

## 7. Feedback Path (INTENSITY Control)

### 7.1 Circuit Topology

The INTENSITY knob controls the amount of signal fed from the playback output(s) back to the record amplifier input. It is a simple resistive pot in the feedback path.

```
Playback Sum ──► INTENSITY pot (50K linear) ──► Record Amp Input
                     │
                     └── Wiper: 0% = no feedback, 100% = unity+ feedback
```

### 7.2 Feedback Gain Range

| INTENSITY Setting | Feedback Gain | Behavior |
|-------------------|---------------|----------|
| 0% (minimum) | 0.0 | No repeats (single echo only) |
| ~30% | ~0.3 | Natural decay, 3-5 audible repeats |
| ~50% | ~0.5 | Many repeats, long decay |
| ~70% | ~0.7 | Very long decay, borderline infinite |
| ~80-85% | ~0.85 | Approximately infinite sustain |
| 90-100% | >0.9 | Self-oscillation (signal grows) |

### 7.3 Self-Oscillation Characteristics

At high INTENSITY settings, the feedback gain exceeds unity (after accounting for tape losses), and the system self-oscillates. This is a KEY creative feature of the RE-201:

- The oscillation frequency depends on the delay time (tape speed)
- Changing Repeat Rate during self-oscillation sweeps the oscillation pitch
- The tape saturation LIMITS the oscillation amplitude (natural soft-limiting)
- The HF loss in each pass means the oscillation settles to a warm, round tone
- Typical self-oscillation frequency: reciprocal of the shortest active head delay

**DSP critical point**: The self-oscillation behavior depends on the saturation model. With pure tanh, the oscillation will be a clean sine-ish tone. Real tape self-oscillation has more harmonic content due to the hysteresis/asymmetry of the saturation curve.

### 7.4 Feedback Filtering

The feedback path does NOT have an explicit filter. However, the cumulative effect of passing through the record/playback cycle acts as a low-pass filter per iteration (see Section 4.5). This is why RE-201 self-oscillation produces a warm tone rather than a harsh one.

---

## 8. Preamp and Tone Circuits

### 8.1 Input Preamp

The RE-201 has TWO input channels:

**Instrument Input:**
- Input impedance: ~470K ohm (suitable for guitar/bass/keys)
- Connector: 1/4" phone jack
- Volume control: INSTRUMENT VOLUME pot (100K audio taper)
- Gain: approximately 20dB at maximum volume

**Microphone Input:**
- Input impedance: ~10K ohm (low impedance, for dynamic mics)
- Connector: 1/4" phone jack (NOT XLR)
- Volume control: MIC VOLUME pot (50K audio taper)
- Gain: approximately 40dB at maximum volume (higher gain for low-output mics)

**Preamp transistors**: 2SC828 (NPN silicon, common in Roland designs of this era)
- hFE: 200-700 (high-gain, selected for low noise)
- Topology: Common-emitter with emitter degeneration
- Coupling: capacitor-coupled between stages
- Operating point: Vce ~6-8V, Ic ~0.5-1.0mA

The 2SC828 is a small-signal NPN silicon transistor, equivalent to modern 2N3904/BC547. It was Roland's workhorse transistor in the 1970s.

### 8.2 Tone Controls

The RE-201 has **BASS** and **TREBLE** controls in the signal path between the input preamp and the record amplifier.

**Topology**: Baxandall-type active tone control, implemented with a transistor gain stage rather than an op-amp.

**Component values (approximate from service manual schematic):**

```
Bass control:
  Shelving frequency: ~300-400 Hz
  Range: approximately +/-12 dB
  Pot: 100K linear taper
  Cap: 47nF (sets bass shelving frequency)

Treble control:
  Shelving frequency: ~2-3 kHz
  Range: approximately +/-12 dB
  Pot: 100K linear taper
  Cap: 4.7nF (sets treble shelving frequency)
```

**Flat position**: Both knobs at center (12 o'clock) gives approximately flat response through the tone stack. The tone controls affect BOTH the direct signal and the echo signal.

**DSP implementation**: Standard Baxandall EQ using biquad shelving filters:
```
Bass: Low shelf at 350 Hz, +/-12dB, Q = 0.7
Treble: High shelf at 2.5 kHz, +/-12dB, Q = 0.7
```

### 8.3 Preamp Coloration

Even with tone controls flat, the RE-201 preamp adds coloration:
- Slight mid-range emphasis around 1-2 kHz from inter-stage coupling caps
- Soft compression at high input levels (transistor saturation)
- The overall tonal signature is "warm but present" -- not dark, not bright
- Input coupling caps (typically 100nF-470nF) create a gentle HPF at ~30-50 Hz

### 8.4 Output Stage

- Output impedance: approximately 600 ohm (line level)
- Output level selector switch: HIGH (-10dBu), MID (-20dBu), LOW (-30dBu)
- Maximum output level: approximately +4 dBu before clipping
- Output coupling cap: blocks DC offset from tape system

---

## 9. Spring Reverb

### 9.1 Tank Specifications

The RE-201 uses an **Accutronics Type 4** (or equivalent) spring reverb tank.

| Parameter | Value |
|-----------|-------|
| Tank type | Accutronics Type 4 (long tank, 2-spring) |
| Number of springs | 2 (parallel, different lengths for density) |
| Reverb time | Approximately 2.5-3.5 seconds (frequency dependent) |
| Input impedance | ~10 ohm (current-driven) |
| Output impedance | ~2200 ohm |
| Tank dimensions | Approximately 40cm x 5cm x 3cm |
| Spring material | Steel music wire |
| Mounting | Suspended on foam or rubber grommets inside chassis |

### 9.2 Spring Reverb Signal Flow

The reverb is independent of the tape echo in terms of signal source:

```
Input Preamp ──► Record Amp (feeds tape)
     │
     └──► Spring Reverb Driver ──► Tank ──► Reverb Recovery Amp
                                                    │
                                                    ▼
                                            REVERB VOLUME pot
                                                    │
                                                    ▼
                                              Output Mixer
```

**Key point**: The reverb is fed from the input preamp, NOT from the tape playback. This means:
- Reverb is applied to the dry signal
- Echo repeats do NOT go through the reverb (unless they feed back to the input via the feedback path)
- Reverb and echo are parallel effects, not series

### 9.3 Reverb Driver Circuit

The spring reverb driver uses a transistor (2SC828) to convert the voltage signal to current for driving the low-impedance tank input transducer:

```
Preamp Output ──► Driver Transistor (CE) ──► Coupling Transformer ──► Tank Input
                      │
                      R_emitter ~= 100 ohm
                      Gain ~= 10-15x
```

### 9.4 Reverb Recovery

The output from the spring tank is very low level (~-40dBV) and requires a high-gain recovery amplifier:
- 2-stage transistor amplifier
- Total gain: approximately 40-50 dB
- Bandwidth limited to approximately 100 Hz - 5 kHz (natural tank response)

### 9.5 DSP Implementation

Our project already has a spring reverb algorithm (see spring-reverb-research.md). The RE-201's spring reverb should use:
- 2-spring configuration (not 3)
- Chirp allpass dispersion chain (12 1st-order AP per spring)
- Tank frequency response: HPF ~100Hz, LPF ~4kHz
- Reverb time: ~3 seconds
- Pre-delay: ~30ms (spring travel time)

---

## 10. Record Amplifier and Bias Oscillator

### 10.1 Record Amplifier

The record amplifier boosts the signal to the level required to magnetize the tape:

```
Tone Output + Feedback Return ──► Summing Resistors ──► Record Amp ──► Record Head
                                                              │
                                                    Bias Oscillator ──┘ (summed)
```

- Transistor: 2SC828 (or 2SC945 in later revisions)
- Gain: approximately 20-30 dB
- Bandwidth: limited to approximately 15kHz by circuit design
- The record amp also sums the feedback signal with the input signal

### 10.2 Bias Oscillator

- Frequency: ~55-65 kHz
- Waveform: approximately sinusoidal (generated by LC oscillator)
- Transistor: 2SC828
- Bias level: factory-adjusted trimmer potentiometer
- The bias signal is mixed with the audio signal at the record head

**For DSP**: We do not need to model the bias oscillator directly (it's supersonic). Its effect on the audio is captured by the tape saturation/frequency response model.

### 10.3 Erase Head

- DC erase (simpler than AC erase)
- Located before the record head in the tape path
- Driven by a DC current source
- Effectiveness limited -- contributes to the "ghosting" effect where previous recordings are not fully erased, especially at higher frequencies

**For DSP**: Model this as incomplete erasure at each pass -- add a very small amount of the previous signal back (approximately -30 to -40dB below the new signal). This subtle ghosting adds to the RE-201's characteristic "living" quality.

---

## 11. Playback Amplifiers

### 11.1 Circuit Design

Each of the three playback heads has its own dedicated preamp:

```
Playback Head ──► Input Coupling Cap (100nF) ──► CE Stage 1 ──► CE Stage 2 ──► Output
                                                   2SC828          2SC828
                                                   Gain ~25dB      Gain ~20dB
```

**Per-channel playback preamp:**
- 2-stage common-emitter amplifier
- Total gain: approximately 40-45 dB (to recover very low head output level)
- Input impedance: matched to head inductance (typically 50-200 ohm)
- Bandwidth: ~80 Hz to ~6 kHz (-3dB points)
- Equalization: playback EQ curve (de-emphasis) compensates for record pre-emphasis

### 11.2 Playback RIAA-like Equalization

The RE-201 does NOT use standard RIAA equalization (that's for vinyl). It uses a proprietary equalization curve:
- Bass boost below ~300 Hz (compensates for head-to-tape spacing loss at LF)
- Treble rolloff above ~3 kHz (compensates for record pre-emphasis)
- Overall: approximately flat from 100 Hz to 3 kHz, rolling off above and below

### 11.3 Head Bump

Each playback head exhibits a "head bump" -- a resonant peak in the low-mid frequency range caused by the interaction between head geometry and tape wavelength:
- Head bump frequency: approximately 80-150 Hz (depends on head gap and tape speed)
- Head bump amplitude: +2 to +4 dB
- This bump gives the RE-201 a slight low-mid warmth/thickness

**DSP model**: Peaking EQ filter at ~100 Hz, +3dB, Q = 1.5, scaled with tape speed:
```
f_bump = 100 * (tape_speed / nominal_speed) Hz
```

---

## 12. Complete Component Reference

### 12.1 Transistors Used

| Designation | Type | Package | Role |
|------------|------|---------|------|
| 2SC828 | NPN Si | TO-92 | Input preamp, tone stage, playback preamps, record amp, bias oscillator, motor control |
| 2SC945 | NPN Si | TO-92 | Used in later production runs as 2SC828 replacement |
| 2SA564 | PNP Si | TO-92 | Complementary stages in output/mixing |

The RE-201 is almost entirely built from 2SC828 transistors -- approximately 15-20 total in the signal path.

### 12.2 Key Passive Components

**Coupling capacitors (inter-stage):**
- Input coupling: 100nF-470nF (electrolytic or film)
- Inter-stage: 10nF-100nF (ceramic or film)
- Output coupling: 10uF-47uF electrolytic

**Bias resistors:**
- Collector loads: 10K-47K typical
- Emitter degeneration: 1K-4.7K typical
- Base bias: 100K-470K typical

**Tone stack (approximate):**
- Bass cap: 47nF
- Treble cap: 4.7nF
- Bass pot: 100K linear
- Treble pot: 100K linear
- Series resistors: 10K-47K

### 12.3 Power Supply

- Input: AC mains (100V/117V/220-240V versions)
- Transformer: Step-down to ~18-24V AC
- Rectification: Bridge rectifier
- Regulation: Zener diode + transistor series regulator
- DC rails: +12V (motor, logic), +24V (preamp stages), -12V (bias)
- Ripple: Capacitor-filtered, not particularly well-regulated (contributes to subtle hum)

---

## 13. Published Specifications (from Roland Service Notes)

| Parameter | Specification |
|-----------|--------------|
| Echo effect | 3 heads, tape loop system |
| Reverb | Spring reverb (2-spring tank) |
| Mode selector | 12 positions |
| Repeat rate | Variable (motor speed control) |
| Instrument input impedance | 470K ohm |
| Mic input impedance | 10K ohm |
| Output impedance | 600 ohm |
| Frequency response (direct) | 40 Hz - 12 kHz (+/-3dB) |
| Frequency response (echo) | 80 Hz - 6 kHz (+/-3dB) |
| Signal-to-noise ratio | >50 dB (direct), >40 dB (echo, single pass) |
| Wow and flutter | <0.35% WRMS |
| Distortion (direct) | <1% THD at rated output |
| Distortion (echo) | <3% THD (intentional -- it's the character) |
| Power consumption | Approximately 25W |
| Dimensions | 400 x 122 x 255 mm (W x H x D) |
| Weight | 7.5 kg (16.5 lbs) |

---

## 14. DSP Implementation Strategy

### 14.1 Architecture Overview

```
Input ──► Preamp Model ──► Tone Stack ──► Saturation ──► Delay Network ──► Output Mixer
                                               │              │     │         │
                                               │         ┌────┘     │    Spring Reverb
                                               │         │          │         │
                                               │    Feedback LP ◄───┘    Reverb Mix
                                               │         │                    │
                                               └─────────┘               Output
```

### 14.2 Delay Line Implementation

**Core delay**: Three-tap variable delay line with interpolation.

```
Total delay buffer size: ceil(max_delay_samples) + margin
  At 48kHz, Head 3 max delay ~= 1579ms = 75,792 samples
  Buffer size: 76,800 samples (with margin) = 300KB per channel (mono)

Interpolation: 3rd-order Lagrange or allpass
  - Linear interpolation introduces LPF coloration at modulation rates
  - Cubic Lagrange: better frequency response, 4 multiplies per tap
  - Allpass interpolation: flat magnitude response but phase distortion (acceptable for echo)

RECOMMENDATION: Allpass interpolation for best tape-like behavior
  y[n] = (1-frac) * x[n-int] + x[n-int-1] - (1-frac) * y[n-1]
  where frac = fractional delay sample, int = integer delay samples
```

**Three taps with fixed ratio:**
```
base_delay = repeat_rate_param * max_delay_samples  // from Repeat Rate knob
head1_delay = base_delay * (1.0 / 3.0)              // ratio 1
head2_delay = base_delay * (2.0 / 3.0)              // ratio 2
head3_delay = base_delay                             // ratio 3
```

### 14.3 Wow and Flutter Implementation

```cpp
// Per-sample wow/flutter modulation
struct WowFlutter {
    float phase1 = 0.0f; // primary wow
    float phase2 = 0.0f; // secondary wow
    float phase3 = 0.0f; // primary flutter
    float phase4 = 0.0f; // secondary flutter

    // Slowly drifting frequencies
    float freq1 = 1.0f;   // Hz, drifts 0.7-1.3
    float freq2 = 3.0f;   // Hz, drifts 2.5-3.5
    float freq3 = 10.0f;  // Hz, drifts 8-12
    float freq4 = 20.0f;  // Hz, drifts 18-22

    // Depth control (0 = pristine, 1 = worn/aged)
    float depth = 0.5f;

    float process(float sampleRate) {
        // Advance phases
        phase1 += freq1 / sampleRate;
        phase2 += freq2 / sampleRate;
        phase3 += freq3 / sampleRate;
        phase4 += freq4 / sampleRate;
        // Wrap phases
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        if (phase3 >= 1.0f) phase3 -= 1.0f;
        if (phase4 >= 1.0f) phase4 -= 1.0f;

        // Sum components (amplitude scaled by depth)
        float wow = depth * (
            0.0010f * fast_sin(phase1) +  // primary wow
            0.0005f * fast_sin(phase2)     // secondary wow
        );
        float flutter = depth * (
            0.0004f * fast_sin(phase3) +   // primary flutter
            0.0001f * fast_sin(phase4)      // secondary flutter
        );

        // Add filtered noise component
        float noise = depth * 0.00015f * bandpassNoise();

        return wow + flutter + noise;
    }
};

// Modulate delay read position:
float mod = wowFlutter.process(sampleRate);
float modulated_delay = base_delay * (1.0f + mod);
// Read from delay line at modulated position
```

**Key insight**: The modulation is applied to the TAPE SPEED, not to individual head delays. Since all heads read from the same tape, they all experience the same speed modulation. In the delay line, this means modulating the write/read rate proportionally.

### 14.4 Tape Saturation in the Feedback Loop

```
Signal flow per sample:
  1. input_sample = preamp(input) + feedback * intensity
  2. saturated = tapeSaturate(input_sample, drive)
  3. Write saturated to delay line
  4. Read from delay line at 3 head positions (with wow/flutter modulation)
  5. Apply per-head LP filter (models head frequency response)
  6. Sum active heads (based on mode)
  7. feedback = head_sum
  8. output = head_sum * echoVolume + reverbOutput * reverbVolume
```

The saturation being INSIDE the feedback loop is critical. Each recirculation adds harmonic content from saturation + loses HF from the head filters. The balance between these determines the character of the repeats.

### 14.5 Per-Head Feedback Loop Filter

```cpp
// 1-pole LPF modeling tape/head HF loss
// Cutoff varies with tape speed
struct TapeLoopFilter {
    double state = 0.0;
    float cutoff = 5000.0f; // Hz, nominal

    void updateCoeffs(float sampleRate, float speedRatio) {
        float fc = cutoff * speedRatio; // HF cutoff scales with speed
        float wc = 2.0f * M_PI * fc / sampleRate;
        coeff = wc / (1.0f + wc); // 1-pole LPF coefficient
    }

    float process(float x) {
        state += coeff * (x - state);
        return (float)state;
    }

    float coeff = 0.0f;
};

// Also: gentle HPF at ~60-80Hz for bass rolloff
struct TapeHPF {
    double state = 0.0;
    float coeff = 0.995; // ~60Hz at 48kHz

    float process(float x) {
        state = coeff * (state + x - prev);
        prev = x;
        return (float)state;
    }
    float prev = 0.0f;
};
```

### 14.6 Head Bump Filter

```cpp
// Peaking EQ at ~100Hz, +3dB, scaled with tape speed
// Use biquad peaking filter
void calcHeadBump(float sampleRate, float speedRatio) {
    float freq = 100.0f * speedRatio;
    float gain_dB = 3.0f;
    float Q = 1.5f;
    // Standard biquad peaking EQ coefficients
    calcPeakingEQ(freq, gain_dB, Q, sampleRate);
}
```

### 14.7 Mode Selector Implementation

```cpp
// Mode configuration: which heads are active, reverb on/off
struct ModeConfig {
    bool head1;
    bool head2;
    bool head3;
    bool reverb;
};

static const ModeConfig modes[12] = {
    {true,  false, false, false},  // Mode 1:  H1
    {false, true,  false, false},  // Mode 2:  H2
    {true,  true,  false, false},  // Mode 3:  H1+H2
    {false, false, true,  false},  // Mode 4:  H3
    {true,  false, true,  false},  // Mode 5:  H1+H3
    {false, true,  true,  false},  // Mode 6:  H2+H3
    {true,  true,  true,  false},  // Mode 7:  H1+H2+H3
    {true,  false, false, true},   // Mode 8:  H1+Rev
    {false, true,  false, true},   // Mode 9:  H2+Rev
    {true,  true,  false, true},   // Mode 10: H1+H2+Rev
    {false, false, true,  true},   // Mode 11: H3+Rev
    {true,  true,  true,  true},   // Mode 12: H1+H2+H3+Rev
};
```

### 14.8 Spring Reverb Integration

The spring reverb runs in parallel, fed from the preamp output:
```
reverb_input = preamp_output  // NOT from tape playback
reverb_output = springReverb.process(reverb_input)
final_output = echo_mix * echoVolume + reverb_output * reverbVolume + dry * directLevel
```

### 14.9 Parameter Mapping

| RE-201 Control | Parameter Range | DSP Mapping |
|---------------|----------------|-------------|
| REPEAT RATE | 0.0-1.0 | Tape speed: 9.5-38 cm/s, maps to delay time ratio |
| INTENSITY | 0.0-1.0 | Feedback gain: 0.0-1.05 (allows self-oscillation) |
| ECHO VOLUME | 0.0-1.0 | Wet level: 0.0-1.0 linear |
| REVERB VOLUME | 0.0-1.0 | Reverb level: 0.0-1.0 linear |
| BASS | 0.0-1.0 | Low shelf at 350Hz: -12 to +12 dB |
| TREBLE | 0.0-1.0 | High shelf at 2.5kHz: -12 to +12 dB |
| MODE | 1-12 | Integer selector, maps to head/reverb configuration |
| WOW/FLUTTER | 0.0-1.0 | Modulation depth (0 = off/pristine, 1 = very worn) |
| SATURATION | 0.0-1.0 | Tape drive level (0 = clean, 1 = heavy saturation) |
| CONDITION | 0.0-1.0 | Tape age: HF rolloff, noise floor, wow increase |

### 14.10 CPU Budget Estimation

| Component | Operations per Sample | Notes |
|-----------|----------------------|-------|
| Preamp (tanh saturation) | ~15 FLOPS | 1 tanh + gain |
| Tone stack (2 biquads) | ~20 FLOPS | Bass + treble shelving |
| Tape saturation | ~15 FLOPS | tanh in feedback loop |
| Delay line (3 taps, allpass interp) | ~30 FLOPS | 3x (4 mul + 2 add) |
| Wow/flutter (4 sines + noise) | ~25 FLOPS | 4 fast_sin + noise filter |
| Feedback LP filter | ~5 FLOPS | 1-pole LPF |
| Feedback HP filter | ~5 FLOPS | 1-pole HPF |
| Head bump (biquad) | ~10 FLOPS | Peaking EQ |
| Mode mixing | ~10 FLOPS | Conditional sums |
| Spring reverb | ~185 FLOPS | 2-spring chirp AP chain |
| Noise generation | ~10 FLOPS | Pink noise + scaling |
| **TOTAL** | **~330 FLOPS/sample** | |

This is comparable to our Dattorro reverb (~280 FLOPS) and very achievable on mobile. No oversampling needed for the delay line -- only the saturation stage benefits from 2x oversampling (if used, adds ~30 FLOPS for the tanh + up/down filtering).

**Memory**: ~300KB for delay buffer (mono) + ~16KB for spring reverb delay lines = ~316KB total.

---

## 15. Sonic Fingerprint: What Makes the RE-201 Sound Like an RE-201

### 15.1 The Five Pillars of the RE-201 Sound

1. **Progressive darkening of repeats**: Each echo is warmer/darker than the previous one. This is the #1 most important characteristic. The 1-pole LPF in the feedback loop at ~5kHz creates this naturally.

2. **Tape saturation warmth**: The soft compression and harmonic enrichment from tape saturation. Most noticeable on transients (drums, picked guitar notes). Adds body without harsh clipping.

3. **Wow and flutter modulation**: The slight pitch wavering gives the repeats a "chorus-like" quality that prevents them from sounding static or digital. Even at low depths, this adds life.

4. **Rhythmic multi-head patterns**: The 1:2:3 head spacing ratio creates musically useful rhythmic relationships. Mode 7 (all three heads) produces a cascading triplet-feel pattern.

5. **Spring reverb integration**: The reverb adds a metallic, dimensional quality that fills the space between echoes. It's independent from the echo path, so it retains brightness even as echoes darken.

### 15.2 Common Emulation Failures

| Problem | Cause | Fix |
|---------|-------|-----|
| Echoes sound too bright/harsh | No HF loss in feedback loop | Add 1-pole LPF at ~5kHz in feedback |
| Echoes sound static/digital | No wow/flutter modulation | Add multi-component speed modulation |
| Self-oscillation too clean | No saturation in feedback path | Add tanh/soft-clip inside feedback loop |
| Echoes don't "sit in mix" | Not enough HF loss per pass | Ensure LPF is INSIDE the feedback loop |
| Reverb sounds wrong | Reverb placed after echo | Reverb should be parallel, fed from input |
| All heads sound the same | Same filter on all heads | Each head is at different distance, slight response variation |
| Repeats build up too fast | Linear feedback scaling | Apply gentle compression in feedback path |

### 15.3 A/B Listening Test Checklist

When comparing against a real RE-201:
1. Single echo (Mode 1): Check warmth, transient character, HF rolloff
2. Multiple echoes (Mode 7): Check progressive darkening, rhythmic pattern
3. High intensity: Check self-oscillation tone, feedback stability
4. Vary Repeat Rate during self-oscillation: Check pitch sweep smoothness
5. Reverb only (Mode 8, echo vol 0): Check spring character, metallic quality
6. Full mix (Mode 12): Check overall density, how echoes and reverb interact
7. Staccato input: Check transient smearing, subtle pitch modulation
8. Sustained note: Check feedback buildup, saturation character

---

## 16. References and Cross-References

### Academic Papers
- Chowdhury, J. (2019). "Real-Time Physical Modelling For Analog Tape Machines." DAFx-2019. (Jiles-Atherton hysteresis, tape loss modeling)
- Zavalishin, V. & Parker, J. (2018). "Efficient emulation of tape-like delay modulation behavior." DAFx-2018. (Wow/flutter, variable delay)
- Mikkonen et al. (2023). "Neural Modeling of Magnetic Tape Recorders." DAFx-2023. (Neural approach to tape emulation)

### Software References
- Chowdhury, J. AnalogTapeModel (ChowTapeModel). GitHub. GPL-3.0. (Reference implementation: hysteresis, loss filters, wow/flutter)
- Boss RE-2 / RE-202 specifications (Roland/Boss official). (12-mode selector, tape modeling approach)
- Strymon Volante (head spacing, mechanics, wear controls as reference)

### Related Project Files
- `spring-reverb-research.md` -- Spring reverb algorithm (reusable for RE-201 reverb section)
- `pedal-accuracy-audit.md` -- Existing effect accuracy standards
- `signal-chain-audit.md` -- DC blocker and soft limiter integration points

---

## 17. Implementation Phases

### Phase 1: Core Tape Echo (MVP)
- 3-tap delay line with allpass interpolation
- Mode selector (12 modes)
- Feedback path with 1-pole LPF
- Repeat Rate (scales all delays proportionally)
- Intensity (feedback gain)
- Echo Volume

### Phase 2: Tape Character
- Tape saturation (tanh in feedback loop)
- Wow and flutter (4-component sinusoidal + noise)
- Head frequency response (LPF + HPF per head)
- Head bump EQ

### Phase 3: Full Feature Set
- Preamp model (input gain + soft saturation)
- Bass/Treble tone stack
- Spring reverb (from existing algorithm)
- Reverb Volume
- Tape condition/age parameter (HF, noise, wow depth)

### Phase 4: Polish
- Tape splice transient (optional, subtle)
- Erase ghosting (optional, very subtle)
- Pink noise floor accumulation
- Self-oscillation tuning and stability
- Parameter smoothing for all controls
- Presets: dub reggae wash, slapback rockabilly, ambient swells, rhythmic patterns
