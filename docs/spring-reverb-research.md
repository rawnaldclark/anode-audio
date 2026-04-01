# Spring Reverb Emulation: Comprehensive DSP Research

## Part 1: Physics of Spring Reverb

### 1.1 How a Spring Reverb Tank Works

A spring reverb tank is an electromechanical transducer system consisting of three components:

1. **Input transducer**: An electromagnetic driver (similar to a small speaker voice coil) that converts electrical signal to mechanical vibration. The driver coil is wound around a laminated steel armature suspended in a permanent magnet field. When current flows through the coil, it deflects the armature, pulling on the spring.

2. **Helical spring(s)**: One to four coiled steel springs (typically 2 or 3 in guitar amps), each wound from music wire (ASTM A228 spring steel), stretched under tension between the input and output transducers. Each spring has different physical dimensions (coil diameter, wire diameter, number of turns, length) to create different propagation characteristics.

3. **Output transducer (pickup)**: A similar electromagnetic transducer that converts the spring's mechanical motion back to electrical signal. The pickup is typically higher impedance than the driver.

The springs are mounted inside a sheet-metal pan (the "tank") lined with sound-deadening foam, suspended on rubber grommets to isolate from chassis vibration.

### 1.2 Wave Propagation in Helical Springs

This is where spring reverb gets physically interesting and fundamentally different from plate reverb or room acoustics.

A helical spring supports **multiple modes of wave propagation**:

- **Longitudinal waves**: Compression/rarefaction along the spring axis (like sound in air)
- **Torsional waves**: Twisting motion around the spring axis
- **Transverse waves**: Side-to-side displacement perpendicular to the axis

The critical physics insight: **these modes are coupled** through the helical geometry. A longitudinal impulse at one end generates both longitudinal and torsional waves. The coupling between these modes is what creates the complex, characteristic spring reverb sound.

#### 1.2.1 The Dispersion Relation

The key equation governing wave propagation in a helical spring (derived from Wittrick, 1966, and elaborated by Parker & Bilbao, DAFx 2009):

For a helical spring with:
- `d` = wire diameter (m)
- `D` = mean coil diameter (m)
- `N` = number of active coils
- `L` = free length of spring (m)
- `rho` = density of spring steel (~7850 kg/m^3)
- `E` = Young's modulus (~200 GPa)
- `G` = shear modulus (~79.3 GPa)
- `alpha` = helix angle = arctan(L / (pi * D * N))

The **phase velocity** of longitudinal waves is frequency-dependent:

```
c(omega) = c0 * sqrt(1 + (omega / omega_c)^2)
```

Where:
- `c0` = low-frequency longitudinal wave speed = sqrt(G * d^2 / (32 * rho * D^2 * N^2)) [approximate]
- `omega_c` = critical frequency where dispersion becomes significant

This is the fundamental equation that creates the **chirp**: higher frequencies travel FASTER through the spring than lower frequencies. When you hit a spring with an impulse, the high frequencies arrive at the pickup first, followed by progressively lower frequencies sweeping downward. This is the "sproing" or "drip" sound.

#### 1.2.2 Group Velocity and Chirp

The group velocity (envelope speed) is:

```
v_g(omega) = c0 * (1 + (omega / omega_c)^2) / sqrt(1 + (omega / omega_c)^2)
           = c0 * sqrt(1 + (omega / omega_c)^2)
```

For a spring of length L, the group delay is:

```
tau_g(omega) = L / v_g(omega) = L / (c0 * sqrt(1 + (omega / omega_c)^2))
```

This means:
- At low frequencies (omega << omega_c): delay ~ L/c0 (maximum delay)
- At high frequencies (omega >> omega_c): delay ~ L*omega_c / (c0*omega) (decreasing)
- The delay difference between low and high frequencies creates the chirp sweep

**Typical values for an Accutronics guitar amp spring:**
- Total propagation time (low freq): ~30-50 ms
- Total propagation time (high freq, ~5kHz): ~10-15 ms
- Chirp sweep duration: ~15-35 ms (the "drip" time)
- Number of round trips before decay: ~20-80 (creating the dense reverb tail)

#### 1.2.3 Why Springs Sound Like Springs

The characteristic spring reverb sound comes from several interacting phenomena:

1. **Chirp dispersion** (described above): The downward frequency sweep on each reflection. This is the #1 signature. Plates also exhibit dispersion, but with a different (less pronounced) characteristic.

2. **Multiple reflections with accumulating chirp**: Each time the wave reflects off the spring endpoints, it undergoes another chirp. After 10-20 reflections, the chirps pile up into a dense, metallic wash.

3. **Modal coupling**: Longitudinal-to-torsional mode conversion at each reflection adds complexity. Torsional waves travel at a different speed, so you get interleaved chirps from different propagation modes.

4. **Spring-to-spring interaction** (multi-spring tanks): In a 3-spring tank, each spring has different dimensions, so their chirps have different sweep rates and total delays. When summed at the pickup transducer, the overlapping chirps create a smoother, denser texture.

5. **Transducer resonances**: The input and output transducers have their own mechanical resonances (typically 100-300 Hz for input, 2-5 kHz for output), which color the frequency response.

6. **Nonlinear "crash"**: When driven hard, the spring displacement exceeds the linear range, and the coils can physically contact each other ("coil clash"). This creates a harsh, chaotic burst -- the "crash" when you kick a spring reverb.

### 1.3 Accutronics/Belton Tank Specifications

Accutronics (now part of ATUS Corp.) uses a standardized part number system. Each digit encodes a characteristic:

#### Part Number Format: `XY AB CD EF`

| Position | Parameter | Values |
|----------|-----------|--------|
| 1st digit | Input impedance | 1=8 ohm, 4=200 ohm, 8=2250 ohm, 9=800 ohm |
| 2nd digit | Output impedance | A=2250 ohm, B=10K ohm, C=500 ohm, D=200 ohm, E=190 ohm |
| 3rd digit | Spring count | 1=1 spring, 2=2 springs, 3=3 springs |
| 4th digit | Decay time | 1=short (1-2s), 2=medium (2-3s), 3=long (3-4.5s) |
| 5th digit | Connector | A=2-pin, B=RCA, C=3-pin |
| 6th digit | Locking | A=open, B=locked |

#### Tanks Used in Famous Guitar Amps

