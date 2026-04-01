# DSP Techniques for Real-Time Guitar Pedal Circuit Emulation

## Research Summary

This document compiles state-of-the-art DSP techniques for converting analog guitar pedal
circuits into real-time digital audio processing code, with specific focus on feasibility
for an Android application using Oboe for audio I/O. It covers physical-modeling methods
(Wave Digital Filters, Nodal DK), classical filter design (bilinear transform), nonlinear
element modeling (diodes, transistors, op-amps), anti-aliasing strategies (ADAA),
specialized component modeling (photocell/LDR for Univibe), and a survey of open-source
implementations and academic literature.

---

## Table of Contents

1. [Wave Digital Filters (WDF)](#1-wave-digital-filters-wdf)
2. [Nodal DK Method (Discrete K-Method)](#2-nodal-dk-method-discrete-k-method)
3. [Circuit-Derived Digital Filters](#3-circuit-derived-digital-filters)
4. [Diode Clipping: Mathematical Models](#4-diode-clipping-mathematical-models)
5. [Antiderivative Anti-Aliasing (ADAA)](#5-antiderivative-anti-aliasing-adaa)
6. [Photocell/LDR Modeling (for Univibe)](#6-photocellldr-modeling-for-univibe)
7. [Specific Pedal Circuit Analyses](#7-specific-pedal-circuit-analyses)
8. [Existing Open-Source Implementations](#8-existing-open-source-implementations)
9. [Neural Network Approaches](#9-neural-network-approaches)
10. [Performance Considerations for Mobile/ARM](#10-performance-considerations-for-mobilearm)
11. [Recommended Architecture for This Project](#11-recommended-architecture-for-this-project)
12. [Academic Papers and References](#12-academic-papers-and-references)
13. [Source Links](#13-source-links)

---

## 1. Wave Digital Filters (WDF)

### 1.1 What Are WDFs?

Wave Digital Filters are a physical-modeling technique introduced by Alfred Fettweis in the
1970s, originally for telecommunications. They have been adopted by the audio DSP community
as a powerful method for creating "virtual analog" models of audio circuitry.

**Core Idea:** Instead of working with Kirchhoff voltages and currents directly, WDFs
transform circuit variables into "wave" variables (incident and reflected waves), analogous
to how signals propagate in transmission lines. Each circuit element is described by a
**port** with an incident wave `a` and a reflected wave `b`, related by a scattering
equation.

The wave variables are defined as:

```
a = V + R_p * I     (incident wave)
b = V - R_p * I     (reflected wave)
```

where `V` is voltage, `I` is current, and `R_p` is the "port resistance" -- a free
parameter chosen for computational convenience.

**Key advantage:** When port resistances are chosen to match component impedances, many
computations simplify to delay-free, explicit formulas -- no iteration needed for linear
elements.

### 1.2 Modeling Circuit Elements as WDF Components

Each analog component maps to a WDF "leaf node" with a specific scattering relation:

#### Resistor (R)

- Port resistance: `R_p = R`
- Reflected wave: `b = 0` (a resistor absorbs all incident energy)
- In code: simply return 0 for the reflected wave
- This is the simplest WDF element

#### Capacitor (C)

- Port resistance: `R_p = 1 / (2 * C * fs)` where `fs` = sample rate
- Uses trapezoidal (bilinear) discretization
- Reflected wave: `b[n] = a[n-1]` (one-sample delay, stores state)
- This is where the "memory" of the circuit lives
- The port resistance depends on the sample rate, so it must be recomputed when
  the sample rate changes

#### Inductor (L)

- Port resistance: `R_p = 2 * L * fs`
- Reflected wave: `b[n] = -a[n-1]` (inverted one-sample delay)
- Dual of the capacitor in the wave domain

#### Voltage Source (ideal)

- Sets voltage regardless of current
- `b = 2*Vs - a` where `Vs` is the source voltage
- Used as the input driver in most WDF circuit models

#### Current Source (ideal)

- Sets current regardless of voltage
- `b = 2*R_p*Is + a` where `Is` is the source current

#### Diode (nonlinear)

- Requires iterative solution or closed-form approximation using Lambert W / Wright Omega
- See Section 1.5 for detailed treatment
- This is the most computationally expensive WDF element

#### Op-Amp

- Can be modeled as a nullor (ideal) or using controlled-source macromodels
- Werner et al. (EUSIPCO 2016) showed how to model op-amp circuits using R-type adaptors
- The WDF approach treats the op-amp as a multiport element absorbed into an R-type adaptor
- Ideal op-amp assumption (infinite gain, infinite input impedance) is usually sufficient
  for guitar pedal modeling

### 1.3 Adaptors (Series, Parallel, R-Type)

Adaptors are the "glue" that connects WDF elements according to circuit topology. They
define how wave signals scatter between connected ports.

#### Parallel Adaptor

- Models components connected in parallel (same voltage, currents sum)
- For a 2-port parallel connection:

```
b1 = -(a1 * (G1-G2)/(G1+G2) - a2 * 2*G2/(G1+G2))
b2 = -(a2 * (G2-G1)/(G1+G2) - a1 * 2*G1/(G1+G2))
```

where G1, G2 are port conductances (1/R_p)

- One port is designated as the "adapted" port where the port resistance is set to
  achieve reflection-free operation
- Generalizes to N-port parallel connections

#### Series Adaptor

- Models components connected in series (same current, voltages sum)
- For a 2-port series connection:

```
b1 = -(a1 * (R1-R2)/(R1+R2) + a2 * 2*R1/(R1+R2))
b2 = -(a2 * (R2-R1)/(R1+R2) + a1 * 2*R2/(R1+R2))
```

- Also has one adapted port for delay-free computation
- Generalizes to N-port series connections

#### R-Type Adaptor

- Handles arbitrary topologies that cannot be decomposed into series/parallel
- Described by an N x N scattering matrix S: `b = S * a`
- Requires N^2 - 1 multiplies for N ports
- The scattering matrix is derived from Modified Nodal Analysis (MNA) of the subcircuit
- Can be accelerated with SIMD intrinsics (matrix-vector multiply)
- Essential for circuits containing bridged-T networks, Wheatstone bridges, or other
  topologies that resist series/parallel decomposition
- Werner et al. (DAFx 2015) formalized the R-type adaptor framework

### 1.4 Building a WDF Tree

A circuit is decomposed into a binary tree structure:

```
        [Root: Voltage Source or Nonlinearity]
                    |
              [Series Adaptor]
              /              \
    [Parallel Adaptor]    [Resistor]
    /              \
[Capacitor]    [Resistor]
```

**Signal flow:** Each time step involves two passes:

1. **Reflected wave pass** (bottom-up): Leaf nodes compute reflected waves, adaptors
   combine them and propagate upward to the root
2. **Incident wave pass** (top-down): Root computes its response, sends incident waves
   back down through the tree to leaf nodes

After both passes, the voltage across any element can be read as:

```
V = (a + b) / 2
```

And the current through any element can be read as:

```
I = (a - b) / (2 * R_p)
```

### 1.5 Handling Nonlinear Elements (Diodes) in WDF

This is the most challenging aspect of WDF modeling. The classic WDF framework only supports
**one nonlinearity**, placed at the root of the tree.

#### Approach 1: Root Nonlinearity with Newton-Raphson

- Place the diode(s) at the root of the WDF tree
- The rest of the tree computes a Thevenin equivalent (open-circuit voltage + series
  resistance) as seen from the nonlinear element
- The reflected wave from the tree gives us: `a = V_thevenin + R_thevenin * I`
- Solve the diode equation `I = Is * (exp(V/(n*Vt)) - 1)` against this equivalent
  using Newton-Raphson iteration
- Typically converges in 2-5 iterations for guitar pedal circuits

#### Approach 2: Lambert W / Wright Omega Closed-Form Solution

- For a single diode or antiparallel diode pair, the Shockley equation combined with the
  Thevenin equivalent from the WDF tree can be solved in closed form using the Lambert W
  function: `W(x) * exp(W(x)) = x`
- The Wright Omega function (a reformulation of Lambert W for specific arguments)
  eliminates the need to compute an exponential at audio rate
- Fast approximations of Wright Omega exist that are suitable for real-time use
  (D'Angelo and Pakarinen, DAFx 2019)
- **This is what chowdsp_wdf uses internally** for diode models
- Bernarding and Werner (2016) formalized this approach

The closed-form solution for an antiparallel diode pair in a WDF is:

```
b = a + 2*R_p*Is - 2*n*Vt*W(R_p*Is/(n*Vt) * exp((a + R_p*Is)/(n*Vt)))
```

where W is the Lambert W function (principal branch).

#### Approach 3: R-Type Adaptor for Multiple Nonlinearities

- Werner et al. showed how to handle multiple nonlinear elements by:
  1. Decomposing the circuit using an SPQR tree decomposition
  2. Pulling all nonlinear elements to the root
  3. Creating an R-type adaptor for the linear subcircuit
  4. Solving the coupled nonlinear system using a multidimensional Newton-Raphson
- RT-WDF library implements this approach
- More CPU-intensive but handles arbitrary circuits with any number of nonlinearities
- The Jacobian matrix for the Newton-Raphson solve is typically small (N_nonlinear x
  N_nonlinear) and can be efficiently inverted

### 1.6 Open-Source WDF Libraries

#### chowdsp_wdf (RECOMMENDED for this project)

- **Repository:** https://github.com/Chowdhury-DSP/chowdsp_wdf
- **Paper:** arXiv:2210.12554 (2022)
- Header-only C++ library (C++14 minimum)
- Template metaprogramming interface for fixed-topology circuits -- the compiler resolves
  the entire tree structure at compile time, enabling aggressive inlining and optimization
- Supports: Resistor, Capacitor, Inductor, Voltage Source, Current Source, Diode
  (Wright Omega), Diode Pair, Op-Amp (ideal nullor)
- Series, Parallel, and R-Type adaptors
- Optional SIMD acceleration via XSIMD library
- No external dependencies (XSIMD is optional)
- Used in production: BYOD plugin, ChowKick, ChowCentaur
- **Very suitable for Android/NDK integration** -- header-only, no platform dependencies
- Maintained by Jatin Chowdhury, who has published extensively on this topic

**Example: RC Lowpass Filter in chowdsp_wdf:**

```cpp
namespace wdft = chowdsp::wdft;

struct RCLowpass {
    wdft::ResistorT<double> r1 { 1.0e3 };           // 1K ohm
    wdft::CapacitorT<double> c1 { 1.0e-6 };         // 1uF
    wdft::WDFSeriesT<double, decltype(r1), decltype(c1)> s1 { r1, c1 };
    wdft::PolarityInverterT<double, decltype(s1)> i1 { s1 };
    wdft::IdealVoltageSourceT<double, decltype(s1)> vs { s1 };

    void prepare(double sampleRate) {
        c1.prepare(sampleRate);
    }

    inline double processSample(double x) {
        vs.setVoltage(x);
        vs.incident(i1.reflected());
        i1.incident(vs.reflected());
        return wdft::voltage<double>(c1);
    }
};
```

**Example: Diode Clipper in chowdsp_wdf:**

```cpp
namespace wdft = chowdsp::wdft;

struct DiodeClipper {
    wdft::ResistorT<float> r1 { 4700.0f };          // 4.7K series resistor
    wdft::CapacitorT<float> c1 { 47.0e-9f };        // 47nF capacitor
    wdft::WDFSeriesT<float, decltype(r1), decltype(c1)> s1 { r1, c1 };
    wdft::PolarityInverterT<float, decltype(s1)> inv { s1 };
    // Diode pair: Is=2.52e-9, Vt=0.02585, nDiodes=2 (antiparallel)
    wdft::DiodePairT<float, decltype(inv)> dp { inv, 2.52e-9f, 0.02585f, 2 };
    wdft::IdealVoltageSourceT<float, decltype(s1)> vs { s1 };

    void prepare(float sampleRate) {
        c1.prepare(sampleRate);
    }

    float processSample(float x) {
        vs.setVoltage(x);
        dp.incident(inv.reflected());
        inv.incident(dp.reflected());
        return wdft::voltage<float>(c1);
    }
};
```

#### RT-WDF

- **Repository:** https://github.com/RT-WDF/rt-wdf_lib
- C++ library supporting arbitrary topologies and multiple nonlinearities
- Uses R-type adaptors with Newton-Raphson solver
- More flexible but heavier than chowdsp_wdf
- Case studies in the paper: switchable attenuator, Fender Bassman tone stack,
  common-cathode triode amplifier stage
- Run-time configurable (topology not fixed at compile time)
- Better suited for prototyping and circuits with many nonlinear elements

#### Faust wdmodels Library

- **Documentation:** https://faustlibraries.grame.fr/libs/wdmodels/
- WDF primitives built into the Faust functional audio programming language
- Includes resistor, capacitor, inductor, diode, voltage/current source, and
  all adaptor types as Faust primitives
- Benchmarks show competitive or better performance than hand-written C++ for
  linear networks (Faust's compiler optimizations)
- Can generate standalone C++ code from Faust programs via `faust2cxx`
- Includes a validated Tube Screamer clipping stage emulation (compared against SPICE)
- Dirk Roosenburg's thesis work on this library is a good reference

---

## 2. Nodal DK Method (Discrete K-Method)

### 2.1 What Is It?

The Nodal DK Method was developed by David Yeh in his 2009 Stanford CCRMA PhD thesis
"Digital Implementation of Musical Distortion Circuits by Analysis and Simulation." It is
the other major approach (alongside WDF) for physically-informed circuit modeling.

**Core Idea:** Rather than transforming to wave variables (like WDF), the DK method works
directly with Kirchhoff voltages/currents using a state-space formulation derived from
Modified Nodal Analysis (MNA).

The high-level process:

1. Write the circuit's Modified Nodal Analysis (MNA) equations
2. Separate linear and nonlinear elements
3. Discretize the linear part using the trapezoidal rule (bilinear transform)
4. Solve the resulting discrete-time nonlinear system using Newton-Raphson iteration

### 2.2 The DK Method Step by Step

#### Step 1: Modified Nodal Analysis

Express the circuit as:

```
(G + s*C) * x = b*u + N*i_nl(v_nl)
```

where:
- `G` = conductance matrix (contributions from resistors)
- `C` = capacitance/inductance matrix (energy-storing elements)
- `x` = vector of node voltages and branch currents (the unknowns)
- `u` = input source vector
- `b` = input source incidence matrix
- `N` = nonlinear element incidence matrix
- `i_nl(v_nl)` = nonlinear current function (e.g., Shockley diode equation)
- `v_nl` = voltages across nonlinear elements

#### Step 2: Trapezoidal Discretization (the "D" in DK)

Replace `s` with the bilinear transform:

```
s = (2/T) * (1 - z^-1) / (1 + z^-1)
```

where T = 1/fs (sample period). This yields a discrete-time system that can be solved
at each time step. The trapezoidal rule preserves the passivity of the original circuit,
which is important for numerical stability.

#### Step 3: K-Method Nonlinear Solve

After discretization, the system takes the form:

```
p = A_hat * x[n-1] + B_hat * u[n]
i_nl = f(v_nl)                        -- nonlinear function (diode, transistor, etc.)
v_nl = D_hat * x[n-1] + E_hat * u[n] + F_hat * i_nl
```

The "K-method" refers to solving for `i_nl` and `v_nl` using Newton-Raphson iteration:

```
v_nl^(k+1) = v_nl^(k) - J^(-1) * g(v_nl^(k))
```

where `J` is the Jacobian of the nonlinear system and `g` is the residual function.
Once `i_nl` and `v_nl` are found, the state update `x[n]` is computed directly.

### 2.3 How Does It Handle Nonlinear Elements?

The DK method naturally handles **multiple nonlinear elements** -- they are all collected
into the nonlinear function vector `i_nl(v_nl)`. This is a major advantage over basic WDF
(which traditionally allows only one nonlinearity at the tree root).

For guitar pedal circuits, Newton-Raphson convergence typically occurs in 3-10 iterations
depending on the number and severity of nonlinearities. The iteration count increases
with signal amplitude (more clipping = harder to converge) and with the number of
coupled nonlinear elements.

### 2.4 Key Findings from Yeh's Thesis

Yeh's thesis specifically analyzed these guitar circuits:
- **Boss DS-1** distortion pedal
- **ProCo RAT** distortion pedal
- **Tube Screamer (TS-808/TS-9)** overdrive
- **Fender Bassman** amplifier tone stack
- **Fuzz Face** transistor fuzz

Key findings:

1. The DK method can automatically generate real-time digital filters from circuit
   schematics -- given a netlist, the matrices can be computed programmatically
2. For simple circuits (1-2 nonlinearities), the method produces efficient real-time code
3. For complex circuits, the matrix inversions and Newton-Raphson iterations become
   expensive and may require optimization (precomputing LU decompositions, etc.)
4. The method accurately captures frequency-dependent distortion behavior, including
   the subtle interaction between EQ and clipping stages
5. Discretization artifacts (frequency warping, aliasing) can be minimized by oversampling
   or using higher-order discretization methods

### 2.5 DK Method vs. WDF Comparison

| Aspect | WDF | Nodal DK Method |
|--------|-----|-----------------|
| **Approach** | Wave variables, tree topology | Kirchhoff variables, state-space |
| **Multiple nonlinearities** | Difficult (needs R-type adaptor) | Natural (built into formulation) |
| **Variable components** | Easy (change port resistance) | Requires matrix recomputation |
| **CPU cost (simple circuit)** | Lower | Higher (matrix operations) |
| **CPU cost (complex circuit)** | Comparable | Comparable |
| **Automation** | Requires manual tree construction | Can be automated from netlist |
| **Stability** | Inherently passivity-preserving | Needs careful discretization |
| **Modularity** | High (swap elements easily) | Lower (whole-system approach) |
| **Parameter changes** | Cheap (update one port) | Expensive (recompute matrices) |
| **Available libraries** | chowdsp_wdf, RT-WDF, Faust | Guitarix dkbuilder |

**Key insight from Chowdhury's comparison paper (arXiv:2009.02833):** For the Klon Centaur:
- WDF was used for the input buffer and tone/output stages (linear portions)
- The nonlinear gain stage was modeled separately (either WDF with root NL, or RNN)
- The RNN approach was most CPU-efficient on embedded ARM for the gain stage
- WDF was most CPU-efficient for linear filter stages
- **Hybrid approach recommended:** Use WDF for linear filter stages, specialized nonlinear
  processing for clipping stages

### 2.6 Guitarix's dkbuilder Tool

Guitarix includes a tool called **dkbuilder** that automates the DK method:
- Input: gschem electronic schematic file (open-source schematic format)
- Output: Guitarix plugin or standalone LV2 plugin
- Calculates the transfer function from the circuit schematic automatically
- Automates the entire analog-to-digital conversion pipeline
- **Source:** https://sourceforge.net/projects/guitarix/

This is the closest thing to a "push-button" circuit-to-code tool that exists in the
open-source world. However, it is tightly coupled to the Guitarix/Linux ecosystem and
would require significant adaptation for Android use.

---

## 3. Circuit-Derived Digital Filters

### 3.1 Analog to Digital Filter Conversion (Bilinear Transform)

The bilinear transform converts an analog transfer function H(s) to a digital transfer
function H(z) by substituting:

```
s = (2/T) * (z - 1) / (z + 1)    where T = 1/fs (sample period)
```

This is the same discretization used in WDF (for capacitors and inductors) and in the
DK method, but here we apply it directly to complete transfer functions rather than
individual circuit elements.

#### RC Low-Pass Filter Example

```
Analog transfer function:
    H(s) = 1 / (1 + s*R*C)
    Cutoff frequency: fc = 1 / (2*pi*R*C)

Digital (after bilinear transform):
    H(z) = (b0 + b1*z^-1) / (a0 + a1*z^-1)

    where:
        wc = 2*pi*fc                     -- analog cutoff frequency
        K  = 2*fs                        -- bilinear constant

        // With frequency pre-warping:
        wc_warped = K * tan(wc / K)      -- corrects for frequency warping

        b0 = wc_warped
        b1 = wc_warped
        a0 = K + wc_warped
        a1 = wc_warped - K

        // Normalize by a0:
        b0 /= a0;  b1 /= a0;  a1 /= a0;

Difference equation:
    y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
```

#### RC High-Pass Filter

```
Analog transfer function:
    H(s) = s*R*C / (1 + s*R*C)

Digital:
    b0 =  K / (K + wc_warped)
    b1 = -K / (K + wc_warped)
    a1 = (wc_warped - K) / (K + wc_warped)
```

#### Frequency Pre-Warping

**Frequency pre-warping is critical.** The bilinear transform compresses the entire
analog frequency axis (0 to infinity) into the digital range (0 to fs/2). This means
the cutoff frequency of the digital filter will be lower than intended unless pre-warping
is applied:

```
wc_warped = (2*fs) * tan(pi * fc / fs)
```

At 48kHz sample rate, the error becomes noticeable above ~5kHz without pre-warping.
For guitar pedal filters with cutoffs below 5kHz, the difference is small but audible
to trained ears. Always pre-warp.

### 3.2 Guitar Tone Stack Modeling

A tone stack (bass/mid/treble controls) can be modeled as a higher-order IIR filter:

**Method (from Yeh's thesis and ampbooks.com):**

1. Write Kirchhoff's Voltage Law equations for the tone circuit loops
2. Solve the resulting simultaneous equations symbolically (matrix inversion)
3. Obtain H(s) as a ratio of polynomials in s, with potentiometer positions as parameters
4. Apply bilinear transform (with pre-warping) to get H(z)
5. Result: typically a 3rd or 4th-order IIR filter

For a typical Fender-style tone stack (3-knob bass/mid/treble):

```
H(z) = (B0 + B1*z^-1 + B2*z^-2 + B3*z^-3 + B4*z^-4) /
       (A0 + A1*z^-1 + A2*z^-2 + A3*z^-3 + A4*z^-4)

Difference equation:
    y[n] = (B0*x[n] + B1*x[n-1] + B2*x[n-2] + B3*x[n-3] + B4*x[n-4]
          - A1*y[n-1] - A2*y[n-2] - A3*y[n-3] - A4*y[n-4]) / A0
```

**Key efficiency point:** Coefficients depend on potentiometer positions. When a knob
changes, recompute the analog transfer function coefficients, then re-apply the bilinear
transform to get new digital coefficients. This recalculation only happens on knob
changes (maybe 100 times per second at most), not at audio rate (48000 times per second).

For implementation stability, decompose high-order IIR filters into cascaded second-order
sections (biquads) to avoid numerical precision issues with floating-point coefficients.

### 3.3 Op-Amp Gain Stage Digital Modeling

**Non-inverting amplifier (most common in guitar pedals):**

```
Gain = 1 + Rf/Rg
```

where `Rf` = feedback resistance, `Rg` = resistance to ground.

In a distortion pedal (e.g., DS-1, Tube Screamer), the gain stage typically has:
- Variable gain (distortion/drive knob controls Rf via a potentiometer)
- Frequency-dependent gain (capacitors in the feedback path create shelving or bandpass
  characteristics)
- Clipping elements (diodes) in or after the feedback loop

#### Soft Clipping (diodes in feedback loop -- Tube Screamer style)

- Diodes reduce the effective gain when signal exceeds their forward voltage
- Model as: gain is linear up to the diode threshold, then a compression curve above
- The "knee" where clipping begins is gradual and musically pleasing
- Produces mostly odd-order harmonics when symmetric, with some compression

#### Hard Clipping (diodes to ground -- DS-1/RAT style)

- Op-amp output is amplified fully, then diodes shunt the signal to ground when it
  exceeds their forward voltage
- Creates sharper clipping with more odd-order harmonics
- Sounds more aggressive than soft clipping
- The clipping is independent of the gain setting (just amplitude-dependent)

**Digital implementation approach:** Model the linear gain stage as an IIR filter (bilinear
transform of the op-amp transfer function including feedback network), followed by a
nonlinear waveshaping function for the diode clipping. This "cascade" approach is
approximate but efficient. For higher accuracy, use WDF with the diode at the root.

---

## 4. Diode Clipping: Mathematical Models

### 4.1 The Shockley Diode Equation

The fundamental equation governing diode behavior:

```
I = Is * (exp(V / (n * Vt)) - 1)
```

where:
- `I`  = current through the diode (amps)
- `Is` = reverse saturation current (amps) -- tiny leakage current when reverse-biased
- `V`  = voltage across the diode (volts)
- `n`  = ideality factor (emission coefficient) -- 1 for ideal, 1-2 for real diodes
- `Vt` = thermal voltage = kT/q ~ 25.85 mV at 20 degrees Celsius

The `-1` term is often dropped in audio modeling since `exp(V/(n*Vt))` dominates for
any significant forward voltage.

### 4.2 Silicon vs. Germanium vs. Schottky Diode Parameters

| Parameter | Silicon (1N4148) | Germanium (1N34A) | Schottky (BAT41) | LED (red) |
|-----------|-----------------|-------------------|-------------------|-----------|
| Is        | ~1e-12 A (1 pA) | ~1e-6 A (1 uA)   | ~1e-8 A (10 nA)  | ~1e-20 A  |
| n         | ~2.0            | ~1.0 - 1.3        | ~1.05             | ~1.5 - 2  |
| Vf (typ)  | ~0.6-0.7 V     | ~0.2-0.3 V       | ~0.3-0.4 V       | ~1.6-2.0 V|
| Clipping  | Hard, abrupt    | Soft, gradual     | Medium            | Very hard |
| Sound     | Aggressive, edgy| Warm, compressed  | In between        | Clean clip|

**Why this matters for guitar:** The diode type fundamentally defines the distortion
character. The Shockley equation parameters translate directly into the shape of the
clipping curve -- the "knee" sharpness, the maximum clipped level, and the harmonic
content of the distorted signal.

Common pedal diode configurations:
- **ProCo RAT:** 1N914 (silicon), symmetric
- **Boss DS-1:** 1N4148 (silicon), symmetric, hard clip to ground
- **Tube Screamer (TS-808):** 1N914 (silicon), symmetric, soft clip in feedback
- **Tube Screamer (TS-9):** MA150 (silicon), asymmetric (2 in one direction, 1 in other)
- **Big Muff:** 1N914 pairs, two clipping stages cascaded
- **Klon Centaur:** Germanium + silicon mix, asymmetric
- **Vintage Fuzz Face:** Germanium transistors (PNP)

### 4.3 Symmetric Clipping (Back-to-Back Diodes)

Two identical diodes in antiparallel: clips both positive and negative equally.
Produces primarily odd-order harmonics (3rd, 5th, 7th...).

```cpp
// Simple symmetric hard clip approximation (crude but fast)
float symmetricHardClip(float x, float threshold) {
    if (x > threshold) return threshold;
    if (x < -threshold) return -threshold;
    return x;
}

// Symmetric soft clip using tanh (good general-purpose approximation)
float tanhSoftClip(float x, float drive) {
    return std::tanh(x * drive);
}

// More physically accurate: Shockley-based symmetric clip
// Models antiparallel diode pair where net current is 2*Is*sinh(V/(n*Vt))
float shockleyClip(float x, float Is, float n, float Vt) {
    float nVt = n * Vt;
    // For antiparallel diodes:
    // I_net = Is*(exp(V/(nVt)) - 1) - Is*(exp(-V/(nVt)) - 1)
    //       = 2*Is*sinh(V/(nVt))
    // Simplified waveshaper (inverse mapping):
    return nVt * std::asinh(x / (2.0f * Is));
}
```

### 4.4 Asymmetric Clipping

Different diodes (or different numbers of diodes) on positive vs. negative half-cycles.
Produces even-order harmonics (2nd, 4th) in addition to odd-order, giving a "tube-like"
warmth.

```cpp
// Asymmetric clip: silicon on positive, germanium on negative
float asymmetricClip(float x) {
    if (x > 0.0f) {
        // Silicon: clips at ~0.7V, hard knee
        return 0.7f * std::tanh(x / 0.7f);
    } else {
        // Germanium: clips at ~0.3V, soft knee
        return -0.3f * std::tanh(-x / 0.3f);
    }
}

// More general asymmetric clip with configurable parameters
float asymmetricClipParametric(float x, float threshPos, float threshNeg,
                                float kneePos, float kneeNeg) {
    if (x > 0.0f) {
        return threshPos * std::tanh(x * kneePos / threshPos);
    } else {
        return -threshNeg * std::tanh(-x * kneeNeg / threshNeg);
    }
}
```

### 4.5 Multi-Stage Clipping

Some pedals (e.g., Big Muff) cascade multiple clipping stages. Each stage adds its own
harmonic content and shapes the waveform differently:

- **First stage:** Creates initial distortion, generates harmonics
- **Second stage:** Clips the already-clipped signal, creating intermodulation between
  existing harmonics and adding new ones
- **Result:** A thicker, more complex distortion with a "wall of sound" character

In digital implementation, simply cascade the waveshapers with appropriate filtering
between stages. Anti-aliasing (ADAA or oversampling) should be applied at each stage
independently.

---

## 5. Antiderivative Anti-Aliasing (ADAA)

### 5.1 The Aliasing Problem

When a nonlinear function (clipping, waveshaping) is applied to a digital signal, it
generates harmonics that can exceed the Nyquist frequency (fs/2). These harmonics "fold
back" into the audio band as inharmonic aliasing artifacts that sound harsh and
unmusical -- a metallic, fizzy quality that is the hallmark of cheap digital distortion.

A 1kHz guitar note distorted with hard clipping generates harmonics at 3kHz, 5kHz, 7kHz,
9kHz, 11kHz, 13kHz, 15kHz, 17kHz, 19kHz, 21kHz, 23kHz, 25kHz... At 48kHz sample rate,
harmonics above 24kHz fold back: the 25kHz harmonic appears at 23kHz, the 27kHz harmonic
appears at 21kHz, etc. These folded harmonics are not musically related to the original
note and sound terrible.

**Traditional solution: Oversampling**
- Upsample by 2x-8x, apply the nonlinearity at the higher rate, then downsample with
  an anti-aliasing filter
- This pushes the Nyquist frequency higher so harmonics have more room before folding
- **Drawback:** CPU cost scales linearly with oversampling factor (4x oversampling = 4x
  the processing cost for the nonlinear stage, plus the cost of the resampling filters)

### 5.2 How ADAA Works

ADAA was introduced by Parker, Zavalishin, and Le Bivic (Native Instruments) at DAFx 2016
in the paper "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time
Convolution."

**Key insight:** Instead of evaluating the nonlinear function `f(x)` at discrete sample
points (which treats the signal as a staircase), ADAA treats the input as a
piecewise-linear continuous signal, applies the nonlinearity to it, and computes the
**integral** of the result over each sample interval.

This is equivalent to convolving the nonlinear function's output with a rectangular
window (for 1st order) or triangular window (for 2nd order), which acts as a low-pass
filter that suppresses the aliased harmonics.

### 5.3 First-Order ADAA

For a memoryless nonlinear function `f(x)` with first antiderivative
`F1(x) = integral(f(x) dx)`:

```
y[n] = (F1(x[n]) - F1(x[n-1])) / (x[n] - x[n-1])
```

**Ill-conditioning:** When `x[n]` is very close to `x[n-1]` (denominator approaches zero),
use the fallback:

```
if |x[n] - x[n-1]| < tolerance:
    y[n] = f((x[n] + x[n-1]) / 2.0)
```

A typical tolerance value is `1e-5` to `1e-7` depending on the application.

### 5.4 Second-Order ADAA

Uses the second antiderivative `F2(x) = integral(F1(x) dx)` for better aliasing
suppression:

```
// First, compute intermediate value:
if |x[n] - x[n-1]| > tolerance:
    d1[n] = (F2(x[n]) - F2(x[n-1])) / (x[n] - x[n-1])
else:
    d1[n] = F1((x[n] + x[n-1]) / 2.0)

// Then compute output:
if |x[n] - x[n-1]| > tolerance:
    y[n] = (2.0 / (x[n] - x[n-1])) * (d1[n] - d1[n-1])
else:
    // Second-level fallback using L'Hopital's rule
    y[n] = f((x[n] + x[n-1]) / 2.0)  // simplified fallback
```

The actual implementation has additional edge cases; see Chowdhury's ADAA repository
for production-quality code with all fallback paths handled correctly.

### 5.5 Antiderivatives for Common Clipping Functions

#### Hard Clip: f(x) = clamp(x, -1, 1)

```
         { -x - 0.5              if x < -1
F1(x) = { x^2 / 2               if -1 <= x <= 1
         { x - 0.5               if x > 1

         { -x^2/2 - x/2 - 1/6   if x < -1
F2(x) = { x^3/6                  if -1 <= x <= 1
         { x^2/2 - x/2 + 1/6    if x > 1
```

These are simple piecewise polynomials -- extremely cheap to compute.

#### Soft Clip: f(x) = tanh(x)

```
F1(x) = log(cosh(x))

F2(x) = 0.5 * (x * log(cosh(x)) + Li2(-exp(-2x)) + x^2/2 + pi^2/24)
```

where `Li2` is the dilogarithm function (Spence's function). The first antiderivative
is simple, but the second requires the dilogarithm, which can be computed using:
- The `polylogarithm` C/C++ library (available on GitHub)
- A polynomial approximation (less accurate but faster)
- Just use 1st-order ADAA for tanh (good enough in practice)

#### Cubic Soft Clip: f(x) = x - x^3/3 (for |x| <= 1)

```
F1(x) = x^2/2 - x^4/12    (for |x| <= 1)
F2(x) = x^3/6 - x^5/60    (for |x| <= 1)
```

Simple polynomials -- very efficient. This is a good compromise between tanh and hard clip.

#### Diode Clipper (Shockley-based)

For WDF diode models, the antiderivatives use Lambert W / Wright Omega functions.
Analytical expressions exist and have been validated in the DAFx 2020 paper by
Bernardini et al., "Antiderivative Antialiasing in Nonlinear Wave Digital Filters."

### 5.6 ADAA vs. Oversampling: Performance Comparison

| Method | Aliasing Reduction | CPU Cost | Latency | Complexity |
|--------|-------------------|----------|---------|------------|
| No anti-aliasing | None | 1x | 0 samples | Trivial |
| 2x oversampling | Moderate | ~2.5x | 1-4 samples | Low |
| 4x oversampling | Good | ~4.5x | 2-8 samples | Low |
| 8x oversampling | Excellent | ~9x | 4-16 samples | Low |
| 1st-order ADAA | Good | ~1.2-1.5x | 0 samples | Moderate |
| 2nd-order ADAA | Very good | ~1.5-2x | 0 samples | Higher |
| ADAA + 2x OS | Excellent | ~3-4x | 1-4 samples | Moderate |

**ADAA advantages:**
- Much cheaper than oversampling for comparable aliasing reduction
- Zero added latency (oversampling filters add a few samples of latency)
- Can be combined with modest oversampling (e.g., 2x + 1st-order ADAA) for best results
  at less cost than 8x oversampling alone

**ADAA limitations:**
- Requires computing antiderivatives analytically -- not possible for all functions
  (must be able to integrate the waveshaping function in closed form)
- Some functions have complex antiderivatives (tanh requires dilogarithm for 2nd order)
- The fallback condition (when consecutive samples are close) can introduce minor
  artifacts if the tolerance is not tuned carefully
- Best for **memoryless** nonlinearities (no internal state). For stateful systems
  (e.g., nonlinearity inside a feedback loop), see Holters, "Antiderivative Antialiasing
  for Stateful Systems," DAFx 2019

### 5.7 ADAA Applicability to Different Clipping Types

| Clipping Type | ADAA Feasibility | Notes |
|---------------|------------------|-------|
| Hard clip | Excellent | Antiderivatives are simple piecewise polynomials |
| Soft clip (tanh) | Good (1st order), complex (2nd) | F1 = log(cosh(x)) is simple |
| Cubic soft clip | Excellent | Simple polynomial antiderivatives |
| Asymmetric clip | Good | Compute ADAA separately for positive/negative halves |
| Diode curves (Shockley) | Good | Lambert W antiderivatives available |
| Exponential waveshaper | Good | Antiderivative is another exponential |
| Foldback distortion | Complex | Piecewise; must compute per segment |
| Feedback nonlinearity | Complex | Requires "stateful ADAA" extension |

### 5.8 C++ Implementations of ADAA

#### Jatin Chowdhury's ADAA Repository

- **URL:** https://github.com/jatinchowdhury18/ADAA
- Contains implementations of 1st and 2nd order ADAA for:
  - Hard clip
  - Tanh (soft clip)
  - Nonlinear waveguide (stateful example)
- Both real-time computation and table-lookup variants
- JUCE-based audio plugin with A/B comparison against oversampling
- Well-documented code with clear mathematical comments

#### Faust AANL Library

- **URL:** https://faustlibraries.grame.fr/libs/aanl/
- Built-in anti-aliased nonlinearities for the Faust programming language
- Includes ADAA versions of: hard clip, cubic clip, tanh, arctan
- Can generate C++ code from Faust programs

#### chowdsp_wdf + ADAA Integration

The DAFx 2020 paper by Bernardini et al. shows how to integrate ADAA directly into WDF
diode models, avoiding the need for oversampling entirely. This modifies the diode's
scattering equation to use antiderivative-based computation rather than instantaneous
evaluation. The result is a WDF circuit model that inherently produces less aliasing
without any additional processing stages.

### 5.9 Recommended ADAA Strategy for This Project

For an Android guitar pedal app, where CPU budget is constrained:

1. **Hard clipping stages (DS-1, RAT):** Use 1st-order ADAA -- trivial antiderivatives,
   massive CPU savings over oversampling
2. **Soft clipping (tanh, Tube Screamer):** Use 1st-order ADAA with `F1(x) = log(cosh(x))`
3. **Diode stages modeled via WDF:** Integrate ADAA into the WDF nonlinear solver per the
   Bernardini et al. approach
4. **For maximum quality:** Combine 1st-order ADAA with 2x oversampling
5. **User-selectable quality:** "Low" = no anti-aliasing, "Medium" = ADAA only,
   "High" = ADAA + 2x oversampling

---

## 6. Photocell/LDR Modeling (for Univibe)

### 6.1 How the Univibe Works

The Univibe (Shin-ei Uni-Vibe, 1968) is an iconic phaser effect used by Jimi Hendrix,
Robin Trower, and David Gilmour. It has a character distinct from standard phasers due to
its unique modulation mechanism.

**Signal path:**

1. Input preamp (BJT transistor)
2. Four first-order all-pass phase-shift stages in series
3. Each stage's cutoff frequency is controlled by the resistance of its LDR (photoresistor)
4. The modulated (phase-shifted) signal is mixed with the dry signal
5. Constructive/destructive interference creates moving peaks and notches (phasing)

**What makes the Univibe sound different from a standard phaser:**

- An incandescent **lamp** (not an LED) is driven by an LFO
- The lamp illuminates four CdS photoresistors (LDRs) mounted at slightly different
  distances and angles from the lamp
- Each LDR therefore has a different resistance response curve
- The four all-pass stages do NOT sweep uniformly -- each sweeps at a slightly different
  rate and over a slightly different range
- This non-uniform phase shift creates a more complex, "chorus-like" modulation compared
  to standard phasers where all stages sweep identically

### 6.2 Lamp Thermal Inertia Model

The incandescent lamp is the key to the Univibe's character:

- The lamp converts electrical energy to heat, then heat to light -- a two-step process
- **Thermal lag:** Light output lags behind the driving electrical signal
- The lag is **asymmetric:** the filament heats up faster than it cools down
- This creates a non-sinusoidal, asymmetric modulation waveform even when driven by a
  pure sine wave LFO
- At higher LFO speeds, the asymmetry increases because the filament cannot fully cool
  between cycles
- The lamp also acts as a low-pass filter on the LFO, rounding off sharp transitions

**Digital model of lamp thermal behavior:**

```cpp
// First-order asymmetric thermal model of an incandescent lamp
class LampModel {
    float temperature = 0.0f;
    float thermalMassUp = 0.002f;    // Heating time constant (faster, smaller value)
    float thermalMassDown = 0.008f;  // Cooling time constant (slower, larger value)

public:
    float process(float lfoVoltage) {
        // Power dissipated in filament is proportional to V^2 (P = V^2/R)
        float target = lfoVoltage * lfoVoltage;

        // Asymmetric time constant: heats faster than it cools
        float tau = (target > temperature) ? thermalMassUp : thermalMassDown;

        // First-order exponential approach to target
        temperature += (target - temperature) * tau;

        // Light output -- for a real filament this follows Stefan-Boltzmann (T^4)
        // but a simplified model works well for audio purposes
        return std::max(0.0f, temperature);
    }

    void setSampleRate(float fs) {
        // Adjust time constants for sample rate
        // These values are approximate and should be tuned to taste
        thermalMassUp   = 1.0f - std::exp(-1.0f / (0.003f * fs));  // ~3ms attack
        thermalMassDown = 1.0f - std::exp(-1.0f / (0.010f * fs));  // ~10ms release
    }
};
```

A more sophisticated model would include:
- Stefan-Boltzmann T^4 radiation law for light output
- Second-order thermal dynamics (filament + glass envelope)
- Nonlinear resistance change of the filament with temperature

### 6.3 LDR (Light Dependent Resistor) Nonlinear Response

LDR (photoresistor) characteristics:
- Resistance decreases as light increases, but **nonlinearly**
- Typical range for CdS LDRs: ~1K ohm (bright) to ~1M ohm (dark)
- The resistance vs. light curve follows a power law:

```
R = A * L^(-gamma)
```

where:
- `L` = light intensity (normalized 0-1)
- `gamma` ~ 0.7-0.9 depending on the CdS cell material and manufacturing
- `A` = scaling constant

- **Hysteresis:** LDRs exhibit a rise/fall asymmetry -- they are slower to increase
  resistance (going dark) than to decrease it (going bright). This adds to the Univibe's
  asymmetric modulation character on top of the lamp's thermal lag.

**Digital LDR model:**

```cpp
class LDRModel {
    float rMin;      // Minimum resistance at full brightness (ohms)
    float rMax;      // Maximum resistance in darkness (ohms)
    float gamma;     // Power law exponent
    float logRMin;   // Precomputed log(rMin)
    float logRange;  // Precomputed log(rMax) - log(rMin)

public:
    LDRModel(float minR = 1000.0f, float maxR = 1000000.0f, float g = 0.8f)
        : rMin(minR), rMax(maxR), gamma(g)
    {
        logRMin = std::log(rMin);
        logRange = std::log(rMax) - logRMin;
    }

    float getResistance(float lightLevel) {
        // lightLevel: 0.0 (dark) to 1.0 (bright)
        lightLevel = std::clamp(lightLevel, 0.001f, 1.0f);

        // Power law: R = rMax * lightLevel^(-gamma), clamped to [rMin, rMax]
        float logR = logRMin + logRange * (1.0f - std::pow(lightLevel, gamma));
        return std::clamp(std::exp(logR), rMin, rMax);
    }
};
```

### 6.4 Phase Shift Stage Implementation

Each Univibe stage is a first-order all-pass filter whose cutoff frequency varies
with LDR resistance:

**Analog all-pass transfer function:**

```
H(s) = (s - w0) / (s + w0)

where w0 = 1 / (R_ldr * C)
```

This has unity gain at all frequencies but introduces a phase shift that varies from
0 degrees (at DC) through -90 degrees (at w0) to -180 degrees (at infinity).

**Digitized via bilinear transform with pre-warping:**

```cpp
struct AllPassStage {
    float c;          // Filter coefficient (varies with LDR resistance)
    float x_prev = 0; // Previous input sample
    float y_prev = 0; // Previous output sample

    void setFrequency(float r_ldr, float cap, float fs) {
        float w0 = 1.0f / (r_ldr * cap);
        // Pre-warped coefficient via bilinear transform
        float t = std::tan(w0 / (2.0f * fs));
        c = (t - 1.0f) / (t + 1.0f);
    }

    float process(float x) {
        float y = c * x + x_prev - c * y_prev;
        x_prev = x;
        y_prev = y;
        return y;
    }
};
```

The complete Univibe uses four such stages in series, each with its own LDR model:

```cpp
class Univibe {
    LampModel lamp;
    LDRModel ldr[4];      // Four LDRs with slightly different parameters
    AllPassStage stage[4]; // Four all-pass stages
    float lfoPhase = 0.0f;
    float lfoRate = 5.0f;  // Hz
    float mixDryWet = 0.5f;

    // LDR capacitor values (typical Univibe values, in Farads)
    const float caps[4] = { 15e-9f, 22e-9f, 47e-9f, 100e-9f };

    void prepare(float sampleRate) {
        lamp.setSampleRate(sampleRate);
        // Each LDR has slightly different min/max/gamma due to placement
        ldr[0] = LDRModel(800.0f,  800000.0f, 0.75f);
        ldr[1] = LDRModel(1000.0f, 900000.0f, 0.80f);
        ldr[2] = LDRModel(1200.0f, 1000000.0f, 0.85f);
        ldr[3] = LDRModel(900.0f,  950000.0f, 0.78f);
    }

    float processSample(float input, float sampleRate) {
        // Generate LFO
        lfoPhase += lfoRate / sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfoValue = 0.5f + 0.5f * std::sin(2.0f * M_PI * lfoPhase);

        // Lamp thermal model transforms LFO into light output
        float light = lamp.process(lfoValue);

        // Phase-shifted signal through four cascaded all-pass stages
        float wet = input;
        for (int i = 0; i < 4; i++) {
            float resistance = ldr[i].getResistance(light);
            stage[i].setFrequency(resistance, caps[i], sampleRate);
            wet = stage[i].process(wet);
        }

        // Mix dry and wet (phase cancellation creates the effect)
        return input * (1.0f - mixDryWet) + wet * mixDryWet;
    }
};
```

### 6.5 DAFx 2019 Paper: "Digital Grey Box Model of the Uni-Vibe"

**Authors:** Champ Darabundit, Pete Bischoff, Russell Wedelich (Eventide Audio)
**Published:** DAFx 2019, Birmingham, UK

Key contributions:

1. **Measured actual LDR characteristics** from an original Univibe unit using
   photodetectors and oscilloscope -- each of the four LDRs has a measurably
   different response curve (different placement relative to the lamp bulb)
2. Combined digital circuit models with measured LDR data -- this is the "grey box"
   approach (part physics-based modeling, part empirical measured data)
3. The non-identical LDR responses create **non-uniform phase shift** across the four
   stages, which is critical to the Univibe's lush, complex, chorus-like sound (as
   opposed to a regular phaser where all stages sweep uniformly)
4. Modeled the lamp's thermal response to capture the asymmetric LFO shape
5. Result: computationally efficient real-time emulation preserving the authentic
   Univibe character

**This paper is the state-of-the-art reference for digital Univibe implementation.**

---

## 7. Specific Pedal Circuit Analyses

### 7.1 Boss DS-1 Distortion

**Source:** https://www.electrosmash.com/boss-ds1-analysis

**Circuit Stages (5 total):**

1. **Input buffer:** 2SC2240 emitter follower
   - Input impedance: 470K ohm
   - High-pass filter at 7.2Hz (removes DC offset)
   - Unity gain, low output impedance

2. **Transistor booster:** 2SC2240 common-emitter
   - Voltage gain: ~35dB (56x) with negative feedback
   - Feedback network: R7=470K, C4=250pF (reduces raw gain from 454x to ~56x)
   - High-pass filters at 3.3Hz and 33Hz (bass content below 33Hz is attenuated)
   - Produces some asymmetric soft clipping against the 9V supply rails

3. **Op-amp gain stage (core distortion):**
   - Op-amp: NJM2904L (or BA728N, M5223AL, TA7136AP in different production runs)
   - Configuration: Non-inverting amplifier
   - Gain range: 1 + VR1/R13 = 1 to 22.3x (0 to 26.5dB), controlled by DIST knob
   - **Hard-clipping diodes:** D4, D5 = 1N4148, back-to-back to virtual ground (4.5V bias)
   - Clipping threshold: +/-0.7V (silicon forward voltage)
   - Input high-pass: C5(68nF) / R10(10K) = 23Hz cutoff
   - Feedback low-pass: R14(2.2K) / C10(0.01uF) = 7.2kHz cutoff
   - C7(100pF) reduces harsh high-frequency harmonics before they reach the clipper
   - Distortion pot high-pass: C8(0.47uF) / R13(4.7K) -- varies from 3Hz to 72Hz

4. **Tone control:** Passive mid-scoop design
   - Low-pass path: R16(6.8K) / C12(0.1uF) = 234Hz cutoff
   - High-pass path: C11(0.022uF) / R17(6.8K) = 1063Hz cutoff
   - Mixing pot: VR3(20K) blends between the LP and HP paths
   - Creates characteristic ~500Hz notch when centered
   - **This is the opposite** of the Tube Screamer's mid-hump -- the DS-1 scoops mids
   - Overall loss through tone stage: ~12dB

5. **Output buffer:** 2SC2240 emitter follower
   - Low impedance output for driving the next pedal/amp
   - High-pass filters at 3.4Hz and 1.6Hz

**Digital Implementation Strategy:**
- Input buffer: 1st-order high-pass IIR (bilinear transform of 7.2Hz HP)
- Transistor booster: Variable-gain IIR filter + soft asymmetric clip (transistor
  compression near supply rails)
- Op-amp gain: Bandpass IIR (23Hz to 7.2kHz) with variable gain + hard clip waveshaper
  with 1st-order ADAA
- Tone control: Crossfade between LP and HP IIR filters (same principle as tone stack)
- Output buffer: Pass-through (or minor HP for DC blocking)
- Total cost estimate: ~5-8 IIR biquad stages + 1 waveshaper with ADAA

### 7.2 ProCo RAT Distortion

**Source:** https://www.electrosmash.com/proco-rat

**Circuit Stages:**

1. **Input/Clipping amplifier (the entire front end is one op-amp stage):**
   - Op-amp: **LM308** (critical to the RAT's character due to its slow slew rate)
   - Configuration: Non-inverting amplifier
   - Gain range: 0dB to 67dB (1x to 2240x), controlled by DISTORTION knob
   - Input impedance: ~494K ohm
   - Input coupling: 22nF capacitor sets high-pass at 7.2Hz
   - High-pass filtering: Dual RC networks at 1539Hz and 60Hz create 40dB/decade
     bass rolloff below 60Hz
   - Low-pass in feedback: 100pF capacitor across the variable resistor,
     cutoff ~16kHz at maximum distortion
   - **Clipping diodes:** 1N914 silicon (functionally identical to 1N4148),
     symmetric back-to-back, clips at +/-0.7V

2. **Tone control:** Passive low-pass filter
   - Components: 100K pot (FILTER knob) + 1.5K series resistor + 3.3nF capacitor
   - Range: 475Hz cutoff (tone fully clockwise) to 32kHz (tone fully counter-clockwise)
   - Creates the "shark fin" waveform shape characteristic of the RAT at high settings

3. **Output stage:** JFET common-drain buffer (2N5458)
   - High input impedance (~1M ohm)
   - Medium-low output impedance
   - 100K audio taper volume pot

**Critical LM308 Characteristics for Accurate Emulation:**

The LM308 is NOT a "transparent" op-amp. Its limitations are sonically defining:

- **Slew rate:** 0.3 V/microsecond -- approximately **40x slower** than a TL071 (13V/us).
  This means the output cannot change faster than 0.3V per microsecond. At high
  frequencies and large amplitudes, this becomes a limiting factor, creating a natural
  "soft" high-frequency rolloff that is distinct from simple low-pass filtering.
  - At 10kHz with a 1V peak signal, the required slew rate is 2*pi*10000*1 = 62.8 kV/s
    = 62.8 V/ms. The LM308 maxes out at 300 V/ms. So at anything above about 5.3kHz,
    the LM308 starts to limit the signal's slew rate.

- **Gain-bandwidth product:** Open-loop frequency response shows that bass frequencies
  receive full amplification while frequencies above 500Hz suffer progressive gain
  attenuation, reaching only ~30dB at 10kHz even when the pot is set for 67dB gain.

- **30pF compensation capacitor:** This value directly affects bandwidth and slew rate.
  Some RAT variants use different compensation caps, changing the sound character.

**Digital Implementation Strategy:**

```cpp
// Slew rate limiter -- critical for RAT character
float slewRateLimit(float input, float& prevOutput, float maxSlewPerSample) {
    float delta = input - prevOutput;
    // maxSlewPerSample = maxSlew_V_per_us * 1e-6 / sampleRate
    float maxDelta = maxSlewPerSample;
    delta = std::clamp(delta, -maxDelta, maxDelta);
    prevOutput += delta;
    return prevOutput;
}

// For the RAT at 48kHz:
// maxSlew = 0.3 V/us = 300000 V/s
// maxSlewPerSample = 300000 / 48000 = 6.25 V per sample
// This is quite large -- the slew limiting only kicks in at high amplitudes
// and high frequencies, which is exactly when it matters for the RAT's tone
```

- Model frequency-dependent gain as an IIR filter (LP in feedback path, HP at input)
- Apply symmetric hard clipping (1N914 diodes) with 1st-order ADAA
- Tone control: 1st-order LP IIR with variable cutoff (475Hz to 32kHz range)

### 7.3 Tube Screamer (TS-808 / TS-9)

**Key differences from DS-1/RAT:**
- **Soft clipping** (diodes are in the op-amp feedback loop, not to ground)
- Creates less aggressive distortion with more natural compression
- Strong mid-hump EQ (the opposite of DS-1's mid-scoop)
- Op-amp: JRC4558D (dual op-amp, known for smooth character)

**Clipping stage details:**
- Two 1N914 diodes in antiparallel **in the feedback path** of the op-amp
- When the output signal exceeds ~0.7V, the diodes conduct and effectively reduce the
  feedback resistance, which reduces the op-amp gain
- This creates gradual compression rather than abrupt hard clipping
- At low levels: gain = 1 + R_drive/R_ground (up to ~240x with 500K pot)
- At high levels: gain drops to ~2-3x due to diode conduction
- The transition between clean and clipped is smooth and "tube-like"

**Digital approach for soft clipping in feedback:**

The diode equation is **coupled** with the op-amp gain equation:

```
V_out = A * V_in - R_f * I_diode(V_out)
```

This is an **implicit** equation -- V_out appears on both sides. Options:

1. **Newton-Raphson iteration** (2-3 iterations typically sufficient at audio rate)
2. **WDF with diode at root** (Wright Omega closed-form solution -- recommended)
3. **Pre-computed lookup table** of the input-output transfer function (fast but
   less accurate for frequency-dependent behavior)
4. **Approximate waveshaper:** `out = threshold * tanh(gain * input / threshold)` --
   simple but misses the frequency-dependent interaction

The WDF approach is recommended because it naturally captures the frequency-dependent
gain reduction (the diode's conductance depends on the feedback signal, which is
frequency-shaped by the capacitors in the circuit).

### 7.4 Fuzz Face

Analyzed by Stanford CCRMA using the WDF framework:
- **Reference:** CCRMA wiki on WDF applied to the Dunlop Fuzz Face
- Two-transistor circuit (PNP germanium in vintage, NPN silicon in modern reissues)
- Highly nonlinear -- requires transistor models (not just diode models)
- WDF approach: model transistors as multiport nonlinear elements at the tree root
- The germanium transistor's temperature sensitivity is part of the Fuzz Face's
  notoriously variable behavior -- some players warm up the pedal before playing

---

## 8. Existing Open-Source Implementations

### 8.1 Guitarix

- **URL:** https://guitarix.org/
- **License:** GPL
- **Language:** C++ with FAUST DSP code
- **Platform:** Linux (JACK audio)
- Features:
  - 25+ built-in effect modules: noise gate, distortion, overdrive, flanger, phaser,
    auto-wah, tremolo, reverb, delay, compressor, EQ, and more
  - Includes **dkbuilder** tool that automates the Nodal DK method -- takes a gschem
    electronic schematic file and outputs a complete audio plugin
  - LADSPA/LV2 plugin format
  - Many effects implemented as FAUST programs that compile to C++
- **Relevance for our project:** Study their FAUST DSP code for effect algorithms;
  the dkbuilder approach is instructive even if we do not use it directly

### 8.2 ChowDSP Plugins (Jatin Chowdhury)

- **URL:** https://chowdsp.com/products.html
- **GitHub:** https://github.com/Chowdhury-DSP
- **License:** GPL v3 (dual-licensed; commercial license available)
- **Language:** C++ with JUCE framework

Key plugins and libraries:

- **ChowCentaur:** Full emulation of the Klon Centaur overdrive pedal
  - Uses WDF for input buffer, tone stack, and output buffer stages
  - Gain stage modeled with both WDF (Wright Omega diode) and RNN options
  - Open source: https://github.com/jatinchowdhury18/KlonCentaur
  - Published comparison paper (arXiv:2009.02833) evaluating WDF vs RNN vs nodal

- **BYOD (Build-Your-Own-Distortion):** Modular distortion plugin
  - 16 Drive modules (including Centaur, Tube Screamer, Big Muff, Fuzz Face circuits)
  - 11 Tone modules (various EQ and filter types)
  - 9 Utility modules
  - 7 Other modules (chorus, compressor, delay, spring reverb)
  - Open source: https://github.com/Chowdhury-DSP/BYOD
  - Runs on macOS, Windows, Linux, and **iOS** (ARM proof of concept!)
  - Available as VST3, AU, CLAP, AAX, LV2, Standalone, AUv3

- **chowdsp_wdf:** The WDF library (see Section 1.6)
- **RTNeural:** Efficient neural network inference engine for real-time audio processing
  - Used by GuitarML and other projects
  - Supports LSTM, GRU, and dense layers
  - Optimized for single-sample processing with minimal latency

### 8.3 GuitarML

- **URL:** https://guitarml.com/
- **GitHub:** https://github.com/GuitarML
- **Developer:** Keith Bloemer
- **License:** Various open source licenses

Key projects:

- **GuitarLSTM:** Train LSTM neural network models from input/output recordings of
  real amps and pedals. Record dry guitar into the amp/pedal, capture the output,
  then train a neural network to replicate the transformation.
- **PedalNetRT:** WaveNet-based pedal modeling for real-time use
- **NeuralPi:** Raspberry Pi guitar pedal using neural networks -- demonstrates
  feasibility on embedded ARM hardware
- **Proteus:** Amp/pedal "capture" plugin (similar concept to Kemper profiling,
  Quad Cortex capture, or IK Multimedia ToneX -- but free and open source)
- **SmartGuitarPedal / SmartGuitarAmp:** JUCE-based plugins with neural network backends
- All projects use **RTNeural** (Chowdhury's library) for efficient neural network
  inference at audio rate

### 8.4 CAPS (C* Audio Plugin Suite)

- **URL:** http://quitte.de/dsp/caps.html
- **Developer:** Tim Goetze
- **License:** GPL
- Features:
  - Virtual guitar amplification and classic effects
  - Signal processors and generators
  - Design philosophy: highest sound quality combined with computational efficiency
    and zero latency
  - Available as LADSPA and LV2 (via mod-audio/caps-lv2 port)
- Includes amp simulators, cabinet models, reverbs, flangers, phasers, and more

### 8.5 DISTRHO Plugin Ports

- **URL:** https://distrho.sourceforge.io/ports.php
- **GitHub:** https://github.com/DISTRHO/DISTRHO-Ports
- Cross-platform audio plugin collection
- Ports various audio plugins to Linux/LV2 and other formats
- Useful as reference implementations for audio effect architectures

### 8.6 Faust Libraries

- **wdmodels:** WDF primitives for physical circuit modeling
- **aanl:** Anti-aliased nonlinearities with built-in ADAA
- **physmodels:** Physical modeling primitives (strings, tubes, etc.)
- **filters:** Standard digital filter library (biquads, SVF, etc.)
- **platform.lib:** Includes `SR` (sample rate) and other platform constants
- Can generate standalone C++ code using `faust2cxx` or `faust2api` for embedding
  in Android NDK projects
- **URL:** https://faustlibraries.grame.fr/

### 8.7 Other Notable Projects

- **ElectroSmash:** Circuit analyses of DS-1, RAT, Tube Screamer, Big Muff, Fuzz Face,
  and many more pedals with complete schematics and component values
  (https://www.electrosmash.com/)
- **Rodent by Valdemar Erlingsson:** Free open-source ProCo RAT emulation plugin
- **TSE R47 by TSE Audio:** Free ProCo RAT emulation VST
- **awesome-audio-dsp:** Curated list of audio DSP resources on GitHub
  (https://github.com/BillyDM/awesome-audio-dsp)

---

## 9. Neural Network Approaches

While the focus of this project is physics-based modeling (WDF, DK, IIR filters + ADAA),
neural network approaches are worth documenting as a complementary or alternative method.

### 9.1 Overview

Neural network guitar effect modeling trains a deep learning model to replicate the
input-output relationship of a real analog device. The model learns the transfer function
empirically from recordings rather than from circuit analysis.

### 9.2 Key Architectures

- **LSTM (Long Short-Term Memory):** Used by GuitarLSTM. Good at capturing time-dependent
  behavior (like the memory effects of capacitors in a circuit).
- **WaveNet:** Used by PedalNetRT. Dilated causal convolutions capture long-range temporal
  dependencies. More computationally expensive than LSTM.
- **GRU (Gated Recurrent Unit):** Simpler than LSTM, often comparable accuracy.
- **Fully connected + recurrent hybrid:** Used by Proteus/NAM.

### 9.3 Pros and Cons

**Advantages:**
- Can model ANY device without knowing the circuit (just need input/output recordings)
- Can capture subtle analog behaviors that are difficult to model mathematically
  (component aging, temperature effects, intermodulation)
- Inference is typically just matrix multiplies -- well-suited to SIMD/NEON optimization

**Disadvantages:**
- No meaningful parameter control (cannot "turn the knobs" on a trained model)
- Training requires careful data collection (clean DI signal + processed signal pairs)
- Models are specific to one device at one setting (though multi-parameter training exists)
- Cannot interpolate/extrapolate to settings not in the training data
- Understanding of "why" the effect sounds the way it does is lost

### 9.4 RTNeural Library

- **URL:** https://github.com/jatinchowdhury18/RTNeural
- C++ library for efficient real-time neural network inference
- Supports: Dense, LSTM, GRU, Conv1D layers
- Optimized for single-sample processing (no buffer latency)
- Template-based: network architecture resolved at compile time for maximum optimization
- Used by GuitarML, NAM (Neural Amp Modeler), and other projects
- Could be integrated into our Android app alongside physics-based models

---

## 10. Performance Considerations for Mobile/ARM

### 10.1 Feasibility

**WDF on mobile ARM: YES, proven feasible.**

Key evidence:
- Jatin Chowdhury's Klon Centaur emulation runs on a **Teensy 4.0** (ARM Cortex-M7
  @ 600MHz) -- this is far less powerful than any modern Android phone
- chowdsp_wdf has been deployed on **iOS** (ARM) as part of the BYOD AUv3 plugin
- The Klon Centaur comparison paper (arXiv:2009.02833) evaluated WDF on embedded ARM
  and found it feasible for real-time processing
- NeuralPi (GuitarML) runs neural network inference on a Raspberry Pi 4 (ARM Cortex-A72
  @ 1.5GHz) in real time

### 10.2 CPU Budget Analysis (48kHz Mono)

At 48kHz sample rate:
- **Total budget per sample:** 1/48000 = ~20,833 nanoseconds (for 100% CPU)
- **Realistic budget (50% CPU target):** ~10,000 ns per sample
  (leaving headroom for UI, OS, garbage collection, etc.)

Estimated per-sample costs on ARM Cortex-A76 class (modern mid-range Android):

| Operation | Cost per Sample | Notes |
|-----------|----------------|-------|
| Simple IIR biquad | ~10-30 ns | 5 multiplies + 4 adds |
| Waveshaper (no AA) | ~5-20 ns | Table lookup or polynomial |
| 1st-order ADAA waveshaper | ~30-60 ns | +1 division, +2 function evals |
| 2nd-order ADAA waveshaper | ~60-120 ns | More complex fallback paths |
| WDF RC filter (2 elements) | ~50-100 ns | Inline template expansion |
| WDF diode clipper (Wright Omega) | ~200-500 ns | Lambert W approximation |
| WDF full pedal (5-8 elements, 1 NL) | ~500-2000 ns | Depends on topology |
| R-type adaptor (4x4 matrix) | ~100-200 ns | 16 multiplies + NEON |
| 2x oversampling overhead | ~100-200 ns | Resampling filters |
| RTNeural LSTM (32 hidden) | ~500-1500 ns | Matrix multiplies |

**Bottom line:** At 48kHz with a 50% CPU budget (~10,000 ns/sample), we can comfortably
run **3-5 pedal effects** simultaneously on a modern Android phone, assuming a mix of
IIR filter stages and WDF/ADAA clipping stages.

### 10.3 Optimization Strategies for Android/ARM

1. **Use `float` instead of `double`:** ARM NEON has 4-wide float32 SIMD versus 2-wide
   float64. Using float throughout gives roughly 2x throughput for SIMD-friendly code.
   Float precision is more than sufficient for audio (24-bit dynamic range).

2. **Use chowdsp_wdf's template approach:** The compiler resolves the entire WDF tree
   structure at compile time, enabling aggressive function inlining and constant
   propagation. The resulting machine code is often comparable to hand-optimized C.

3. **NEON SIMD for matrix operations:** R-type adaptors involve matrix-vector multiplies
   that map naturally to NEON intrinsics. Use XSIMD library or hand-written NEON code.

4. **Limit Newton-Raphson iterations:** For diode models, 2-3 iterations are usually
   sufficient. Use the previous sample's solution as the starting guess (warm start).

5. **Use ADAA instead of oversampling:** 1st-order ADAA provides aliasing suppression
   comparable to 4x oversampling at a fraction of the CPU cost.

6. **Avoid memory allocation in the audio thread:** Pre-allocate all buffers. Use
   Oboe's callback model correctly -- no locks, no allocation, no I/O in the callback.

7. **Profile on target hardware early:** ARM NEON performance varies between devices
   and between big/little cores. Measure with real audio workloads, not synthetic
   benchmarks.

8. **Consider block processing:** Instead of processing one sample at a time through
   the entire chain, process blocks through each stage. This improves cache locality
   and allows the compiler to auto-vectorize inner loops.

### 10.4 Oboe-Specific Considerations

- Use exclusive mode (AAudio) when available for lowest latency
- Target 48kHz sample rate (native rate on most Android devices)
- Request smallest possible buffer size (typically 48-192 samples / 1-4ms)
- Set audio thread priority to SCHED_FIFO via Oboe's built-in management
- Keep audio callback execution time well under the buffer duration

---

## 11. Recommended Architecture for This Project

### 11.1 Technology Stack

```
[Guitar Input via USB Audio / Headphone Jack]
    |
[Oboe Audio Input Stream (48kHz, mono, float)]
    |
[Input Gain/Buffer Stage (simple multiply)]
    |
[Effect Chain: Pedal 1 -> Pedal 2 -> ... -> Pedal N]
    |  (Each pedal: IIR filter stages + nonlinear stages with ADAA)
    |
[Output Level Control (simple multiply)]
    |
[Oboe Audio Output Stream]
    |
[Headphones / USB Audio Output]
```

### 11.2 Per-Effect Implementation Strategy

| Effect Type | Recommended Method | Anti-Aliasing | Est. Cost |
|-------------|-------------------|---------------|-----------|
| **Distortion (DS-1)** | Hybrid: IIR filters + waveshaper | 1st-order ADAA | ~2000 ns |
| **Overdrive (TS-style)** | WDF (chowdsp_wdf) or IIR+NL | ADAA in WDF solver | ~2500 ns |
| **Fuzz (Fuzz Face)** | WDF with transistor models | 2x OS + ADAA | ~4000 ns |
| **RAT** | IIR filters + slew limiter + clip | 1st-order ADAA | ~2000 ns |
| **Univibe** | 4x all-pass + lamp/LDR model | Not needed (linear) | ~500 ns |
| **Phaser** | Cascaded all-pass filters | Not needed | ~300 ns |
| **Chorus/Flanger** | Modulated delay line + interpolation | Not needed | ~200 ns |
| **Delay** | Circular buffer + feedback + LP filter | Not needed | ~100 ns |
| **Reverb** | Schroeder/FDN network | Not needed | ~1000 ns |
| **Wah** | Variable bandpass filter (SVF or WDF) | Not needed | ~200 ns |
| **Tone controls** | IIR biquad (bilinear transform) | Not needed | ~30 ns |
| **Noise gate** | Envelope follower + gain control | Not needed | ~50 ns |

### 11.3 C++ Library Integration for Android NDK

**Required:**
- **chowdsp_wdf** -- header-only, drop directly into jni/cpp/ or app/src/main/cpp/
  directory. No build system changes needed beyond adding the include path.

**Recommended:**
- Custom DSP code for effects that don't need full WDF treatment (simple IIR filters,
  delay lines, etc.)
- ADAA implementation for waveshaping stages (can adapt from Chowdhury's ADAA repo)

**Optional (for future expansion):**
- **XSIMD** -- for R-type adaptor SIMD acceleration (header-only, easy to integrate)
- **RTNeural** -- if adding neural-network-based amp/pedal capture models later

### 11.4 Base Effect Class

```cpp
// Base class for all audio effects
class Effect {
public:
    virtual ~Effect() = default;

    // Called once when sample rate changes or effect is first loaded
    virtual void prepare(double sampleRate) = 0;

    // Process a single sample -- called at audio rate (48000x/sec)
    virtual float processSample(float input) = 0;

    // Process a block of samples -- default implementation calls processSample
    virtual void processBlock(float* buffer, int numSamples) {
        for (int i = 0; i < numSamples; i++) {
            buffer[i] = processSample(buffer[i]);
        }
    }

    // Update a parameter (called from UI thread, must be thread-safe)
    virtual void setParameter(int paramId, float value) = 0;

    // Enable/bypass
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

protected:
    double sampleRate = 48000.0;
    bool enabled = true;
};
```

### 11.5 Oboe Audio Callback

```cpp
// Oboe audio callback -- runs on high-priority audio thread
oboe::DataCallbackResult onAudioReady(
    oboe::AudioStream* stream,
    void* audioData,
    int32_t numFrames)
{
    auto* input  = static_cast<float*>(inputBuffer);  // from input stream
    auto* output = static_cast<float*>(audioData);     // to output stream

    for (int i = 0; i < numFrames; i++) {
        float sample = input[i] * inputGain;

        // Process through effect chain (order matters!)
        for (auto& effect : activeEffects) {
            if (effect->isEnabled()) {
                sample = effect->processSample(sample);
            }
        }

        output[i] = sample * outputLevel;
    }

    return oboe::DataCallbackResult::Continue;
}
```

---

## 12. Academic Papers and References

### 12.1 Essential Reading (Ordered by Relevance)

1. **Chowdhury, J.** "A Comparison of Virtual Analog Modelling Techniques for Desktop and
   Embedded Implementations" (2020). arXiv:2009.02833.
   - Compares WDF, nodal analysis, and RNN methods for the Klon Centaur pedal
   - Benchmarks on desktop (x86) and embedded ARM (Teensy 4.0)
   - Concludes hybrid WDF + RNN approach is optimal

2. **Chowdhury, J.** "chowdsp_wdf: An Advanced C++ Library for Wave Digital Circuit
   Modelling" (2022). arXiv:2210.12554.
   - Describes the recommended WDF library's API, design, and optimization strategies
   - Includes benchmarks comparing against RT-WDF and Faust implementations

3. **Yeh, D.T.** "Digital Implementation of Musical Distortion Circuits by Analysis and
   Simulation" (2009). Stanford University PhD Thesis (CCRMA).
   - The foundational academic work on digital modeling of DS-1, RAT, Tube Screamer,
     Fuzz Face, and Bassman amplifier
   - Develops the Nodal DK method

4. **Parker, J., Zavalishin, V., Le Bivic, E.** "Reducing the Aliasing of Nonlinear
   Waveshaping Using Continuous-Time Convolution" (2016). Proceedings of DAFx-16.
   - The original ADAA paper from Native Instruments
   - Introduces 1st and 2nd order antiderivative anti-aliasing

5. **Bernardini, A., Vergani, A.E., Sarti, A.** "Antiderivative Antialiasing in Nonlinear
   Wave Digital Filters" (2020). Proceedings of DAFx-20.
   - Shows how to integrate ADAA directly into WDF diode models
   - Eliminates need for oversampling in WDF circuits with diode nonlinearities

6. **Werner, K.J., Smith, J.O., Abel, J.S.** "Wave Digital Filter Adaptors for Arbitrary
   Topologies and Multiport Linear Elements" (2015). Proceedings of DAFx-15.
   - Formalizes R-type adaptors for WDF
   - Enables WDF modeling of circuits with arbitrary topology

7. **Werner, K.J., Nanez, J., Smith, J.O., Abel, J.S.** "Wave Digital Filter Modeling of
   Circuits with Operational Amplifiers" (2016). Proceedings of EUSIPCO.
   - How to model op-amp circuits in the WDF framework
   - Essential for pedals with op-amp gain stages

8. **Darabundit, C., Bischoff, P., Wedelich, R.** "Digital Grey Box Model of the Uni-Vibe
   Effects Pedal" (2019). Proceedings of DAFx-19.
   - State-of-the-art Univibe digital modeling
   - Includes measured LDR characteristics from a real unit

9. **Bernardini, A., Werner, K.J.** "Modeling Nonlinear Wave Digital Elements Using the
   Lambert Function" (2016). IEEE Transactions on Circuits and Systems I.
   - Closed-form diode models for WDF using Lambert W function
   - Foundation for chowdsp_wdf's diode implementation

10. **D'Angelo, S., Pakarinen, J.** "Fast Approximation of the Lambert W Function for
    Virtual Analog Modelling" (2019). Proceedings of DAFx-19.
    - Efficient Wright Omega function approximation for real-time diode models
    - Critical for making WDF diode models fast enough for mobile

11. **Holters, M., Zolzer, U.** "Physical Modelling of a Wah-wah Effect Pedal as a Case
    Study for Application of the Nodal DK Method to Circuits with Variable Parts" (2011).
    Proceedings of DAFx-11.
    - DK method applied to effects with potentiometers (variable components)
    - Addresses the challenge of real-time parameter changes

12. **Eichas, F., Zolzer, U.** "Physical Modeling of the MXR Phase 90 Guitar Effect Pedal"
    (2014). Proceedings of DAFx-14.
    - JFET phaser modeling using white-box approach
    - Relevant to phaser effects in our app

13. **Holters, M.** "Antiderivative Antialiasing for Stateful Systems" (2019).
    Proceedings of DAFx-19.
    - Extends ADAA to systems with internal state (feedback loops)
    - Important for soft clipping in feedback (Tube Screamer style)

14. **Yeh, D.T., Abel, J.S., Smith, J.O.** "Simplified, Physically-Informed Models of
    Distortion and Overdrive Guitar Effects Pedals" (2007). Proceedings of DAFx-07.
    - Earlier conference paper from Yeh's thesis work
    - Good introduction to the concepts before reading the full thesis

### 12.2 Supplementary References

- **Fettweis, A.** "Wave digital filters: Theory and practice" (1986). Proceedings of the
  IEEE. -- The original WDF paper from the inventor of the technique.
- **Smith, J.O.** "Physical Audio Signal Processing" (online book, CCRMA). -- Free textbook
  covering digital waveguides, WDF, and physical modeling for audio.
- **Vicanek, M.** "Note on Alias Suppression in Digital Distortion." -- Alternative ADAA
  analysis and derivations.
- **ElectroSmash** (https://www.electrosmash.com/) -- Detailed circuit analyses of dozens
  of guitar pedals with complete schematics and component values.
- **ampbooks.com** -- Tone stack calculator and DSP formulas for guitar amp modeling.

---

## 13. Source Links

### Libraries and Code

- [chowdsp_wdf (GitHub)](https://github.com/Chowdhury-DSP/chowdsp_wdf) -- Recommended WDF library
- [chowdsp_wdf Paper (arXiv)](https://arxiv.org/abs/2210.12554)
- [RT-WDF Library (GitHub)](https://github.com/RT-WDF/rt-wdf_lib)
- [ADAA Experiments (GitHub)](https://github.com/jatinchowdhury18/ADAA)
- [RTNeural (GitHub)](https://github.com/jatinchowdhury18/RTNeural)
- [KlonCentaur (GitHub)](https://github.com/jatinchowdhury18/KlonCentaur)
- [BYOD (GitHub)](https://github.com/Chowdhury-DSP/BYOD)
- [Faust wdmodels Library](https://faustlibraries.grame.fr/libs/wdmodels/)
- [Faust AANL Library](https://faustlibraries.grame.fr/libs/aanl/)
- [Faust WDF Library (GitHub)](https://github.com/droosenb/faust-wdf-library)
- [WDF Bakeoff Benchmarks (GitHub)](https://github.com/jatinchowdhury18/wdf-bakeoff)

### Open-Source Plugin Suites

- [Guitarix](https://guitarix.org/)
- [ChowDSP Products](https://chowdsp.com/products.html)
- [GuitarML](https://guitarml.com/)
- [GuitarML Proteus (GitHub)](https://github.com/GuitarML/Proteus)
- [GuitarML NeuralPi (GitHub)](https://github.com/GuitarML/NeuralPi)
- [GuitarML GuitarLSTM (GitHub)](https://github.com/GuitarML/GuitarLSTM)
- [GuitarML PedalNetRT (GitHub)](https://github.com/GuitarML/PedalNetRT)
- [CAPS Audio Plugin Suite](http://quitte.de/dsp/caps.html)
- [DISTRHO Ports](https://distrho.sourceforge.io/ports.php)
- [awesome-audio-dsp (GitHub)](https://github.com/BillyDM/awesome-audio-dsp)

### Circuit Analyses

- [Boss DS-1 Analysis (ElectroSmash)](https://www.electrosmash.com/boss-ds1-analysis)
- [ProCo RAT Analysis (ElectroSmash)](https://www.electrosmash.com/proco-rat)
- [Tube Screamer Analysis (ElectroSmash)](https://www.electrosmash.com/tube-screamer-analysis)
- [Big Muff Analysis (ElectroSmash)](https://www.electrosmash.com/big-muff-pi-analysis)
- [Fuzz Face Analysis (ElectroSmash)](https://www.electrosmash.com/fuzz-face)

### Academic Papers (Direct Links)

- [Klon Centaur Comparison Paper (arXiv)](https://arxiv.org/abs/2009.02833)
- [David Yeh PhD Thesis (Stanford CCRMA)](https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf)
- [David Yeh Publications Page](https://ccrma.stanford.edu/~dtyeh/papers/pubs.html)
- [ADAA Practical Considerations (Chowdhury)](https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html)
- [Lambert W Fast Approximation (DAFx 2019)](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_5.pdf)
- [ADAA in WDF (DAFx 2020)](https://dafx2020.mdw.ac.at/proceedings/papers/DAFx2020_paper_35.pdf)
- [DAFx Paper Archive (all years)](https://www.dafx.de/paper-archive/)
- [CCRMA WDF Fuzz Face Wiki](https://ccrma.stanford.edu/wiki/Wave_Digital_Filters_applied_to_the_Dunlop_%22Fuzz_Face%22_Distortion_Circuit)

### Other Resources

- [Tone Stack DSP Calculator (ampbooks)](https://www.ampbooks.com/mobile/dsp/tonestack/)
- [Bilinear Transform Tutorial (WolfSound)](https://thewolfsound.com/bilinear-transform/)
- [Eventide Univibe Blog](https://www.eventideaudio.com/blog/whats-so-special-about-the-uni-vibe/)
- [Geofex Univibe Technology Page](http://www.geofex.com/article_folders/univibe/univtech.htm)
- [Diode Clipping Guide (UV Effects)](https://uveffects.com/tonelab/clipping-diodes-guide/)
- [Note on Alias Suppression (Vicanek)](https://vicanek.de/articles/AADistortion.pdf)
