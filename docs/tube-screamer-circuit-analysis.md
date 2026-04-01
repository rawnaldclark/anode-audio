# Ibanez TS-808 / TS9 Tube Screamer: Complete Circuit Analysis for DSP Emulation

**Date**: 2026-03-30
**Purpose**: Component-level schematic reference for 1:1 digital emulation in C++
**Sources**: Electrosmash, R.G. Keen (GEOfex), Analogman, Yeh 2009 CCRMA thesis, original Maxon schematics, multiple cross-references

---

## Part 1: Complete Signal Flow (Input Jack to Output Jack)

```
Input Jack
    |
[R_input = 510K to VBIAS] ---- Sets input impedance
    |
[C1 = 0.022uF] ---- Input coupling / DC blocking
    |
[Q1: 2SC1815GR] ---- NPN emitter follower (input buffer)
    R_bias = 510K to VBIAS (4.5V)
    R_emitter = 10K to ground
    |
[C2 = 1uF electrolytic] ---- Coupling to clipping stage (blocks DC)
    |
[IC1a: JRC4558D pin 3 (+)] ---- Non-inverting clipping amplifier
    |                              (THE HEART OF THE CIRCUIT)
    |--- Feedback network (pin 1 output to pin 2 (-)):
    |    |
    |    +-- [D1 + D2: 1N914 anti-parallel] -- Soft clipping diodes
    |    |   [C4 = 51pF across diodes] -- Treble rolloff / corner softening
    |    |
    |    +-- [R6 = 51K + VR1 = 500K Drive pot] in series
    |
    |--- From pin 2 (-) to AC ground:
    |    [R4 = 4.7K] in series with [C3 = 0.047uF]
    |
[C5 = 0.22uF] ---- Coupling to tone stage
    |
[Passive Tone Control Network]
    |--- [R7 = 1K] ---- Mixing resistor (signal always passes through this)
    |--- [C6 = 0.22uF] in series with [R8 = 220 ohm] to ground
    |    VR2 = 20K Tone pot wiper taps between R7 output and C6/R8 junction
    |
[IC1b: JRC4558D] ---- Unity gain buffer (second op-amp section)
    pin 5 (+) from tone network
    pin 7 output to pin 6 (-) (100% negative feedback = gain of 1)
    |
[C7 = 1uF] ---- Output coupling
    |
[Q2: 2SC1815GR] ---- NPN emitter follower (output buffer)
    R_bias = 510K to VBIAS
    R_emitter = 10K to ground
    |
[VR3 = 100K Volume pot (audio taper)]
    |
[Output Network] ---- ***THIS IS WHERE TS-808 AND TS9 DIFFER***
    |
Output Jack
```

---

## Part 2: Input Buffer Stage

### Transistor: Q1 -- 2SC1815GR (Toshiba NPN silicon)
- **hFE (current gain)**: 200-700 (GR rank = 200-400 typical)
- **Ic max**: 150mA
- **Vceo**: 50V
- **fT**: 80 MHz
- **Pinout**: ECB (Emitter-Collector-Base) -- Japanese TO-92 pinout

### Biasing Network
| Component | Value | Function |
|-----------|-------|----------|
| R2 (base bias) | 510K | Sets base voltage to VBIAS (4.5V) via voltage divider |
| R3 (emitter) | 10K | Sets quiescent current: Ic ~ (4.5V - 0.6V) / 10K = 0.39mA |
| C1 (input coupling) | 0.022uF (22nF) | DC blocking, HP corner with R_input at ~14Hz |

### Input Impedance Calculation
The input impedance seen by the guitar is:
```
Z_in = R2 || (hFE * R3)
     = 510K || (300 * 10K)     [using typical hFE = 300]
     = 510K || 3M
     = (510K * 3M) / (510K + 3M)
     = 436K ohms
```

The 510K bias resistor dominates. This is adequate for passive pickups (~100-500K source impedance from volume pot) but marginal -- it loads the guitar's tone pot somewhat. This is why some players notice slight treble loss with the TS in bypass when using the electronic (JFET) bypass switching.

### Why BJT, Not JFET?
The original Maxon engineers (Susumu Tamura designed the circuit, manufactured by Nisshin Onpa for Ibanez/Maxon) chose BJTs for cost reasons in 1979. The 2SC1815 cost approximately 5 yen vs 30+ yen for a suitable JFET. The emitter follower topology provides:
- Voltage gain: ~0.98 (effectively unity, slight loss from Vbe drop)
- Current gain: hFE (300x) -- drives the low-impedance clipping stage input
- Low output impedance: R_emitter / hFE ~ 33 ohms

Modern clones (e.g., Analogman) sometimes substitute 2N5089 or MPSA18 (hFE 500-900) for slightly higher input impedance.

### Input Coupling High-Pass
```
f_HP = 1 / (2 * pi * R_in * C1)
     = 1 / (2 * pi * 436K * 22nF)
     = 16.6 Hz
```
This removes DC and subsonic content. Well below the guitar's lowest fundamental (E2 = 82.4Hz, drop D = 73.4Hz).

---

## Part 3: Clipping Amplifier Stage (The Heart of the Tube Screamer)

This is the most important stage and the most complex to model digitally. It is a **non-inverting op-amp configuration with diodes in the negative feedback loop**.

### Op-Amp: JRC4558D (New Japan Radio Company)