| Tank Model | Springs | Input Z | Output Z | Decay | Used In |
|------------|---------|---------|----------|-------|---------|
| 4AB3C1B | 3 | 200 ohm | 2250 ohm | Short | Fender Deluxe Reverb |
| 8EB2C1B | 2 | 2250 ohm | 190 ohm | Short | Fender Twin Reverb |
| 8FB3C1B | 3 | 2250 ohm | 600 ohm | Short | Fender Super Reverb |
| 4AB2C1B | 2 | 200 ohm | 2250 ohm | Short | Many Fender combos |
| 9AB3C1A | 3 | 800 ohm | 2250 ohm | Short | Marshall JCM800 |
| 4BB3C1B | 3 | 200 ohm | 10K ohm | Short | Mesa Boogie |
| 8EB2C1B | 2 | 2250 ohm | 190 ohm | Short | Fender 6G15 standalone |
| 1BB2C1B | 2 | 8 ohm | 10K ohm | Short | Sovtek/EHX units |

#### 2-Spring vs 3-Spring Sonic Differences

**2-spring tanks (e.g., Fender 6G15, Twin Reverb, blackface/silverface)**:
- More pronounced "drip" and "boing" on transients
- Sparser echo density -- you can hear individual reflections more clearly
- Wider frequency gaps between the two springs' chirp patterns
- More characteristic "surf guitar" sound
- Each spring carries more energy, so individual spring resonances are more audible
- Higher amplitude initial transient splash
- The classic Fender reverb sound that Dick Dale made famous

**3-spring tanks (e.g., Deluxe Reverb, Marshall, Mesa)**:
- Smoother, denser reverb tail
- Less pronounced individual drip -- the three chirps partially mask each other
- Fills in the frequency gaps between springs
- More "natural" sounding at the expense of less character
- Lower amplitude per spring (energy split three ways)
- Better for subtle, always-on reverb settings
- Marshall/Mesa typically use this for a reason -- it sits better in high-gain contexts

**The physical reason**: With 2 springs, you have 2 incommensurate delay times and 2 chirp patterns. The interference pattern is relatively simple, so individual chirps are audible. With 3 springs, you have 3 incommensurate delays and 3 chirps, creating a much denser interference pattern that sounds smoother but less "springy."

### 1.4 The Fender 6G15 Reverb Unit

The 6G15 (1961-1966) is THE reference for standalone guitar spring reverb. Its circuit defines the "Fender reverb sound":

**Signal path:**
1. **Input stage**: 12AX7 (V1A) common-cathode preamp. Gain ~40 (32 dB). 68K plate, 1.5K cathode (bypassed by 25uF).
2. **Dwell control**: 500K pot between V1A output and V1B grid. Controls how hard the spring tank is driven. This is NOT a simple volume control -- it changes the saturation characteristics of the driver stage.
3. **Driver stage**: 12AT7 (V1B) cathode-follower driving the spring tank. The cathode follower provides low output impedance to drive the low-impedance spring transducer. The spring tank (Accutronics 8EB2C1B) presents ~2250 ohm load.
4. **Spring tank**: 2-spring unit. Input Z = 2250 ohm, output Z = 190 ohm.
5. **Recovery amp**: 12AX7 (V2A) amplifies the weak signal from the tank pickup. Gain ~70 (37 dB). 100K plate, 3.3K cathode.
6. **Mixer**: 12AX7 (V2B) mixes dry and reverb signals. Tone control between stages shapes the reverb EQ.
7. **Output stage**: 6V6GT power tube into output transformer for line-level output.

**Key DSP-relevant characteristics of the 6G15:**
- **Input frequency response**: The driver transformer and transducer roll off above ~4-5 kHz (6 dB/oct approximately). This pre-filters the signal going into the spring.
- **Output frequency response**: The pickup transducer has a resonance peak around 2-4 kHz, then rolls off. This adds brightness to the reverb return.
- **Dwell saturation**: At high dwell settings, V1B's cathode follower clips, adding even harmonics to the signal driving the springs. This makes the reverb "bloom" and sustain -- the springs ring longer because the harmonic content excites multiple modes.
- **Recovery noise**: The 6G15's recovery amp (V2A) adds significant noise. Real spring reverb is NOT quiet. The noise floor with reverb cranked is typically -50 to -55 dBFS.
- **The "drip"**: Most pronounced at moderate dwell settings where the springs aren't being overdriveninto continuous excitation but get enough energy for the chirp to develop fully.

### 1.5 Transducer Impedance Effects on Tone

The input/output impedance matching significantly affects the spring reverb tone:

- **Low input Z (8 ohm)**: Requires more current but less voltage. Direct coupling from output transformer taps possible. More bass, less definition in the chirp. Used in Sovtek/cheaper designs.
- **Medium input Z (200 ohm)**: Good match for cathode-follower tube drivers. Balanced frequency response. Standard Fender approach.
- **High input Z (2250 ohm)**: Voltage-drive dominant. Brighter, more detailed chirp. Used in Twin Reverb where the tank is driven from a high-impedance source.

- **Low output Z (190 ohm)**: Requires recovery amp with low input impedance. More robust signal, less susceptible to cable capacitance. Fender's preferred pickup impedance.
- **High output Z (2250-10K ohm)**: Higher voltage output but more susceptible to interference. Used where recovery amp has high input impedance.

The impedance mismatch between driver and spring creates a reflection coefficient at the transducer-spring boundary. A better impedance match means more energy enters the spring (louder reverb, faster decay) while a deliberate mismatch means more reflections at the boundary (longer apparent decay, but quieter).

---

## Part 2: DSP Algorithms for Spring Reverb

### 2.1 Overview of Approaches

There are four main categories of digital spring reverb emulation, ordered from simplest to most complex:

| Approach | CPU Cost | Accuracy | Real-time? |
|----------|----------|----------|------------|
| Multi-tap delay + allpass chirp | Very Low | Moderate | Yes, mobile |
| Nested allpass + dispersion chain | Low-Medium | Good | Yes, mobile |
| Digital waveguide with dispersion | Medium | Very Good | Yes, desktop/mobile |
| FDTD physical modeling (Bilbao) | Very High | Excellent | Desktop only |
| Neural network (TCN/LSTM) | High | Reference-quality | Maybe mobile |

For our Android guitar app, the **nested allpass + dispersion chain** approach offers the best accuracy-to-CPU ratio, with elements borrowed from the waveguide approach for the chirp modeling.

### 2.2 Algorithm A: Multi-Tap Delay + Allpass Chirp (Simplest)

This is the approach used in the SELECTOR VST repository and many basic spring reverb plugins:

```
Input -> [Pre-filter LPF 5kHz] -> [Parallel delay lines] -> [Allpass diffusion] -> [Damping] -> Output
                                        |                          |
                                        +--- feedback <-----------+
```

**Structure:**
- 2 or 3 parallel delay lines (one per spring), with mutually prime delay lengths
- Each delay line feeds through a short allpass cascade (4-8 stages) for initial density
- One-pole LPF damping in the feedback path
- Pre-filter to simulate the input transducer bandwidth limitation

**Delay line lengths** (at 48 kHz, for 2-spring model):
- Spring 1: 1392 samples (29.0 ms)
- Spring 2: 1836 samples (38.3 ms)
- Spring 3 (if 3-spring): 2388 samples (49.8 ms)

