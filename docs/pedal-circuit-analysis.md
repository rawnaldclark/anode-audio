# Guitar Pedal Circuit Analysis for DSP Emulation

Comprehensive circuit topology, component values, and signal flow documentation
for five target pedals. This document provides the technical foundation needed to
implement accurate digital signal processing models.

---

## 1. Boss DS-1 Distortion

### Signal Flow (Block Diagram)

```
Input -> Input Buffer (Q1 emitter follower)
      -> Transistor Booster (Q2, frequency filtering)
      -> Op-Amp Gain Stage (IC1, with hard clipping diodes D4/D5)
      -> Passive Tone Control (Big Muff Pi style)
      -> Level Control (VR2)
      -> Output Buffer (Q3)
      -> Output
```

### Active Components

| Component | Part Number | Type | Role |
|-----------|-------------|------|------|
| Q1, Q2, Q3 | 2SC2240-GR | NPN BJT (hFE 200-700) | Input buffer, booster, output buffer |
| IC1 | NJM3404A (current); historically TA7136AP (1978), BA728N (1994), M5223AL (2000), NJM2904L (2006) | Dual Op-Amp | Gain/clipping stage |
| D4, D5 | 1N4148 | Silicon switching diode | Hard clipping pair |
| D8 | 1N4148 | Silicon switching diode | Additional clipping (some revisions) |
| D1 | 1N4004 | Rectifier diode | Reverse polarity protection |

### Clipping Topology

**Type: Hard Clipping (Shunt to Ground)**

- Two 1N4148 silicon diodes (D4, D5) wired back-to-back from the op-amp output
  to AC ground (4.5V virtual ground).
- **Symmetric** clipping: both diodes are identical silicon types.
- Forward voltage ~0.6-0.7V per diode.
- Signal is clipped to approximately +/-0.7V, producing a squared waveform.
- The transistor booster stage (Q2) also produces some asymmetric clipping due to
  its biasing, introducing even-order harmonics before the main clipping stage.

### Gain Stage Details

**Transistor Booster (Q2):**
- Configuration: Common-emitter with feedback
- Gain: ~56x (35 dB)
- R9 = 22 ohm (emitter resistor -- critical component, many mods change this)
- Frequency shaping: dual high-pass filters at 3.3 Hz and 33 Hz

**Op-Amp Stage (IC1):**
- Non-inverting configuration
- VR1 = 100K (DIST pot) in feedback path
- R13 = 4.7K (feedback resistor to ground)
- Maximum gain: 1 + (100K / 4.7K) = ~22.3x (26.5 dB)
- Minimum gain: unity (0 dB)
- C10 = 0.01 uF in feedback creates frequency-dependent gain

### Component Values

**Capacitors:**
| ID | Value | Role |
|----|-------|------|
| C1, C3 | 47 nF | Input/output coupling |
| C2, C8, C9 | 0.47 uF | Transistor stage coupling/bypass |
| C4 | 250 pF | High-frequency shaping |
| C5 | 68 nF | Frequency filtering |
| C7 | 100 pF | Feedback compensation |
| C10 | 0.01 uF (10 nF) | Op-amp feedback (frequency-dependent gain) |
| C11 | 0.022 uF (22 nF) | Tone high-pass leg |
| C12 | 0.1 uF (100 nF) | Tone low-pass leg |
| C13 | 0.047 uF (47 nF) | Output coupling |
| C14 | 1 uF | Power supply |
| C15 | 47 uF | Power supply bulk |
| C23 | 100 uF | Power supply bulk |

**Resistors:**
| ID | Value | Role |
|----|-------|------|
| R1, R22 | 1K | Input/output series |
| R2, R7 | 470K | Bias/feedback |
| R3, R8, R18, R21, R24, R25 | 10K | Biasing, voltage divider |
| R4, R5, R10, R11, R23 | 100K | Various |
| R9 | 22 ohm | Emitter degeneration (critical for tone) |
| R13 | 4.7K | Op-amp feedback to ground |
| R14, R15 | 2.2K | Op-amp stage |
| R16, R17 | 6.8K | Tone control |
| R19, R20 | 1M | High impedance bias |
| R39 | 47K | Various |

**Potentiometers:**
| ID | Value | Function |
|----|-------|----------|
| VR1 | 100K B (linear) | DIST (gain) |
| VR2 | 20K B (linear) | LEVEL (output volume) |
| VR3 | 20K B (linear) | TONE |

### Tone Control Topology

**Type: Passive "Big Muff Pi" style mixer**

```
              R16 (6.8K)     C12 (0.1 uF)
Signal ---+---[====]---+---||---+
          |            |        |
          |          [VR3]      |    --> Output
          |          (TONE)     |
          |            |        |
          +---||-------+---[====]---+
              C11 (22nF)   R17 (6.8K)
```

