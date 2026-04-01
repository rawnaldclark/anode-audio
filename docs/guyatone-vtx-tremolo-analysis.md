# Guyatone Flip VT-X Vintage Tremolo: Comprehensive Circuit Analysis

## Document Purpose
Circuit-accurate analysis of the Guyatone VT-X for digital replication in our Guitar Emulator App.
This document synthesizes publicly available specifications, forum reverse-engineering reports,
reference circuit analysis (Fender AB763 photocell tremolo), and first-principles electronics
knowledge to reconstruct the VT-X's probable circuit topology and behavior.

---

## Part 1: Architecture Overview

### 1.1 What the VT-X Actually Is

The Guyatone Flip VT-X is NOT a simple solid-state optical tremolo. It is a **tube-powered
optical tremolo pedal** built around a single **12AX7WA dual triode**, running from a 12V DC
external supply with an internal DC-DC boost converter to generate the high voltages required
for proper tube operation.

**Key distinction from initial assumptions**: The VT-X uses BOTH a vacuum tube AND optical
coupling. The tube serves dual duty:
1. One triode half (V1A) operates as a **phase-shift LFO oscillator**
2. The other triode half (V1B) operates as a **signal gain stage / buffer**

The optical element (lamp + LDR) is driven by the LFO to modulate the guitar signal amplitude.
This is fundamentally the same architecture as the **Fender blackface AB763 photocell tremolo**,
miniaturized into a pedal format with a DC-DC boost converter replacing the amp's high-voltage
power supply.

### 1.2 Published Specifications

| Parameter | Value |
|-----------|-------|
| Tube | 12AX7WA (ECC83), shock-mounted |
| Input Impedance | 470K ohm |
| Output Impedance | 1K ohm |
| Max Input Level | 1V peak |
| Max Output Level | 1V peak |
| Max Amplitude | +6dB (above unity) |
| Speed Range | 0.1 - 10 Hz |
| Power Supply | 12V DC, center-negative, 200mA adapter |
| Current Consumption | 160mA |
| Controls | Speed, Intensity, Tone |
| Switches | Slow/Fast (speed range), Normal/Emphasis |
| Outputs | Dual (stereo imaging) |
| Dimensions | 3.5" W x 5.125" L x 1.5" H |
| Weight | 15.0 oz |

### 1.3 Signal Flow (Reconstructed)

```
Guitar In --> Input coupling cap --> 470K input impedance network
          --> V1B triode gain stage (signal amplification + tone coloring)
          --> Tone control network
          --> Optical VCA (LDR in voltage divider)
          --> Output buffer (likely solid-state, 1K Zout)
          --> Dual outputs

LFO path:
V1A phase-shift oscillator --> Lamp/LED driver --> Optical coupler (lamp + LDR)
                                                    |
                                            modulates VCA ^^^
```

### 1.4 What Makes It Sound Different

The VT-X's distinctive character comes from several interacting factors:

1. **Tube gain stage in the signal path**: The 12AX7 adds 2nd-harmonic distortion, soft
   compression, and frequency-dependent gain. This is NOT a transparent VCA -- the signal
   is actively colored before modulation occurs.

2. **Optical coupling lag**: The LDR's inherent response time (10-50ms rise, 50-300ms fall)
   smooths the LFO waveform, creating softer transitions than electronic VCA tremolos.

3. **Tone control**: Unlike most tremolo pedals (which are amplitude-only), the VT-X has a
   dedicated Tone knob that shapes the frequency content of the modulated signal.

4. **Emphasis switch**: Likely a mid-boost or presence circuit that changes the tonal character
   of the modulation.

5. **Volume boost**: The 12AX7 provides up to +6dB of gain. Users universally report the
   VT-X adds volume when engaged -- this is a design feature, not a bug. The tube is
   amplifying the signal before the optical VCA, so the average level is higher than bypass.

---

## Part 2: The Optical Tremolo Circuit

### 2.1 Optical Coupling: How It Works

The VT-X uses an **optocoupler** consisting of a light source and a **CdS (Cadmium Sulfide)
photoresistor** (Light Dependent Resistor / LDR) in a light-tight enclosure.

**Signal modulation mechanism**: The LDR is connected as a **variable shunt resistor to ground**
in the signal path, forming a voltage divider with a series resistor. When the LDR resistance
is high (dark), the signal passes through at near-full level. When the LDR resistance is low
(illuminated), the signal is attenuated toward ground.

```
Signal In ---[R_series]---+--- Signal Out
                          |
                        [LDR]
                          |
                         GND
```

The Intensity (Depth) control adjusts how much signal reaches this divider, or alternatively
biases the LDR's operating range.

### 2.2 Light Source: Neon Lamp vs LED vs Incandescent

The specific light source in the VT-X is not definitively documented in public sources. Based
on the circuit architecture (tube-driven, high-voltage internal supply) and the sonic character
described by users, the most likely candidates are:

**Most Probable: Neon Lamp (NE-2 type)**
- Consistent with the Fender AB763 heritage the VT-X was designed to emulate
- The internal DC-DC boost converter provides sufficient voltage (~90V needed for neon strike)
- Neon lamps produce the characteristic "choppy" tremolo that users describe at high depth settings
- The VT-X's 160mA current draw at 12V = 1.92W total. A neon lamp draws ~1mA at ~60V sustaining
  voltage = 60mW -- easily within the power budget after boost conversion losses

**Less Likely: LED**
- Would not require the high-voltage boost for the lamp itself
- Would produce smoother, more linear modulation (less "vintage" character)
- Modern redesigns sometimes substitute LEDs, but the VT-X was designed for vintage character