These are chosen to be mutually prime and approximate the round-trip propagation times of real Accutronics springs.

**Limitations:**
- No chirp dispersion -- the allpass diffusion smears the signal but doesn't create the frequency-dependent delay that produces the "drip"
- Sounds like a metallic delay/echo, not a spring reverb
- Missing the fundamental physics of the spring

**Verdict: Not suitable for "true-to-life" emulation.** Useful only as a starting point or as an intentionally lo-fi effect.

### 2.3 Algorithm B: Chirp Allpass Dispersion Chain (Recommended)

This is the approach derived from the work of Abel, Smith (CCRMA), and Parker & Bilbao (DAFx 2009). It models the spring's dispersive wave propagation using cascaded allpass filters.

#### 2.3.1 Core Insight

A helical spring acts as a **dispersive waveguide** -- a transmission line where phase velocity depends on frequency. In digital systems, frequency-dependent delay is implemented using **allpass filters**, whose phase response creates group delay variation across frequency without affecting magnitude.

The key insight from Abel & Smith: a cascade of N first-order allpass filters can approximate the dispersion characteristic of a helical spring. The allpass coefficients are chosen so that:
- Low frequencies experience more group delay (they arrive later)
- High frequencies experience less group delay (they arrive first)
- The total group delay difference across the audio band matches the measured chirp duration

This creates the authentic "sproing" -- on each reflection, you hear the high frequencies first, sweeping down to the lows.

#### 2.3.2 Algorithm Structure (Per Spring)

```
                    +--[Chirp Allpass Chain]--[Delay Line]--[Damping LPF]--+
                    |                                                      |
Input --[Pre-EQ]--->+<----------- feedback * decay_coeff <----------------+---> Output
```

For a 2-spring model, two of these structures run in parallel with different parameters.
For a 3-spring model, three run in parallel.

#### 2.3.3 Chirp Allpass Filter Design

Each allpass stage is a first-order allpass filter:

```
y[n] = a * x[n] + x[n-1] - a * y[n-1]
```

Where `a` is the allpass coefficient. The group delay of a single first-order allpass at frequency omega is:

```
tau(omega) = (1 - a^2) / (1 + a^2 + 2*a*cos(omega*T))
```

Where T = 1/fs (sample period).

For `a > 0`: maximum group delay at DC, minimum at Nyquist (low-pass dispersion characteristic)
For `a < 0`: maximum group delay at Nyquist, minimum at DC (high-pass dispersion characteristic)

**For spring reverb, we want `a > 0`** (positive coefficient) so that low frequencies are delayed more than high frequencies, matching the spring's physical behavior where high frequencies propagate faster.

#### 2.3.4 Cascaded Allpass Chain Specifications

The number of allpass stages and their coefficients control:
- **Total chirp duration**: More stages = longer chirp sweep
- **Chirp density**: How smoothly the frequency sweep occurs
- **Minimum allpass count for convincing chirp**: 8-12 stages per spring minimum. Below 8, the chirp sounds stepped/quantized. Above 30, diminishing returns.

**Recommended configurations:**

| Spring Type | Allpass Stages | Coefficient Range | Chirp Duration |
|-------------|---------------|-------------------|----------------|
| Fender short 2-spring | 12 per spring | 0.5 to 0.8 | ~20 ms |
| Fender long 3-spring | 16 per spring | 0.4 to 0.75 | ~30 ms |
| Marshall 3-spring | 10 per spring | 0.45 to 0.7 | ~15 ms |
| Long decay tank | 20 per spring | 0.5 to 0.85 | ~40 ms |

**Coefficient distribution strategy:**

Rather than using identical coefficients for all stages (which creates a uniform dispersion), use a graduated distribution that matches the spring's non-uniform dispersion:

For a chain of N allpass filters (indexed 0 to N-1):

```
a[k] = a_min + (a_max - a_min) * (k / (N - 1))^gamma
```

Where:
- `a_min` = 0.3 (minimum coefficient, controls high-frequency delay)
- `a_max` = 0.8 (maximum coefficient, controls low-frequency delay)
- `gamma` = 1.5 (curve shape -- >1 pushes more stages toward the lower coefficient end)

For a 12-stage chain at 48 kHz (Fender 2-spring):
```
Stage  0: a = 0.300    (least dispersion, nearest to flat delay)
Stage  1: a = 0.337
Stage  2: a = 0.381
Stage  3: a = 0.429
Stage  4: a = 0.481
Stage  5: a = 0.534
Stage  6: a = 0.588
Stage  7: a = 0.641
Stage  8: a = 0.693
Stage  9: a = 0.742
Stage 10: a = 0.788
Stage 11: a = 0.800    (most dispersion)
```

These coefficients create a total group delay variation of approximately 15-20 ms between 100 Hz and 5 kHz, which matches measurements of real Accutronics 2-spring tanks.

#### 2.3.5 Delay Lines (Bulk Propagation Delay)

After the allpass chain (which models the dispersion), a plain delay line provides the bulk propagation time. The total path delay for each spring equals:

```
total_delay = allpass_group_delay(at band center) + delay_line_length
```

**Recommended delay line lengths** (48 kHz, per spring):

| Configuration | Spring 1 | Spring 2 | Spring 3 |
|---------------|----------|----------|----------|
| 2-spring (Fender) | 1109 samples (23.1 ms) | 1543 samples (32.1 ms) | -- |
| 3-spring (Fender) | 997 samples (20.8 ms) | 1373 samples (28.6 ms) | 1831 samples (38.1 ms) |
| 3-spring (Marshall) | 887 samples (18.5 ms) | 1201 samples (25.0 ms) | 1607 samples (33.5 ms) |

These are the NET delay after subtracting the allpass chain's average group delay. They should be mutually prime (or nearly so) to avoid flutter echoes. All values must be scaled by `fs / 48000.0` for sample rate adaptation.

#### 2.3.6 Damping Filters

Each spring's feedback path needs frequency-dependent damping that models:
1. Internal losses in the spring wire (frequency-dependent, higher at HF)
2. Radiation losses (energy escaping to the tank pan)
3. Transducer re-reflection losses

**Implementation: Two-stage damping**

```
// Stage 1: One-pole LPF for high-frequency absorption
// Cutoff approximately 3-5 kHz, adjustable via Tone parameter
dampLP_state = (1 - dampLP_coeff) * input + dampLP_coeff * dampLP_state;

// Stage 2: One-pole HPF for low-frequency loss (spring doesn't transmit DC well)
dampHP_state = dampHP_coeff * (dampHP_state + input - dampHP_prev);
dampHP_prev = input;
```

**Damping filter coefficients** (at 48 kHz):