| Specification | JRC4558D | TA75558P (Toshiba) | Notes |
|--------------|----------|---------------------|-------|
| Slew Rate | 1.0 V/us | 1.0 V/us | Identical |
| GBW Product | 3 MHz | 2.5 MHz | Slight difference |
| Input Bias Current | 50nA | 200nA | JRC better |
| Input Offset | 0.5mV | 2mV | JRC better |
| Output Swing (9V supply) | +/-3.5V | +/-3.5V | Similar |
| Noise (1kHz) | 8 nV/rtHz | 10 nV/rtHz | Marginal |

**Sonic difference**: The JRC4558D has slightly lower noise and a smoother open-loop rolloff, which affects the transition from clean to clipped. At the gain levels and frequencies in the TS circuit (gain 12-118x, bandwidth limited to ~720Hz-5.6kHz), the practical sonic difference is **extremely small**. The 4558D has become mythologized far beyond its actual contribution. The diodes, passive component values, and circuit topology matter orders of magnitude more than the op-amp choice.

**Why it matters at all**: The 4558D's internal compensation and slew rate determine how the op-amp recovers from clipping asymmetry when the diodes are conducting heavily. A faster op-amp (like TL072) sounds slightly brighter and more aggressive because it follows the diode transitions more accurately. The 4558D's 1V/us slew rate adds a tiny amount of additional smoothing at high frequencies under heavy clipping -- but this is a second-order effect compared to the RAT's LM308 (0.3V/us) where slew rate is THE defining character.

### Complete Feedback Network

```
                    +--[D1: 1N914]--+--[D2: 1N914]--+
                    |   (anode->)   |   (<-anode)    |
                    |               |                |
IC1a pin 1 (out) --+--[C4: 51pF]---+                +-- IC1a pin 2 (-)
                    |                                |
                    +--[R6: 51K]--[VR1: 500K Drive]--+
```

From pin 2 (-) to AC ground (VBIAS):
```
IC1a pin 2 (-) --[R4: 4.7K]--[C3: 0.047uF]-- VBIAS (AC ground)
```

### Component Values (Clipping Stage BOM)

| Ref | Value | Tolerance | Function |
|-----|-------|-----------|----------|
| R4 | 4.7K ohm | 5% carbon film | Sets minimum gain (with R6) and bass HP corner |
| R5 | 10K ohm | 5% | Bias resistor to VBIAS (sets DC operating point) |
| R6 | 51K ohm | 5% | Fixed series resistance in gain path (minimum drive) |
| VR1 | 500K ohm | Log (audio) taper | Drive control -- variable feedback resistance |
| C2 | 1uF electrolytic | 20% | Input coupling to op-amp (blocks DC from buffer) |
| C3 | 0.047uF (47nF) | 10% polyester | **Bass rolloff cap** -- THE mid-hump component |
| C4 | 51pF | 5% ceramic/silver mica | Treble rolloff across diodes |
| D1 | 1N914 (or MA150/1N4148) | - | Forward clipping diode |
| D2 | 1N914 (or MA150/1N4148) | - | Reverse clipping diode |

### Gain Equation (Frequency-Dependent)

For a non-inverting op-amp:
```
G(f) = 1 + Zf(f) / Zi(f)
```

Where:
- **Zi(f)** = impedance from pin 2 to AC ground = R4 + 1/(j*2*pi*f*C3) = R4 + 1/(jwC3)
- **Zf(f)** = impedance of the feedback network (diodes || C4 || (R6 + VR1))

#### Below Clipping Threshold (Small Signal, Diodes OFF)

When the signal is small enough that the diodes don't conduct (below ~0.5V at the op-amp output), the feedback impedance is purely:
```
Zf = (R6 + VR1) || (1/(jwC4))
```

At audio frequencies (100Hz-10kHz), C4=51pF has very high impedance (>>R6+VR1):
```
Xc4 @ 1kHz = 1/(2*pi*1000*51e-12) = 3.12M ohm
```
So C4 is effectively open circuit at audio frequencies when diodes are OFF.

Therefore small-signal gain simplifies to:
```
G_small(f) = 1 + (R6 + VR1) / (R4 + 1/(jwC3))
```

**At high frequencies** (f >> 720Hz, C3 is short circuit):
```
G_max = 1 + (R6 + VR1) / R4
```
- **Drive = 0 (VR1 = 0)**: G = 1 + 51K/4.7K = **11.85 (21.5 dB)**
- **Drive = max (VR1 = 500K)**: G = 1 + 551K/4.7K = **118.2 (41.5 dB)**

**At low frequencies** (f << 720Hz, C3 impedance dominates):
```
G_low -> 1 + (R6 + VR1) / (large impedance) -> approaches 1 (0 dB)
```

This is THE source of the Tube Screamer's famous mid-hump: bass frequencies get **no gain** and therefore **no clipping**. Only mid and treble frequencies are amplified into the diodes.

### Bass Rolloff Frequency (The Mid-Hump Corner)

The -3dB point of the bass rolloff is set by R4 and C3:
```
f_bass = 1 / (2 * pi * R4 * C3)
       = 1 / (2 * pi * 4700 * 0.047e-6)
       = 1 / (2 * pi * 220.9e-6)
       = 720.5 Hz
```

**f_bass = 720 Hz**

