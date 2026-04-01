# Mosrite Fuzzrite - Complete Circuit Research for Digital Emulation

## 1. History & Versions

The Mosrite Fuzzrite was designed by **Ed Sanner** for Semie Moseley's Mosrite company. Production began in 1966.

### Germanium Version (1966-1968)
- **Transistors**: 2N2613 (Q1, PNP Ge) + 2N408 (Q2, PNP Ge)
- **Production**: ~200-250 units (Sanner's recollection), 91+ documented survivors
- **Configuration**: Positive ground (PNP), negative supply rail
- **Construction variants** (chronological):
  1. Perfboard (c. 1966) - point-to-point, rarest
  2. Phenolic paper "reversed" (c. 1966-67) - components face outward
  3. Phenolic paper conventional (c. 1966-67) - most common Ge version
  4. Eyelet board (c. 1967-68) - final Ge variant
- **Capacitor drift across production**: Early units used 100nF coupling caps (Cornell Dubilier), mid-production switched to 50nF, late production used 47nF ceramic. This means NO single "correct" Ge schematic exists -- values varied.
- Abandoned due to temperature instability of germanium transistors

### Silicon Version (1968+)
- **Transistors**: Originally marked "TZ82" (proprietary Mosrite marking)
- **Modern equivalents**: BC107/BC108, 2N3904, PN2222A, 2N3903
- **Configuration**: Negative ground (NPN), standard 9V positive supply
- More stable, became the standard production version
- Used on Iron Butterfly's "In-A-Gadda-Da-Vida" (1968)

### Artist Usage
- **Iron Butterfly** (Eric Braunn): Almost certainly **silicon version** (1968 recording, Ge was discontinued by then)
- **John Frusciante**: Uses the **germanium version**. Has a custom Triungulo Lab "JF Fuzz" which is a Ge Fuzzrite reissue with an internal mode switch ("original" vs "JF" mode). Used on Stadium Arcadium tour (2006). Often stacked with Boss DS-2.
- **The Ventures**: "2,000 Pound Bee" (1962) - one of earliest recorded uses
- **Ron Asheton** (The Stooges): First LP
- **Davie Allen & The Arrows**

---

## 2. Complete Schematic - Silicon Version (Canonical Reference)

This is the most consistently documented version across multiple traced originals.

### Circuit Topology
Two cascaded common-emitter NPN gain stages with AC coupling. NO global feedback (unlike Fuzz Face). Outputs of both stages are blended via the Depth potentiometer.

```
INPUT --> C1 --> Q1 base --> Q1 collector --> C2 --> Depth pot pin 3
                                                     |
         C4 --> Q2 base --> Q2 collector --> C3 --> R_HPF --> Depth pot pin 1
                                                     |
                                              Depth pot wiper --> Volume pot --> OUTPUT
```

### Complete BOM - Silicon Version

| Ref  | Value     | Type            | Notes |
|------|-----------|-----------------|-------|
| R1   | 1M        | Carbon comp     | Input pulldown / bias for Q1 base |
| R2   | 470K      | Carbon comp     | Q1 collector load (Vcc to collector) |
| R3   | 470K      | Carbon comp     | Q1 base-collector feedback bias |
| R4   | 470K      | Carbon comp     | Q2 collector load (Vcc to collector) |
| R5   | 470K      | Carbon comp     | Q2 base-collector feedback bias |
| R6   | 22K       | Carbon comp     | Q2 output HPF with C3. CRITICAL for tone. Often omitted in bad schematics |
| R7   | 2.2K      | Carbon comp     | Current limiting resistor (CLR) in some versions |
| C1   | 47nF      | Film            | Input coupling cap, Q1 |
| C2   | 2.2nF     | Film/ceramic    | Q1 output coupling to Depth pot |
| C3   | 2.2nF     | Film/ceramic    | Q2 output coupling to Depth pot |
| C4   | 47nF      | Film            | Interstage coupling, Q1 collector to Q2 base |
| C5   | 100uF     | Electrolytic    | Power supply filter |
| D1   | 1N4001    | Rectifier       | Reverse polarity protection |
| Q1   | BC107     | NPN Si          | First gain stage. hFE ~100-200 |
| Q2   | BC108     | NPN Si          | Second gain stage. hFE ~50-100 |
| Depth| 500KB     | Linear/Rev log  | Original 350K. Use 500K + 1M2 parallel for ~353K |
| Vol  | 50KA      | Audio/log       | Original 33K. Use 50K + 100K parallel for ~33K |

**Power supply**: 9V DC (battery), positive ground for PNP Ge version, negative ground for NPN Si version.

### Complete BOM - Germanium Version (Traced from originals)

| Ref  | Value     | Type            | Notes |
|------|-----------|-----------------|-------|
| R1   | 4.7M-10M  | Carbon comp    | Input pulldown / Q1 base bias. Higher value = more "character". 10M in some originals |
| R2   | 470K      | Carbon comp     | Q1 collector load |
| R3   | 470K      | Carbon comp     | Q1 base-collector feedback bias |
| R4   | 470K      | Carbon comp     | Q2 collector load |
| R5   | 470K      | Carbon comp     | Q2 base-collector feedback bias. Some units use 100K |
| R6   | 22K       | Carbon comp     | Q2 output HPF. Critical component |
| C1   | 50nF (40-100nF) | Film      | Input coupling. Varied across production |
| C2   | 2nF       | Ceramic/film    | Q1 output coupling to Depth |
| C3   | 2nF       | Ceramic/film    | Q2 output coupling to Depth |
| C4   | 50nF (40-100nF) | Film      | Interstage coupling. Varied with C1 |
| C5   | 25uF      | Electrolytic    | Power filter. Non-functional in some originals |
| Q1   | 2N2613    | PNP Ge          | High gain, somewhat leaky (250-500uA Icbo) |
| Q2   | 2N408     | PNP Ge          | Low gain, low leakage (<80uA Icbo) |
| Depth| 350K rev log | "C" taper    | Blends Q1 and Q2 outputs |
| Vol  | 33K audio | Log             | Output level |

---

## 3. Transistor Specifications

### 2N2613 (Q1, Germanium PNP) - First Stage
| Parameter | Value |
|-----------|-------|
| Material  | Germanium |
| Polarity  | PNP |
| Package   | TO-1 |
| Pc max    | 0.12W |
| Vcb max   | 30V |
| Vce max   | ~25V (estimated) |
| Ic max    | 50mA |
| hFE min   | 120 |
| hFE typical | 120-200 (high-gain selected) |
| ft        | 4 MHz |
| Cob       | 20 pF |
| Tj max    | 100C |
| Vbe (Ge)  | ~0.15-0.3V |
| Is (model)| ~5.0e-6 A (Ge diode) |
| Icbo (leakage) | 250-500 uA (intentionally leaky for this circuit) |

### 2N408 (Q2, Germanium PNP) - Second Stage
| Parameter | Value |
|-----------|-------|
| Material  | Germanium |
| Polarity  | PNP |
| Package   | TO-1 |
| Pc max    | 0.15W |
| Vcb max   | 20V |
| Vce max   | 18V |
| Veb max   | 2V |
| Ic max    | 70mA |
| hFE min   | 65 |
| hFE typical | 65-100 (low-gain selected) |
| ft        | 6.7 MHz |
| Tj max    | 85C |
| Vbe (Ge)  | ~0.15-0.3V |
| Is (model)| ~5.0e-6 A (Ge diode) |
| Icbo (leakage) | <80 uA (low-leakage selected) |

### 2N3565 / BC107 (Q1, Silicon NPN) - First Stage
| Parameter | Value |
|-----------|-------|
| Material  | Silicon |
| Polarity  | NPN |
| hFE min   | 70 (2N3565), 110 (BC107) |
| hFE typical | 180-220 (2N3565) |
| Ic max    | 50mA (2N3565), 100mA (BC107) |
| Vce max   | 25V (2N3565), 45V (BC107) |
| ft        | 40 MHz (2N3565), 300 MHz (BC107) |
| Vbe       | ~0.6-0.7V |
| Is (model)| ~2.5e-9 A (Si diode) |

### BC108 (Q2, Silicon NPN) - Second Stage
| Parameter | Value |
|-----------|-------|
| Material  | Silicon |
| Polarity  | NPN |
| hFE min   | 110 |
| hFE typical | 200-450 |
| Vce max   | 20V |
| Ic max    | 100mA |
| Vbe       | ~0.6-0.7V |

### Critical Transistor Selection Notes
- **Q1 gain should be HIGHER than Q2**, at least 2:1 ratio
- For silicon: Q1 = low gain (10-30 hFE works surprisingly well per Small Bear), Q2 = 20-30 hFE
- Wait -- there's conflicting info here. Small Bear recommends VERY low gain Si transistors (10-30 hFE), while other sources say ~200 hFE. The key insight: **hFE matters less than Icbo (leakage) for germanium**, and the self-biasing circuit compensates for hFE variation
- For germanium: leakage current (Icbo) is MORE critical than hFE. Too high or too low = circuit won't bias correctly
- The self-biasing topology (470K base-collector feedback) makes the circuit "fairly immune to Beta/hfe variation"

---

## 4. Circuit Analysis

### 4.1 Biasing (Each Stage Identical)

Each transistor uses **collector-to-base feedback biasing** via a 470K resistor:

```
Vcc (9V) ---[R_load 470K]--- Collector ---[R_bias 470K]--- Base
                                                              |
                                                            [C_in]
                                                              |
                                                           Input signal
```

The 470K collector-to-base feedback resistor creates a DC negative feedback loop:
- If Ic increases -> Vc drops -> Ib drops (through 470K) -> Ic decreases
- This makes the bias point self-stabilizing (somewhat)
- **DC operating point**: Collector sits at approximately **0.6V** (Si) or slightly different for Ge
- With 9V supply, 470K collector load, and Vc~0.6V: Ic = (9-0.6)/470K = ~17.8uA

This is an extremely low quiescent current. The transistors are barely on at idle. This is by design -- the circuit is meant to be driven hard into clipping.

### 4.2 Gain Per Stage

Voltage gain of each CE stage with collector-to-base feedback:

```
Av = -gm * (R_load || R_bias || r_o)
   = -gm * (470K || 470K || r_o)
   ≈ -gm * 235K  (ignoring r_o)
```

At Ic = 17.8uA:
- gm = Ic/Vt = 17.8e-6/26e-3 = 0.685 mS
- r_pi = beta/gm (with beta=120 for 2N2613) = 175K
- Av ≈ 0.685e-3 * 235K ≈ **161 (44 dB)**

This matches the reported "approximately 44dB gain per stage."

**Total cascaded gain**: 44 + 44 = **88 dB** (2 stages, ~25,000x voltage gain before clipping)

This massive gain ensures the transistors are ALWAYS clipping for any guitar signal. The circuit doesn't "clean up" much.

### 4.3 Input Impedance

The input impedance is set by R1 in parallel with the input impedance of Q1's base:

**Silicon version**:
```
Zin = R1 || r_pi(Q1) || R_bias
    = 1M || r_pi || 470K
```
With Q1 hFE=200, Ic=17.8uA:
- r_pi = 200 * 26mV / 17.8uA = 292K
- Zin = 1M || 292K || 470K ≈ **155K**

**Germanium version**:
```
Zin = R1 || r_pi(Q1) || R_bias
    = 4.7M || r_pi || 470K
```
With Q1 hFE=120, Ic=17.8uA:
- r_pi = 120 * 26mV / 17.8uA = 175K
- Zin = 4.7M || 175K || 470K ≈ **128K**

This is LOW for guitar (most pickups want to see 250K-1M). It will load the guitar pickups, rolling off highs and reducing volume. This is part of the Fuzzrite character -- but notably it's HIGHER than a Fuzz Face (~2-10K input impedance) because there is no global feedback network pulling the input impedance down.

### 4.4 Output Impedance

The output impedance depends on the Depth pot position:
- **Depth at minimum (Q1 only)**: Output impedance ≈ through the Volume pot, so ~33K (or 50K modified)
- The coupling caps (2.2nF) add significant series impedance at low frequencies
- At 100Hz: Xc = 1/(2*pi*100*2.2e-9) = 723K -- this means very little bass passes through C2/C3

### 4.5 Frequency Response - THE KEY TO THE FUZZRITE SOUND

This is what makes the Fuzzrite sound like "angry hornets" or "bees in a can":

#### Low-Frequency Rolloff (Input Coupling)
C1 and C4 (47-50nF) with the input impedance create a high-pass filter:
```
f_low = 1 / (2 * pi * Zin * C1)
      = 1 / (2 * pi * 155K * 47e-9)
      ≈ 22 Hz (Si)
```
For Ge with 50nF: ~25 Hz. This is low enough to pass full guitar range.

**BUT** there's a resonant peak around **120 Hz** due to the interaction of coupling caps with the transistor's input impedance. This peak gives the Fuzzrite extra body on low notes.

#### HIGH-FREQUENCY EMPHASIS / BANDWIDTH LIMITING (This is the buzzy secret)

The output coupling caps C2 and C3 are tiny: **2.2nF**. Combined with the Depth pot (350K-500K) as load:

```
f_high = 1 / (2 * pi * R_depth * C_out)
       = 1 / (2 * pi * 350K * 2.2e-9)
       ≈ 207 Hz  (full depth, both stages)
```

But the actual -3dB point depends on the Depth pot position and loading:
```
With effective load of ~100K:
f_high = 1 / (2 * pi * 100K * 2.2e-9) ≈ 723 Hz
```

This means **the Fuzzrite has aggressive high-pass filtering on its output**. The 2.2nF caps block most of the bass content. What passes through is predominantly upper-mid and treble harmonic content from the clipped signal.

#### The Critical 22K Resistor + C3 HPF

R6 (22K) and C3 (2.2nF) on Q2's output form an additional high-pass filter:
```
f_HPF = 1 / (2 * pi * 22K * 2.2e-9)
      = 1 / (2 * pi * 22000 * 0.0000000022)
      ≈ 3,289 Hz
```

This **3.3kHz HPF** on Q2's output is what gives the Fuzzrite its signature buzz-saw character. The second stage output has almost no bass or low-mids -- only sizzling upper harmonics from the clipped signal. When you blend in more Q2 via the Depth control, you add more of this high-frequency buzzy content.

**This is THE defining characteristic of the Fuzzrite sound.** Remove the 22K and you get a much bassier, thicker fuzz (which many bad schematics accidentally produce by omitting it).

### 4.6 The Depth Control - Detailed Analysis

The Depth potentiometer is a **blend/mix control** between the two stage outputs:

```
C2 (Q1 out) ----> Depth pot pin 3
                   |
                   Wiper ----> Volume pot ----> OUTPUT
                   |
R6+C3 (Q2 out) -> Depth pot pin 1
```

- **Depth at minimum** (wiper toward pin 3): You hear mostly Q1's output through C2. This is a single-stage fuzz: cleaner, fatter, more bass content (47nF coupling vs 2.2nF coupling). Sound is rounder, less aggressive.

- **Depth at maximum** (wiper toward pin 1): You hear mostly Q2's output through C3+R6. This is the full two-stage cascaded fuzz with the 3.3kHz HPF. Sound is thin, buzzy, aggressive, maximum fuzz with almost no bass.

- **Intermediate positions**: Blend of both. This creates the most interesting territory:
  - The two signals have different harmonic content (Q1 = softer clips, Q2 = harder clips)
  - The two signals have different frequency content (Q1 = full bandwidth, Q2 = treble-heavy)
  - Voltage-starved/gated sounds appear at certain blend points
  - Pseudo ring-mod / upper-octave harmonics emerge from the mixing of differently-clipped signals

**Pot taper**: Originally 350K reverse-log ("C" taper). This means most of the tonal change happens in the first portion of knob travel. Standard 500KB (linear) with a 1M2 parallel resistor approximates the original value but not the taper.

### 4.7 Clipping Characteristics

#### How the Fuzzrite Clips (vs Fuzz Face / Big Muff)

**Fuzzrite clipping mechanism**: Pure transistor saturation. No diode clipping anywhere in the circuit. Both stages clip by driving into saturation (Vce_sat) and cutoff.

- With 44dB gain per stage, any guitar signal will slam both transistors into hard clipping
- Collector voltage swings from ~0.6V to nearly Vcc (9V) during cutoff, and to Vce_sat (~0.1-0.3V for Si, ~0.05-0.2V for Ge) during saturation
- **Clipping is highly asymmetric**: The transistor saturates on one half-cycle and cuts off on the other. This produces strong even harmonics (2nd, 4th, 6th) in addition to odd harmonics
- The 2.2nF output caps then strip the fundamental and lower harmonics, leaving mostly the upper harmonics = buzz

**vs Fuzz Face**:
- Fuzz Face has **global negative feedback** (collector-to-base of Q1 from Q2's emitter). This feedback controls gain, affects clipping symmetry, and creates that smooth, compressed, "touch-sensitive" response
- Fuzz Face has **very low input impedance** (~2-10K) due to the feedback. Fuzzrite is ~128-155K
- Fuzz Face clips more symmetrically (especially with gain pot adjustment), giving smoother, warmer clipping
- Fuzzrite has **NO feedback** -- it's just raw cascaded gain slamming into saturation. Harsher, buzzier, more aggressive

**vs Big Muff**:
- Big Muff uses **diode clipping** (1N914 pairs) after each gain stage, then a passive tone stack
- Big Muff has 4 stages (vs Fuzzrite's 2), with gentler clipping per stage
- Big Muff's tone stack is a sophisticated LP/HP blend with mid-scoop
- Fuzzrite's "tone control" is the Depth pot blending two differently-filtered clipped signals

**Summary**: The Fuzzrite is the rawest, most aggressive of the three. No feedback smoothing (Fuzz Face), no diode softening (Big Muff). Just naked transistor saturation with treble-emphasizing output filtering.

---

## 5. Interstage Loading & Interaction

The two stages are **AC-coupled** via C4 (47-50nF), with no intentional interstage loading beyond the 470K bias resistor of Q2.

```
Q1 collector --[C4 47nF]-- Q2 base
                               |
                          [R5 470K to Q2 collector]
```

The input impedance of Q2 (r_pi) is in parallel with R5 (470K base-collector feedback):
- Q2 input Z = r_pi || R5 ≈ 175K || 470K ≈ 128K (Ge)
- This loads Q1's collector, reducing its gain slightly

The interstage coupling cap C4 with Q2's input impedance creates another HPF:
```
f_interstage = 1 / (2 * pi * 128K * 47e-9) ≈ 26 Hz
```
This is low enough to pass full guitar bandwidth between stages.

**Key interaction**: Since there's no emitter resistor and no feedback between stages, the two stages are essentially independent. The only coupling is through the DC bias point (which can shift with temperature for Ge) and the AC signal path through C4.

---

## 6. Power Supply Effects

### Voltage
- Designed for 9V (single battery)
- No significant voltage sag effects documented (unlike Fuzz Face where sag from dying battery is a feature)
- The very low quiescent current (~17.8uA per stage, total ~36uA) means battery lasts extremely long
- No evidence of "dying battery" tone being a sought-after Fuzzrite sound

### Voltage Sensitivity
- The self-biasing (collector-to-base feedback) makes the circuit relatively supply-voltage independent
- Reducing supply voltage will:
  - Lower the clipping ceiling (earlier saturation)
  - Reduce available headroom
  - Slightly change the bias point
- But the effect is less dramatic than in a Fuzz Face because there's no global feedback loop to destabilize

### Germanium Temperature Sensitivity
- This is why the Ge version was abandoned after ~200 units
- Ge transistor leakage (Icbo) roughly doubles every 10C
- Higher leakage -> bias point shifts -> collector voltage drops -> less headroom -> more sputtery/gated
- At high temperatures, the circuit can become completely unusable (transistors conduct through even without signal)
- At low temperatures, leakage drops to near-zero and the circuit becomes starved/gated

---

## 7. DSP Implementation Strategy

### Recommended Approach: Direct Circuit Simulation (NOT WDF)

The Fuzzrite is actually simpler than a Fuzz Face for digital emulation because:
1. No global feedback loop (no circular dependencies)
2. No emitter resistors (simpler topology)
3. Two independent, identically-structured stages
4. The nonlinearity is purely from transistor saturation

**WDF is NOT recommended** for this circuit because:
- WDF excels at circuits with reactive elements in feedback loops (like the Fuzz Face's Q1-Q2 feedback)
- The Fuzzrite has no such feedback -- it's a pure feedforward topology
- Direct nodal analysis or transistor model evaluation per-sample is more efficient

### Recommended Architecture

```
Input -> HPF(C1) -> TransistorStage1(Q1) -> HPF(C4) -> TransistorStage2(Q2)
     -> HPF(R6+C3) -> DepthBlend(with C2 path from Q1) -> VolumePot -> Output
```

### Per-Stage Model

Each CE stage with collector-to-base feedback:

```cpp
// Simplified transistor saturation model
// vIn = input voltage to base (after coupling cap)
// Returns collector voltage
float processStage(float vIn, float hFE, float Is, float Vt,
                   float Rload, float Rbias, float Vcc) {
    // Base current through bias resistor: Ib = (Vc - Vbe) / Rbias
    // Collector current: Ic = hFE * Ib (in active region)
    // Vc = Vcc - Ic * Rload
    // But saturated: Vc >= Vce_sat (~0.1V Si, ~0.05V Ge)
    // And cutoff: Ic >= 0

    // The coupling cap + bias resistor form a DC feedback loop
    // that self-biases. For AC analysis, we need to track the
    // capacitor voltage (DC component) separately from the AC signal.

    // Simplified per-sample approach:
    float Vbe = Vt * std::log(Ib/Is + 1.0); // Ebers-Moll
    float Ic = Is * (std::exp(Vbe / (nF * Vt)) - 1.0); // forward
    Ic = std::min(Ic, (Vcc - Vce_sat) / Rload); // saturation limit
    Ic = std::max(Ic, 0.0f); // cutoff limit
    float Vc = Vcc - Ic * Rload;
    return Vc;
}
```

### Filter Implementation

All filters in this circuit are simple first-order RC high-pass filters. Use one-pole HPF (TPT topology for stability):

```cpp
// TPT (trapezoidal) first-order HPF
struct OnePoleHPF {
    double s1 = 0.0; // state (use double for filter state)
    double g;        // coefficient

    void setFreq(double fc, double fs) {
        g = std::tan(M_PI * fc / fs);
    }

    float process(float x) {
        double v = (x - s1) / (1.0 + g);
        double hp = v;       // high-pass output
        double lp = v * g + s1;
        s1 = lp + v * g;     // state update
        return static_cast<float>(hp);
    }
};
```

### Filter Frequencies (at standard component values)

| Filter | Components | Frequency | Purpose |
|--------|-----------|-----------|---------|
| Input HPF | C1(47nF) + Zin(155K) | ~22 Hz | Block DC, slight bass rolloff |
| Interstage HPF | C4(47nF) + Zin_Q2(128K) | ~26 Hz | Block DC between stages |
| Q1 output HPF | C2(2.2nF) + Depth pot | ~207-723 Hz | Treble-pass Q1 output |
| Q2 output HPF | R6(22K) + C3(2.2nF) | **~3,289 Hz** | THE buzz-saw filter |
| Q2 to Depth HPF | C3(2.2nF) + Depth pot | ~207-723 Hz | Additional bass cut on Q2 |

### Depth Pot Implementation

```cpp
// depthParam = 0.0 (Q1 only, fat) to 1.0 (Q2 only, buzzy)
// But with reverse-log taper:
float depthMapped = reverseLogTaper(depthParam); // most change in first half of travel

float q1Signal = hpfC2.process(q1Output);  // Q1 through 2.2nF
float q2Signal = hpfR6C3.process(q2Output); // Q2 through 22K+2.2nF HPF at 3.3kHz
float q2Coupled = hpfC3.process(q2Signal);  // additional coupling cap

float blended = q1Signal * (1.0f - depthMapped) + q2Coupled * depthMapped;
```

### Oversampling Requirement

With 88dB total gain and hard transistor saturation, the clipping generates harmonics well above Nyquist.

**Recommendation**: 4x oversampling minimum.
- 2x oversampling at 48kHz = 96kHz Nyquist at 48kHz. May show aliasing artifacts on aggressive settings.
- 4x oversampling at 48kHz = processing at 192kHz. Should be clean for most signals.
- ADAA (antiderivative anti-aliasing) on the transistor saturation function can reduce the oversampling requirement to 2x.

### Anti-aliasing Strategy

The transistor saturation is a smooth function (exp/tanh), not a hard discontinuity. ADAA is effective:

For tanh-based clipping: F(x) = ln(cosh(x)) is the antiderivative.

```cpp
// First-order ADAA for tanh waveshaper
float processADAA(float x) {
    float F_x = std::log(std::cosh(x)); // antiderivative of tanh
    float result;
    if (std::abs(x - x_prev) > 1e-5f) {
        result = (F_x - F_prev) / (x - x_prev);
    } else {
        result = std::tanh(x); // fallback for small differences
    }
    x_prev = x;
    F_prev = F_x;
    return result;
}
```

### Germanium vs Silicon in DSP

The key differences to model:

| Aspect | Germanium | Silicon | DSP Impact |
|--------|-----------|---------|------------|
| Vbe | 0.15-0.3V | 0.6-0.7V | Different bias point, earlier conduction |
| Is | ~5e-6 A | ~2.5e-9 A | Ge conducts more readily, softer knee |
| hFE Q1 | 120-200 | 70-220 | Higher gain ratio Q1/Q2 for Ge |
| hFE Q2 | 65-100 | 50-200 | Lower gain in Q2 for Ge |
| Leakage | 250-500uA (Q1), <80uA (Q2) | negligible | Ge bias shifts with "temperature" |
| Clipping char | Softer saturation, rounder knee | Harder saturation, sharper knee | Ge = slightly warmer buzz |
| Temperature | Dramatic drift | Stable | Optional param for Ge |

### Recommended Implementation Parameters

For the silicon version (simpler, more stable, Iron Butterfly tone):
```cpp
// Transistor model parameters
constexpr float kVcc = 9.0f;
constexpr float kRload = 470000.0f;  // collector load
constexpr float kRbias = 470000.0f;  // base-collector feedback
constexpr float kR1_input = 1000000.0f; // input pulldown
constexpr float kR6_hpf = 22000.0f;  // Q2 output HPF
constexpr float kC1 = 47e-9f;        // input coupling
constexpr float kC2 = 2.2e-9f;       // Q1 output coupling
constexpr float kC3 = 2.2e-9f;       // Q2 output coupling
constexpr float kC4 = 47e-9f;        // interstage coupling
constexpr float kVbe_Si = 0.65f;
constexpr float kIs_Si = 2.52e-9f;
constexpr float kVt = 0.026f;        // thermal voltage at 25C

// Gain per stage ~44dB
constexpr float kQ1_hFE = 150.0f;    // mid-range
constexpr float kQ2_hFE = 80.0f;     // lower gain
```

For the germanium version (Frusciante tone):
```cpp
constexpr float kR1_input_Ge = 4700000.0f; // 4.7M
constexpr float kC1_Ge = 50e-9f;     // 50nF (mid-production)
constexpr float kC4_Ge = 50e-9f;
constexpr float kVbe_Ge = 0.20f;
constexpr float kIs_Ge = 5.0e-6f;
constexpr float kQ1_hFE_Ge = 150.0f; // high gain, leaky
constexpr float kQ2_hFE_Ge = 65.0f;  // low gain, tight
// Leakage modeled as additional base current
constexpr float kQ1_Icbo = 350e-6f;  // 350uA typical
constexpr float kQ2_Icbo = 40e-6f;   // 40uA typical
```

---

## 8. Known Schematic Discrepancies

Multiple conflicting schematics exist. The most common errors in published schematics:

1. **Missing 22K resistor (R6)**: This is THE most commonly omitted component. Found soldered directly to the Depth pot lug in originals, not on the PCB. Without it, the pedal loses its signature buzzy character and becomes much bassier/thicker.

2. **Wrong coupling cap values**: Some schematics show 100nF for C1/C4 (only correct for earliest Ge units). Most production units used 47-50nF.

3. **Wrong pot values**: Original 350K depth / 33K volume are non-standard. Many schematics substitute 500K/50K without noting the parallel resistors needed.

4. **Missing R1 input resistor**: Some schematics omit the 1M (Si) or 4.7-10M (Ge) input pulldown. This affects input impedance and Q1 bias.

5. **Ge vs Si confusion**: Many "Fuzzrite schematics" don't specify which version. The two versions have different coupling cap values, different R1 values, and obviously different transistor polarity.

---

## 9. Summary: What Makes the Fuzzrite Sound Like a Fuzzrite

1. **No feedback** -- unlike Fuzz Face, no smoothing or compression. Raw, uncontrolled clipping.
2. **Massive gain** (88dB cascaded) -- everything clips, all the time. Minimal dynamics.
3. **Tiny output coupling caps** (2.2nF) -- strips bass and low-mids from clipped signal, leaving only buzzy upper harmonics.
4. **The 22K + 2.2nF HPF at 3.3kHz on Q2** -- the second stage output is ONLY treble harmonics. This is the buzz-saw.
5. **Depth as a blend** -- not a gain control. Mixing two differently-filtered clipped signals creates the unique tonal range.
6. **Relatively high input impedance** (~128-155K) -- doesn't load the guitar as aggressively as Fuzz Face, preserving some clarity.
7. **Self-biased at very low quiescent current** -- transistors are barely on at idle, which means they transition sharply between cutoff and saturation = aggressive, gated character.

---

## Sources

- [Cloning and Modifying a Mosrite Fuzzrite (homoelectromagneticus)](https://homoelectromagneticus.com/projects/fuzzrite.php)
- [The Fur's Rite - Small Bear Electronics](https://diy.smallbearelec.com/Projects/FursRite/FursRite.htm)
- [Mosrite Fuzzrite: A Detailed History (Fuzzboxes.org)](https://fuzzboxes.org/features/fuzzrite)
- [Guitar FX Layouts: Fuzzrite Silicon](https://tagboardeffects.blogspot.com/2010/02/mosrite-fuzzrite.html)
- [Guitar FX Layouts: Fuzzrite Germanium](https://tagboardeffects.blogspot.com/2015/08/mosrite-fuzzrite-germanium.html)
- [Vero P2P: Fuzzrite Silicon Version](https://vero-p2p.blogspot.com/2022/10/mosrite-fuzzrite-silicon-version.html)
- [PedalPCB Forum: Starboard Fuzz Tutorial](https://forum.pedalpcb.com/threads/starboard-fuzz-mossrite-fuzzrite.10428/)
- [Perf and PCB Layouts: Mosrite Fuzzrite](https://effectslayouts.blogspot.com/2014/11/mosrite-fuzzrite.html)
- [Aion FX: Orpheus Silicon Fuzz](https://aionfx.com/project/orpheus-silicon-fuzz/)
- [Aion FX: Orpheus Germanium Fuzz](https://aionfx.com/project/orpheus-germanium-fuzz/)
- [Triungulo Lab JF Fuzz](https://triungulolab.com/products/jf-fuzz-germanium-fuzzrite-reissue)
- [Catalinbread Fuzzrite Germanium](https://catalinbread.com/products/fuzzrite-germanium)
- [Mosrite Fuzzrite (Equipboard)](https://equipboard.com/items/mosrite-fuzzrite)
- [2N2613 Datasheet](https://www.uxpython.com/electronics/bjt/2n2613/pnp-transistor-specifications-datasheet)
- [2N408 Transistor Specs (alltransistors)](https://alltransistors.com/transistor.php?transistor=3918)
- [2N3565 Datasheet (alltransistors)](https://alltransistors.com/transistor.php?transistor=3343)
- [PedalParts.co.uk FuzzRite V2 Documentation](http://pedalparts.co.uk/docs/FuzzRite-V2.pdf)
- [Ellzey Electronics: Fuzzrite](https://ellzeyelectronics.com/jellzey/projects/fuzz-rite.html)
- [Ellzey Electronics: Germanium Fuzzrite](https://ellzeyelectronics.com/jellzey/projects/fuzz-rite.html)
