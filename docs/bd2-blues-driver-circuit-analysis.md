# Boss BD-2 Blues Driver: Complete Circuit Analysis for Digital Replication

## Part 1: Overview & Architecture

### What Makes the BD-2 Unique

The Boss BD-2 Blues Driver (1995) is fundamentally different from op-amp-based overdrives like the Tube Screamer. Where the TS uses an IC op-amp (JRC4558) with diodes in the feedback loop to create a compressed, mid-humped sound, the BD-2 uses **discrete JFET-based operational amplifiers** that clip gracefully when overdriven — behaving more like a tube amplifier than a traditional pedal.

Key differentiators from the Tube Screamer:
- **No mid-hump**: Relatively flat frequency response (the TS has a pronounced 720Hz peak)
- **Transparent tone**: Preserves the guitar's natural voicing and the amp's character
- **Touch-sensitive dynamics**: At low gain, clean signals pass with minimal distortion; at higher input levels, the discrete op-amps smoothly saturate
- **Dual-stage gain structure**: Two discrete op-amps clip progressively, like cascaded preamp tubes
- **Rail-to-rail clipping**: The discrete op-amps saturate against the supply rails (~0V and ~8V), not at fixed diode thresholds

### Published Specifications (from Boss)
- **Input Impedance**: 1 Mohm
- **Output Impedance**: 1 kohm
- **Recommended Load Impedance**: 10 kohm or greater
- **Current Draw**: 13 mA (at DC 9V)
- **Power Supply**: 9V DC (center negative) or 9V battery

### Overall Signal Flow

```
Guitar Input
    |
    v
[JFET Input Buffer] -- Q10 (2SK184-GR), unity gain, high Zin
    |
    v
[First Discrete Op-Amp Gain Stage] -- Q3/Q11 differential pair + Q9 output (2SK184-GR + 2SA1049-GR)
    |--- Negative feedback via GAIN1 pot (250KA dual gang, first section)
    |--- D3, D4 back-to-back 1SS133 diode clipping (to ground, inter-stage)
    |
    v
[Fixed Fender-Style Tone Stack] -- passive RC network (fixed Treble=0, Bass=10, Mid≈6)
    |
    v
[Second Discrete Op-Amp Gain Stage] -- Q13/Q14 differential pair + Q12 output (2SK184-GR + 2SA1049-GR)
    |--- Negative feedback via GAIN2 pot (250KA dual gang, second section)
    |--- D7, D8, D9, D10 back-to-back 1SS133 diode clipping (to ground)
    |
    v
[TONE Control] -- passive treble cut (C100=18nF, C101, TONE pot 250KA)
    |
    v
[IC1 Output Stage] -- M5218AL op-amp with gyrator bass boost
    |--- Gyrator: R25, R26, C21, C22 (56nF) → virtual inductance ~3H
    |--- Creates ~6dB bass peak at ~120Hz
    |--- Q7 emitter follower output buffer
    |
    v
[LEVEL Pot] -- master volume (250KA)
    |
    v
Output
```

---

## Part 2: Power Supply

### Voltage Regulation
The BD-2 runs on 9V DC but internally regulates to approximately **8V** for the audio stages:

- **D2** (S5688G): Reverse polarity protection diode
- **D11** (RD5.6EB3): 5.6V Zener diode (used in power supply regulation)
- **C25**: Power supply filter capacitor
- **R31, C26, Q5** (2SC2459-GR): **Capacitance multiplier** circuit — Q5 makes C26 appear (hFE+1) times larger, providing excellent ripple rejection without a bulky electrolytic
- **R32, R33**: Voltage divider creating a **4.0V bias reference** (Vref), filtered by C28. This biases the audio stages to mid-rail

### Supply Rails
- V+ = ~8V (after the drop through Q5 capacitance multiplier)
- Vref = ~4.0V (mid-rail bias point for the discrete op-amps)
- The discrete op-amps can swing from near 0V to near 8V — this is the **hard clipping ceiling**

---

## Part 3: Semiconductor Inventory (Complete)

### JFETs
| Designation | Part Number | Role |
|-------------|-------------|------|
| Q3, Q10, Q11, Q13, Q14 | **2SK184-GR** | Discrete op-amp differential pairs + input buffer |
| Q4, Q6, Q8 | **2SK118Y** | Bypass switching / routing FETs |

**2SK184-GR Characteristics** (critical for DSP modeling):
- Type: N-channel JFET, low noise, low cutoff
- Vgs(off): -0.2V to -1.5V (GR grade)
- Idss: 2.6 to 6.5 mA (GR grade)
- gm: ~15 mS typical (at Vds=10V, Vgs=0V)
- Vds(max): 50V
- Pd(max): 200 mW
- Equivalent: 2SK209-GR (SMD), J201 (similar but not identical), BF245A (close)

### PNP BJTs (Discrete Op-Amp Output Stages)
| Designation | Part Number | Role |
|-------------|-------------|------|
| Q9, Q12 | **2SA1049-GR** | Output transistor of each discrete op-amp |

**2SA1049-GR Characteristics**:
- Type: PNP silicon BJT
- hFE: typically 200-400
- fT: ~150 MHz
- Low noise, used as common-emitter gain/output stage in the discrete op-amps

### NPN BJTs (Support)
| Designation | Part Number | Role |
|-------------|-------------|------|
| Q1, Q5, Q7 | **2SC2459-GR** | Capacitance multiplier (Q5), bass boost gyrator (Q7), support (Q1) |
| Q2, Q15, Q16 | **2SC2458-GR** | Bypass switching / routing |