This means:
- At 720Hz, gain is 3dB below maximum
- At 360Hz (1 octave below), gain is ~6dB below maximum
- At 72Hz (low E fundamental), gain is ~20dB below maximum
- Bass frequencies pass through essentially clean (gain ~1)

This is why the Tube Screamer "tightens up" the low end when used as a boost into an already-overdriven amp -- it removes the muddy bass that would otherwise cause flubby distortion in the power amp.

### Treble Rolloff Frequency (C4 = 51pF)

C4 across the diodes creates a treble rolloff whose corner frequency depends on the effective resistance it sees. When the diodes are conducting, their dynamic resistance is very low (10-50 ohms depending on current), so C4 sees a low impedance in parallel. The relevant treble rolloff occurs in the feedback path:

**With diodes OFF (small signal)**:
```
f_treble_off = 1 / (2 * pi * (R6 + VR1) * C4)
```
- Drive min: f = 1/(2*pi*51K*51pF) = **61.2 kHz** (inaudible, no effect)
- Drive max: f = 1/(2*pi*551K*51pF) = **5.66 kHz** (audible treble rolloff)

**With diodes ON (clipping)**:
When the diodes conduct, they present a dynamic resistance of approximately:
```
r_d = nVt / Id  (where n~1.7 for Si signal diodes, Vt=26mV)
```
At moderate clipping currents (Id ~ 0.1-1mA):
```
r_d ~ 1.7 * 26mV / 0.5mA = 88 ohms
```
So C4 sees approximately:
```
f_treble_on = 1 / (2 * pi * r_d * C4)
            = 1 / (2 * pi * 88 * 51e-12)
            = 35.5 MHz  (way above audio -- C4 has minimal effect here)
```

**The key insight**: C4's primary audible effect is at high drive settings with small signals (diodes not yet conducting). It rolls off treble in the gain path, softening the transition INTO clipping. Once the diodes conduct, C4's impedance is swamped by the diodes' low dynamic resistance. This is why C4 "softens the corners" -- it rounds the waveform just before and after the clipping threshold.

### Diodes in Feedback Loop: Why This Sounds Different from Diodes-to-Ground

This is the most critical design distinction in the entire pedal world.

**Tube Screamer (diodes in feedback):**
```
Vout = Vin * G     when |Vout| < Vf (diodes off, full gain)
Vout = +-Vf        when |Vout| >= Vf (diodes on, gain collapses)
```

But the transition is NOT abrupt. When the diodes begin conducting, they present a decreasing impedance in parallel with the gain resistors. The gain smoothly decreases from G_max toward 1 as the signal increases. This creates **soft, rounded clipping** with a gradual compression characteristic.

The gain during clipping is approximately:
```
G_clip(Vout) = 1 + (Zf_resistive || r_d(Vout)) / Zi
```
Where r_d is the diode's signal-dependent dynamic resistance.

**Boss DS-1 / RAT (diodes to ground, hard clipping):**
```
Vout_preClip = Vin * G_fixed    (full gain always applied)
Vout_final = clamp(Vout_preClip, -Vf, +Vf)   (abrupt limiting)
```

The op-amp sees full gain always. The diodes then hard-limit the output. This creates **sharp, angular clipping** with more odd-order harmonics and a more aggressive, buzzy character.

**In the TS topology, the op-amp's negative feedback loop is dynamically modulated by the diodes.** As the output signal exceeds the diode threshold, current flows through the diodes, reducing the effective feedback impedance, which reduces the gain. The op-amp "fights" to maintain its virtual ground at pin 2, creating a smooth, compressive transition into clipping. This is why the Tube Screamer sounds "warm" and "tube-like" -- real tubes also have a smooth gain compression characteristic (unlike hard clipping which is more like a solid-state limiter).

### Diode Specifications

| Parameter | 1N914 | MA150 | 1N4148 |
|-----------|-------|-------|--------|
| Vf @ 1mA | 0.65V | 0.70V | 0.65V |
| Vf @ 10mA | 0.75V | 0.90V | 0.75V |
| Is (saturation current) | 2.52nA | ~5nA | 2.52nA |
| n (ideality factor) | 1.752 | ~1.8 | 1.752 |
| Junction capacitance | 2pF | 4pF | 2pF |
| Reverse recovery | 4ns | 6ns | 4ns |

The 1N914 and 1N4148 are electrically identical (same die, different package/era). The MA150 has slightly higher Vf, which means clipping onset is ~50mV higher -- the signal gets ~1dB louder before clipping begins. This makes the MA150 version slightly more dynamic/touch-sensitive at low drive settings. At high drive settings, the difference is inaudible because the signal far exceeds both thresholds.

For DSP emulation, use the Shockley diode equation:
```
Id = Is * (exp(Vd / (n * Vt)) - 1)
```
Where:
- Is = 2.52e-9 A (1N914/1N4148)
- n = 1.752
- Vt = kT/q = 25.85mV at 25C (thermal voltage)

---

## Part 4: Tone Control Stage

The tone control is a **passive variable low-pass / low-cut blend network**, NOT an active filter. It sits between the clipping stage output and the second op-amp section (which is a unity-gain buffer).

### Circuit

```
From clipping stage output:
    |
[C5: 0.22uF] ---- DC blocking / coupling
    |
    +---[R7: 1K]---+--- To IC1b pin 5 (+)
    |               |
    |         [VR2: 20K Tone]
    |               |
    |    [R8: 220 ohm]
    |         |
    +---[C6: 0.22uF]---+
                        |
                      VBIAS (AC ground)
```