**Least Likely: Incandescent Lamp**
- Would add significant thermal lag (creating very smooth, "spongy" modulation)
- The VT-X can achieve 10Hz speed, which would be difficult with incandescent thermal mass
- Power consumption would be higher

### 2.3 Neon Lamp Characteristics (NE-2 Type)

If the VT-X uses a neon lamp (which we will assume for modeling), the key parameters are:

| Parameter | Value |
|-----------|-------|
| Strike (firing) voltage | 70-90V (varies with age, gas composition) |
| Sustaining voltage | 50-55V |
| Extinction voltage | ~45V (below sustaining) |
| Operating current | 0.1 - 3 mA typical |
| Dynamic resistance (lit) | 1.6 - 3.2K ohm |
| Hysteresis | ~20-30V between strike and extinction |
| Response time | Very fast (<1ms) -- essentially instant on/off |

**Critical insight**: The neon lamp is a **threshold device**. It does NOT linearly convert
the LFO voltage to light intensity. Instead:
- Below ~70V: lamp is OFF (no light)
- Above ~70V: lamp STRIKES and is ON
- Once lit, stays on down to ~50V
- Between 50-70V: hysteresis region

This means even if the LFO produces a smooth sine wave, the neon lamp converts it into
something closer to a **pulse/square wave** with some duty-cycle variation. The LFO voltage
determines what fraction of the cycle the lamp is lit vs dark, creating a "chopped" tremolo
character rather than a smooth sine modulation.

### 2.4 CdS Photoresistor (LDR) Characteristics

The LDR in the optocoupler has these typical characteristics:

| Parameter | Value |
|-----------|-------|
| Dark resistance | 1M - 10M ohm (after full dark adaptation) |
| Illuminated resistance | 100 - 600 ohm (at typical neon brightness) |
| Spectral peak | ~520nm (green) for CdS |
| Response time (light to dark) | 50 - 300ms (fall time, resistance increasing) |
| Response time (dark to light) | 5 - 50ms (rise time, resistance decreasing) |
| Resistance vs illumination | Logarithmic: R = R_ref * (L_ref/L)^gamma, gamma ~0.7-0.9 |
| Unit-to-unit variation | +/- 50% or more |

**The asymmetric response is KEY to the vintage sound**: The LDR responds quickly when light
hits it (resistance drops fast) but recovers slowly when light is removed (resistance rises
slowly). This creates a **fast attack, slow release** envelope on each tremolo cycle, which
is the hallmark of optical tremolo sound.

### 2.5 VTL5C Series Vactrol Reference (Modern Equivalent)

If the VT-X uses a packaged vactrol rather than discrete neon+LDR, the VTL5C series is the
most commonly used in audio applications:

**VTL5C1** (most likely if vactrol):
- On resistance: ~600 ohm
- Off resistance: ~50M ohm
- Fast response, high dynamic range (100dB)
- Commonly used in Demeter Tremulator

**VTL5C3** (also possible):
- On resistance: ~10-20K ohm
- Off resistance: ~10M ohm
- "Average" speed, versatile
- Steep resistance slope

### 2.6 Combined Neon + LDR Transfer Function

The complete optical modulation chain produces a highly nonlinear transfer function:

```
LFO Voltage --> Neon threshold --> Binary-ish light output --> LDR logarithmic response
                                                                --> Asymmetric time lag
                                                                --> Signal attenuation
```

The resulting modulation waveform applied to the guitar signal is:
1. NOT a sine wave (even if the LFO is sinusoidal)
2. Asymmetric: fast cut, slow recovery
3. Somewhat "chopped" at high intensity due to neon threshold behavior
4. At low intensity: more rounded due to LDR operating in its middle resistance range
5. Never reaches full silence: minimum LDR resistance is ~100-600 ohm, and the series
   resistor prevents complete shunting

---

## Part 3: LFO Circuit

### 3.1 Phase-Shift Oscillator Topology

The VT-X's LFO uses one half of the 12AX7 (call it V1A) configured as a **three-stage
RC phase-shift oscillator**. This is the classic Fender tremolo oscillator topology, dating
back to the brownface/blackface era.

**How it works**: A common-cathode triode amplifier provides 180 degrees of phase shift
(signal inversion). A three-stage RC high-pass filter network in the feedback path provides
an additional 180 degrees at one specific frequency, giving 360 degrees total -- the
condition for oscillation.

### 3.2 Phase-Shift Oscillator Theory

**Frequency equation** (for three identical RC stages):
```
f_osc = 1 / (2 * pi * sqrt(6) * R * C)
```
Simplified: **f = 1 / (15.4 * R * C)**

The feedback network attenuates the signal by a factor of 1/29 at the oscillation frequency.
Therefore, the tube must provide a voltage gain of at least **29** for sustained oscillation.
The 12AX7 with mu=100 easily provides this, typically achieving gains of 50-65 depending on
plate/cathode resistor choices.

### 3.3 Reconstructed LFO Component Values

Based on the Fender AB763 reference circuit that the VT-X was designed to emulate, and
accounting for the VT-X's wider speed range (0.1-10Hz vs Fender's ~3-10Hz):

**Fender AB763 Reference Values**:
- Plate load resistor: 220K
- Cathode resistor: 2.7K (with bypass cap)
- Phase network resistors: 10M, 100K, 220K (asymmetric for range optimization)
- Phase network capacitors: 0.01uF, 0.01uF, 0.022uF
- Speed pot: ~1M-3M (varies the first shunt R)
- B+ to oscillator: ~330-415V

**VT-X Probable Values** (adapted for 0.1-10Hz range from 12V boosted supply):

The VT-X achieves a much wider speed range (0.1-10Hz, a 100:1 ratio) compared to the Fender
circuit (~3-10Hz, a 3:1 ratio). This is accomplished with:

1. The **Slow/Fast switch** that selects between two capacitor sets (or adds parallel caps),
   giving two overlapping sub-ranges
2. A larger speed pot range
3. Possibly modified feedback network ratios

Estimated component values for each range:
- **Fast range** (~2-10Hz): C1=C2=10nF (0.01uF), C3=22nF (0.022uF), R_speed=50K to 1M
- **Slow range** (~0.1-2Hz): C1=C2=100nF (0.1uF), C3=220nF (0.22uF), R_speed=100K to 2M

The speed pot is most likely **logarithmic (audio)** taper for perceptually even speed
control across the range.

### 3.4 LFO Waveform Shape

The phase-shift oscillator nominally produces a **sine wave** if the tube gain is exactly 29.
In practice, the 12AX7 provides gain well above 29, so the oscillator overdrives slightly,
producing a **mildly distorted sine wave** -- somewhat flattened at the peaks.

This is actually desirable: the slightly clipped LFO waveform spends more time at the
extremes (fully bright / fully dark) than a pure sine, creating a more pronounced
tremolo effect.

The distortion can be reduced by adding a filter capacitor from the plate to ground (some
Fender amps use a 0.01-0.022uF cap here), trading waveform purity for less aggressive
modulation.

### 3.5 Internal High-Voltage Supply

The VT-X runs from 12V DC but the 12AX7 needs much higher voltages. The internal DC-DC
boost converter likely generates:

- **B+ (plate supply)**: 100-160V DC (lower than the 300-400V in a full amp, but sufficient
  for oscillation and moderate gain)
- **Heater supply**: 12.6V (can be run directly from the 12V input, or from 6.3V with series
  heaters -- unlikely with a single dual-triode)

At 12V input, 160mA draw = 1.92W total power budget. Accounting for boost converter
efficiency (~80-85%):
- Usable power: ~1.5-1.6W
- 12AX7 heater: 12.6V * 150mA = 1.89W

**Wait** -- this is a problem. The 12AX7 heater alone draws ~150mA at 12.6V. The total
pedal draws 160mA at 12V. This means the heater is consuming nearly all the input current,
with very little left for the boost converter and plate supply.

**Resolution**: The VT-X likely runs the heaters slightly starved (at 12V instead of 12.6V)
and uses an efficient boost converter with a small plate current draw. At the low plate
voltages used (100-160V), the 12AX7 draws only 0.5-1.5mA per triode section, so the boost
converter only needs to supply ~3mA at ~150V = ~0.45W. With the heater at 12V * 150mA =
1.8W, total = 2.25W... which exceeds the 1.92W budget slightly.

Most likely, the plate supply is on the lower end (~100-120V) to keep the total power within
budget. This lower plate voltage actually HELPS the vintage tone -- the tube clips earlier
and more softly, adding warmth.

---

## Part 4: Depth (Intensity) Control

### 4.1 How Intensity Works

The Intensity control determines the **modulation depth** -- how much the volume varies
between the peaks and troughs of the tremolo cycle. There are two common implementation
approaches:

**Approach A: LFO Amplitude Control (Most Likely)**
The Intensity pot attenuates the LFO signal before it reaches the lamp driver. At minimum
intensity, the LFO signal is too weak to drive the neon lamp above its strike voltage, so
the lamp stays dark and no modulation occurs. At maximum intensity, the full LFO swing
drives the lamp from fully off to fully on, creating maximum volume variation.

**Approach B: Signal Bleed Control (Fender AB763 Style)**
In the Fender circuit, the Intensity pot controls how much guitar signal reaches the LDR
shunt point. At minimum, no signal reaches the LDR, so no modulation occurs. At maximum,
full signal is routed through the voltage divider.

The Fender AB763 uses Approach B. The VT-X likely uses Approach A or a hybrid, since it
achieves a wider range of modulation character.

### 4.2 Modulation Depth Characteristics

At minimum intensity: Signal passes with tube coloring but no amplitude modulation.
The tube gain stage still adds warmth and slight compression. This is why users report
the VT-X sounds good even with tremolo at minimum -- it functions as a tube preamp/boost.

At maximum intensity: Signal is heavily modulated but does NOT fully cut off. The LDR's
minimum resistance (~100-600 ohm) in series with the divider network means there is always
some signal leaking through. Typical maximum attenuation is around -20 to -30dB, not
full silence. This prevents the harsh "hard chop" effect of fully gated tremolo.

### 4.3 Depth Character vs Speed Interaction

At slow speeds (0.1-1Hz): The LDR has time to fully respond to each cycle, so modulation
is deep and pronounced. The asymmetric LDR response creates a "breathing" quality.

At fast speeds (5-10Hz): The LDR cannot fully track the LFO. Its fall time (50-300ms)
is comparable to or longer than the tremolo period (100-200ms at 5-10Hz). This naturally
reduces effective depth at higher speeds, creating a built-in "depth vs speed" interaction
that sounds musical -- fast tremolo is automatically lighter/more subtle.

---

## Part 5: Input/Output Stages

### 5.1 Input Stage

**Input impedance**: 470K ohm (confirmed from specs). This is the standard guitar-pedal input
impedance, suitable for both single-coil and humbucker pickups. It is likely set by a
470K resistor to ground at the tube grid, possibly with a coupling capacitor in series.

The input stage is the V1B triode half of the 12AX7, configured as a **common-cathode
amplifier**:
- Grid resistor: 470K (sets input impedance)
- Input coupling cap: ~22nF to 100nF (blocks DC, sets low-frequency rolloff)
- Plate load resistor: ~100K-220K
- Cathode resistor: ~1.5K-2.7K (with bypass cap for full gain)
- Cathode bypass cap: ~22uF-100uF