### Diodes
| Designation | Part Number | Role |
|-------------|-------------|------|
| D1, D3, D4, D5, D6, D7, D8, D9, D10 | **1SS133** | Signal clipping and input protection |
| D2 | **S5688G** | Reverse polarity protection |
| D11 | **RD5.6EB3** | 5.6V Zener diode (power supply) |

**1SS133 Characteristics** (critical for clipping model):
- Type: Silicon signal diode
- Vf: ~0.55-0.6V at typical signal currents
- Equivalent to: 1N4148, 1N914, BAY38, BA318
- Two back-to-back = ~1.1-1.2V peak-to-peak clipping threshold per pair

### Op-Amp IC
| Designation | Part Number | Role |
|-------------|-------------|------|
| IC1 | **M5218AL** | Output stage buffer/bass boost (Mitsubishi, SIP-8 format) |

The M5218 is a dual op-amp in SIP-8 package widely used by Boss. Only one section (IC1a) is used for the output stage with gyrator bass boost.

---

## Part 4: The Discrete Op-Amp — Core Architecture

This is the heart of the BD-2 and what makes it sound like an amplifier. The BD-2 contains **two identical discrete op-amp stages**, each constructed from:

### Topology: JFET Differential Pair + PNP Common-Emitter Output

```
                    V+ (~8V)
                      |
                    [Rc load]
                      |
         Vout <-------+-----> To feedback network
                      |
                   Q9 (2SA1049 PNP)
                  B ___|___ E
                 /         \
                |           |
           [R_bias]      [R_emitter]
                |           |
               GND         V+

    Vref (4V) ---+--- Vin (+)
                 |     |
              Q3 (2SK184)  Q11 (2SK184)
              D __|__       __|__ D
             /       \    /       \
            S         S  S         S
            |              |
            +------+-------+
                   |
                 [R_tail]
                   |
                  GND
```

**How it works:**
1. **Q3 and Q11** (2SK184-GR JFETs) form a **differential pair** — the input stage. One gate receives the audio signal (via coupling cap), the other receives the feedback signal (via the gain pot network). Their common source connection goes through a tail resistor to ground
2. **Q9** (2SA1049-GR PNP BJT) is the **output/gain stage** — a common-emitter amplifier driven by the differential pair's drain output. Being PNP with collector to ground, it provides voltage gain and the output signal
3. The output is taken from Q9's collector and fed back through the negative feedback network

### Why This Topology Creates Amp-Like Clipping

**This is the critical insight for DSP modeling:**

- When the output tries to swing beyond V+ (~8V) or below ground (0V), the discrete op-amp **saturates against the rails**
- Unlike IC op-amps that clip abruptly at well-defined rail voltages, discrete circuits exhibit **soft saturation** — the JFET differential pair gradually loses transconductance as the signal increases, and the PNP output stage enters saturation smoothly
- The JFET diff pair clips differently than BJT pairs — JFETs have a **square-law** transfer characteristic (Id = Idss*(1-Vgs/Vp)^2) which naturally creates **2nd-harmonic-dominant distortion** (even harmonics, tube-like)
- The PNP output stage adds its own saturation characteristic on top

**This is fundamentally different from:**
- Tube Screamer: Hard diode clips in op-amp feedback → abrupt, compressed
- RAT: Hard diode clips to ground after op-amp → harsh, aggressive
- Big Muff: Cascaded CE BJT stages with diode clips → thick, sustaining

**The BD-2 clips like a tube amp because it IS a miniature tube amp in topology** — differential input stage → gain stage → output stage, all clipping against the supply rails.

---

## Part 5: First Discrete Op-Amp Gain Stage (Detailed)

### Signal Entry
The guitar signal passes through the JFET input buffer (Q10, 2SK184-GR source follower, unity gain, ~1 Mohm input impedance) and enters the first discrete op-amp through a DC-blocking coupling capacitor.

### Feedback Network (Gain Control)

The first section of the **250KA dual-gang potentiometer** (GAIN1) controls the negative feedback:

```
Q9 collector (output)
    |
    +--[GAIN1 pot: 0-250K]--+--[R8: 1.5K]--+
    |                                        |
    [C4: 150nF]                         [R9: ground ref]
    |                                        |
    GND                                     GND

    [C5: ~100-120pF across feedback path — treble rolloff]
```

**Gain calculation:**
- **Minimum gain** (GAIN1 = 0 ohms): Gain ≈ 1 + R8/R9 (approximately unity to a few dB — essentially bypassed)
- **Maximum gain** (GAIN1 = 250K): Gain ≈ 1 + (250K + R8)/R9 — approximately **40+ dB** (100x)
- The gain peaks between **2 kHz and 3 kHz** due to the frequency-shaping caps

**Frequency shaping in feedback:**
- **R5 + C4** (1.5K + 150nF): Creates a high-pass in the feedback path at **fc = 1/(2*pi*1500*150e-9) ≈ 707 Hz**. This means frequencies BELOW 707 Hz receive LESS feedback, hence MORE gain. BUT — this is in the feedback path, so actually: frequencies below 707 Hz are rolled off MORE at the output because C4 blocks low-frequency feedback, making the feedback stronger at low frequencies. Wait — let me be precise:
  - C4 to ground in the feedback network creates a **bass cut** at the output (HPF behavior) starting at ~700 Hz
  - The gain stage rolls off bass below ~700 Hz
- **C5** (~100-120pF): Rolls off treble above the gain peak (~3 kHz), stabilizes the op-amp against oscillation