### How It Works

The signal passes through R7 (1K) to the op-amp input. VR2 (20K tone pot) blends between two paths:

1. **Wiper toward R7 output (Tone = 10, bright)**: The C6/R8 path to ground is disconnected. Signal passes through R7 with minimal filtering. The only rolloff is from R7 into the op-amp's input capacitance -- effectively no filtering.

2. **Wiper toward C6/R8 junction (Tone = 0, dark)**: The C6 (0.22uF) + R8 (220 ohm) path to ground is connected in parallel with the signal path. This forms a voltage divider that attenuates high frequencies (where C6's impedance is low) while passing low frequencies (where C6's impedance is high).

### Cutoff Frequency Calculations

**Tone = 0 (full dark, wiper at C6/R8 end):**
The effective filter is R7 (1K) in series, with C6 (0.22uF) + R8 (220 ohm) shunting to ground:
```
f_dark = 1 / (2 * pi * (R7 + R8) * C6)
       = 1 / (2 * pi * 1220 * 0.22e-6)
       = 593 Hz
```

But the actual response is more complex because VR2 (20K) is in the path. At full dark, VR2 adds series resistance between the tap point and C6:
```
f_dark_actual ~ 1 / (2 * pi * R7 * C6)
              = 1 / (2 * pi * 1000 * 0.22e-6)
              = 723 Hz
```