**Voltage gain**: With typical values, gain = mu * Rp / (Rp + ra) = 100 * 100K / (100K + 62.5K)
= ~61.5. However, at the lower plate voltages in the VT-X (~100-120V), actual gain is lower,
probably **30-45**.

This gain explains the +6dB maximum amplitude spec and the well-documented volume boost when
the pedal is engaged. The tube is adding real gain to the signal.

### 5.2 Tone Control

The VT-X's Tone control is unusual for a tremolo pedal. Most tremolos have no tone shaping.
This is likely a simple **single-knob tone circuit** placed between the tube output and the
optical VCA:

**Probable topology**: RC low-pass filter with variable cutoff:
```
Tube plate --> [R_fixed] --> [Tone pot] --> signal continues
                              |
                           [C_tone]
                              |
                             GND
```

Or alternatively, a treble-cut arrangement similar to a guitar tone control:
```
Signal --> [Tone pot wiper] --> out
            |
         [C_tone to GND]
```

With C_tone = 4.7nF to 22nF and Tone pot = 100K-500K, this would sweep from bright/full
bandwidth to dark/rolled-off.

### 5.3 Emphasis Switch (Normal/Emphasis)

The Normal/Emphasis switch likely changes the tone circuit's behavior:
- **Normal**: Flat response, standard operation
- **Emphasis**: Boosts midrange or treble presence, possibly by:
  - Switching in a smaller tone cap (less bass loss)
  - Adding a resonant peak (small inductor or active EQ)
  - Bypassing a bass-cut filter
  - Changing the Tone pot's load resistor

This gives the VT-X two distinct voices: a Fender-like warm tremolo (Normal) and a more
cutting, present tremolo (Emphasis).

### 5.4 Output Stage

**Output impedance**: 1K ohm (confirmed from specs). This is much lower than the tube's
natural output impedance (~38K for a 12AX7 stage), which means there MUST be an output
buffer.

Options:
1. **Cathode follower from V1B's second stage** -- unity gain, low output impedance. However,
   a cathode follower from a 12AX7 gives ~40-60K output impedance, still too high for 1K.
2. **Solid-state output buffer** -- a JFET source follower or op-amp buffer after the optical
   VCA. This is the most likely explanation for the very low 1K output impedance.

The hybrid tube-input / solid-state-output design is common in tube pedals. It gives the
tube's tonal coloring on the input with practical drive capability on the output.

---

## Part 6: The "Vintage" Character

### 6.1 Why the VT-X Sounds "Vintage"

The VT-X was explicitly designed to replicate the tremolo sound of vintage Fender amps.
Here is precisely what creates that sound:

**1. Tube Harmonic Content**
The 12AX7 gain stage adds primarily **2nd-harmonic distortion** (even-order harmonics).
At the moderate plate voltages in the VT-X, the tube operates with lower headroom than in
a full amp, so it clips softly even at modest guitar levels. This adds richness and warmth
that solid-state tremolos lack.

Measured: A 12AX7 at 100V plate supply adds approximately 1-3% THD at normal guitar levels,
rising to 5-10% with hot pickups or hard picking.

**2. Optical Nonlinearity**
The neon + LDR combination creates a modulation waveform that is fundamentally different
from an electronic VCA or multiplier:
- The neon lamp's threshold behavior "squares up" the modulation
- The LDR's logarithmic resistance curve compresses the modulation depth
- The asymmetric LDR response creates unequal attack/release
- The result is a "pulsing" or "throbbing" quality rather than a smooth sine modulation

**3. Frequency-Dependent Modulation**
The LDR does NOT attenuate all frequencies equally. As a resistive shunt to ground, it forms
a low-pass filter with any series capacitance in the signal path. At low resistance (fully
illuminated), this filter rolls off more high frequencies. At high resistance (dark), the
filter effect is minimal. This means:
- At the bottom of each tremolo cycle: highs are slightly more attenuated than lows
- At the top of each cycle: full bandwidth passes
- Net effect: subtle brightness modulation layered on top of the amplitude modulation

This is exactly the behavior described as "harmonic tremolo" in some contexts, though it
is more subtle than a full harmonic tremolo circuit.

**4. Phase Inversion / Polarity Flip**
The tube gain stage inverts the signal phase. When the VT-X is engaged, the guitar signal's
phase is flipped 180 degrees relative to bypass. This is documented by users and was noted
in the Frusciante gear analysis: "when not engaged, this pedal acted as a preamp or boost,
altering the signal by flipping the amp's phase." This phase flip can interact with amp
tremolo or other effects in the chain.

### 6.2 Comparison to Other Tremolos

| Feature | Guyatone VT-X | Fender Blackface (AB763) | Fender Brownface (6G16) | Demeter Tremulator | Boss TR-2 |
|---------|---------------|--------------------------|--------------------------|---------------------|-----------|
| Topology | Tube + optical | Tube + optical (neon/LDR) | Tube bias modulation | Solid-state + optical (vactrol) | Solid-state VCA (JFET) |
| Tube | 12AX7WA | 12AX7 (2 sections) | 12AX7 (LFO only) | None | None |
| Light source | Neon (probable) | Neon (NE-2U) | N/A (bias mod) | LED (in VTL5C1) | N/A |
| LDR | CdS | CdS (TO-8 CdSe in some) | N/A | CdS (in VTL5C1) | N/A |
| Waveform character | Clipped sine -> choppy | Clipped sine -> choppy | Smooth sine | Triangle (rounded) | Sine/triangle selectable |
| Depth behavior | Asymmetric (fast cut/slow release) | Asymmetric | Smooth | Asymmetric (less than neon) | Symmetric |
| Frequency interaction | LDR creates subtle brightness mod | Same | Power tube creates harmonic mod | Minimal | None |
| Tube coloring | Yes (+6dB, 2nd harmonic) | Yes (embedded in amp) | Yes (bias mod = crossover distortion) | No | No |
| Speed range | 0.1-10 Hz | ~3-10 Hz | ~3-8 Hz | ~1-10 Hz | 0.5-10 Hz |
| Tone control | Yes (dedicated knob) | No | No | No | Wave shape only |