**Result**: The first stage has a **bandpass-like gain curve** peaking at 2-3 kHz, with bass rolloff below 700 Hz and treble rolloff above 3 kHz. Maximum gain: ~40 dB (100x).

### Inter-Stage Diode Clipping (D3, D4)

After the first discrete op-amp output, **D3 and D4** (1SS133 silicon diodes, back-to-back) clip to ground:

```
First opamp output ---+--- [to inter-stage tone stack]
                      |
                   D3 ^  D4 v  (anti-parallel 1SS133)
                      |
                     GND
```

- These are **shunt diodes to ground** (NOT in the feedback path like a TS)
- Clipping threshold: ~±0.55V per diode = ~1.1 Vpp
- At low gain settings, these provide **soft clipping** before the second stage
- At high gain, the first discrete op-amp hits the rails (0V/8V) BEFORE the diodes engage, so the rail saturation dominates
- **D1 and D5** protect the JFET gates of the first op-amp — they never conduct during normal operation

**Important note from circuit analysis**: D3, D4, D5, D6 reportedly have **negligible effect on tone** at most settings. The circuit functions nearly identically with or without them. The real clipping comes from the discrete op-amp rail saturation.

---

## Part 6: Inter-Stage Fixed Tone Stack

Between the two discrete op-amp gain stages sits a **passive tone-shaping network** that emulates a Fender-style amp tone stack with fixed control positions:

### Component Network
```
From first opamp output
    |
    [C9]---+---[R14]---+--- To second opamp input
           |            |
         [C11]        [R16]
           |            |
         [R17]        [C12]
           |            |
          GND          GND

    [R18 to GND]
```

Components: **C9, C11, C12, R14, R16, R17, R18**

### Fixed EQ Curve
This network is equivalent to a Fender tone stack with:
- **Treble**: at minimum (0)
- **Mid**: at approximately 6 (moderate)
- **Bass**: at maximum (10)