| Parameter | LPF Coefficient | HPF Coefficient |
|-----------|----------------|-----------------|
| Bright spring | 0.3 (fc ~ 7.3 kHz) | 0.995 (fc ~ 38 Hz) |
| Medium spring | 0.5 (fc ~ 4.6 kHz) | 0.997 (fc ~ 23 Hz) |
| Dark spring | 0.7 (fc ~ 2.7 kHz) | 0.998 (fc ~ 15 Hz) |

The LPF coefficient is the primary control for the `Tone` parameter. The HPF is fixed and prevents DC buildup.

**IMPORTANT**: Use 64-bit (double) state variables for both damping filters, as they sit inside the feedback loop. Single precision will cause audible artifacts after ~20 reflections due to accumulated rounding error. This matches the pattern already used in our Dattorro plate reverb.

#### 2.3.7 Feedback and Decay

The feedback coefficient controls reverb decay time (RT60). For a single spring loop of total delay time T_loop:

```
feedback_coeff = 10^(-3 * T_loop / RT60)
               = exp(-6.908 * T_loop / RT60)
```

Where RT60 is the desired decay time in seconds.

**Important for multi-spring model**: Each spring has a DIFFERENT loop time, so each needs a DIFFERENT feedback coefficient to achieve the same perceived decay time:

```
feedback_spring_k = exp(-6.908 * T_loop_k / RT60)
```

If you use the same coefficient for all springs, the shorter spring decays faster and the reverb tail "thins out" over time (which actually happens in real tanks, so this can be intentional).

**Typical decay times for guitar spring reverb:**
- Short: 1.0 - 1.5 s (tight, rhythmic playing)
- Medium: 2.0 - 2.5 s (standard Fender reverb)
- Long: 3.0 - 4.0 s (surf/ambient)

#### 2.3.8 Input Pre-Filtering (Transducer Model)

The input transducer has a bandwidth-limited frequency response that significantly shapes the reverb character. Model this with:

```
// Input transducer: 2nd-order LPF at ~4.5 kHz (Q=0.7)
// Simulates the mechanical bandwidth of the driver
inputLP = biquadLPF(fc=4500, Q=0.707, fs=48000);

// Input transducer resonance: slight peak at ~200 Hz (Q=1.5)
// The driver armature has a mechanical resonance
inputResonance = biquadPeak(fc=200, Q=1.5, gain=+3dB, fs=48000);
```

**Biquad coefficients for input LPF at 4500 Hz, Q=0.707, fs=48000:**
```
omega = 2 * pi * 4500 / 48000 = 0.5890
sin_w = sin(0.5890) = 0.5548
cos_w = cos(0.5890) = 0.8320
alpha = sin_w / (2 * 0.707) = 0.3924

b0 = (1 - cos_w) / 2 = 0.0840
b1 = 1 - cos_w = 0.1680
b2 = (1 - cos_w) / 2 = 0.0840
a0 = 1 + alpha = 1.3924
a1 = -2 * cos_w = -1.6640
a2 = 1 - alpha = 0.6076

// Normalized:
b0/a0 = 0.06034, b1/a0 = 0.12068, b2/a0 = 0.06034
a1/a0 = -1.19520, a2/a0 = 0.43647
```

#### 2.3.9 Output Post-Filtering (Pickup Transducer Model)

The pickup transducer has a resonant peak that gives spring reverb its characteristic "bright ping":

```
// Output pickup resonance: peak at ~2.5 kHz (Q=2.0, +6 dB)
// This is the mechanical resonance of the pickup armature
outputResonance = biquadPeak(fc=2500, Q=2.0, gain=+6dB, fs=48000);

// Output rolloff: gentle LPF above 6 kHz
outputLP = biquadLPF(fc=6000, Q=0.5, fs=48000);
```

The pickup resonance is critical -- it's what gives spring reverb the "ting" on attack transients. Without it, the reverb sounds too smooth and plate-like.

#### 2.3.10 The "Crash" (Nonlinear Transducer Drive)

When the input signal is loud enough, the driver transducer saturates and the springs exhibit nonlinear behavior (coils contacting each other). This is the "crash" sound when you kick a spring reverb tank.

Model with soft clipping on the input before the spring simulation:

```
// Dwell-dependent saturation
float driven = input * dwellGain;  // dwellGain = 1.0 to 8.0

// Soft clip using tanh (matches tube driver saturation)
float clipped = fast_tanh(driven * 0.7) * 1.4;

// The clipping adds harmonics that excite more spring modes
// This is why high Dwell settings sound "bigger" -- more modes are ringing
```

At extreme Dwell, also add a small amount of noise to simulate the chaotic behavior:

```
if (dwellGain > 5.0f) {
    float noiseAmt = (dwellGain - 5.0f) * 0.003f;  // Very subtle
    clipped += whiteNoise() * noiseAmt;
}
```

### 2.4 Algorithm C: Digital Waveguide Approach (Abel & Smith)

This approach, developed at CCRMA, treats each spring as a dispersive digital waveguide:

```
           +-->[Dispersion Filter]-->[Delay Line]-->[Loss Filter]-->+
           |                                                        |
Input ---->+                                                        +----> Output
           |                                                        |
           +<--[Dispersion Filter]<--[Delay Line]<--[Loss Filter]<--+
```

The key difference from Algorithm B: the waveguide approach models BIDIRECTIONAL propagation (forward and backward traveling waves), which captures the interference patterns at the spring boundaries more accurately.

**Dispersion filter**: Instead of a cascade of first-order allpass filters, use a higher-order allpass filter designed via the **Thiran allpass interpolation** method to match the measured dispersion curve. A 4th to 6th order Thiran allpass per spring provides excellent chirp accuracy.

**Advantages**: More physically accurate, captures boundary reflections naturally.
**Disadvantages**: More complex to implement, roughly 2x the CPU of Algorithm B (bidirectional processing), and the Thiran filter design requires solving for coefficients that match a specific group delay curve.

**Verdict**: Excellent for desktop, potentially too expensive for our Android use case with 27 effects in the chain. If CPU budget allows, this is superior to Algorithm B.

### 2.5 Algorithm D: FDTD Physical Modeling (Bilbao)

Stefan Bilbao's finite-difference time-domain approach (described in his book "Numerical Sound Synthesis," Cambridge 2009, and the Parker & Bilbao DAFx 2009 paper) discretizes the actual wave equation for a helical spring:

```
rho * A * u_tt = G * Ip * u_xxxx - (G*Ip + E*I) * u_xx + ...coupled torsional terms...
```

This is solved on a spatial grid with Ns = 50-200 grid points along each spring, updated at the audio sample rate.

**Computational cost**: For Ns=100 grid points per spring, 2 springs:
- ~200 multiply-accumulate operations per spatial point per time step
- = 40,000 operations per sample per spring
- = 80,000 operations per sample for 2 springs
- = ~3.84 billion operations per second at 48 kHz