- **Low-pass path:** R16 (6.8K) + C12 (0.1 uF) -> fc = 234 Hz, -6 dB/octave
- **High-pass path:** C11 (22 nF) + R17 (6.8K) -> fc = 1063 Hz, -6 dB/octave
- VR3 (20K TONE pot) mixes between the two paths.
- At center position: mid-scoop notch centered around ~500 Hz (~3 dB dip).
- Full CCW: bass-heavy, treble-cut.
- Full CW: treble-heavy, bass-cut.

### Key Frequency Response Characteristics

- Input high-pass: 7.2 Hz cutoff (blocks DC, subsonic)
- Transistor stage: high-pass filters at 3.3 Hz and 33 Hz
- Op-amp stage: 72 Hz high-pass (depends on distortion setting), 7.2 kHz low-pass
- Tone control: mid-scoop around 500 Hz
- Overall: emphasis on bass and treble with scooped mids (similar to Big Muff Pi)

### What Makes the DS-1 Sound Like a DS-1

1. **Hard clipping with symmetric silicon diodes** -- produces harsh, buzzy, squared
   waveform rich in odd harmonics.
2. **Transistor booster pre-gain** -- Q2's asymmetric clipping adds even-order harmonics
   before the main clipping stage, giving the distortion some "musicality."
3. **Big Muff Pi style tone control** -- the mid-scoop around 500 Hz is the signature
   frequency character, giving it that "scooped" rock tone.
4. **Moderate gain range** -- max ~60 dB total gain across both stages, enough for
   rock/metal but not extreme.
5. **R9 = 22 ohm emitter resistor** -- this particular value sets the booster stage
   gain and frequency character; changing it is the most common DS-1 modification.

---

## 2. Boss DS-2 Turbo Distortion

### Signal Flow (Block Diagram)

```
Input -> Input Buffer (JFET Q6)
      -> [Mode II: Q23 boost stage engaged]
      -> Discrete Op-Amp Gain Stage (Q16/Q19 JFET differential pair
         + Q17/Q18 followers)
      -> Q22 Gain/Clipping Stage
      -> Hard Clipping Diodes (D14, D15)
      -> Tone/EQ Control (Q12, Q13)
      -> Output Stage (Q5)
      -> Output
```

### Active Components

| Component | Part Number | Type | Role |
|-----------|-------------|------|------|
| Q5, Q11, Q12, Q13, Q18, Q22, Q23 | 2SC3378GR | NPN BJT | Various gain/buffer stages |
| Q6, Q16, Q19 | 2SK184GR | N-channel JFET | Input buffer, differential pair |
| Q8, Q9 | 2SA1048GR | PNP BJT | Switching/control |
| Q17 | 2SA1335GR | PNP BJT | Follower in discrete op-amp |
| D3-D13 | 1SS-133 (later 1N4148) | Silicon switching diode | Various clipping/switching |
| D14, D15 | 1SS-188FM | Silicon/Germanium (varies by production era) | Hard clipping |
| D1 | MTZ5.6B | 5.6V Zener | Voltage regulation |
| D2 | S5500G | Rectifier | Polarity protection |

### Clipping Topology

**Type: Both Soft and Hard Clipping**

- The DS-2 uses a fully discrete design -- NO operational amplifier ICs.
- Q16 and Q19 (JFETs) form a differential pair; Q17 and Q18 form the follower
  stage, creating a "discrete op-amp."
- D14 and D15 (1SS-188FM) provide hard clipping at Q22's base.
- Additional diodes in the 1SS-133 series provide soft clipping in earlier stages.

**Production Variation:**
- Early DS-2 units: germanium clipping diodes in one mode, silicon in the other.
- Later production: all silicon diodes (1N4148 replacing 1SS-133) for both modes.

### Mode I vs Mode II (Turbo Switch)

**Mode I (Standard Distortion):**
- Similar tonal character to the DS-1 but with a slightly softer attack and
  flatter frequency response.
- Signal bypasses Q23 boost stage.
- Produces warm, mellow distortion.

**Mode II (Turbo):**
- Q23 boost stage is switched INTO the signal path before the main gain stages.
- This pre-boost drives the downstream discrete op-amp harder, producing more
  saturation and compression.
- Adds a massive ~10 dB mid boost centered around 900 Hz.
- Narrow frequency cut of ~10 dB at approximately 2500 Hz.
- Also inserts D8 and D9 in the feedback of the discrete op-amp.
- Adds modifications to the output stage frequency response.
- Result: more aggressive, "honky" distortion ideal for lead playing.

### Tone Control

- Q12 and Q13 handle EQ and output duties.
- The tone control provides a slight overdrive-style mid hump across its range.
- Frequency response difference between modes is the primary tonal distinction:
  Mode I = relatively flat; Mode II = strong mid-boost at 900 Hz with treble cut.

### Key Differences from DS-1

| Feature | DS-1 | DS-2 |
|---------|------|------|
| Amplification | IC-based op-amp | Fully discrete transistor design |
| Modes | Single | Dual (Mode I / Mode II Turbo) |
| Clipping | Hard clip only (1N4148) | Both soft and hard clipping |
| Tone Character | Mid-scoop | Mode I: flatter; Mode II: mid-boost at 900 Hz |
| Gain Stages | Transistor booster + op-amp | Multiple discrete stages + switchable boost |
| Attack | Sharp | Softer (closer to BD-2 Blues Driver topology) |