The result is:
- **Bass**: Mostly preserved (compensates for the ~700 Hz bass rolloff from the first gain stage's C4)
- **Mids**: Moderate level — NOT scooped like a Fender at noon, NOT boosted like a TS
- **Treble**: Rolled off — prevents the 2-3 kHz gain peak from becoming harsh

### Frequency Response
At the output of this tone stack:
- Below 100 Hz: ~3 dB of bass rolloff
- 100-500 Hz: Relatively flat
- Above 1 kHz: Gentle treble reduction (~6 dB, via C10+C13+R15 network)
- Overall insertion loss: Significant (this is a passive network)

**Critical DSP note**: This is a **high-impedance network** that is very sensitive to loading by the subsequent stage. The second discrete op-amp's input impedance affects the tone stack's response. This interaction must be modeled accurately.

---

## Part 7: Second Discrete Op-Amp Gain Stage

### Identical Topology, Different Voicing

The second stage uses the same JFET differential pair (Q13, Q14: 2SK184-GR) + PNP output (Q12: 2SA1049-GR) architecture, but with slightly different feedback component values:

**Feedback network:**
- **GAIN2 pot**: Second section of the 250KA dual-gang pot
- Feedback resistors and caps are slightly different from stage 1
- Maximum gain: just under **40 dB**
- Frequency response: **flat from 100 Hz to ~6 kHz** (broader than stage 1)

**Key behavioral difference**: The second stage has a FLATTER frequency response than the first stage. The first stage shapes the tone; the second stage adds gain with relatively neutral frequency character.

### Main Clipping Network (D7, D8, D9, D10)

```
Second opamp output ---+--- [to TONE control]
                       |
                    D7 ^    D9 ^
                    D8 v    D10 v   (two pairs of anti-parallel 1SS133)
                       |
                      GND
```

- **D7+D8**: One pair of back-to-back 1SS133 diodes
- **D9+D10**: Second pair of back-to-back 1SS133 diodes
- These are in **parallel** — the doubled configuration provides slightly different clipping character than a single pair
- Stock clipping threshold: ~**2.2 Vpp** (due to the signal path impedance at this point)

### Clipping Hierarchy (The Key to BD-2's Dynamic Response)

**This is THE most important thing to understand about the BD-2:**

1. **At low gain** (Gain pot at 7-9 o'clock): Signal stays below all clipping thresholds. Clean boost with tonal shaping. The fixed tone stack and EQ contouring are the primary effect.

2. **At moderate gain** (Gain pot at 9-12 o'clock): The **first-stage diodes (D3/D4) begin soft-clipping**. The second stage adds gain but hasn't saturated yet. Result: **smooth, blues-like breakup** with predominantly **2nd-order harmonics** (even harmonics = tube-like warmth).

3. **At high gain** (Gain pot at 12-3 o'clock): The **second discrete op-amp saturates against the supply rails** (0V to 8V). This is hard clipping from the amplifier stage itself — like a tube amp's output stage driven into saturation. The diodes are now largely irrelevant because the op-amp clips before the signal reaches diode threshold. Result: **3rd-order harmonics added** (odd harmonics = more aggressive, but still musical).

4. **At maximum gain** (Gain pot fully clockwise): Both stages are saturating. Total end-to-end gain: ~**52 dB** max. Heavy distortion but still more dynamic than a typical distortion pedal because the progressive multi-stage saturation preserves envelope dynamics. Enters "fuzz territory" per circuit analysts.

**Because the gain pot is dual-gang**, both stages increase simultaneously — but the second stage, being after the tone stack's insertion loss, clips FIRST. This cascaded progressive clipping is what makes the BD-2 feel like playing through a cranked amp.

---

## Part 8: Tone Control (Post-Clipping)

After the second gain stage and clipping network, the signal passes through a **passive treble-cut tone control**:

### Circuit
```
From D7-D10 output ---[C100: 18nF]---+---[TONE pot: 250KA]--- To output stage
                                       |
                                     [C101]
                                       |
                                      GND
```

### Behavior
- **Tone at minimum** (fully CCW): Maximum treble cut. C100 (18nF) determines the cutoff frequency
- **Tone at noon** (12 o'clock): Approximately flat response
- **Tone at maximum** (fully CW): Minimal filtering, brightest tone

The cutoff frequency with C100 = 18nF:
- fc ≈ 1/(2*pi*250000*18e-9) ≈ **35 Hz** (at full resistance — meaning almost everything passes)
- At low resistance: fc rises dramatically, cutting treble

**Keeley Phat Mod**: Changes C100 to 33nF (lowers cutoff, passes more bass) and adds a switchable 68nF or 82nF cap in parallel for "Phat mode" — even more bass content through the tone circuit.

---

## Part 9: Output Stage — M5218 Op-Amp with Gyrator Bass Boost

### IC1a (M5218AL) — Active Output Stage

The M5218AL is a dual op-amp (SIP-8 package). One section (IC1a) provides the output amplification and bass boost. The other section may be used for additional buffering or is unused.

### Gyrator Bass Boost Circuit

This is one of the BD-2's signature features — a **gyrator-based bass boost** in the output stage:

```
IC1a output ---[feedback loop]--- IC1a inverting input
                    |
              +--[R25]--+--[C21: 56nF]--+
              |         |                |
            [R26]     [C22: 56nF]       GND
              |         |
             GND      [Q7 collector]
                        |
                      [R_e]
                        |
                       GND
```

**Gyrator parameters:**
- Virtual inductance: L = C22 * R25 * R26 ≈ **3 H** (with the specific R values)
- Series resistance: ~1.2 kohm
- This virtual inductor resonates with C21 (56nF) to create a **peak filter**

**Important**: The gyrator is in the **feedback loop** of IC1a. A notch in the feedback = a **peak at the output**. This creates approximately **6 dB of gain boost centered at ~120 Hz**.

**Q7** (2SC2459-GR or similar low-noise NPN) is part of the gyrator circuit. PedalPCB analysis notes that Q7's noise is multiplied by R22/R20 = **392x (52 dB)**, making this transistor a potential noise source. Using a low-noise type (MPSA18, BC549C) is recommended.

### Output Buffer
The output of IC1a drives **Q7** configured as an emitter follower for low output impedance (~1 kohm as specified). The LEVEL pot (250KA) sits between the output stage and the final output jack.

---

## Part 10: Complete Capacitor Reference

Based on cross-referencing schematics, mod guides (Keeley, Fromel, Wampler, PedalPCB Boneyard), and circuit analyses:

| Designation | Stock Value | Type | Function |
|-------------|-------------|------|----------|
| C1 | 100nF (0.1uF) | Film | Input coupling (to first gain stage) |
| C2 | 10nF (0.01uF) | Film | Second stage input coupling |
| C3 | 10uF | Electrolytic | First opamp stabilization/decoupling |
| C4 | 150nF (0.15uF) | Film | First stage feedback network — bass rolloff (fc≈707Hz with R5=1.5K) |
| C5 | 100-120pF | Ceramic | First stage feedback — treble rolloff / stability |
| C6 | 10uF | Electrolytic | Coupling/decoupling |
| C7 | 10uF | Electrolytic | Coupling/decoupling |
| C8 | 100pF | Ceramic | Second stage feedback — treble rolloff / stability |
| C9 | ~390-560pF | Ceramic | Inter-stage tone stack |
| C10 | 5.6nF | Film | Treble reduction network (pre-stage 2) |
| C11 | — | Film | Inter-stage tone stack |
| C12 | — | Film | Inter-stage tone stack |
| C13 | 10uF | Electrolytic | Coupling |
| C14 | 47nF (0.047uF) | Film | Tone shaping (Keeley mod → 100nF for more bass) |
| C15 | 10uF | Electrolytic | Coupling |
| C17 | 56nF | Film | Gyrator resonance cap (with virtual inductor) |
| C18 | 100pF (BD-2) / 47pF (BD-2w) | Ceramic | Gyrator — "not audible" difference |
| C19 | 5.6nF (BD-2) / 6.8nF (BD-2w) | Film | Tone shaping |
| C20 | 100pF | Ceramic/Mica | Filtering |
| C21 | 56nF | Film | Gyrator peak filter (resonates with virtual L≈3H → ~120Hz) |
| C22 | 56nF | Film | Gyrator — sets virtual inductance (L=C22*R25*R26) |
| C23 | 47pF | Ceramic/Mica | Filtering |
| C25 | — | Electrolytic | Power supply filter |
| C26 | 220pF | Ceramic | Capacitance multiplier (appears as 220pF*(hFE+1)) |
| C28 | — | Electrolytic | Vref filter |
| C100 | 18nF | Film | TONE control cutoff cap |

**Note on C21/C22**: The Analog Is Not Dead analysis flagged that these are **56nF, not 5.6nF** as sometimes mislabeled on schematics. The 56nF value is confirmed by the gyrator resonance frequency calculation: with L≈3H and C=56nF, f = 1/(2*pi*sqrt(3*56e-9)) ≈ 123 Hz, matching the measured ~120 Hz bass peak.

---

## Part 11: Key Resistor Reference

| Designation | Value | Function |
|-------------|-------|----------|
| R2 | 220K (BD-2) / 2M (BD-2w) | Input bias / HPF with C1 |
| R5 | 1.5K | First stage feedback — sets bass rolloff with C4 (fc≈707Hz) |
| R6 | 4.7K (adjustable to ~8.2K) | JFET differential pair tail / balance |
| R7 | ~4.7-8.2K | Second stage JFET balance |
| R8 | 1.5K (BD-2) / 1.2K (BD-2w) | First stage feedback — sets minimum gain |
| R9 | ~10K | First stage feedback ground reference |
| R10 | ~9.1K | Second stage feedback |
| R11 | ~15K | Second stage feedback |
| R14 | — | Inter-stage tone stack |
| R15 | 1.5K (BD-2) / 1.2K (BD-2w) | Second stage — "raises max gain by ~2dB" at 1.2K |
| R16, R17, R18 | — | Inter-stage tone stack (R18 = 1M stock, 10K in BD-2w) |
| R20 | 1.2K (BD-2) / 820R (BD-2w) | Gyrator |
| R21 | — | Tone network |
| R22, R23 | — | Gyrator (R22/R20 = 392x noise multiplier) |
| R25, R26 | — | Gyrator — set virtual inductance L=C22*R25*R26≈3H |
| R27 | 6.8K | Lowers minimum gain of 2nd stage (mod reference) |
| R29 | 4.7K | Lowers minimum gain of 1st stage (mod reference) |
| R31 | — | Capacitance multiplier (with Q5, C26) |
| R32, R33 | — | Vref voltage divider (output: 4.0V) |
| R34 | 10K | Gain-related |
| R36 | ~33K | EQ slope resistor |
| R37 | 220K | Tone/EQ network |

**BD-2w changes**: R2: 220K→2M (higher input impedance), R8: 1.5K→1.2K (more gain), R15: 1.5K→1.2K (+2dB gain), R18: 1M→10K (significant bass tightening), R20: 1.2K→820R (gyrator tuning change).

---

## Part 12: Transfer Function & Clipping Behavior

### Input-Output Transfer Curve

At different Gain pot positions, the BD-2 exhibits fundamentally different clipping behavior:

**Gain at 0% (7 o'clock) — Clean Boost:**
```
Output
  ^
  |      /
  |     /
  |    /   (nearly linear, ~unity gain)
  |   /
  |  /
  | /
  +---------> Input
```
- Gain: ~0-6 dB
- No clipping — signal stays well within headroom
- Tone stack provides gentle EQ contouring
- Output gyrator adds ~6 dB bass boost at 120 Hz

**Gain at 25% (9 o'clock) — Edge of Breakup:**
```
Output
  ^
  |    .--~~
  |   /
  |  /     (soft compression on peaks)
  | /
  |/
  +---------> Input
```
- Gain: ~15-20 dB
- First-stage diodes (D3/D4) begin soft-clipping on peaks
- Second stage is still mostly clean
- **This is the "sweet spot"** — touch-sensitive blues tone
- Harmonics: predominantly 2nd order (tube-like warmth)

**Gain at 50% (12 o'clock) — Moderate Overdrive:**
```
Output
  ^     ___
  |    /
  |   /
  |  |     (S-curve, progressive saturation)
  |   \
  |    \___
  +---------> Input
```
- Gain: ~25-35 dB
- First stage: Diodes clipping, approaching rail saturation
- Second stage: Beginning to hit rails on peaks
- **Progressive saturation** — quiet notes are clean, loud notes break up
- Harmonics: Mix of 2nd and 3rd order
- This is what makes the BD-2 "dynamic" — the multi-stage progressive clipping preserves the guitar's envelope

**Gain at 100% (fully CW) — Heavy Overdrive/Fuzz:**
```
Output
  ^     ____
  |    |
  |    |
  |    |    (hard rail clipping, near-square wave on loud signals)
  |    |
  |    |____
  +---------> Input
```
- Gain: ~45-52 dB (end-to-end maximum)
- Both stages saturating against supply rails (0V and 8V)
- Diodes are irrelevant — the op-amps clip before reaching diode thresholds
- Enters "fuzz territory" — sustained, thick distortion
- Still somewhat more dynamic than a typical fuzz because the two stages clip progressively

### How the Gain Knob Changes the Circuit

The dual-gang 250KA pot acts as a **variable resistor in the negative feedback path** of both discrete op-amps simultaneously:

- **Gain at 0**: Pot resistance = 0 ohms. Maximum feedback. Gain ≈ 1 + Rmin/R_ground ≈ few dB
- **Gain at max**: Pot resistance = 250K ohms. Minimum feedback. Gain ≈ 1 + 250K/R_ground ≈ 40+ dB per stage

The gain changes are applied to both stages simultaneously via the ganged pot, but the second stage clips first because it receives a signal that has already been boosted and shaped by the first stage plus the tone stack's insertion loss pushes it harder.

---

## Part 13: Frequency Response Analysis

### Overall Frequency Response (Signal Path)

**At 12 o'clock Tone setting**, the BD-2 has a remarkably flat response:
- **Below 60 Hz**: Rolling off (~6 dB/octave from input coupling caps)
- **60-100 Hz**: Bass boost from output gyrator (~6 dB peak at 120 Hz)
- **100-500 Hz**: Relatively flat, only ~3 dB of variation
- **500 Hz - 3 kHz**: Flat to slightly boosted (first stage gain peak)
- **3-6 kHz**: Beginning to roll off
- **Above 6 kHz**: Significant rolloff from C5, C8 treble-limiting caps

### Stage-by-Stage Frequency Response

**First gain stage** (at maximum gain):
- HPF below ~700 Hz (from C4/R5 in feedback)
- Gain peak at 2-3 kHz
- LPF above 3 kHz (from C5)
- Bandpass-like character — this shapes the "voice" of the BD-2

**Inter-stage tone stack**:
- Fender-type passive EQ: Treble=0, Mid≈6, Bass=10
- Compensates for first stage's bass rolloff
- Significant insertion loss (~10-15 dB)
- Additional treble reduction above 1 kHz (~6 dB via C10, C13, R15)

**Second gain stage** (at maximum gain):
- Flat from ~100 Hz to ~6 kHz
- Does NOT add additional tone shaping — provides clean gain

**Tone control**:
- Passive treble cut after clipping
- At noon: approximately flat
- At minimum: significant treble reduction above ~1 kHz

**Output gyrator**:
- +6 dB peak at ~120 Hz
- Restores the low-end warmth lost in the first stage
- This is why the BD-2 doesn't sound thin despite the first stage's 700 Hz bass rolloff

---

## Part 14: Comparison with BD-2w (Waza Craft)

The BD-2w adds a "Custom" mode (standard mode is identical to the original BD-2). Key circuit differences in Custom mode:

| Component | BD-2 (Standard) | BD-2w (Custom) | Effect |
|-----------|-----------------|----------------|--------|
| C1 | 100nF | 2.2uF | Much more bass into first stage |
| C2 | 2.2nF | 12nF | More bass into second stage |
| C8 | 100pF | 47pF | Higher treble bandwidth in stage 2 |
| C10 | 5.6nF | 6.8nF | Slightly different mid-treble shaping |
| C13 | 5.6nF | 10nF | More treble reduction |
| C14 | 18nF | 39nF | More bass in tone shaping |
| C17 | 56nF | 68nF | Gyrator shifted slightly lower |
| C18 | 100pF | 47pF | "Not audible" |
| C19 | 5.6nF | 6.8nF | Slightly different tone shaping |
| R2 | 220K | 2M | Higher input impedance |
| R8 | 1.5K | 1.2K | ~2 dB more gain in stage 1 |
| R15 | 1.5K | 1.2K | ~2 dB more gain in stage 2 |
| R18 | 1M | 10K | Significant bass tightening |
| R20 | 1.2K | 820R | Gyrator tuning |
| D5, D6 | 1SS133 | Removed | Slightly different clipping behavior |

The BD-2w Custom mode has: fuller bass, slightly more gain (~4 dB total), tighter low-end definition, and no D5/D6 protection diodes. The IC switches (IC2-1, IC2-2) engage a 10K resistor in parallel with the tone stack's bass control (R20) when in Custom mode.

---

## Part 15: DSP Implementation Strategy

### Architecture Overview

The BD-2 should be modeled as a **5-stage signal chain**:

```
Input Buffer → Gain Stage 1 (with feedback) → Tone Stack → Gain Stage 2 (with feedback) → Output Stage (gyrator EQ)
```

With diode clipping networks between stages 1→tone stack and after stage 2.

### Stage 1 & 2: Discrete Op-Amp Modeling

**Recommended approach: Soft-clipping waveshaper with frequency-dependent gain**

Each discrete op-amp stage should be modeled as:

1. **Input HPF** (from coupling cap): 1st-order HPF, fc depends on coupling cap and bias resistor
2. **Frequency-dependent gain**: The feedback network creates a frequency-dependent gain curve. Implement as a **biquad filter** whose gain is controlled by the Gain parameter:
   - Stage 1: Bandpass-like, peak at 2-3 kHz, Q depends on feedback components
   - Stage 2: Broadband, flat 100 Hz - 6 kHz
3. **Soft saturation waveshaper**: Model the JFET diff pair + PNP output stage clipping

**Waveshaper options for the discrete op-amp saturation:**

Option A — **Asymmetric tanh** (recommended for mobile):
```
// JFET square-law + PNP saturation composite
float discreteOpAmpClip(float x, float headroom) {
    // headroom = supply voltage range (nominally ~8V, normalized)
    // Asymmetric because PNP clips differently on positive vs negative swings
    float normalized = x / headroom;
    // Soft knee approaching rails
    float positive = tanh(normalized * 1.5f);  // softer positive rail
    float negative = tanh(normalized * 2.0f);  // slightly harder negative rail
    return (normalized >= 0) ? positive * headroom : negative * headroom;
}
```

Option B — **JFET transfer function** (higher accuracy):
```
// Square-law JFET model: Id = Idss * (1 - Vgs/Vp)^2
// Combined with PNP CE stage saturation
// More accurate but more expensive per sample
```

Option C — **Polynomial waveshaper with ADAA** (best anti-aliasing):
A 5th-order Chebyshev polynomial fitted to SPICE simulation data of the actual circuit, with first-order ADAA anti-aliasing. This would capture the exact shape of the real circuit's transfer function.

**Critical**: The discrete op-amps produce **predominantly even harmonics** (2nd order) at low gain, transitioning to **odd harmonics** (3rd, 5th) at high gain as they hit the rails. The waveshaper must capture this transition. A simple symmetric tanh() produces ONLY odd harmonics — the BD-2 needs **asymmetric** saturation.

### Diode Clipping Networks

The 1SS133 diodes clip to ground (shunt configuration). Model as:

```
// Shunt diode clipper (back-to-back silicon)
// Vf ≈ 0.55V per diode, n ≈ 1.0 (silicon ideality)
float diodeClipToGround(float x) {
    const float Vf = 0.55f;
    // Soft clip approaching Vf using exponential diode equation
    // Simplified: tanh-based approximation of diode I-V curve
    if (abs(x) < Vf * 0.5f) return x;  // below threshold, pass through
    return Vf * tanh(x / Vf);  // soft clip to ±Vf
}
```

**Important**: The diodes are LESS significant than the op-amp rail saturation. At high gain, the op-amps clip first. The diodes only matter at moderate gain settings where the signal reaches diode threshold but hasn't hit the rails yet. The DSP model must get this priority right — the op-amp saturation is the PRIMARY distortion mechanism.

### Inter-Stage Tone Stack

Model as a **biquad filter cascade** or transfer function derived from the passive RC network:

- Implement the Fender tone stack equations with fixed pot positions (Treble=0, Mid=0.6, Bass=1.0)
- Account for the insertion loss (~10-15 dB) — this is CRITICAL because it determines how hard the second stage is driven
- The high-impedance interaction with the second stage's input impedance should be captured in the filter coefficients

### Tone Control (Post-Clipping)

Simple 1st-order LPF with variable cutoff:
```
fc = 1 / (2 * pi * (Tone_pot_resistance) * 18e-9)
// Tone=0: R≈250K, fc≈35Hz (everything passes — minimal cut)
// Wait, that's backwards. Let me reconsider:
// Tone pot at max = minimum resistance in signal path = brightest
// Tone pot at min = maximum resistance = most treble cut
// The actual topology is a variable RC lowpass
```

### Output Gyrator Bass Boost

Model as a **parametric EQ boost**:
- Center frequency: ~120 Hz
- Gain: +6 dB
- Q: moderate (~1-2)
- Implement as a single biquad peak filter

### Anti-Aliasing Considerations

- **2x oversampling minimum** for the waveshaper stages
- The BD-2 is less prone to aliasing than hard-clipping pedals (DS-1, RAT) because:
  - The discrete op-amp saturation is inherently soft (gradual roll-into-clip)
  - The inter-stage tone stack acts as a natural anti-aliasing filter between stages
  - The treble-limiting caps (C5, C8) in the feedback networks roll off content above 3-6 kHz
- **4x oversampling** recommended if targeting desktop-quality accuracy
- **ADAA** (antiderivative anti-aliasing) on the waveshaper would allow 1x or 2x operation with good results

### Preserving Touch Sensitivity (The BD-2's Killer Feature)

The BD-2's dynamics come from three mechanisms that MUST all be present in the digital model:

1. **Progressive multi-stage clipping**: At low input levels, the signal passes through both stages without clipping. As input increases, first the diodes clip, then the second op-amp saturates, then the first op-amp saturates. Each threshold adds more distortion progressively. The model must compute each stage independently — you cannot collapse this into a single waveshaper.

2. **Frequency-dependent gain before clipping**: The first stage's bandpass character means that pick attack transients (which are treble-heavy) get more gain than the sustained fundamental. This creates a "crunch on attack, clean on sustain" dynamic that is essential to the BD-2's feel.

3. **No compression from feedback-loop diodes**: Unlike the Tube Screamer where diodes in the feedback loop create automatic gain compression (louder input = more clipping = effectively less gain), the BD-2's shunt diodes to ground do NOT compress the gain. The op-amp's gain stays constant regardless of signal level — only the output gets clipped. This preserves the full dynamic range of the input signal's envelope.

### Parameter Mapping

| User Parameter | DSP Control |
|---------------|-------------|
| **Gain** (0-1) | Simultaneously controls feedback resistance in both discrete op-amp stages. Maps to variable resistance 0-250K in each feedback network. Non-linear (audio taper) |
| **Tone** (0-1) | Controls cutoff frequency of post-clipping LPF. Maps to variable resistance 0-250K |
| **Level** (0-1) | Output volume. Linear or audio taper gain control, post-processing |

### Computational Cost Estimate

Per sample at 2x oversampling:
- Input HPF: ~4 FLOPS
- Stage 1 gain filter (biquad): ~10 FLOPS
- Stage 1 waveshaper (asymmetric tanh): ~15 FLOPS
- Stage 1 diode clipper: ~8 FLOPS
- Inter-stage tone stack (2-3 biquads): ~20-30 FLOPS
- Stage 2 gain filter (biquad): ~10 FLOPS
- Stage 2 waveshaper: ~15 FLOPS
- Stage 2 diode clipper: ~8 FLOPS
- Tone control LPF: ~5 FLOPS
- Gyrator bass boost (biquad): ~10 FLOPS
- **Total: ~105-115 FLOPS/sample at 1x, ~210-230 FLOPS at 2x oversampled**

This is comparable to the DS-1 (~180 FLOPS at 2x) and well within our mobile budget.

---

## Part 16: Known Mods and What They Reveal About the Circuit

### Keeley "Seeing Eye" / "Phat" Mod
- Replaces electrolytic caps (C1, C6, C7, C12, C13, C15) with non-polarized → cleaner coupling
- C14: 47nF → 100nF (more bass)
- C100: 18nF → 33nF + switchable 68nF parallel ("Phat mode" = even more bass)
- D7+D8 → single 1N4001 (asymmetric clipping — adds 2nd harmonics)
- Silver mica caps replace ceramics (lower noise)

**What this reveals**: The stock BD-2 is slightly bass-shy (by design — it's meant to sit well in a mix). The coupling caps are the main bass-limiting components. The diode mod confirms that asymmetric clipping adds tube-like character.

### Wampler Mods
- R36 → 33K, C34/C35 → 0.022uF, C26 → 470pF: "Marshall-type tonality before clipping"
- R50 → 100R: Flatter EQ response
- Trim pots at R37, R50, R51: Variable tone stack tuning

**What this reveals**: The stock tone stack is designed for a specific Fender-like voicing. The EQ can be radically reshaped by changing the passive component values.

### Fromel Mod
- C21/C23: 47pF → 33pF (shifts high-frequency response)
- C10: stock → 47nF (changes mid-treble shaping)
- C26: 220pF → 270pF (slightly more supply filtering)
- D3, D7, D8, D10 → 1N400x series; D9 → jumper wire
- The D9 jumper is interesting: it removes one diode from the second clipper pair, creating asymmetric clipping

**What this reveals**: The diode configuration can be simplified without losing the core character. Removing D9 (replacing with a jumper) creates a 2-diode vs 1-diode asymmetric clipper in the second stage — this adds even-order harmonics.

---

## Part 17: Summary — Key Characteristics for Accurate Digital Replication

1. **The discrete op-amp is the sound**: NOT the diodes. The JFET differential pair + PNP output stage saturating against supply rails is the primary distortion mechanism. Any DSP model that focuses on diode clipping and neglects the op-amp saturation will sound wrong.

2. **Dual-stage progressive clipping**: Both stages must be modeled independently with their own gain, frequency response, and saturation characteristics. The second stage clips first (due to tone stack insertion loss feeding it harder). Cannot be collapsed into one stage.

3. **Asymmetric saturation**: The PNP output stage clips differently on positive vs negative swings. This creates even-order harmonics (2nd, 4th) that are essential to the tube-like warmth. A symmetric tanh() is insufficient.

4. **Frequency-dependent gain in stage 1**: The bandpass character (peak at 2-3 kHz, bass rolloff at 700 Hz) shapes the tone BEFORE clipping. This is why the BD-2 doesn't sound muddy at high gain — the bass is reduced before the signal hits the clipping stages.

5. **The inter-stage tone stack matters**: It's not just EQ — its insertion loss determines the drive level into the second stage. Getting this wrong will make the gain knob's response feel wrong.

6. **The gyrator bass boost restores warmth**: After all the bass-cutting in the gain stages and tone stack, the output gyrator adds back ~6 dB at 120 Hz. Without this, the BD-2 would sound thin and harsh.

7. **Touch sensitivity comes from the gain structure**: The multi-stage topology with different clipping thresholds at each stage means quiet notes can be clean while loud notes distort. This MUST be preserved — it's the BD-2's defining characteristic.

8. **Total end-to-end gain: ~52 dB max**. More than a Klon (~30 dB), less than a Big Muff (~70+ dB).

---

## Sources

- [Analog Is Not Dead — Circuit Analysis: The Boss BD2](https://www.analogisnotdead.com/article25/circuit-analysis-the-boss-bd2)
- [Atomium Amps — Boss BD-2 Mods & Analysis](https://atomiumamps.tumblr.com/post/184586374741/boss-bd-2-mods-analysis-the-bd-2-is-bosss)
- [PedalPCB — Blues Driver BD-2 & BD-2w Breadboard Analysis (Parts 1-4)](https://forum.pedalpcb.com/threads/this-week-on-the-breadboard-blues-driver-bd-2-bd-2w-part-1.7390/)
- [Aion FX — Sapphire Amp Overdrive (BD-2 Clone)](https://aionfx.com/project/sapphire-amp-overdrive/)
- [Fromel Electronics — BD-2 Mod Kit Installation Guide](https://fromelelectronics.dozuki.com/Guide/Boss+BD-2+Blues+Driver+Mod+Kit+2022/16)
- [DIY Boss BD-2 Blues Driver "Keeley Mod"](https://www.cpearson.me.uk/2021/12/20/diy-boss-bd-2-blues-driver-keeley-mod/)
- [Gaussmarkov DIY FX — Blues Driver](https://www.freestompboxes.org/museum/gaussmarkov.net/circuits/blues-driver/)
- [Thermionic Studios — BD-2 Wiki](https://thermionic-studios.com/wiki/index.php?title=BD-2)
- [Freestompboxes.org — Boss BD-2 Blues Driver 1995 Thread](https://www.freestompboxes.org/viewtopic.php?t=1062)
- [Premier Guitar — Boss BD-2 Mods (Brian Wampler)](https://www.premierguitar.com/gear/boss-bd-2-mods)
- [AllTransistors — 2SK184 JFET Datasheet](https://alltransistors.com/mosfet/transistor.php?transistor=14775)
- [Aion FX — 2SK209-GR JFET (2SK184 equivalent)](https://aionfx.com/component/2sk209-gr-jfet/)
- [DIYStompboxes — Where is the clipping stage in the BD-2?](https://www.diystompboxes.com/smfforum/index.php?topic=66485.0)
- [DIYStompboxes — BD-2 Gain JFETs and BJT topology questions](https://www.diystompboxes.com/smfforum/index.php?topic=123528.0)
- [BD-2 and Asymmetric Clipping discussion](https://alt.guitar.narkive.com/zuysxYlX/bd-2-and-asymmetric-cliiping)
- [Guitar Pedals Visualized — Boss BD-2w](https://guitarpedalsvisualized.wordpress.com/2022/03/08/boss-bd-2w/)