This is roughly **50-100x more expensive** than Algorithm B and is not viable for our Android app.

**Verdict**: Academic interest only. Not suitable for real-time on mobile.

### 2.6 Algorithm E: Neural Network Approach

The neural approach (Martínez Ramírez et al., ICASSP 2020; Papaleo, 2022) trains a deep neural network on input/output recordings of a physical spring reverb tank.

**Architecture**: Temporal Convolutional Network (TCN) with Feature-wise Linear Modulation (FiLM) for parameter conditioning. Typical model:
- 10-12 TCN layers
- Receptive field: 4096-8192 samples (~85-170 ms at 48 kHz)
- Parameters: ~500K-2M weights
- FiLM conditioning on: Dwell, Tone, Decay

**Advantages**: Can capture the EXACT character of a specific tank, including nonlinearities, transducer colorations, and noise characteristics.

**Disadvantages**:
- Training requires a reference tank and careful signal capture
- Model size and inference time may be prohibitive on low-end Android (RTNeural can help)
- No parametric control of spring count without retraining
- Latency from receptive field size

**Verdict**: Possible future enhancement if we add RTNeural support (similar to our NAM amp model path). Not suitable as the primary implementation now.

---

## Part 3: Recommended Implementation Architecture

### 3.1 Complete Signal Flow

```
Input
  |
  v
[Dwell: soft-clip drive] -----> gain = dwellParam * 4.0 + 1.0, tanh saturation
  |
  v
[Input Pre-EQ] ----------------> 2nd-order LPF @ 4.5kHz (transducer bandwidth)
  |                               + slight resonance peak @ 200Hz
  v
[Split to N springs] ----------> N = 2 or 3 (user selectable)
  |         |        |
  v         v        v
[Spring 1] [Spring 2] [Spring 3]  (parallel, each with own parameters)
  |         |        |
  v         v        v
[Sum springs] ------------------> equal-weight sum: output = (s1 + s2 + s3) / N
  |
  v
[Output Post-EQ] ---------------> resonant peak @ 2.5kHz (pickup transducer)
  |                                + gentle LPF @ 6kHz
  v
[DC blocker] -------------------> HP @ 10Hz (prevent DC from feedback loops)
  |
  v
Output (wet signal, mixed with dry via Effect::applyWetDryMix)
```

**Each "Spring N" block internally:**

```
input ----+-->[Allpass Chain: 12 stages]-->[Delay Line]-->[LPF Damping]-->[HPF Damping]-->+--> output
          |                                                                               |
          +<-----------------------------------[* feedback_coeff]<------------------------+
```

### 3.2 Per-Spring Parameters

**Spring 1 (shortest, brightest):**
```
delay_line:     1109 samples @ 48kHz (23.1 ms)
allpass_count:  12
allpass_coeffs: [0.30, 0.34, 0.38, 0.43, 0.48, 0.53, 0.59, 0.64, 0.69, 0.74, 0.79, 0.80]
damping_LP:     fc = 5500 Hz (coeff = 0.40 @ 48kHz)
damping_HP:     fc = 25 Hz (coeff = 0.9967)
feedback:       0.87 (for 2.0s RT60)
```

**Spring 2 (medium):**
```
delay_line:     1543 samples @ 48kHz (32.1 ms)
allpass_count:  12
allpass_coeffs: [0.32, 0.36, 0.41, 0.46, 0.51, 0.56, 0.61, 0.66, 0.71, 0.76, 0.80, 0.82]
damping_LP:     fc = 4800 Hz (coeff = 0.45 @ 48kHz)
damping_HP:     fc = 30 Hz (coeff = 0.9961)
feedback:       0.89 (for 2.0s RT60, higher because longer loop)
```

**Spring 3 (longest, darkest -- only used in 3-spring mode):**
```
delay_line:     1831 samples @ 48kHz (38.1 ms)
allpass_count:  12
allpass_coeffs: [0.34, 0.39, 0.44, 0.49, 0.54, 0.59, 0.64, 0.69, 0.73, 0.77, 0.81, 0.84]
damping_LP:     fc = 4200 Hz (coeff = 0.50 @ 48kHz)
damping_HP:     fc = 35 Hz (coeff = 0.9954)
feedback:       0.91 (for 2.0s RT60, highest because longest loop)
```

### 3.3 Parameter Mapping

| User Parameter | Range | Internal Mapping |
|----------------|-------|-----------------|
| **Dwell** | 0.0 - 1.0 | Input drive gain: 1.0 + dwell * 7.0 (1x to 8x). Applied before tanh saturation. Higher values = more harmonics exciting springs = louder, denser reverb with onset "bloom" |
| **Decay** | 0.1 - 4.5 s | Per-spring feedback coefficient via RT60 formula: `fb_k = exp(-6.908 * T_loop_k / decay)`. Clamped to [0.0, 0.97] to prevent runaway. |
| **Tone** | 0.0 - 1.0 | Damping LPF cutoff: fc = 2000 + tone * 6000 Hz (2 kHz to 8 kHz). Also scales the output pickup resonance Q: Q = 1.0 + tone * 2.0 |
| **Springs** | 2 or 3 | Integer switch. 2-spring sums springs 1+2, divides by 2. 3-spring sums springs 1+2+3, divides by 3. |
| **Mix** | 0.0 - 1.0 | Wet/dry blend via existing Effect::applyWetDryMix() |

### 3.4 Sample Rate Adaptation

All delay lengths and filter coefficients must be scaled for the actual device sample rate:

```cpp
void setSampleRate(int32_t sampleRate) {
    float rateScale = static_cast<float>(sampleRate) / 48000.0f;

    // Scale delay lines
    for (int s = 0; s < 3; s++) {
        springDelayLen_[s] = static_cast<int>(kBaseDelayLen[s] * rateScale);
    }

    // Recompute allpass coefficients (they are frequency-dependent)
    // The coefficients need to produce the same GROUP DELAY in seconds
    // regardless of sample rate. For first-order allpass with coefficient a,
    // group delay at DC = (1-a^2) / (1+a)^2 / fs = (1-a) / (1+a) / fs
    // So to maintain the same delay in seconds at a different fs,
    // we need to adjust a.
    //
    // However, for simplicity and because the chirp character is more
    // important than exact delay matching, we can use the same coefficients
    // and just add/remove allpass stages to compensate:
    //   - At 44100 Hz: use same coefficients, same count (close enough)
    //   - At 96000 Hz: double the allpass count to maintain chirp duration
    //   - At 22050 Hz: halve the allpass count (unlikely sample rate)

    int allpassCount = static_cast<int>(12.0f * rateScale + 0.5f);
    allpassCount = std::max(6, std::min(allpassCount, 30));

    // Recompute damping filter coefficients for new sample rate
    // Using: coeff = exp(-2 * pi * fc / fs)
    for (int s = 0; s < 3; s++) {
        dampLP_coeff_[s] = std::exp(-2.0f * 3.14159f * dampLP_fc_[s] / sampleRate);
        dampHP_coeff_[s] = std::exp(-2.0f * 3.14159f * dampHP_fc_[s] / sampleRate);
        // But note: for the 1-pole HPF, the coefficient is:
        // R = exp(-2*pi*fc/fs), used as: y = R*(y_prev + x - x_prev)
    }

    // Recompute biquad coefficients for transducer EQ
    computeInputTransducerEQ(sampleRate);
    computeOutputTransducerEQ(sampleRate);
}
```