### What Makes the DS-2 Sound Like a DS-2

1. **Fully discrete design** -- no op-amp IC means the clipping and saturation
   character is inherently different from the DS-1's IC-based clipping.
2. **Turbo Mode II's 900 Hz mid-boost** -- this is the signature feature, adding
   a "honk" that cuts through a band mix for solos.
3. **Dual clipping topology** -- soft AND hard clipping creates a more complex
   harmonic structure than the DS-1's hard-clip-only approach.
4. **JFET differential pair** -- Q16/Q19 provide a different input impedance and
   distortion character than a standard op-amp.

---

## 3. ProCo RAT (RAT2)

### Signal Flow (Block Diagram)

```
Input -> Input Coupling (C1, 22nF)
      -> High-Pass Filter (R1/R2 1M, C1 22nF)
      -> Non-Inverting Op-Amp Gain Stage (LM308)
         [with frequency-dependent gain via C3, C4 in feedback]
      -> Hard Clipping Diodes to Ground (D1, D2: 1N914)
      -> Passive Low-Pass Tone Filter (R, C8 3.3nF, RTONE 100K pot)
      -> JFET Output Buffer (Q1: 2N5458)
      -> Volume Control (RVOLUME 100K pot)
      -> Output
```

### Active Components

| Component | Part Number | Type | Role |
|-----------|-------------|------|------|
| U1 | LM308N (original Motorola); later OP07DP (TI replacement) | Single Op-Amp | Gain stage |
| Q1 | 2N5458 | N-channel JFET | Output buffer |
| D1, D2 | 1N914 (RAT1); 1N4148 (RAT2) | Silicon switching diode | Hard clipping |
| D3 | 1N4002 | Rectifier | Reverse polarity protection |

### Op-Amp: The LM308 and Its Significance

The LM308 is **central to the RAT's character.** Key specifications:
- **Slew rate: 0.3 V/us** (compared to TL071 at 13 V/us -- roughly 40x slower).
- This slow slew rate means the op-amp itself cannot faithfully reproduce fast
  transients, adding its own soft limiting and compression to the signal BEFORE
  the diode clipping stage.
- The slow slew rate effectively acts as a low-pass filter on the distorted signal,
  rolling off harsh high-frequency content.
- **Compensation pin (pins 1 and 8):** C3 (30 pF) is connected between these pins,
  which controls the op-amp's frequency response and further limits bandwidth.
- Later RAT2 models using OP07DP (faster slew rate) have a noticeably different,
  "brighter" and "harsher" character.

### Clipping Topology

**Type: Hard Clipping (Shunt to Ground) -- AFTER the gain stage, NOT in the feedback loop**

```
              D1 (1N914)
              --|>|--
Op-amp out ---+------+--- To Tone Filter
              --|<|--
              D2 (1N914)
                |
               GND (via virtual ground 4.5V)
```

- D1 clips the positive half-cycle to +Vf (~0.6V).
- D2 clips the negative half-cycle to -Vf (~0.6V).
- **Symmetric** clipping: both diodes are identical silicon types.
- This is "hard" clipping because the diodes abruptly shunt signal to ground
  when Vf is exceeded, producing a flat-topped, nearly square waveform.

**Critical distinction from soft clipping (e.g., Tube Screamer):**
- In soft clipping, diodes are in the op-amp's negative feedback loop, which
  gradually reduces gain as the signal increases, producing a smooth rolloff.
- In the RAT's hard clipping, the op-amp outputs its FULL amplified signal,
  and then the diodes brutally chop the peaks, producing more aggressive,
  "buzzier" distortion with stronger high-order harmonics.

### Gain Stage Details

**Non-inverting op-amp configuration:**
- Minimum gain: 1 (0 dB) -- when RDISTORTION pot is at minimum
- Maximum theoretical gain: 1 + (100K / 47) = ~2128x (67 dB)
- In practice, the LM308's limitations and the diode clipping limit actual
  output swing to about +/-0.6V regardless of gain setting.

**Frequency-dependent gain (critical for RAT character):**
- C3 = 30 pF (op-amp compensation, pins 1-8)
- C4 = 100 pF in feedback loop parallel with RDISTORTION
- C2 = 1 nF shunt at non-inverting input
- These capacitors create a bandpass characteristic in the gain stage:
  - At low gain settings: wider bandwidth
  - At high gain settings: C4 (100 pF) dominates, creating a low-pass
    filter with cutoff as low as ~16 kHz at maximum distortion.
  - The gain stage naturally emphasizes mid-frequencies.

### Component Values