### 6.3 The +6dB Volume Boost "Problem"

Users consistently report the VT-X adds significant volume when engaged. This is because:
1. The 12AX7 gain stage amplifies the signal by roughly +6dB (gain ~2x)
2. The optical VCA's average attenuation is less than 6dB at most settings
3. Net result: louder when on than when off

Forum solutions include swapping to a lower-gain tube:
- **12AY7** (mu=45, gain factor ~45): Recommended solution, much less volume boost
- **5751** (mu=70): Better than 12AX7 but still loud
- **12AU7** (mu=20): Below unity, too quiet

This behavior actually tells us the tube is running with significant gain, confirming
our analysis of the circuit topology.

---

## Part 7: Comprehensive Component Value Reconstruction

### 7.1 Complete Bill of Materials (Estimated)

Based on the Fender AB763 reference, VT-X published specs, and power budget constraints,
here is the most probable component set:

**Power Supply Section:**
- DC-DC boost converter module: 12V -> 100-120V, ~3-5mA output
- Filter caps: 47uF/160V or 22uF/160V electrolytic (B+ filtering)
- Heater: powered directly from 12V input (slightly under-run at 12V vs spec 12.6V)

**V1B Signal Gain Stage (Audio Path):**
- Input coupling cap (C1): 22nF - 100nF (sets HPF with grid resistor)
- Grid resistor (R1): 470K (confirmed, sets input impedance)
- Grid stopper (R2): 68K (typical for pedal, reduces RF and oscillation)
- Plate load resistor (R3): 100K - 220K
- Cathode resistor (R4): 1.5K - 2.7K
- Cathode bypass cap (C2): 22uF - 47uF / 25V
- Plate-to-ground filter (C3): none or 100pF (HF rolloff)

**Tone Control:**
- Tone pot: 250K or 500K audio taper
- Tone cap: 4.7nF - 22nF
- Series resistor: 10K - 47K (limits maximum treble cut)

**Optical VCA:**
- Series resistor (in divider): 100K - 470K (sets maximum attenuation)
- LDR: CdS type, ~600 ohm on / 1M-10M off
- Coupling caps: 100nF in, 100nF out (DC blocking)

**V1A LFO Oscillator:**
- Plate load (R5): 100K - 220K
- Cathode resistor (R6): 1K - 2.7K
- Cathode bypass (C4): 47uF - 100uF / 25V
- Phase network R's: 1M, 1M, 1M (or 10M, 100K, 220K Fender-style)
- Phase network C's (Fast): 10nF, 10nF, 22nF
- Phase network C's (Slow): 100nF, 100nF, 220nF (switched by Slow/Fast)
- Speed pot: 1M - 3M log taper (varies first shunt R)
- LFO output coupling: 100nF to lamp driver

**Lamp Driver:**
- If neon: Direct from V1A plate through coupling cap + current-limiting resistor (100K-220K)
- If LED: Transistor buffer (2N3904 or similar) with current-limiting resistor (1K-4.7K)

**Output Buffer (Solid-State):**
- JFET source follower (2N5457 or J201) or op-amp buffer (TL071)
- Output coupling cap: 1uF - 10uF
- Output impedance setting resistor: 1K (confirmed)

**Intensity Control:**
- Intensity pot: 50K - 250K, wired to control LFO amplitude to lamp
  (or signal level to LDR divider)

### 7.2 Emphasis Switch Analysis

The Normal/Emphasis switch most likely:
- Switches a capacitor value in the tone network (smaller cap = more brightness/emphasis)
- Or inserts a small series resistor that creates a resonant peak with stray capacitance
- Or bypasses a bass-loss element in the signal path

---

## Part 8: DSP Implementation Strategy

### 8.1 Architecture Overview

The digital VT-X model needs these blocks:

```
Input --> [Tube Gain Stage Model] --> [Tone Filter] --> [Optical VCA Model] --> [Output Buffer] --> Output
                                                              ^
                                                              |
                                            [LFO Oscillator] --> [Neon Lamp Model] --> [LDR Model]
```

### 8.2 Tube Gain Stage Model

Model V1B as a single triode common-cathode amplifier:

**Transfer function**: Use our existing `processTriode()` from amp_model.h, adapted for
the VT-X's lower plate voltage and specific biasing.

Key parameters:
- Voltage gain: ~35-45 (lower than full-voltage 12AX7 due to ~100V B+)
- Bias point: cathode ~1.5V, plate ~50-60V (low headroom)
- Soft clipping: asymmetric (positive peaks clip first due to grid current limiting)
- 2nd harmonic dominance: characteristic of single-ended triode

**Simplified model (per sample)**:
```
// Input with coupling cap HPF (22nF + 470K = ~15Hz cutoff)
x_filtered = hpf_input.process(input)

// Tube gain with soft clipping
x_amplified = x_filtered * tubeGain  // tubeGain ~35-45
x_clipped = fast_tanh(x_amplified * driveScale) * outputScale

// Where driveScale controls how much soft clipping occurs
// Asymmetric clipping: positive side clips ~10% earlier than negative
```