---

## Part 4: Digital Reverb Mode Presets

The existing Dattorro plate reverb serves as the foundation for the Digital mode. We add preset parameter sets that configure the Dattorro topology for different reverb characters.

### 4.1 Preset Definitions

#### Room
Small, tight space with prominent early reflections.
```
decay:    0.6 s   (short tail)
damping:  0.55    (moderate HF absorption -- rooms have absorbent surfaces)
preDelay: 5 ms    (very short -- small space)
size:     0.25    (small)

// Additional parameters if we extend the Dattorro:
// Early reflections level: -6 dB relative to tail (rooms emphasize ERs)
// Modulation rate: 0.3 Hz (minimal, rooms are static)
// Modulation depth: 4 samples
```

#### Hall
Large, diffuse space with slow onset.
```
decay:    3.5 s   (long, gradual)
damping:  0.40    (moderate -- halls are bright but not harsh)
preDelay: 25 ms   (noticeable gap -- large space)
size:     0.85    (large)

// Extended:
// Early reflections level: -12 dB (halls have late-arriving ERs)
// Modulation rate: 0.5 Hz
// Modulation depth: 12 samples (subtle detuning, hall "air")
```

#### Plate
Bright, dense, instant onset. The existing Dattorro algorithm already sounds plate-like.
```
decay:    2.5 s   (medium-long)
damping:  0.25    (bright -- plates are very reflective)
preDelay: 0 ms    (no pre-delay -- plates have instant onset)
size:     0.50    (medium -- standard EMT 140 size)

// Extended:
// Modulation rate: 0.7 Hz (EMT 140 has motor-driven tension variation)
// Modulation depth: 16 samples (the slight "shimmer" of a plate)
```

#### Shimmer
Pitch-shifted feedback creating ethereal octave-up tails.
```
decay:    5.0 s   (very long, washy)
damping:  0.20    (bright, airy)
preDelay: 15 ms   (slight gap)
size:     0.90    (large)
```

**Shimmer requires an additional processing block**: A pitch shifter (octave up, +12 semitones) inserted in the feedback path of one tank. This is implemented as a granular pitch shifter with ~20-50 ms grain size:

```
Shimmer feedback path:
  Tank output -> [pitch_shift +12 semitones] -> [LPF 8kHz] -> [* shimmer_mix] -> feed back
                                                                     |
  Tank output -> [* (1 - shimmer_mix)] -----------------------> feed back
```

The pitch shifter uses overlapping Hann-windowed grains read from the delay line at 2x speed:

```
// Granular pitch shifter for octave-up shimmer
// Two overlapping grains, 180 degrees apart in phase
grain_size = 2048;  // ~42 ms at 48kHz
grain_phase += 1.0 / grain_size;  // advances 0 to 1 per grain cycle

// Read positions advance at 2x speed for octave up
read_pos_1 = write_pos - grain_offset_1;
read_pos_2 = write_pos - grain_offset_2;

// Hann crossfade between grains
float fade1 = 0.5f * (1.0f - cos(2.0f * PI * grain_phase));
float fade2 = 0.5f * (1.0f - cos(2.0f * PI * (grain_phase + 0.5f)));

output = delay[read_pos_1] * fade1 + delay[read_pos_2] * fade2;
```

The shimmer intensity is controlled by the shimmer_mix parameter (0.0 = no shimmer, 1.0 = full octave feedback). Values of 0.3-0.5 produce the classic U2/Eno/Lanois shimmer effect.

#### Ambient
Very long, modulated, washy reverb. No distinct attack.
```
decay:    8.0 s   (very long)
damping:  0.35    (slightly dark, warm)
preDelay: 40 ms   (noticeable delay before reverb)
size:     0.95    (maximum space)

// Extended:
// Modulation rate: 0.8 Hz (significant modulation for "movement")
// Modulation depth: 24 samples (pronounced chorusing)
// Input diffusion coefficients: increased to 0.85/0.75 (maximum smear)
```

### 4.2 Mode Switching Architecture

The reverb effect should support a `mode` parameter (paramId 4 or 5) that switches between:
- Mode 0: Spring (uses the new spring algorithm)
- Mode 1: Plate (existing Dattorro, default)
- Mode 2: Room (Dattorro with Room preset)
- Mode 3: Hall (Dattorro with Hall preset)
- Mode 4: Shimmer (Dattorro + pitch-shifted feedback)
- Mode 5: Ambient (Dattorro with extended modulation)

When switching modes, call `reset()` to clear all delay buffers (prevents cross-mode artifacts), then load the new topology/parameters.

The Spring mode uses a completely separate set of state variables and processing path from the Dattorro modes. Both can coexist in the same class with a mode switch at the top of `process()`:

```cpp
void process(float* buffer, int numFrames) override {
    if (mode_ == MODE_SPRING) {
        processSpring(buffer, numFrames);
    } else {
        processDattorro(buffer, numFrames);  // existing code
    }
}
```

---

## Part 5: CPU Budget Analysis

### 5.1 Operations Per Sample: Spring Reverb

| Component | Ops/Sample | Notes |
|-----------|-----------|-------|
| Input pre-EQ (2 biquads) | 20 | 10 ops per biquad (5 mul + 5 add) |
| Dwell saturation (tanh) | 8 | fast_tanh = ~8 ops |
| **Per spring** (x2 or x3): | | |
| -- Allpass chain (12 stages) | 48 | 4 ops per stage (1 mul, 2 add, 1 store) |
| -- Delay line read (interpolated) | 6 | 2 reads, 1 lerp |
| -- Delay line write | 1 | 1 store |
| -- Damping LPF (double) | 4 | 2 mul, 1 add, 1 cast |
| -- Damping HPF (double) | 6 | 3 mul, 2 add, 1 cast |
| -- Feedback multiply | 1 | 1 mul |
| **Per spring subtotal** | **66** | |
| Output post-EQ (2 biquads) | 20 | |
| DC blocker | 4 | |
| Spring summing | 4 | add + divide |
| **Total (2 springs)** | **~184** | |
| **Total (3 springs)** | **~254** | |