**Capacitors:**
| ID | Value | Role |
|----|-------|------|
| C1 | 22 nF | Input coupling / high-pass with R1 |
| C2 | 1 nF | Input high-frequency shunt |
| C3 | 30 pF | Op-amp compensation (pins 1-8) |
| C4 | 100 pF | Feedback low-pass (frequency-dependent gain) |
| C5 | 2.2 uF | Input network high-pass |
| C6, C7 | 4.7 uF | Coupling/filtering |
| C8 | 3.3 nF | Tone control (Filter knob) |
| C9 | 22 nF | DC blocking |
| C10, C13 | 1 uF | Supply filtering |
| C11 | 100 uF | Power supply bulk |
| C12 | 0.1 uF | Power supply bypass |

**Resistors:**
| ID | Value | Role |
|----|-------|------|
| R1, R2, R8 | 1M | Input network / bias |
| R3, R6 | 1K | Various |
| R4, R10 | 47 ohm | Feedback / current limiting |
| R5 | 560 ohm | Input network |
| R7 | 1.5K | Various |
| R9 | 10K | Various |
| R11, R12 | 100K | Voltage divider (4.5V virtual ground) |

**Potentiometers:**
| ID | Value | Function |
|----|-------|----------|
| RDISTORTION | 100K A (log) | Gain control |
| RFILTER | 100K A (log) | Tone (low-pass filter cutoff) |
| RVOLUME | 100K A (log) | Output level |

### Tone Control ("Filter")

**Type: Passive First-Order Low-Pass Filter**

```
Signal in ---[R_FILTER pot]---+--- Output
                              |
                            [C8]
                           (3.3nF)
                              |
                             GND
```

- This is NOT a traditional "tone" control -- it is a simple RC low-pass filter.
- Cutoff frequency: f = 1 / (2 * pi * R_FILTER * C8)
  - Filter at minimum (R=0): fc = very high (essentially bypassed, full treble)
  - Filter at maximum (R=100K): fc = 1 / (2 * pi * 100K * 3.3nF) = ~482 Hz
- **Full CW = dark/muffled** (most resistance, lowest cutoff)
- **Full CCW = bright/fizzy** (least resistance, highest cutoff)
- This is the opposite of most "tone" controls where CW = brighter.
- The filter acts AFTER the clipping diodes, so it removes high-frequency
  harmonics generated by the hard clipping.

### Key Frequency Response Characteristics

- Input high-pass filters: ~1.5 kHz pole (R4-C5) and ~60 Hz pole (R5-C6)
  creating a 20 dB/decade and 40 dB/decade attenuation at low frequencies.
- Op-amp gain stage: pronounced mid-frequency emphasis around 1 kHz.
- C4 (100 pF) feedback cap: creates variable low-pass from ~16 kHz (max dist)
  to higher at lower gain settings.