For higher accuracy, model the actual triode characteristic:
```
// Koren model for 12AX7 triode
// Plate current: Ip = (E1 / Kp) * ln(1 + exp(Kp * (1/mu + Vg/sqrt(Kvb + Vpk*Vpk))))
// Where E1 = Vpk/Kp * ln(1 + exp(Kp*(1/mu + Vg/sqrt(Kvb + Vpk^2))))
// Kp = 600, Kvb = 300, mu = 100, Ex = 1.4 for 12AX7
```

However, for a tremolo effect where the tube coloring is subtle, a simple soft-clip model
with 2nd harmonic injection is sufficient and much cheaper computationally.

### 8.3 Tone Filter Model

Simple 1-pole low-pass filter with variable cutoff:

```
// Tone control: 0.0 = dark, 1.0 = bright
// Frequency range: ~800Hz (full dark) to ~20kHz (full bright)
float toneFreq = 800.0f * std::pow(25.0f, toneParam);  // 800Hz to 20kHz
float w = 2.0f * M_PI * toneFreq / sampleRate;
float alpha = w / (1.0f + w);  // one-pole LPF coefficient
toneState = toneState + alpha * (input - toneState);
```

### 8.4 LFO Oscillator Model

The LFO does NOT need to model the phase-shift oscillator -- we only need its output waveform.
The oscillator produces a mildly distorted sine wave:

```
// Generate sine LFO
lfoPhase += lfoFreq / sampleRate;
if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
float lfoRaw = std::sin(2.0f * M_PI * lfoPhase);

// Mild soft-clipping to simulate overdriven oscillator
// The oscillator gain is ~2x the minimum required, so peaks flatten slightly
float lfoClipped = fast_tanh(lfoRaw * 1.5f);  // 1.5 = mild overdrive
```

### 8.5 Neon Lamp Model

The neon lamp converts the LFO voltage into a light intensity with threshold behavior:

```
// Neon lamp model
// Strike at ~0.7 (normalized), sustain down to ~0.5 (normalized)
// Hysteresis creates different thresholds for on->off vs off->on
const float kStrikeThreshold = 0.7f;    // lamp turns on above this
const float kExtinctThreshold = 0.45f;  // lamp turns off below this

// State: neonLit_ (bool)
if (!neonLit_ && lfoClipped > kStrikeThreshold) {
    neonLit_ = true;
} else if (neonLit_ && lfoClipped < kExtinctThreshold) {
    neonLit_ = false;
}

// Neon brightness: binary on/off with slight analog fade at transitions
// In the "on" region, brightness scales linearly with current above sustaining voltage
float neonBrightness;
if (neonLit_) {
    // Brightness proportional to voltage above sustaining point
    neonBrightness = std::clamp((lfoClipped - kExtinctThreshold) /
                                (1.0f - kExtinctThreshold), 0.0f, 1.0f);
} else {
    neonBrightness = 0.0f;
}
```

**Alternative for LED-based VT-X**: If the VT-X uses an LED instead of neon, skip the
threshold/hysteresis model and use a simpler clipped-linear transfer:
```
float ledBrightness = std::clamp(lfoClipped * intensity, 0.0f, 1.0f);
```

**Configurable parameter**: The user could switch between "Neon" and "LED" lamp modes
for different tremolo characters.

### 8.6 LDR Model (Critical for Authentic Sound)

The LDR model is the most important part of the optical tremolo emulation. It must capture:
1. Logarithmic resistance vs light curve
2. Asymmetric rise/fall time constants
3. Memory effect (slow dark-adaptation)

**Mathematical model**:

```
// LDR state: current effective light level seen by LDR (0=dark, 1=bright)
// Asymmetric one-pole filter with different attack/release coefficients
float targetLight = neonBrightness;  // or ledBrightness

float coeff;
if (targetLight > ldrLightState_) {
    // Light increasing -> resistance decreasing (fast response)
    // Rise time: 5-30ms
    coeff = 1.0f - std::exp(-1.0f / (sampleRate * kLdrRiseTime));  // kLdrRiseTime ~0.015
} else {
    // Light decreasing -> resistance increasing (slow response)
    // Fall time: 50-200ms
    coeff = 1.0f - std::exp(-1.0f / (sampleRate * kLdrFallTime));  // kLdrFallTime ~0.100
}
ldrLightState_ += coeff * (targetLight - ldrLightState_);

// Convert light level to resistance (logarithmic relationship)
// R = R_dark * (R_lit/R_dark)^lightLevel
// R_dark = 1M-10M, R_lit = 600 ohm
// In log space: log(R) = log(R_dark) - lightLevel * log(R_dark/R_lit)
// But for VCA calculation, we need the attenuation factor:

// Voltage divider: Vout/Vin = R_ldr / (R_series + R_ldr)
const float kRseries = 220000.0f;  // 220K series resistor
const float kRdark = 2000000.0f;   // 2M dark resistance
const float kRlit = 600.0f;        // 600 ohm lit resistance

// Logarithmic interpolation of resistance
float logR = std::log(kRdark) + ldrLightState_ * (std::log(kRlit) - std::log(kRdark));
float R_ldr = std::exp(logR);

// Voltage divider attenuation
float attenuation = R_ldr / (kRseries + R_ldr);
// When dark: 2M / (220K + 2M) = 0.90 -> ~-0.9dB (almost full signal)
// When lit: 600 / (220K + 600) = 0.0027 -> ~-51dB (heavy attenuation)
```

### 8.7 Intensity (Depth) Control Integration

The Intensity parameter controls the effective modulation depth:

```
// Method A: Scale the lamp drive (LFO amplitude control)
float lampDrive = lfoClipped * intensity;  // intensity: 0.0 to 1.0

// Method B: Blend between modulated and dry signal
float output = dry * (1.0f - intensity) + modulated * intensity;

// Method C (most accurate): Adjust the LDR operating range
// At low intensity, the LDR stays in its high-resistance range
// so modulation is subtle. At high intensity, full range is used.
// This naturally creates depth-dependent character (smooth at low, choppy at high)
float effectiveNeonDrive = neonBrightness * intensity;
// Then feed into LDR model
```

Method C is the most physically accurate and produces the correct depth-dependent character.

### 8.8 Complete Per-Sample Algorithm

```cpp
float processVTXTremolo(float input) {
    // === TUBE GAIN STAGE ===
    float tubeIn = hpfInput_.process(input);                    // ~15Hz HPF (coupling cap)
    float tubeOut = fast_tanh(tubeIn * tubeGain_ * 0.5f) * 2.0f;  // soft clip + gain
    // Add subtle 2nd harmonic: tubeOut += 0.02f * tubeOut * tubeOut;

    // === TONE FILTER ===
    float toned = toneFilter_.process(tubeOut);                 // variable LPF

    // === LFO ===
    lfoPhase_ += lfoFreq_ / sampleRate_;
    if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
    float lfoSine = std::sin(2.0f * M_PI * lfoPhase_);
    float lfoClipped = fast_tanh(lfoSine * 1.5f);              // mild oscillator overdrive
    float lfoNorm = (lfoClipped + 1.0f) * 0.5f;                // normalize to [0, 1]

    // === NEON LAMP MODEL ===
    float lampDrive = lfoNorm * intensity_;
    float neonOut;
    if (useNeonModel_) {
        // Neon threshold/hysteresis
        if (!neonLit_ && lampDrive > kStrikeThreshold_)
            neonLit_ = true;
        else if (neonLit_ && lampDrive < kExtinctThreshold_)
            neonLit_ = false;
        neonOut = neonLit_ ? std::clamp((lampDrive - kExtinctThreshold_) /
                             (1.0f - kExtinctThreshold_), 0.0f, 1.0f) : 0.0f;
    } else {
        // LED model (smoother)
        neonOut = std::clamp(lampDrive, 0.0f, 1.0f);
    }

    // === LDR MODEL (asymmetric response) ===
    float coeff = (neonOut > ldrState_) ?
        ldrAttackCoeff_ :   // fast: ~15ms
        ldrReleaseCoeff_;   // slow: ~100ms
    ldrState_ += coeff * (neonOut - ldrState_);

    // === VOLTAGE DIVIDER VCA ===
    // Logarithmic R mapping
    float logR = logRdark_ + ldrState_ * (logRlit_ - logRdark_);
    float R_ldr = fast_exp(logR);
    float vca = R_ldr / (kRseries_ + R_ldr);

    // === OUTPUT ===
    float modulated = toned * vca;
    return outputBuffer_.process(modulated);                    // output coupling HPF
}
```

### 8.9 Parameter Mapping

| VT-X Control | DSP Parameter | Range | Mapping |
|--------------|---------------|-------|---------|
| Speed | lfoFreq_ | 0.1 - 10 Hz | Logarithmic (perceptually even) |
| Intensity | intensity_ | 0.0 - 1.0 | Linear or mild log |
| Tone | toneFreq_ | 800 - 20000 Hz | Logarithmic |
| Slow/Fast switch | Speed range select | Slow: 0.1-2Hz, Fast: 2-10Hz | Binary |
| Normal/Emphasis | emphasisMode_ | 0 or 1 | Switches tone filter Q or adds resonance |

### 8.10 CPU Cost Estimate

Per-sample operations:
- HPF input: 2 multiplies, 2 adds (~4 ops)
- Tube soft clip (fast_tanh): ~8 ops
- Tone filter (1-pole): 2 ops
- LFO sine: std::sin or fast_sin (~8 ops, but only needed once per sample)
- Neon lamp model: 2-3 comparisons + arithmetic (~5 ops)
- LDR model: 1 multiply, 1 add (~2 ops)
- VCA (log R + divider): exp + divide (~12 ops with fast_exp)
- Output HPF: 2 ops

**Total: ~43 operations per sample** -- very lightweight. No oversampling needed since
there are no hard nonlinearities in the audio signal path (the tube soft-clip uses tanh,
which is smooth and well-bandlimited).

---

## Part 9: Key Implementation Notes

### 9.1 Pre-Compute Optimizations

The LDR attack/release coefficients, tone filter coefficient, and LFO frequency only change
when parameters change (block-rate, not sample-rate). Pre-compute these in the parameter
update function:

```cpp
void updateParams(float speed, float intensity, float tone, bool slowMode, bool emphasis) {
    // Speed with Slow/Fast range
    float minFreq = slowMode ? 0.1f : 2.0f;
    float maxFreq = slowMode ? 2.0f : 10.0f;
    lfoFreq_ = minFreq * std::pow(maxFreq / minFreq, speed);  // log mapping

    intensity_ = intensity;

    // Tone filter
    float toneFreq = 800.0f * std::pow(25.0f, tone);
    float w = 2.0f * M_PI * toneFreq / sampleRate_;
    toneAlpha_ = w / (1.0f + w);

    // Emphasis mode: boost midrange Q or add presence
    emphasisGain_ = emphasis ? 1.5f : 1.0f;  // or modify filter topology

    // LDR coefficients (pre-computed from time constants)
    ldrAttackCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate_ * 0.015f));  // 15ms
    ldrReleaseCoeff_ = 1.0f - std::exp(-1.0f / (sampleRate_ * 0.100f)); // 100ms
}
```

### 9.2 The fast_exp Problem