**Tone = 10 (full bright, wiper at R7 end):**
C6 is effectively disconnected (VR2 adds 20K between the signal and C6, making C6's shunt impedance irrelevant):
```
f_bright ~ 1 / (2 * pi * (R7 + VR2) * C6)  -- but C6 is disconnected
         = essentially no filtering (flat response)
```

The practical audible range of the tone control is approximately:
- **Dark**: -3dB at ~723Hz, -10dB at ~3kHz
- **Bright**: essentially flat to >10kHz
- **Noon**: -3dB at approximately 2-3kHz

### The "Double 723Hz" Coincidence

Note that both the clipping stage bass rolloff (R4/C3 = 720Hz) and the tone control's dark setting (R7/C6 ~ 723Hz) are essentially the same frequency. This is NOT coincidental -- Tamura designed the tone control to complement the clipping stage's inherent frequency response. At the dark setting, the tone control reinforces the natural bandwidth of the clipping stage. At the bright setting, it opens up the treble that the clipping stage passes through.

### Post-Tone Buffer

IC1b (second half of the JRC4558D) is wired as a unity-gain voltage follower:
- Pin 5 (+): input from tone network
- Pin 7 (output): connected directly to pin 6 (-) (100% negative feedback)
- Gain = 1.0 (0 dB)
- Purpose: impedance conversion -- transforms the high-impedance tone network output to low impedance for driving the volume pot and output buffer

---

## Part 5: Output Buffer and Volume Stage -- TS-808 vs TS9

This is the ONLY circuit difference between the TS-808 and TS9 (apart from op-amp selection).

### Common Elements (Both Versions)

| Component | Value | Function |
|-----------|-------|----------|
| C7 | 1uF electrolytic | Output coupling (DC blocking from IC1b) |
| Q2 (TS-808) / Q3 (TS9) | 2SC1815GR | NPN emitter follower output buffer |
| R12 (bias) | 510K | Base bias to VBIAS |
| R13 (emitter) | 10K | Emitter resistor |
| VR3 (Volume) | 100K audio taper | Output level control |
| C9 | 10uF electrolytic | Final output coupling |

### TS-808 Output Network

```
Q2 emitter ---> VR3 (100K Volume pot) wiper ---> [R_series: 100 ohm] ---> Output Jack
                                                        |
                                                  [R_shunt: 10K]
                                                        |
                                                      Ground
```

**Output impedance at Volume = max**:
```
Z_out = R_series + (R_shunt || R_emitter_follower)
      = 100 + (10K || 33)  -- emitter follower output Z ~ R_e/hFE
      ~ 133 ohms
```

But through the volume pot at mid position:
```
Z_out_mid ~ R_series + R_shunt || (VR3_wiper_impedance)
          ~ 100 + 10K || 50K
          ~ 8.4K ohms
```

The 10K shunt to ground forms a voltage divider with R_series (100 ohm):
```
Attenuation = 10K / (10K + 100) = 0.99 (-0.09 dB) -- negligible at max volume
```

### TS9 Output Network

```
Q2 emitter ---> VR3 (100K Volume pot) wiper ---> [R_series: 470 ohm] ---> Output Jack
                                                        |
                                                  [R_shunt: 100K]
                                                        |
                                                      Ground
```

**Output impedance at Volume = max**:
```
Z_out = R_series + (R_shunt || R_emitter_follower)
      = 470 + (100K || 33)
      ~ 503 ohms
```

The 100K shunt means:
```
Attenuation = 100K / (100K + 470) = 0.995 (-0.04 dB) -- even less attenuation
```

### Sonic Impact of the Difference

| Parameter | TS-808 | TS9 |
|-----------|--------|-----|
| Series R | 100 ohm | 470 ohm |
| Shunt R | 10K | 100K |
| Output Z (Vol max) | ~133 ohm | ~503 ohm |
| Output Z (Vol mid) | ~8.4K | ~50K |
| HF rolloff with cable | Lower (more treble preserved) | Slightly higher (marginal treble loss) |
| Drive next stage | Stronger | Slightly weaker |

**The practical difference**: The TS-808's lower output impedance is better at driving long cables and low-impedance amp inputs without treble loss. The TS9's higher output impedance can interact with the input impedance of the next pedal or amp, creating a subtle treble rolloff. Into a 1M input impedance tube amp, the difference is approximately:

TS-808: f_cable = 1/(2*pi*133*500pF) = 2.39 MHz (no cable effect)
TS9: f_cable = 1/(2*pi*503*500pF) = 633 kHz (no cable effect)

With a 20-foot cable (~600pF):
TS-808: f = 1/(2*pi*(133+8400)*600e-12) = 31.1 kHz
TS9: f = 1/(2*pi*(503+50000)*600e-12) = 5.25 kHz

**The TS9 will roll off treble into a long cable run**, especially at mid-volume settings where the pot wiper impedance is highest. This is the "slightly brighter" character of the TS-808 that players describe. It is real but subtle -- most A/B tests show the difference is on the order of 1-2dB above 5kHz.

**For DSP emulation**: Model both as a simple output attenuation + one-pole LPF. The TS-808 version can omit the LPF (cutoff is above audio). The TS9 version should include a gentle one-pole LPF whose cutoff depends on the volume pot position and assumed load impedance (1M default for amp input).

---

## Part 6: Power Supply and Bias Network

### 4.5V Virtual Ground (VBIAS)

```
+9V ---[R15: 10K]---+---[R16: 10K]--- Ground
                     |
                [C10: 10uF electrolytic]
                     |
                   Ground

Junction = VBIAS = 4.5V
```

Additional decoupling:
- C11: 47uF electrolytic (main supply filter, 9V rail to ground)
- C12: 100nF ceramic (high-frequency bypass on 9V rail)

### Why 4.5V Bias Matters

1. **Headroom**: With a 9V supply and 4.5V bias, the op-amp can swing +/-3.5V before hitting the supply rails (the JRC4558D's output swing is about 1.5V from each rail). Since the diodes clip at +/-0.65V, the op-amp has massive headroom above the clipping point -- it never hits the rails during normal operation. This ensures the clipping is entirely from the diodes, not from op-amp rail clipping (which would sound harsh and asymmetric).

2. **Clipping Symmetry**: The 4.5V bias centers the signal exactly between the supply rails. Both positive and negative half-cycles clip symmetrically against the diodes. If the bias drifted (e.g., from battery sag), the clipping would become slightly asymmetric, introducing even-order harmonics. This is actually desirable to some players and is why "dying battery" TS pedals sometimes sound warmer.

3. **DC Operating Point**: All coupling capacitors (C1, C2, C5, C7, C9) block DC. The transistors and op-amp all reference VBIAS as their "ground." The actual signal is AC-coupled throughout, riding on the 4.5V DC bias.

### Battery Sag Modeling

At 9V (fresh battery):
- VBIAS = 4.5V, symmetric clipping at +/-0.65V around 4.5V

At 7.5V (dying battery):
- VBIAS = 3.75V, headroom reduced to +/-2.25V
- Clipping threshold unchanged (still +/-0.65V)
- BUT: op-amp open-loop gain decreases, slew rate decreases slightly
- Result: slightly more compressed, warmer, with subtle even-order harmonics from bias shift

---

## Part 7: The "Mid-Hump" -- Complete Frequency Analysis

The Tube Screamer's famous mid-hump is created by the interaction of THREE frequency-shaping mechanisms:

### 1. Bass Rolloff (Clipping Stage): 720 Hz High-Pass

**Source**: C3 (0.047uF) in series with R4 (4.7K) from pin 2 (-) to AC ground.

```
f_bass = 1 / (2*pi*R4*C3) = 1 / (2*pi*4700*47e-9) = 720.5 Hz
```

- Below 720Hz: gain drops at 6dB/octave (first-order HP)
- At 72Hz: gain is ~20dB below max
- Bass guitar fundamentals (41-330Hz) get almost no gain = almost no clipping
- Guitar fundamentals above open A (110Hz) get partial gain
- Guitar harmonics above 720Hz get full gain and clip

**This is THE defining frequency of the Tube Screamer.** Change C3 and you fundamentally alter the pedal's character:
- C3 = 0.1uF (Keeley mod): f = 339Hz, more bass into the clipping = fatter, woolier
- C3 = 0.022uF (bass cut mod): f = 1537Hz, extreme treble focus = thin, cutting

### 2. Treble Rolloff (Feedback Cap): 5.6 kHz Low-Pass (at max drive)

**Source**: C4 (51pF) in parallel with the diodes and gain resistors.

At maximum drive (R6 + VR1 = 551K):
```
f_treble = 1 / (2*pi*(R6+VR1)*C4) = 1 / (2*pi*551K*51pF) = 5.66 kHz
```

At minimum drive (R6 = 51K):
```
f_treble = 1 / (2*pi*R6*C4) = 1 / (2*pi*51K*51pF) = 61.2 kHz
```

At maximum drive, this creates a gentle treble rolloff above 5.66kHz. Combined with the 720Hz bass rolloff, the effective bandwidth of the clipping stage at max drive is approximately **720Hz to 5.7kHz** -- a bandpass centered around ~2kHz.

### 3. Tone Control Post-Filter: Variable 723Hz to Flat

As analyzed in Part 4, the tone control adds additional treble rolloff in the dark position (f ~ 723Hz) or passes signal flat in the bright position.

### Combined Frequency Response

At **Drive = max, Tone = noon**:
```
Below 200Hz:  -20 to -30 dB (C3 bass rolloff)
200-500Hz:    -10 to -3 dB (rising toward peak)
500-2kHz:     0 dB (peak -- THE MID HUMP)
2-5kHz:       -1 to -3 dB (C4 treble rolloff beginning)
5-10kHz:      -3 to -10 dB (C4 rolloff + tone control)
Above 10kHz:  -15 to -20 dB (steep rolloff)
```

The peak is broad, centered around **700-1500 Hz** depending on drive and tone settings. This is often cited as "720Hz" or "1kHz" or "the 700Hz-1kHz mid-hump." The exact center depends on the drive setting because higher drive shifts the treble rolloff downward.

---

## Part 8: Complete Bill of Materials

### TS-808 Tube Screamer (Audio Signal Path Only)

| Ref | Value | Type | Stage | Notes |
|-----|-------|------|-------|-------|
| R1 | 1K | Carbon film 1/4W | Input buffer collector load | (Some schematics show this as part of JFET switch) |
| R2 | 510K | Carbon film 1/4W | Input buffer base bias | Sets input Z |
| R3 | 10K | Carbon film 1/4W | Input buffer emitter | Sets Ic ~ 0.39mA |
| R4 | 4.7K | Carbon film 1/4W | Clipping stage ground impedance | Sets min gain & bass corner |
| R5 | 10K | Carbon film 1/4W | Op-amp bias to VBIAS | DC operating point |
| R6 | 51K | Carbon film 1/4W | Feedback fixed resistance | Min drive gain |
| R7 | 1K | Carbon film 1/4W | Tone mixing resistor | Signal always passes through |
| R8 | 220 ohm | Carbon film 1/4W | Tone ground path | With C6, sets dark cutoff |
| R9 | 10K | Carbon film 1/4W | (Second op-amp bias) | DC operating point |
| R10 | 220 ohm | Carbon film 1/4W | (Tone network, some schematics) | Parallels R8 |
| R12 | 510K | Carbon film 1/4W | Output buffer base bias | Matches R2 |
| R13 | 10K | Carbon film 1/4W | Output buffer emitter | Matches R3 |
| R14 | 100 ohm | Carbon film 1/4W | Output series (TS-808) | Low output Z |
| R15 | 10K | Carbon film 1/4W | Bias divider upper | VBIAS generation |
| R16 | 10K | Carbon film 1/4W | Bias divider lower | VBIAS generation |
| R_shunt | 10K | Carbon film 1/4W | Output shunt (TS-808) | Voltage divider |
| C1 | 0.022uF (22nF) | Polyester film | Input coupling | HP corner ~14Hz |
| C2 | 1uF | Electrolytic | Clipping stage input coupling | HP corner ~0.3Hz |
| C3 | 0.047uF (47nF) | Polyester film | **Bass rolloff (feedback)** | f = 720Hz |
| C4 | 51pF | Ceramic C0G or silver mica | **Treble rolloff (feedback)** | f = 5.6-61kHz |
| C5 | 0.22uF (220nF) | Polyester film | Tone stage coupling | |
| C6 | 0.22uF (220nF) | Polyester film | Tone LP cap | With R8, dark cutoff |
| C7 | 1uF | Electrolytic | Output coupling | |
| C9 | 10uF | Electrolytic | Final output coupling | |
| C10 | 10uF | Electrolytic | VBIAS bypass | Low-freq decoupling |
| C11 | 47uF | Electrolytic | Supply filter | Main 9V decoupling |
| C12 | 100nF | Ceramic X7R | Supply HF bypass | HF noise rejection |
| VR1 | 500K | Log (audio) taper | Drive control | |
| VR2 | 20K | Linear (or W-taper) | Tone control | |
| VR3 | 100K | Log (audio) taper | Volume control | |
| Q1 | 2SC1815GR | NPN Si, TO-92 | Input buffer | hFE 200-400 |
| Q2 | 2SC1815GR | NPN Si, TO-92 | Output buffer | hFE 200-400 |
| IC1 | JRC4558D | Dual op-amp, DIP-8 | Clipping amp + tone buffer | |
| D1, D2 | 1N914 | Si signal diode | Clipping (anti-parallel, in feedback) | Vf=0.65V, Is=2.52nA |

### TS9 Differences Only

| Ref | TS-808 Value | TS9 Value | Notes |
|-----|-------------|-----------|-------|
| R14 (series) | 100 ohm | 470 ohm | Higher output Z |
| R_shunt | 10K | 100K | Less attenuation, higher Z |
| IC1 | JRC4558D | TA75558P (some runs) | Marginal sonic diff |

Everything else is identical.

---

## Part 9: DSP Implementation Recommendations

### Approach Selection: WDF vs Virtual Analog (VA) vs Hybrid

The Tube Screamer clipping stage presents a unique modeling challenge because the diodes are **inside the op-amp feedback loop**. This creates an implicit nonlinear equation that cannot be solved with a simple memoryless waveshaper.

#### Option A: Full WDF Model (chowdsp_wdf)

**Pros**: Physically accurate, handles all frequency-dependent interactions automatically, consistent with existing FuzzFace/BigMuff/RAT implementations in the codebase.

**Cons**: The diode-in-feedback topology creates a **delay-free loop** in the WDF tree. Standard WDF trees cannot handle this without either (a) an R-type adaptor with Newton-Raphson iteration, or (b) breaking the loop with a one-sample delay (which introduces phase error).

**Implementation with chowdsp_wdf**:
```cpp
// The clipping stage can be modeled as:
// Source -> R4 -> C3 -> ground (the Zi path)
// Parallel feedback: diodes || C4 || (R6 + VR1_drive)
// This requires an R-type adaptor for the implicit loop

// Using chowdsp::wdft::RtypeAdaptor with DiodePairT
// Port 1: Op-amp output (voltage source through gain)
// Port 2: R4 + C3 (series connection to ground)
// Port 3: R6 + VR1 (variable resistance)
// Port 4: C4 (51pF capacitor)
// Port 5: DiodePairT (anti-parallel 1N914)
```

**CPU cost estimate**: ~80-120 FLOPS/sample at 2x oversampling (Newton-Raphson iterations for diode solve). Comparable to RAT WDF implementation.

#### Option B: Yeh's Simplified Model (Recommended for Mobile)

David Yeh's 2009 CCRMA thesis proposes decomposing the TS clipping stage into:

1. **Pre-emphasis filter** (models the frequency-dependent gain before clipping)
2. **Memoryless nonlinearity** (models the diode soft clipping)
3. **Post-emphasis filter** (models the frequency-dependent attenuation after clipping)

```
Input -> [HP filter @ 720Hz] -> [Gain] -> [Soft clip (tanh or diode model)] -> [LP filter] -> Output
```

**The key insight**: At frequencies well above 720Hz and well below 5.6kHz, the clipping stage behaves like a simple gain stage followed by a soft clipper. The frequency-dependent gain can be separated from the nonlinearity without significant perceptual error, because the ear is not sensitive to the exact phase relationship between the pre-filtering and the clipping.

**Soft clipping function**: The diode pair in feedback creates a transfer function well-approximated by:
```cpp
// Antithetical saturation model (Yeh 2009)
// For symmetric Si diode pair in feedback:
float softClip(float x) {
    // Scale x by 1/Vclip where Vclip = n*Vt*ln(1 + G*Vin_peak/Is/R)
    // Simplified: tanh approximation with appropriate scaling
    const float Vclip = 0.65f; // diode forward voltage
    return Vclip * std::tanh(x / Vclip);
}
```

But for better accuracy, use the actual diode equation solved for Vout:
```cpp
// Implicit equation: Vout = G * Vin - R_fb * Id(Vout)
// Where Id(Vout) = Is * (exp(Vout/(n*Vt)) - exp(-Vout/(n*Vt)))
// This requires Newton-Raphson iteration per sample
float diodeFeedbackClip(float Vin, float gain, float Rfb) {
    const float Is = 2.52e-9f;
    const float nVt = 1.752f * 0.02585f; // = 0.04529V
    float Vout = prevVout_; // warm start from previous sample

    for (int iter = 0; iter < 4; ++iter) {
        float Id = Is * (std::exp(Vout / nVt) - std::exp(-Vout / nVt));
        float dId = (Is / nVt) * (std::exp(Vout / nVt) + std::exp(-Vout / nVt));
        float f = Vout - gain * Vin + Rfb * Id;
        float df = 1.0f + Rfb * dId;
        Vout -= f / df;
    }
    prevVout_ = Vout;
    return Vout;
}
```

**CPU cost**: ~30-50 FLOPS/sample (4 NR iterations with exp()). Much cheaper than full WDF.

#### Option C: Hybrid Approach (Best Quality/Cost Ratio -- RECOMMENDED)

Combine the Yeh decomposition with a proper diode solver:

1. **Input coupling HP**: One-pole HP at ~14Hz (C1/R_in)
2. **Frequency-dependent gain**: Biquad HP shelving at 720Hz (models C3/R4)
3. **Diode soft clipper with NR solver**: 4 iterations per sample (models D1/D2 in feedback)
4. **Treble rolloff**: One-pole LP at f_treble (models C4, drive-dependent)
5. **Tone control**: Variable one-pole LP, cutoff swept by tone parameter
6. **Output scaling**: Volume pot attenuation
7. **Output impedance filter**: One-pole LP for TS9 variant (optional)

```
Process chain:
  inputHP -> gain(freq_dependent) -> [2x oversample] -> diodeSoftClip(NR)
  -> [2x downsample] -> trebleLP -> toneControl -> volumeScale
```

### Oversampling Recommendation

**2x oversampling is sufficient.** Here's why:

The Tube Screamer's soft clipping generates fewer harmonics than hard clipping (DS-1, RAT). The diode feedback topology limits the output to approximately +/-0.65V with smooth transitions. The 51pF cap further softens the harmonic content. Combined with the 720Hz bass rolloff limiting the input bandwidth, the aliased energy at 2x oversampling is well below -60dB.

For comparison:
- RAT (hard clip): Needs 2x minimum, 4x preferred
- DS-1 (hard clip): Needs 2x minimum
- Big Muff (cascaded hard clip): Needs 2x minimum
- **Tube Screamer (soft clip in feedback): 2x is more than adequate**

If targeting 48kHz native rate: oversample to 96kHz for the clipping stage only.

### Anti-Aliasing Strategy

Use ADAA (Antiderivative Anti-Aliasing) if using a memoryless approximation:
```cpp
// For tanh soft clipping, the antiderivative is:
// F(x) = ln(cosh(x))  -- first antiderivative of tanh(x)
// ADAA1: y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
```

If using Newton-Raphson diode solver, ADAA is not directly applicable (the function is implicit, not closed-form). In that case, rely on oversampling alone. 2x with the half-band filter in our existing Oversampler class is sufficient for the TS's soft clipping character.

### Parameter Mapping

| User Param | Range | Internal Mapping | Formula |
|------------|-------|-----------------|---------|
| Drive (0-1) | 0.0 to 1.0 | R_feedback = R6 + VR1*drive | R_fb = 51K + 500K * drive^2 (log taper) |
| | | Gain = 1 + R_fb / R4 | G = 1 + R_fb / 4700 |
| | | Range: 11.85x to 118.2x | 21.5 dB to 41.5 dB |
| Tone (0-1) | 0.0 to 1.0 | LP cutoff frequency | f = 723 + (20000-723) * tone (Hz) |
| Level (0-1) | 0.0 to 1.0 | Output attenuation | gain = level * 2.0 (with makeup) |

### Numerical Stability Notes

1. **Denormals**: All filter states should use `fast_math::denormal_guard()` per project convention
2. **NR convergence**: Initialize from previous sample's output (warm start). If |Vin_change| > 1V between samples, reset to 0. Max 8 iterations with convergence check (|delta| < 1e-6)
3. **Exp overflow**: Clamp diode equation input to +/-20 (exp(20) ~ 5e8, exp(-20) ~ 2e-9)
4. **DC offset**: The soft clipping function is symmetric, so no DC offset is generated. But add a DC blocker after the clipper anyway (standard practice)
5. **Filter state precision**: Use `double` for all IIR filter states (project convention: 64-bit accumulators)

### Suggested File Structure

Following the existing codebase pattern:
```
app/src/main/cpp/effects/tube_screamer.h
```

Class name: `TubeScreamer` (index 31 in signal chain, or could replace/augment the generic `Overdrive` at index 4)

Parameters:
- 0: Drive (0.0-1.0)
- 1: Tone (0.0-1.0)
- 2: Level (0.0-1.0)
- 3: Variant (0 = TS-808, 1 = TS9) -- controls output stage modeling

---

## Part 10: Summary of Critical DSP Frequencies

| Frequency | Source | Component | Effect |
|-----------|--------|-----------|--------|
| 14-17 Hz | Input coupling HP | C1 (22nF) / R_in (510K) | Removes DC, subsonic |
| 0.3 Hz | Clipping input coupling | C2 (1uF) / R5 (10K) | DC blocking (inaudible) |
| **720 Hz** | **Bass rolloff (feedback)** | **C3 (47nF) / R4 (4.7K)** | **THE mid-hump corner** |
| **5.66 kHz** | **Treble rolloff (feedback)** | **C4 (51pF) / (R6+VR1)** | **Softens at max drive** |
| 61.2 kHz | Treble rolloff (min drive) | C4 (51pF) / R6 (51K) | Inaudible |
| 723 Hz | Tone control (full dark) | C6 (220nF) / R7 (1K) | Dark setting LP |
| Flat | Tone control (full bright) | C6 disconnected | No filtering |

**The Tube Screamer's tonal signature in three numbers: 720Hz bass rolloff, +/-0.65V soft clip, 5.7kHz treble rolloff at max drive.**

---

## References

1. Electrosmash, "Tube Screamer Circuit Analysis" -- https://www.electrosmash.com/tube-screamer-analysis
2. R.G. Keen, "The Technology of the Tube Screamer," GEOfex, 1998 -- http://www.geofex.com/article_folders/tstech/tsxtech.htm
3. Analogman, "Tube Screamer History" -- https://www.analogman.com/tshist.htm
4. D. Yeh, "Digital Implementation of Musical Distortion Circuits by Analysis and Simulation," PhD thesis, CCRMA, Stanford, 2009
5. D. Yeh, J. Abel, J.O. Smith, "Simplified, Physically-Informed Models of Distortion and Overdrive Guitar Effects Pedals," DAFx-07, 2007
6. D. Yeh, J. Abel, J.O. Smith, "Simulation of the Diode Limiter in Guitar Distortion Circuits by Numerical Solution of Ordinary Differential Equations," DAFx-07
7. m0xpd, "Analysis of the Tube Screamer" -- http://m0xpd.blogspot.com/2012/11/analysis-of-tube-screamer.html
8. Spicy Pedals, "The Ultimate Tube Screamer Showdown: TS808 vs. TS9 vs. TS10" -- https://spicypedals.com/the-ultimate-tube-screamer-showdown-ts808-vs-ts9-vs-ts10/
9. PedalPCB Forum, "Tone Stacks Part 3: Tube Screamer, HM-2 & Gyrators" -- https://forum.pedalpcb.com/threads/tone-stacks-part-3-tube-screamer-hm-2-gyrators.8557/
10. Barbarach BC, "Tube Screamer on a Breadboard" -- https://barbarach.com/tube-screamer-on-a-breadboard/