- Filter control: sweeps low-pass from ~480 Hz to ~32 kHz.
- Overall: strong mid-emphasis (unlike the DS-1's mid-scoop).

### What Makes the RAT Sound Like a RAT

1. **LM308's slow slew rate (0.3 V/us)** -- the op-amp itself adds compression
   and smooths transients before the diodes even clip. This is THE defining
   characteristic. Replacing the LM308 with a faster op-amp (TL071, etc.)
   fundamentally changes the pedal's character.
2. **Hard clipping to ground** -- more aggressive than feedback-loop soft clipping,
   producing a "woolly," "fuzzy" distortion character.
3. **The Filter control** -- a low-pass filter (not a tone control) that lets you
   go from extremely bright/raspy to extremely dark/muffled. Many players use it
   to tame the harshness of the hard clipping.
4. **Massive gain range** -- 0 to 67 dB theoretical, much more than the DS-1.
5. **Mid-emphasis frequency response** -- the opposite of the DS-1's mid-scoop,
   giving the RAT a thick, aggressive character that sits forward in a mix.
6. **C3 (30 pF) compensation cap** -- directly affects the LM308's bandwidth and
   transient response; different values produce audibly different results.

---

## 4. Boss HM-2 Heavy Metal

### Signal Flow (Block Diagram)

```
Input -> Input Buffer (Q1: 2SK30A-Y JFET)
      -> Discrete Pre-Gain Stage (Q2/Q3: 2SC732TM-GR)
      -> Complementary Gain Stage (Q6: 2SC2240-GR / Q7: 2SA970-GR NPN/PNP pair)
      -> Asymmetric Soft Clipping (IC1B: NJM4558S, diodes in feedback)
      -> Germanium "Coring" / Noise Gate (D6, D7: 1S188FM, 0.3V Vf)
      -> Hard Clipping (D8, D9: 1S2473, shunt to ground)
      -> Treble Rolloff (C16)
      -> Gyrator-Based Active EQ / Tone Stack (IC2, IC3: NJM4558S)
         [LOW control: ~87 Hz peak]
         [HIGH control: ~960 Hz + ~1280 Hz dual peak]
      -> Output Buffer (Q10: 2SK30A-Y JFET)
      -> Output
```

### Active Components

| Component | Part Number | Type | Role |
|-----------|-------------|------|------|
| IC1, IC2, IC3 | M5218L or NJM4558S | Dual Op-Amp | Clipping stage, gyrator EQ |
| Q1, Q4, Q5, Q10 | 2SK30A-Y | N-channel JFET | Input/output buffers, switching |
| Q2, Q3 | 2SC732TM-GR | NPN BJT | Discrete pre-gain |
| Q6 | 2SC2240-GR | NPN BJT | Complementary gain stage |
| Q7 | 2SA970-GR | PNP BJT | Complementary gain stage |
| Q8, Q9 | 2SC945-P | NPN BJT | Additional gain/buffer |
| D1, D3, D5, D8-D12 | 1S2473 | Silicon switching diode | Hard clipping, switching |
| D6, D7 | 1S188FM | Germanium diode (Vf=0.3V) | Coring/noise gate |
| D13 | RD5.1EB-3 | 5.1V Zener | Voltage regulation |
| D14 | SLP135B | Red LED | Status indicator |

### Clipping Topology -- THREE STAGES

The HM-2 is unique in having **three distinct clipping mechanisms** in series:

**Stage 1: Asymmetric Soft Clipping (IC1B)**
- Tube Screamer-style diodes in the feedback loop of IC1B.
- **Asymmetric**: positive half can reach 2x the amplitude of the negative half
  before limiting begins.
- Nearly identical to the Boss SD-1 Super OverDrive clipping stage.
- This produces even-order harmonics due to the asymmetry.

**Stage 2: Germanium "Coring" Circuit (D6, D7)**
- D6 and D7 are germanium diodes (1S188FM) wired in SERIES with the signal path.
- Measured forward voltage: 0.3V.
- Any signal below ~300 mV is blocked (both positive and negative).
- Acts as a primitive noise gate.
- Also introduces **crossover distortion** -- the signal is "cored" (the center
  of the waveform is removed), similar to a Class B amplifier artifact.
- This crossover distortion adds a harsh, gritty texture unique to the HM-2.

**Stage 3: Hard Clipping (D8, D9)**
- Standard silicon diode shunt clippers to ground (like RAT/Distortion+).
- D8, D9 are 1S2473 silicon switching diodes.
- C16 shunts treble to ground, rounding off the edges of the square wave.

### Gyrator EQ / Tone Stack (THE Signature Element)

The HM-2's tone stack uses **gyrator circuits** -- active networks that use an
op-amp, resistors, and capacitors to simulate an inductor. This allows creation
of resonant LC-style filters without physical inductors.

**Low Control Gyrator:**
- Components: R21, R25, C13, IC2.2
- Simulated inductance: L = R21 * R25 * C13 = 2.24 H
- Center frequency: **~87 Hz** (right on the low E string fundamental at 82 Hz)
- Provides boost/cut of bass frequencies.

**High Control Gyrator (DUAL):**
- Two stagger-tuned resonant networks:
  - Peak 1: **~960 Hz** (vocal midrange)
  - Peak 2: **~1280 Hz** (upper midrange)
- This dual-peaked response creates a wide boost in the upper midrange.
- The "High" control actually boosts midrange more than true treble.

**Resulting Frequency Response (all controls maxed):**
```
dB
+20 |    *                              * *
+10 |   * *                            *   *
  0 |  *   *                          *     *
-10 | *     *         *              *       *
-20 |*       *      * * *           *         *
    +--+------+----+-----+---------+----------+-->
    40  87   150  240   500  700  960  1280   5K  Hz
         |         |                |    |
       Low peak  Deep scoop    High peaks
```

- Massive low-frequency boost centered on 87 Hz.
- **Deep mid-scoop centered around 240 Hz** (the "mud" frequency range).
- Strong dual-peaked boost at 960 Hz and 1280 Hz.
- This "suspension bridge" EQ curve is the HM-2's sonic signature.

**Noise characteristics:**
- Gyrators are inherently noisy: noise gain = R21/R25.
- The HM-2's ratio produces approximately 50 dB noise gain.
- This is near the practical limit for pedal circuits and contributes to the
  HM-2's famously noisy character.

### What Makes the HM-2 Sound Like the HM-2 ("Swedish Chainsaw")

1. **Three-stage clipping** -- asymmetric soft clip -> germanium coring -> hard clip.
   No other common pedal uses this exact combination. The germanium coring stage
   is particularly unusual and adds the gritty, broken-speaker texture.
2. **Gyrator tone stack** -- the specific boost/scoop/boost EQ curve:
   - 87 Hz bass boost sits on the guitar's fundamental for crushing lows.
   - 240 Hz scoop removes "mud," creating the characteristic hollow midrange.
   - 960/1280 Hz boost emphasizes vocal/attack frequencies.
3. **Crossover distortion from D6/D7** -- the germanium coring circuit blocks
   low-level signals, creating a unique gating/sputtering effect at low levels.
4. **All controls maxed ("dimed")** -- the classic Swedish death metal tone requires
   all knobs at maximum, where the EQ stage itself begins to clip continuously
   around 1000 Hz, adding yet another layer of distortion.
5. **The NPN/PNP complementary pair (Q6/Q7)** -- this pre-gain stage has its own
   distortion character that feeds into the IC1B clipping stage.

---

## 5. Shin-ei Uni-Vibe

### Signal Flow (Block Diagram)

```
Input -> Input Buffer/Preamp (Q1: 2SC539, common emitter)
      -> Signal Split:
         |
         +-> DRY path (direct to output mixer)
         |
         +-> WET path:
              -> Phase Shift Stage 1 (Q, LDR1, C = 15 nF)
              -> Phase Shift Stage 2 (Q, LDR2, C = 0.22 uF / 220 nF)
              -> Phase Shift Stage 3 (Q, LDR3, C = 470 pF)
              -> Phase Shift Stage 4 (Q, LDR4, C = 4 nF)
              |
      -> Output Mixer (Chorus mode: dry + wet; Vibrato mode: wet only)
      -> Output Buffer
      -> Output

LFO (Phase Shift Oscillator) -> Lamp Driver (Darlington pair)
                              -> Incandescent Bulb (12V/0.04A)
                              -> 4x LDRs (surrounding bulb)
```

### Active Components

| Component | Part Number | Type | Role |
|-----------|-------------|------|------|
| Q1 (input) | 2SC539 | NPN BJT (high gain) | Input preamp |
| Q2-Q10 (signal path) | 2SC838c | NPN BJT | Phase shift stages, mixers, buffers |
| Q11-Q12 (LFO/driver) | MPSA13 (Darlington) or 2SC838c | NPN BJT/Darlington | LFO oscillator, lamp driver |
| LDR1-LDR4 | Vactec V5C2/V5C4 or CLM6000 | Photoresistor (CdS) | Variable resistance in phase shift |
| D (LFO) | 1N914 | Silicon | LFO waveform shaping |
| D (power) | 1N4001-1N4007 | Rectifier | Full-wave bridge rectifier |
| Lamp | 12V / 0.04A incandescent | Miniature bulb | Light source for LDRs |

### Phase Shift Stages -- The Core of the Uni-Vibe

Each of the four stages is a **first-order all-pass filter** built from a discrete
BJT (not op-amp) phase splitter with an LDR providing the variable resistance:

```
           Vcc
            |
           [Rc]
            |
Input --[C]--+---> Output (phase-shifted)
             |
            [Q] (BJT common emitter)
             |
           [LDR] (variable resistance to ground)
             |
            GND
```

The phase shift at any given frequency depends on the RC time constant formed by
the fixed capacitor (C) and the LDR resistance (which varies with light intensity).

**The Mismatched Capacitor Values (Critical for Uni-Vibe Character):**

| Stage | Capacitor Value | Approximate Center Frequency* |
|-------|----------------|-------------------------------|
| 1 | 15 nF (0.015 uF) | ~100 Hz |
| 2 | 220 nF (0.22 uF) | ~7 Hz |
| 3 | 470 pF | ~3.4 kHz |
| 4 | 4 nF (0.004 uF) | ~400 Hz |

*Center frequencies are approximate and depend on LDR resistance at any given moment.

**Why the mismatch matters:**
- A standard phaser (MXR Phase 90) uses IDENTICAL capacitors in all stages,
  producing evenly-spaced notches that sweep together uniformly.
- The Uni-Vibe's wildly different capacitor values mean each stage operates at
  a fundamentally different frequency range.
- The notches in the frequency response are unevenly spaced.
- As the LDRs sweep, different frequency ranges are affected at different rates.
- This creates a complex, non-uniform phase response that sounds more like a
  spinning speaker (Leslie) than a standard phaser.

### LDR + Lamp Interaction (Critical for DSP Modeling)

**The incandescent bulb's thermal behavior:**
- Unlike an LED (which responds essentially instantly to voltage changes), an
  incandescent bulb operates thermally.
- The filament must heat up to produce light, creating a **thermal lag** between
  the driving voltage and light output.
- The heating time constant differs from the cooling time constant (the bulb
  cools slower than it heats), creating **asymmetric modulation**.
- At faster LFO speeds, this asymmetry becomes more pronounced.

**LDR response characteristics:**
- CdS (cadmium sulfide) photoresistors have their own nonlinear response:
  - Resistance does NOT vary linearly with light intensity.
  - The rise time (resistance decreasing as light increases) is faster than
    the fall time (resistance increasing as light decreases).
  - Typical resistance range: 50K (bright) to 250K+ (dark).
- Combined with the bulb's thermal lag, the LDR sweep produces a distinctly
  non-sinusoidal modulation waveform even when driven by a sine LFO.

**LDR placement and matching:**
- All four LDRs surround a single bulb in a reflective metal enclosure.
- Each LDR is at a slightly different distance/angle from the bulb.
- The LDRs are NOT matched -- each has different tolerances and response curves.
- This means each phase shift stage is modulated slightly differently in both
  depth and timing, contributing to the complex, organic character.

### LFO Circuit

**Type: Phase Shift Oscillator**

- Uses 2 transistors (Q11, Q12) in a feedback configuration.
- Produces a **skewed, non-symmetric sine wave** (not a pure sine or triangle).
- The asymmetric LFO waveform has a faster rise than fall (or vice versa),
  creating the characteristic "double-pulse" or "galloping" feel.
- 1N914 diode(s) in the LFO circuit contribute to the waveform asymmetry.
- Speed controlled by a 250K dual-ganged potentiometer.

### Chorus vs Vibrato Mode

**Chorus Mode:**
- Output = Dry signal + Wet (phase-shifted) signal mixed ~50/50.
- The combination of dry and phase-shifted signals creates frequency-dependent
  constructive/destructive interference (the "phaser" effect).
- Perceived as "chorus" because the modulating phase shift creates subtle
  pitch modulation when mixed with the dry signal.

**Vibrato Mode:**
- Output = 100% Wet (phase-shifted) signal only. Dry signal is cut.
- Without the dry reference, the ear perceives the phase modulation as
  pure pitch vibrato.
- More extreme, "wobbly" effect.

### Key Component Values

**Resistors (selected critical values):**
| Value | Qty | Role |
|-------|-----|------|
| 150 ohm | 1 | Current limiting |
| 1K (1/2W) | 1 | Lamp/driver current |
| 4.7K | 17 | Collector/emitter resistors in phase shift stages |
| 47K | 8 | Various bias, including lamp driver base series |
| 100K | 11 | Collector-base feedback, various bias |
| 1.2M | 2 | High-impedance bias |
| 2.2M | 1 | Input bias |

**Capacitors (non-phase-shift):**
| Value | Qty | Role |
|-------|-----|------|
| 1 uF / 25V electrolytic | 16 | Coupling, bypass throughout circuit |
| 100 uF / 25V | 2 | Power supply filtering |
| 220 uF / 25V | 1 | Power supply |
| 1000 uF / 25V | 1 | Main power supply filter |

**Potentiometers:**
| Value | Function |
|-------|----------|
| 250K dual-gang | SPEED (LFO rate) |
| 1K trimpot (PCB mount) | Bias adjustment |

**Power Supply:**
- Original: 24 VAC transformed down to ~15V regulated (7815 regulator).
- Full-wave bridge rectifier (4x 1N4001-1N4007).

### What Makes the Uni-Vibe Sound Like a Uni-Vibe

1. **Mismatched phase shift capacitors** -- 470 pF, 4 nF, 15 nF, 220 nF span
   nearly three decades of value, creating unevenly spaced notches that sound
   like a rotating speaker rather than a standard phaser.
2. **Incandescent bulb thermal lag** -- the bulb's slow thermal response creates
   asymmetric modulation that no LED or JFET-based design can replicate without
   explicit modeling of the thermal time constants.
3. **LDR nonlinear response** -- CdS photoresistors have different rise and fall
   times, adding another layer of asymmetry to the modulation shape.
4. **Unmatched LDRs** -- each stage is modulated slightly differently, unlike
   a standard phaser where matched JFETs ensure uniform modulation.
5. **Discrete BJT phase splitters** -- the transistors add their own nonlinearity
   (gain compression, slight distortion) that op-amp-based phasers do not have.
6. **Asymmetric LFO waveform** -- the phase shift oscillator produces a skewed
   sine that creates the "throbbing" or "galloping" feel.
7. **DSP modeling challenge**: Eventide found that accurately modeling the Uni-Vibe
   required measuring 640 unique LDR resistance curves for each LDR, representing
   every combination of Speed and Intensity settings -- the nonlinear, history-
   dependent behavior of the lamp/LDR system cannot be captured with a simple
   sine-wave modulation of a linear parameter.

---

## Comparative Summary: Clipping Topologies

| Pedal | Clipping Type | Diodes | Position | Symmetry |
|-------|--------------|--------|----------|----------|
| DS-1 | Hard clip | 1N4148 (Si) | Shunt to ground after op-amp | Symmetric |
| DS-2 | Soft + Hard | 1SS-133/1SS-188FM | Feedback loop + shunt | Mode-dependent |
| RAT | Hard clip | 1N914/1N4148 (Si) | Shunt to ground after op-amp | Symmetric |
| HM-2 | Soft + Coring + Hard | IC feedback + 1S188FM (Ge) series + 1S2473 (Si) shunt | Three stages | Asymmetric soft clip |
| Uni-Vibe | N/A (modulation effect) | N/A | N/A | N/A |

## Comparative Summary: Tone/Filter Character

| Pedal | Tone Topology | Character |
|-------|--------------|-----------|
| DS-1 | Passive Big Muff Pi mixer (HP + LP) | Mid-scoop at ~500 Hz |
| DS-2 | Discrete transistor EQ | Mode I: flat; Mode II: 900 Hz mid-boost |
| RAT | Simple RC low-pass filter | Variable LPF, 480 Hz - 32 kHz |
| HM-2 | Active gyrator EQ (simulated inductors) | 87 Hz boost, 240 Hz scoop, 960/1280 Hz boost |
| Uni-Vibe | Phase shift network | 4-stage mismatched allpass, not a filter |

## DSP Implementation Notes

### Key Modeling Considerations Per Pedal

**DS-1:**
- Model the transistor booster's asymmetric clipping separately from the op-amp's
  hard clipping. Two-stage waveshaper.
- The Big Muff Pi tone control can be modeled as a variable crossfade between
  a low-pass and high-pass filtered version of the signal.

**DS-2:**
- The discrete op-amp (JFET differential pair) needs separate modeling from a
  standard IC op-amp -- it will have different slew rate and nonlinear behavior.
- Mode switching requires two signal path models or a conditional path.

**RAT:**
- The LM308 slew rate limiting is critical and should be modeled explicitly
  (not just the diode clipping). A slew-rate limiter before the waveshaper
  is essential.
- The Filter control is a simple one-pole low-pass: H(s) = 1 / (1 + s*R*C).
- C3 (30 pF) compensation cap affects the gain stage's frequency response
  and must be included in the transfer function.

**HM-2:**
- Three separate clipping stages must be modeled in series.
- The germanium coring (D6/D7) is unusual: it's a deadband nonlinearity
  (signal below 300 mV is zeroed). Model as: if |x| < 0.3V then y = 0,
  else y = x - sign(x)*0.3V.
- The gyrator EQ is best modeled as parametric EQ bands at the specified
  frequencies with the measured Q values and boost/cut ranges.
- The EQ stage clipping at extreme settings adds a fourth distortion mechanism.

**Uni-Vibe:**
- Four first-order all-pass filters with different fixed capacitors and a
  shared, nonlinearly-modulated resistance parameter.
- The LDR/lamp system requires a thermal model: a first-order (or higher)
  IIR filter on the LFO output to simulate thermal lag, with different
  time constants for heating vs cooling.
- Each LDR should ideally have slightly different response curves.
- The LFO should produce an asymmetric waveform (not a pure sine).
- Wet/dry mixing ratio determines chorus (50/50) vs vibrato (100% wet) mode.

---

## Sources

- [ElectroSmash - Boss DS-1 Distortion Analysis](https://www.electrosmash.com/boss-ds1-analysis)
- [ElectroSmash - ProCo Rat Analysis](https://www.electrosmash.com/proco-rat)
- [Boss HM-2 Heavy Metal Circuit Analysis - David Ross Musical Instruments](https://davidrossmusicalinstruments.com/boss-hm-2-circuit-analysis/)
- [Boss HM-2 Analysis - Electronic States (Atomium Amps)](https://atomiumamps.tumblr.com/post/139197356031/boss-hm-2-analysis)
- [PedalPCB Forum - Tone Stacks Part 3: Tube Screamer, HM-2 & Gyrators](https://forum.pedalpcb.com/threads/tone-stacks-part-3-tube-screamer-hm-2-gyrators.8557/)
- [Boss DS-2 Turbo Distortion - Hobby Hour Schematic](https://www.hobby-hour.com/electronics/s/boss-ds2-turbo-distortion.php)
- [Boss DS-2 Turbo Distortion - Mirosol](https://mirosol.kapsi.fi/2014/09/boss-ds-2-turbo-distortion/)
- [Freestompboxes.org - Boss DS-2 Turbo Distortion](https://www.freestompboxes.org/viewtopic.php?t=6746)
- [Geofex - Uni-Vibe Technical](http://www.geofex.com/article_folders/univibe/univtech.htm)
- [Montagar - Uni-Vibe Build Notes](https://www.montagar.com/~patj/univibe.txt)
- [Catalinbread - Shin-Ei Uni-Vibe](https://catalinbread.com/blogs/kulas-cabinet/shin-ei-uni-vibe)
- [Eventide Audio - What's So Special About the Uni-Vibe?](https://www.eventideaudio.com/blog/whats-so-special-about-the-uni-vibe/)
- [CCRMA Stanford - Digital Grey Box Model of the Uni-Vibe (DAFx2019)](https://ccrma.stanford.edu/~champ/files/DAFx2019_paper_31.pdf)
- [Guitar Pedal X - A Brief Hobbyist Primer on Clipping Diodes](https://www.guitarpedalx.com/news/news/a-brief-hobbyist-primer-on-clipping-diodes)
- [Wikipedia - Uni-Vibe](https://en.wikipedia.org/wiki/Uni-Vibe)
- [Wikipedia - Pro Co RAT](https://en.wikipedia.org/wiki/Pro_Co_RAT)
- [Aion FX - Straylight (Uni-Vibe clone) Documentation](https://aionfx.com/app/files/docs/straylight_documentation.pdf)
- [Boss DS-2 Stock Diode Configuration - PedalPCB Forum](https://forum.pedalpcb.com/threads/boss-ds-2-stock-diode-configuration-or-did-the-previous-owner-mess-with-this-thing.18435/)