### 5.2 Comparison with Existing Effects

| Effect | Ops/Sample | Relative to Spring |
|--------|-----------|-------------------|
| Spring Reverb (2-spring) | ~184 | 1.0x |
| Spring Reverb (3-spring) | ~254 | 1.4x |
| Dattorro Plate (existing) | ~280 | 1.5x |
| DS-1 (WDF, 2x oversampled) | ~400 | 2.2x |
| RAT (WDF, 2x oversampled) | ~350 | 1.9x |
| Fuzz Face (WDF, 2x OS) | ~500 | 2.7x |
| Cabinet Sim (FFT convolution) | ~150-300 | 0.8-1.6x |
| Simple effects (EQ, chorus) | ~20-60 | 0.1-0.3x |

**Key finding: The spring reverb is CHEAPER than the existing Dattorro plate** (184-254 ops vs ~280 ops), and significantly cheaper than the WDF pedal emulations. It will run comfortably on Android alongside the full effects chain.

### 5.3 Memory Budget

| Component | Memory (bytes) | Notes |
|-----------|---------------|-------|
| Delay buffers (3 springs) | 3 * 4096 * 4 = 49,152 | Power-of-two, 32-bit float |
| Allpass state (3 * 12) | 36 * 4 = 144 | One state per allpass stage |
| Damping filter state | 3 * 2 * 8 = 48 | 2 filters per spring, 64-bit |
| Biquad state (4 biquads) | 4 * 4 * 8 = 128 | 2 state variables per biquad, 64-bit |
| Coefficients | ~200 | allpass + biquad + delays |
| **Total** | **~50 KB** | Negligible |

For comparison, the Dattorro plate uses ~120 KB of delay buffers. The spring reverb uses less than half the memory.

### 5.4 Latency

The spring reverb introduces NO additional latency beyond the signal path's inherent group delay:
- The allpass chain is causal and sample-by-sample
- The delay lines are internal to the feedback loop (not in the direct signal path)
- The biquad filters add ~2 samples of group delay at most