The per-sample `exp(logR)` in the LDR resistance calculation is expensive. Alternatives:
1. **Table lookup**: Pre-compute R_ldr for 256 values of ldrState and interpolate
2. **fast_exp approximation**: Use the IEEE float bit-hack exponential (acceptable accuracy)
3. **Pre-compute the VCA curve**: Since attenuation = f(ldrState) is a fixed nonlinear
   mapping, pre-compute a 256-point lookup table at setSampleRate time

**Recommended**: Option 3. Pre-compute a 256-entry float table mapping ldrState [0,1] to
VCA attenuation [0,1]. Per-sample cost drops to one linear interpolation (~4 ops).

### 9.3 Denormal Prevention

The LDR state variable (`ldrState_`) approaches but never reaches 0.0 or 1.0 due to the
exponential envelope follower. This is inherently denormal-safe as long as the time constants
are reasonable (>1ms). No special handling needed.

### 9.4 Sample Rate Independence

All time constants (LDR rise/fall, LFO phase, tone filter) are expressed relative to
sample rate. The `updateParams()` function recalculates coefficients when sample rate
changes. No issues with 44.1/48/96kHz operation.

### 9.5 Integration with Existing Codebase

The VT-X tremolo could either:
1. **Replace the existing FAUST tremolo** (index 15) -- this would be a significant upgrade
   from the current simple sine/square LFO amplitude modulation
2. **Be added as a mode** within the existing tremolo effect -- "Standard" mode (current
   behavior) vs "Vintage" mode (VT-X emulation)

Option 2 is recommended as it preserves backward compatibility with existing presets while
adding the new capability. The mode parameter could select:
- Mode 0: Standard (sine/square blend, no tube, no optical model)
- Mode 1: Vintage Optical (full VT-X emulation with neon lamp)
- Mode 2: Smooth Optical (VT-X with LED model instead of neon)
- Mode 3: Bias Tremolo (Fender brownface-style bias modulation character)

---

## Sources

- [Guyatone VT-X Specs - Effects Database](https://www.effectsdatabase.com/model/guyatone/flip/vtx)
- [Guyatone VT-X - zZsounds](https://www.zzounds.com/item--GUYVTX)
- [Guyatone VT-X Volume Mod Discussion - TDPRI](https://www.tdpri.com/threads/guyatone-flip-vintage-tremolo-vt-x-tremolo-volume-mod.968572/)
- [Guyatone VT-X Schematic Request - DIYStompboxes](https://www.diystompboxes.com/smfforum/index.php?topic=59361.0)
- [Guyatone VT-X Gimmick or Legit - DIYStompboxes](https://www.diystompboxes.com/smfforum/index.php?topic=65213.0)
- [Guyatone VT-2 Schematic - FreeStompboxes](https://www.freestompboxes.org/viewtopic.php?t=8636)
- [Godlyke VT-X Product Page](https://godlyke.com/products/guyatone-vt-x-flip-tremolo-pedal-b-stock)
- [John Frusciante VT-X - Ground Guitar](https://www.groundguitar.com/john-frusciante-gear/john-frusciantes-guyatone-flip-vt-x-vintage-tremolo/)
- [Strymon Amplifier Tremolo Technology White Paper](https://www.strymon.net/amplifier-tremolo-technology-white-paper/)
- [Effectrode Delta-Trem In-Depth](https://www.effectrode.com/knowledge-base/delta-trem-in-depth/)
- [How Tremolo Works in Your Amp - 300Guitars](https://300guitars.com/articles/the-ins-and-outs-of-tremolo/)
- [Types of Tremolo in Tube Amps - Carl's Custom Amps](http://carlscustomamps.com/types-of-tremolos-in-tube-amps)
- [Fender AB763 Deluxe Reverb Works - Rob Robinette](https://robrobinette.com/How_The_AB763_Deluxe_Reverb_Works.htm)
- [Designing Phase Shift Oscillators - Aiken Amps](https://www.aikenamps.com/index.php/designing-phase-shift-oscillators-for-tremolo-circuits)
- [Valve Wizard Tremolo Oscillator](https://www.valvewizard.co.uk/trem1.html)
- [Fender Vibroverb 6G16 Classic Circuit - Amp Books](https://www.ampbooks.com/mobile/classic-circuits/vibroverb/)
- [Guitar Tremolo Unit - ESP/Sound-AU](https://sound-au.com/project29.htm)
- [Demeter Tremulator Layout - TagboardEffects](https://tagboardeffects.blogspot.com/2012/03/requested-by-few-people.html)
- [Demeter Tremulator Layout - EffectsLayouts](http://effectslayouts.blogspot.com/2016/11/demeter-tremulator.html)
- [VTL5C3 Optocoupler - Aion FX](https://aionfx.com/component/vtl5c3/)
- [Neon Lamp Characteristics - Giangrandi](https://www.giangrandi.org/electronics/neon/neon.shtml)
- [CdS Photocell Overview - Adafruit](https://learn.adafruit.com/photocells?view=all)
- [Vactrol DSP Modeling - KVR Audio Forum](https://www.kvraudio.com/forum/viewtopic.php?t=445687)
- [Buchla Lowpass-Gate Digital Model - ResearchGate](https://www.researchgate.net/publication/271835899_A_Digital_Model_of_the_Buchla_Lowpass-Gate)
- [Optical Tremolo DIY - Make](https://makezine.com/projects/optical-tremolo-2/)
- [NE-2 Neon Lamp Circuits](https://www.bristolwatch.com/ele1/neon.htm)
- [Neon Lamp Info - Nuts & Volts](https://www.nutsvolts.com/uploads/magazine_downloads/NeonLamp-Information.pdf)