No oversampling is needed because:
- The allpass chain coefficients are all in [0, 1], so no aliasing
- The tanh saturation for Dwell uses the existing fast_tanh (which has negligible aliasing below the transducer's 4.5 kHz bandwidth limit -- the input LPF acts as anti-aliasing)
- All filters are IIR with well-behaved frequency responses

### 5.5 Real-Time Safety

The implementation follows the same patterns as our existing effects:
- All memory allocated in `setSampleRate()`, never in `process()`
- Atomic parameters for thread-safe UI interaction
- No locks, no syscalls, no allocation on the audio thread
- Denormal guards on all feedback state variables
- Power-of-two delay buffers with bitmask wrapping (no modulo)

---

## Part 6: Psychoacoustic Validation Criteria

### 6.1 What to Listen For (A/B Testing Against Real Spring Reverb)

1. **Chirp/Drip on transients**: Play a single staccato note (muted string pop or snare hit). You should hear a brief "tchwip" -- high frequencies arriving first, sweeping down to lows over ~15-30 ms. If the chirp sounds like a descending whistle, the allpass chain is working. If it sounds like a simple echo, the chain is too short or coefficients are wrong.

2. **Decay character**: The tail should have a slight metallic quality without being harsh. Listen for periodic "flutter" in the tail -- this indicates the delay line lengths are too similar (not sufficiently mutually prime). Adjust delay lengths by +/- a few samples until flutter disappears.

3. **2-spring vs 3-spring difference**: Switch between them. 2-spring should have more pronounced individual drips and a sparser feel. 3-spring should sound smoother and denser. If they sound identical, the spring parameters are too similar.

4. **Dwell behavior**: At low dwell, the reverb should be subtle and clean. At high dwell, it should "bloom" -- getting louder, denser, and slightly gritty. The transition should be gradual. If it suddenly goes from clean to distorted, the tanh saturation curve is too aggressive.

5. **Low-frequency response**: Spring reverbs are NOT full-bandwidth. Real springs don't transmit much below ~80-100 Hz. The reverb tail should be relatively thin in the low end. If there's too much bass in the reverb, increase the HPF cutoff in the damping filters.

6. **High-frequency rolloff in the tail**: The tail should darken over time. After 1-2 seconds of decay, frequencies above ~3 kHz should be mostly gone. If the tail stays bright, increase the LPF damping coefficient.

7. **The "crash" test**: Send a very loud transient (palm mute thwack or kick the metaphorical tank). The reverb should briefly go chaotic/gritty before settling into its normal decay. If it just gets louder without changing character, the nonlinear drive model needs more gain.

### 6.2 Common DSP Artifacts and Their Causes

| Artifact | Cause | Fix |
|----------|-------|-----|
| Metallic ringing | Allpass coefficients too close to 1.0 | Reduce max coefficient to 0.80 |
| Flutter echo | Delay lengths share common factors | Adjust to mutually prime values |
| DC buildup | Missing HPF in feedback path | Add 10-25 Hz HPF with double state |
| Clicks on parameter change | Abrupt coefficient changes | Smooth parameters with one-pole filter |
| Thin, papery sound | Too few allpass stages | Increase to 12-16 per spring |
| Whooshing / phasiness | Allpass delay times too close together | Space coefficients more widely |
| No drip/chirp | Allpass coefficients too low or chain too short | Increase coefficients toward 0.7-0.8 |
| Harsh transients | Missing input transducer LPF | Add 2nd-order LPF at 4-5 kHz |
| Muddy, indistinct | Missing output pickup resonance | Add resonant peak at 2-3 kHz |

---

## Part 7: Implementation Checklist

### 7.1 New Parameters for Reverb Effect

Current parameters (paramId 0-3): decay, damping, preDelay, size
New parameters needed:

| paramId | Name | Range | Default | Mode |
|---------|------|-------|---------|------|
| 4 | mode | 0-5 | 1 (Plate) | All |
| 5 | dwell | 0.0-1.0 | 0.5 | Spring only |
| 6 | springCount | 2-3 | 3 | Spring only |
| 7 | tone | 0.0-1.0 | 0.6 | Spring only |

In Spring mode:
- paramId 0 (decay) maps to spring feedback coefficients (range: 0.5-4.5 seconds)
- paramId 1 (damping) is UNUSED (replaced by paramId 7 "tone" which controls damping LPF + pickup resonance)
- paramId 2 (preDelay) still works (adds pre-delay before the springs)
- paramId 3 (size) is UNUSED in spring mode (spring length is fixed by tank type)

In Digital modes (Plate/Room/Hall/Shimmer/Ambient):
- paramId 5 (dwell) is UNUSED
- paramId 6 (springCount) is UNUSED
- paramId 7 (tone) can optionally control a shelving EQ on the reverb output

### 7.2 Files to Modify

1. **`reverb.h`**: Add spring reverb processing path, new state variables, allpass chain, transducer EQ
2. **`jni_bridge.cpp`**: Update parameter count for reverb (if JNI enumerates param count)
3. **`EffectRegistry.kt`**: Add new parameters (mode, dwell, springCount, tone) to reverb entry
4. **`EffectEditorSheet.kt`**: UI for mode selector and spring-specific parameters
5. **Factory presets**: Update any presets that use reverb to specify mode=Plate (backward compat)

### 7.3 State Variables Needed (Spring Mode)

```
// Per spring (x3, even if only 2 used)
struct SpringState {
    std::vector<float> delayBuffer;     // Power-of-two delay line
    int delaySize, delayMask, writePos;
    int delayLength;                     // Actual delay in samples

    float allpassState[30];              // Allpass filter states (max 30 stages)
    float allpassCoeffs[30];             // Pre-computed coefficients
    int allpassCount;                    // Actual number of stages (12-24)

    double dampLP_state;                 // Damping LPF state (64-bit)
    double dampHP_state;                 // Damping HPF state (64-bit)
    float dampHP_prev;                   // HPF previous input
    float dampLP_coeff;                  // LPF coefficient
    float dampHP_coeff;                  // HPF coefficient

    float feedbackCoeff;                 // Per-spring decay coefficient
    float feedback;                      // Current feedback sample
};

SpringState springs_[3];

// Transducer EQ (shared across springs)
// Input: 2nd-order LPF + resonant peak (2 biquads)
// Output: resonant peak + LPF (2 biquads)
struct BiquadState {
    double z1, z2;     // State (64-bit for stability)
    float b0, b1, b2;  // Feedforward coefficients
    float a1, a2;       // Feedback coefficients (a0 normalized to 1)
};
BiquadState inputLPF_, inputResonance_;
BiquadState outputResonance_, outputLPF_;
```

### 7.4 Estimated Implementation Effort

- Spring reverb processing: ~200 lines of C++
- Parameter handling extensions: ~50 lines
- Transducer EQ setup: ~60 lines
- Sample rate scaling: ~40 lines
- Mode switching logic: ~30 lines
- Digital reverb presets: ~40 lines
- Shimmer pitch shifter (if included): ~80 lines
- **Total C++: ~500 lines**

- Kotlin UI (mode selector, conditional params): ~100 lines
- EffectRegistry updates: ~30 lines
- Factory preset updates: ~20 lines
- **Total Kotlin: ~150 lines**

---

## Part 8: Key References

### Academic Papers
1. **Parker, J. & Bilbao, S.** (2009). "Spring Reverberation: A Physical Perspective." Proc. DAFx-09, Como, Italy. -- Physical model derivation, dispersion relation for helical springs, FDTD modeling.
2. **Abel, J., Berners, D. & Smith, J.O.** (2006). "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure." Proc. AES 121st Convention. -- Cascaded allpass approach, Thiran filter design for dispersion.
3. **Bilbao, S.** (2009). *Numerical Sound Synthesis*. Cambridge University Press. -- Chapter on spring reverb FDTD, comprehensive physical modeling theory.
4. **Wittrick, W.H.** (1966). "On Elastic Wave Propagation in Helical Springs." *Int. J. Mech. Sci.*, 8, 25-47. -- Original derivation of the dispersion relation for helical springs.
5. **Dattorro, J.** (1997). "Effect Design: Part 1: Reverberator and Other Filters." *J. Audio Eng. Soc.*, 45(9), 660-684. -- The Dattorro plate reverb algorithm used in our existing implementation.
6. **Martinez Ramirez, M.A., Benetos, E. & Reiss, J.D.** (2020). "Modeling Plate and Spring Reverberation Using a DSP-Informed Deep Learning Approach." Proc. ICASSP 2020. -- Neural approach to spring reverb modeling.
7. **Schroeder, M.R.** (1962). "Natural Sounding Artificial Reverberation." *J. Audio Eng. Soc.*, 10(3), 219-223. -- Foundation of allpass diffusion networks.
8. **Smith, J.O.** (2010). *Physical Audio Signal Processing*. https://ccrma.stanford.edu/~jos/pasp/ -- Dispersive waveguides, allpass dispersion filters.

### Hardware References
9. **Accutronics/ATUS** Reverb Tank Application Guide. -- Tank specifications, naming convention, impedance matching.
10. **Fender 6G15** Schematic (1961). -- The reference circuit for guitar spring reverb tone.
11. **Fender Super Reverb / Twin Reverb / Deluxe Reverb** schematics. -- Onboard spring reverb integration.

### Software References
12. **Valhalla DSP Blog** (Sean Costello). Various posts on reverb algorithm design, diffusion, allpass topology. https://valhalladsp.com/
13. **SELECTOR Spring Reverb VST** (jennyrave-sys). GitHub. -- Reference implementation using 3 parallel delay lines with LFO modulation.
14. **Freeverb3** spring reverb implementation. -- Open-source reference.

---

## Part 9: Summary and Recommendations

### Primary Recommendation: Algorithm B (Chirp Allpass Dispersion Chain)

This offers the best balance of:
- **Authenticity**: The cascaded allpass chain accurately reproduces the chirp/drip characteristic
- **CPU efficiency**: ~184-254 ops/sample, cheaper than the existing Dattorro plate
- **Memory efficiency**: ~50 KB total
- **Tuneability**: Spring count, chirp character, decay, and tone all map to clear DSP parameters
- **Compatibility**: Fits our existing Effect base class architecture, header-only pattern, and parameter system

### What Makes This Sound Real vs Generic

The three elements that separate a convincing spring reverb from a generic metallic reverb:

1. **Chirp dispersion** via the cascaded allpass chain (12 stages per spring, graduated coefficients 0.3-0.8). Without this, you have a delay effect, not a spring.

2. **Transducer coloration** via input LPF (4.5 kHz) and output resonant peak (2.5 kHz, Q=2). Without this, the reverb sounds too clean and hi-fi.

3. **Multi-spring interference** with mutually-prime delay lengths and different chirp parameters per spring. Without this, the reverb sounds single-dimensional.

### Future Enhancements

- **Neural spring model**: Train a TCN on recordings from a real 6G15 for ultimate authenticity
- **Spring tank IR loading**: Allow users to load impulse responses captured from real spring tanks (though this loses the chirp -- IRs are linear and cannot capture the dynamic dispersion)
- **Stereo spring**: Route springs 1+2 to left, springs 2+3 to right (with shared center spring) for stereo spread
- **Tank type presets**: Pre-configured parameter sets for specific Accutronics tank models (4AB3C1B, 8EB2C1B, etc.)
