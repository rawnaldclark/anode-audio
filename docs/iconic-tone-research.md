# Iconic Guitar Tones Research -- Comprehensive Analysis

Exhaustive circuit-level research for building 1:1 digital replicas of six iconic guitar tones.
Covers exact gear, component values, signal chains, knob settings, and DSP implementation strategy.

Document produced for the Guitar Emulator App project (21 effects, C++/Oboe/Android).

---

# PART 1: ICONIC TONE PRESETS

---

## 1. Eric Clapton -- "Woman Tone" (Cream Era, 1966-68)

### Exact Gear

**Guitar:**
- 1964 Gibson SG Standard ("The Fool" -- hand-painted by Dutch artists Simon Posthuma and Marijke Koger)
- Prior to Cream (Bluesbreakers era): 1959/1960 Gibson Les Paul Standard ("Beano" tone)
- **Pickup selection: Neck humbucker (PAF-style)**
- **Tone knob: Rolled to 0 or near-0** -- this is the single most important element
- Volume: Full (10)

**The "Woman Tone" Explained at Circuit Level:**
- Gibson humbuckers of this era used a 0.022uF tone capacitor (sometimes 0.047uF)
- When the tone pot (500K audio taper) is rolled to 0, the capacitor is fully engaged
- This creates a first-order low-pass filter: fc = 1 / (2 * pi * Rpickup * C)
- With humbucker impedance ~8K and 0.022uF: fc approximately 900Hz
- With 0.047uF: fc approximately 430Hz
- The resonant peak of the pickup shifts from ~4kHz down to ~600-900Hz
- Result: All treble above ~1kHz is dramatically rolled off, creating a thick, vocal, "wailing" quality
- The neck pickup position already emphasizes fundamentals over harmonics; combined with rolled-off tone, the guitar output is almost purely midrange

**Amplifier:**
- Cream sessions: Marshall JTM45/100 head (100W, KT66 power tubes in early versions, EL34 in later)
- Some sessions: Marshall 1962 "Bluesbreaker" combo (2x12, 30W, 2xEL34 / 2x6L6 depending on era)
- **Marshall settings: EVERYTHING on 10** -- full volume, full treble, full bass, full presence
- Channel: Normal (NOT bright/high treble) -- further reduces high-frequency content
- The amp is pushed into heavy saturation at these settings, providing natural compression and sustain
- Clapton's own words (1968 Rolling Stone): "I set them full on everything, full treble, full bass and full presence"

**Cabinet:**
- Marshall 1960B (straight front) 4x12 with Celestion G12M-25 "Greenback" speakers (25W, ceramic magnet)
- Greenbacks have a natural treble rolloff above 4kHz and a presence peak around 2.5kHz

**Pedals:**
- **Dallas Rangemaster Treble Booster: DISPUTED** -- rumors persist but NO photographic evidence from Bluesbreakers sessions. More likely used by other players of the era (Tony Iommi, Brian May definitively used them). If used, it would seem contradictory with the woman tone (boosts treble), UNLESS it was used to hit the amp harder while the guitar tone was rolled off, effectively adding more preamp saturation to the already-dark signal.
- **No confirmed pedals for the Cream-era "woman tone"** -- this is entirely guitar-into-cranked-amp

**The "Secret" of the Tone:**

The woman tone is NOT about any magic gear. It is the interaction of three elements:

1. Neck humbucker (emphasizes fundamental, low harmonic content)
2. Tone knob at 0 (removes everything above ~900Hz from the guitar itself)
3. Cranked Marshall amp (saturates the remaining midrange signal heavily, adding sustain and compression)

The result is a thick, singing, vocal quality with almost no treble "fizz" -- the distortion is entirely in the midrange, which is why it sounds "warm" and "vocal" rather than "harsh" and "buzzy."

### Signal Chain
```
Guitar (neck HB, tone=0, vol=10) -> Marshall JTM45/100 (all on 10, normal channel) -> Marshall 1960B 4x12 (G12M Greenback)
```

### What Our App Can Already Replicate
- AmpModel (Crunch mode) -- Marshall-style breakup. However, our current Crunch model uses preGain=4.0 which is moderate; Clapton's cranked-to-10 Marshall would need MUCH higher gain.
- CabinetSim (4x12 closed) -- approximates 1960B, though our IR may not match Greenback frequency response precisely
- ParametricEQ can simulate the rolled-off tone control

### What New Components Are Needed
- **Guitar Tone Rolloff Simulation**: A first-order low-pass filter at ~900Hz with a resonant peak, placed BEFORE the amp model. This simulates rolling the guitar's tone knob to 0. Our current ParametricEQ could approximate this, but a dedicated "guitar tone knob" filter would be more accurate. Implementation: simple one-pole LPF or second-order LPF with mild resonance at the cutoff point.
- **Marshall JTM45/100 Amp Model**: Our Crunch mode is a generic Marshall approximation. A proper JTM45 model would need:
  - Higher gain (all knobs on 10)
  - Normal channel voicing (less treble than bright channel)
  - Correct tone stack (Marshall TMB type with specific component values)
  - More even-harmonic content from KT66/EL34 power amp saturation
- **Greenback Cabinet IR**: A specific impulse response captured from G12M-25 Greenback speakers in a 1960B cab

### Preset Settings (approximation with current effects)
```
Effect Chain Order: [7, 5, 6] (ParametricEQ -> AmpModel -> CabinetSim)
ParametricEQ: Band 1: 900Hz, Q=1.5, Gain=-15dB (simulate rolled-off tone)
              Band 2: 3500Hz, Q=0.7, Gain=-20dB (kill treble)
AmpModel: inputGain=2.0 (maximum), outputLevel=0.7, modelType=1 (Crunch)
CabinetSim: type=1 (4x12 closed-back)
```

---

## 2. Jimi Hendrix -- "Purple Haze" / "Voodoo Child" (1967-70)

### Exact Gear

**Guitar:**
- Fender Stratocaster (various years, typically late 1960s)
- RIGHT-HANDED guitar played LEFT-HANDED (strung upside down)
- **Sonic significance of reverse stringing:** The bridge saddle angles are reversed relative to string gauge. On a normal right-hand Strat, the low E string has the longest scale length. When flipped, the high E string gets the longest scale. This subtly alters intonation and creates slightly different string tension distribution. More importantly, the pickup pole pieces are now offset from the strings differently, changing the harmonic content picked up. The slant of the bridge pickup reverses relative to the string vibration nodes.
- Pickups: Standard Fender single-coils of the era
- Hendrix typically used the neck or middle position

**Pedals:**

**A. Dallas Arbiter Fuzz Face (PNP Germanium)**
- Transistors: AC128 (PNP germanium, Mullard)
  - Q1: low gain, beta=70-80
  - Q2: high gain, beta=110-130
- Component values:
  - C1: 2.2uF (input coupling)
  - C2: 20uF (emitter bypass)
  - C3: 0.01uF (10nF) (output coupling)
  - R1: 33K (input bias)
  - R2: 470 ohm (Q2 collector)
  - R3: 8.2K (Q1 collector)
  - R4: 100K (feedback)
  - Rfuzz: 1K linear pot
  - Rvol: 500K audio pot
- Bias points: Q1 collector = -0.6V, Q2 collector = -4.5V
- **Temperature sensitivity**: Germanium transistors' gain (beta) and leakage current (Icbo) change significantly with temperature. A circuit biased correctly at room temperature will bias differently in a hot stage environment. This is why many Fuzz Face users report the pedal sounding "different every night."
- Circuit topology: 2-stage common-emitter with shunt-series feedback through R4 (100K)
- Frequency response: Input HPF at 14Hz (C1), output HPF at 31Hz (C3)

**B. Vox Clyde McCoy Wah**
- Inductor: Halo type, 500mH
- Transistors: BC109 (lower gain than modern Cry Baby's MPSA18)
- Key components: C1=0.01uF, C2=0.01uF, C3=4.7uF, C4=0.22uF, C5=0.22uF, R1=68K, R2=1.5K, R3=22K
- Pot: ICAR taper (specialized sweep curve)
- Frequency sweep: 450Hz to 1.6kHz (centered at 750Hz in mid position)
- The inductor-based design creates a resonant bandpass peak that moves with the pedal position
- Lower gain BC109 transistors give a rounder, more musical bass response than modern Cry Babies

**C. Roger Mayer Octavia**
- Circuit: Full-wave rectification using a transformer (original) or transistor pair (later version)
- The octave effect works by splitting the signal into two paths (in-phase and out-of-phase), then rectifying each to cancel alternating half-cycles, then combining -- the result emphasizes the second harmonic (octave up)
- Uses germanium diodes (BAT46 modern replacement) for clipping
- 3 transistors in the all-silicon version
- The Octavia responds to playing dynamics: produces clearest octave effect on the upper strings (above 12th fret) and becomes more of a "ring modulator" effect on lower notes

**Amplifier:**
- Marshall Super Lead 1959 100W "Plexi"
- **Cranked to maximum volume** -- all controls on 10
- 3x ECC83 (12AX7) preamp tubes, 4x EL34 power tubes
- Preamp circuit details (from robrobinette analysis):
  - V1A (Normal channel): Cathode R=820 ohm, bypass C=330uF, coupling cap=0.022uF
  - V1B (Bright/Lead channel): Cathode R=2.7K, bypass C=0.68uF, coupling cap=0.0022uF
  - V2A (Second gain stage): mixer resistors 470K, bright cap 470pF
  - V2B: Cathode follower (first stage to distort)
  - Tone stack: Marshall TMB type (passive, can only cut frequencies)
  - Phase inverter: Long-tail pair
  - NFB: 56K resistor from 2-ohm speaker tap
  - Bright cap: 5000pF (500pF on normal channel pot)
- Channels often "jumped" (linked with a patch cable) to drive both channels simultaneously
- The EL34 power tubes, when pushed hard, produce a pronounced midrange "bark" and aggressive clipping character

**Cabinet:**
- Marshall 1960A (angled front) with Celestion G12H-30 (30W, ceramic magnet, heavier cone than Greenback)
- The G12H-30 has a tighter, more focused low-end and a slightly more prominent upper midrange than the G12M Greenback

### Signal Chain for Key Songs

**"Purple Haze" (1967):**
```
Guitar (Strat, neck/middle) -> Fuzz Face (fuzz ~70%, vol ~80%) -> Roger Mayer Octavia (for solo section only) -> Marshall 1959 (cranked) -> Marshall 1960A
```
- The main riff uses the Fuzz Face for the thick, sustaining distortion
- The Octavia was used for the solo to add the octave-up "whistle" quality
- Amp: full volume, both channels possibly jumped

**"Voodoo Child (Slight Return)" (1968):**
```
Guitar (Strat, neck) -> Vox Clyde McCoy Wah -> Fuzz Face -> Marshall 1959 (cranked) -> Marshall 1960A
```
- The opening is the wah pedal being slowly rocked through its range
- The Fuzz Face provides the thick distortion foundation
- Wah BEFORE fuzz is the key to this sound -- the wah sweeps a resonant peak through the fuzz's frequency spectrum, creating the "talking" quality
- NOTE: wah before fuzz vs fuzz before wah creates very different sounds. Wah-first gives a more vocal, expressive quality; fuzz-first gives a more "synthy" quality.

### What Our App Can Already Replicate
- Wah (index 2) -- our auto-wah with TPT bandpass can approximate the Vox Clyde McCoy, though it is an envelope follower rather than a pedal-position controller. The fixed mode could work if we add a manual sweep control.
- Overdrive (index 4) -- can approximate light fuzz with high drive settings
- AmpModel (Crunch, index 5) -- generic Marshall crunch
- CabinetSim (4x12, index 6)

### What New Components Are Needed
- **Dallas Arbiter Fuzz Face**: A dedicated 2-transistor germanium fuzz circuit emulation. This is fundamentally different from our Overdrive (which uses polynomial waveshaping). The Fuzz Face's character comes from:
  - The interaction between the two transistors' bias points
  - The feedback path through R4 (100K) which creates the "sputtery" gated quality at low fuzz settings
  - The input impedance interaction with the guitar's volume control (Fuzz Face is famously sensitive to what is plugged into it)
  - Temperature-dependent germanium behavior
  - DSP approach: WDF (wave digital filter) modeling of the two-transistor circuit, or direct nodal analysis. The simple 2-transistor circuit is actually tricky to model because of the nonlinear feedback.
  - Reference: Yeh & Smith, "Simulating Guitar Distortion Circuits Using Wave Digital and Nonlinear State-Space Formulations" (DAFx 2008)

- **Roger Mayer Octavia**: Octave-up fuzz using full-wave rectification. DSP approach: split signal into in-phase and inverted copies, half-wave rectify each, sum them -- this doubles the fundamental frequency. Add fuzz/clipping stage. The octave effect is frequency-dependent (works best above ~500Hz).

- **Marshall Plexi 1959 Amp Model**: Our current Crunch mode is too mild. Need a proper Plexi model with:
  - 4 cascaded gain stages (V1A/B -> V2A -> V2B cathode follower -> phase inverter)
  - Marshall TMB tone stack (different topology from Fender/Hiwatt)
  - Bright cap behavior (500pF / 5000pF)
  - Channel jumping behavior
  - EL34 power amp saturation character
  - Lower negative feedback than Hiwatt (more distortion at same volume)

- **Vox Clyde McCoy Wah**: Our current wah is an auto-wah with envelope following. A proper Vox wah would need:
  - Manual position control (0-1 sweep parameter)
  - Inductor-based bandpass (500mH inductor simulation)
  - Specific frequency sweep range (450Hz-1.6kHz)
  - ICAR pot taper curve
  - Could adapt our existing Wah to add a "manual sweep" mode

---

## 3. Jack White -- "Seven Nation Army" / White Stripes Era (2001-2007)

### Exact Gear

**Guitar:**
- 1964 JB Hutto Montgomery Ward Airline: Res-O-Glas (fiberglass) body, single-coil pickup. This guitar has a distinctive boxy, midrange-heavy tone due to the fiberglass body and low-output single-coil. It emphasizes mids in a way that a typical Strat or Les Paul does not.
- Also used: Kay hollowbody (for slide work)

**Pedals:**

**A. DigiTech Whammy (WH-4, 4th generation)**
- Set to "Whammy 1 octave down" for the famous riff
- The riff sounds like a bass guitar but is actually guitar pitched down one octave
- Algorithm: Time-domain pitch shifting using SOLA (Synchronous Overlap-Add)
- The WH-4 has a characteristic "warble" and slight latency in its pitch tracking that is part of the sound
- Settings: Whammy mode, octave down, expression pedal in heel-down position (full shift)

**B. Electro-Harmonix Big Muff Pi**
- Version: Most likely a late 1990s/early 2000s NYC reissue
- Settings for Seven Nation Army: Volume=6, Tone=7, Sustain=9 (high sustain, bright tone)
- Used primarily for the slide solo, NOT the main riff
- Circuit: 4 cascaded common-emitter amplifier stages:
  - Stage 1 (Input booster): ~19.6dB gain, Q1 (BC239/2N5088)
  - Stage 2 (Clipping Q2): ~23dB gain, D1-D2 (1N914) back-to-back clipping to ~+/-0.6V
  - Stage 3 (Clipping Q3): ~25dB gain, D3-D4 (1N914) back-to-back clipping
  - Stage 4 (Output Q4): ~13dB recovery gain
  - Tone stack: Passive mid-scoop filter creating a notch at ~1kHz
  - Key component values (NYC version):
    - Clipping stage resistors: 10K collector, 100K feedback
    - Clipping caps: 100nF coupling
    - Tone stack: 39K + 22K resistors, 10nF + 3.9nF caps, 100K tone pot
    - Emitter degeneration: 150 ohm

**C. MXR Micro Amp (clean boost)**
- Used as an always-on boost to hit the amp harder
- Circuit: Single TL061 op-amp, non-inverting configuration
- Components: R1=22M, R2=10M, R3=1K, R4=56K, R5=500K pot (reverse log), R6=2.7K, R7/R8=100K, R9=470, R10=10K, C1=0.1uF, C2=47pF, C3=4.7uF, C4=1uF, C5=15uF
- Gain range: 0dB to 26.2dB
- Gain formula: Gv = (1 + R4/(R5+R6)) * (R10/(R9+R10))
- Essentially flat frequency response (transparent boost)

**Amplifier:**
- Sears Silvertone 1485
- 4x 6L6GC power tubes, 3x 12AX7 preamp tubes, 1x 12AX7 for tremolo, 2x 6CQ7 (phase inverter + reverb driver)
- 6x 10" Jensen C10Q ceramic speakers
- Settings for Seven Nation Army: Channel 2, Volume=6, Bass=5, Treble=7, Reverb OFF, Tremolo OFF
- The Silvertone 1485 has a distinct "trashy," compressed quality at moderate volumes -- it is not a high-headroom amp
- Secondary amp: Fender Twin Reverb (used for cleaner tones)

**Cabinet:**
- Built-in to the Silvertone 1485: 6x10" Jensen C10Q speakers
- 10" speakers have less bass extension than 12" but more focused midrange

### Signal Chain for "Seven Nation Army"

```
Guitar (Airline, neck pickup) -> DigiTech Whammy (octave down, heel position) -> [MXR Micro Amp always on] -> Silvertone 1485 (Ch2, Vol=6, Bass=5, Treb=7)
```
- The Big Muff is OFF for the main riff -- it is only kicked on for the slide solo
- The famous "bass" riff is the guitar through the Whammy set to one octave down
- The Micro Amp provides extra drive into the Silvertone

### What Our App Can Already Replicate
- Overdrive (index 4) -- can approximate the Big Muff's distortion character in general terms
- AmpModel (index 5) -- Clean mode could approximate the Silvertone with EQ adjustments
- Boost (index 3) -- can approximate the MXR Micro Amp
- CabinetSim (index 6) -- none of our current cabs match a 6x10

### What New Components Are Needed
- **Pitch Shifter / Whammy Effect**: This is the BIGGEST gap. We have no pitch shifting capability. Implementing a real-time pitch shifter would require:
  - SOLA/PSOLA algorithm for time-domain pitch shifting
  - Pitch detection (autocorrelation or YIN algorithm)
  - Expression pedal input (or fixed intervals: octave down, octave up, 5th, etc.)
  - This is a significant development effort -- probably a sprint in itself

- **Big Muff Pi**: We do not have this pedal. Would need:
  - 4-stage cascaded clipping (our HM-2 already does 3-stage)
  - 1N914 diode pairs per stage (symmetric hard clipping, +/-0.6V)
  - Passive mid-scoop tone stack (similar to DS-1's Big Muff Pi style tone)
  - WDF or direct DSP implementation
  - This is a strong candidate for our next WDF pedal emulation

- **Silvertone 1485 Amp Model**: Very different from Marshall or Fender. 6L6GC power tubes give it a different character from EL34 Marshalls. Would need specific preamp modeling.

- **6x10 Cabinet IR**: No standard IR exists for this unusual configuration

---

## 4. John Frusciante -- RHCP Era (1991-2006)

### Exact Gear

**Guitar:**
- Fender Stratocaster (1962 vintage, with standard single-coil pickups)
- Typically uses neck or middle+neck position for clean tones, bridge for aggression

**Dual Amp Setup:**
- **Marshall Major 200W** (clean channel): Extremely high headroom, stays clean at high volumes. KT88 power tubes. Used for clean and lightly driven tones.
- **Marshall Silver Jubilee 2555** (dirty channel): Based on JCM800 with enhanced gain staging. Three gain modes (Clean, Rhythm Clip, Lead). EL34 power tubes.
  - Settings: Presence=4-5, Bass=3, Middle=3-6, Treble=7-8, Output Master=7-8, Lead Master=7-8, Input Gain=5
  - The Silver Jubilee has a smoother, darker distortion than the JCM800 due to diode clipping in the preamp

**Signal Splitting:**
- Signal from pedalboard splits to BOTH amps via the Boss CE-1 (which has a stereo output)
- The blend between the two amps' signals in the room creates the full Frusciante sound
- For our app: we would need to model this as a parallel signal path or just pick one amp per preset

**Pedals:**

**A. Boss DS-2 Turbo Distortion (WE HAVE THIS -- index 18)**
- Frusciante's settings: Mode II (Turbo), Dist=60%, Tone=70%, Level=70%
- Mode II is ESSENTIAL -- the 900Hz mid-boost is what gives his distorted lead tone its "honky," cutting quality
- Used on: "Dani California," "Suck My Kiss," heavy sections

**B. Ibanez WH-10 Wah**
- Different from Vox wah -- uses a multiple feedback op-amp bandpass filter (no inductor)
- 50K linear pot for frequency sweep, 500K log pot for gain correction
- Frequency range: wider than Vox, extends lower and higher
- Used extensively on: "Under the Bridge" (intro solo melody), "Don't Forget Me"
- The WH-10 is known for a more "vocal" and "quacky" sound than the Vox, with a more pronounced bass response

**C. Boss CE-1 Chorus Ensemble**
- THE original chorus pedal (1976)
- BBD-based: Matsushita MN3002 (512-stage BBD, 0.32-25.6ms delay)
- **Preamp section**: Op-amp preamp that "sweetens" the signal + optional transistor gain stage (high mode)
- Two modes:
  - Chorus: Dry + wet mixed, subtle thickening, ~7ms center delay
  - Vibrato: Wet only, more extreme pitch modulation
- The CE-1's preamp section is a HUGE part of its appeal -- it adds warmth and slight compression to the signal even when the chorus effect is minimal
- Used on: "Under the Bridge" (clean chorus), "Scar Tissue," "Snow"
- Our current Chorus (index 8) approximates this but lacks the BBD character and the preamp warmth

**D. Electro-Harmonix Big Muff (Russian Green version)**
- Key difference from NYC version: uses 470pF or 500pF caps (vs 470pF for NYC, but different transistors)
- The Green Russian is smoother, with slightly less sustain and a more "woolly" character
- Less mid-scoop than NYC -- retains more low-mids, giving it a fatter sound
- Used on: heavier tones, stacked with other effects

**E. MXR Micro Amp**
- Always-on clean boost (same circuit as described in Jack White section)
- Used to hit the front of the amps harder

### Signal Chain for Key Songs

**"Under the Bridge" (clean + chorus):**
```
Guitar (Strat, neck) -> [CE-1 Chorus mode] -> Marshall Major (clean, high headroom)
```
- The clean tone with chorus IS the defining sound
- Very little gain -- the Major stays clean
- The CE-1's preamp adds subtle warmth

**"Scar Tissue" (clean crunch):**
```
Guitar (Strat, middle position) -> [MXR Micro Amp] -> Marshall Silver Jubilee (low gain setting)
```
- Light overdrive from the Jubilee
- The slight crunch on picking attack, clean on soft playing
- Very dynamic and responsive

**"Snow (Hey Oh)" (clean + compression):**
```
Guitar (Strat, neck+middle) -> [Compressor, light] -> [CE-1 light chorus] -> Marshall Major (clean)
```
- The fast arpeggio pattern requires compression to keep all notes even
- Light chorus adds shimmer

**"Dani California" (heavy distortion):**
```
Guitar (Strat, bridge) -> Boss DS-2 (Mode II, Dist=60%, Tone=70%) -> Marshall Silver Jubilee (Lead mode, Gain=5)
```
- DS-2 Mode II provides the honky midrange
- Silver Jubilee adds power amp saturation
- The combination of DS-2 into a driven amp is the signature heavy Frusciante tone

### What Our App Can Already Replicate
- **Boss DS-2 (index 18)** -- We have this! Mode II with proper settings is available
- Compressor (index 1) -- can provide the light compression for "Snow"
- Chorus (index 8) -- approximates CE-1 chorus mode
- Wah (index 2) -- our auto-wah can approximate, but WH-10 has different character
- AmpModel (index 5) -- Crunch mode for Silver Jubilee approximation; Clean mode for Major
- CabinetSim (index 6)

### What New Components Are Needed
- **Boss CE-1 Chorus Ensemble**: Our chorus lacks the BBD character and the preamp section. A proper CE-1 model would need:
  - BBD delay line emulation (sample-and-hold with anti-aliasing and reconstruction filters)
  - The preamp section (op-amp + optional transistor gain stage)
  - Vibrato mode (100% wet)
  - The specific LFO shape and rate range

- **Ibanez WH-10 Wah**: Different from Vox -- op-amp bandpass rather than inductor-based. Our current Wah could be adapted by changing the filter topology and frequency range.

- **Marshall Silver Jubilee Amp Model**: Different from the Plexi or JCM800 -- has additional gain stages and diode clipping in the preamp. Would need its own AmpModel preset or a new amp model entirely.

- **Big Muff Pi (Russian Green)**: Same architecture as above but with different component values for the smoother character.

---

## 5. David Gilmour -- "Comfortably Numb" Solo (The Wall, 1979)

### Exact Gear

**Guitar:**
- Fender Stratocaster ("Black Strat")
- **Bridge pickup: DiMarzio FS-1** (hotter output than stock Fender pickups)
- For the first solo: bridge + neck pickups together (via custom toggle switch)
- For the second solo: bridge pickup alone (FS-1)
- The FS-1 has approximately 12K DC resistance vs ~6K for stock Fender pickups -- this drives the front-end effects harder

**Pedals:**

**A. Electro-Harmonix Big Muff Pi (Ram's Head version, c.1974)**
- Settings: Volume=4, Tone=6, Sustain=6
- The Ram's Head (1973-77) is considered the "best" version for Gilmour's tone
- Differences from other versions:
  - More consistent than Triangle (less unit-to-unit variation)
  - Slightly darker/smoother than Triangle
  - More sustain and compression than NYC
  - More aggressive than Green Russian
- Circuit: Same 4-stage topology as described above
- Key Ram's Head component differences from other versions:
  - Uses 2N5133 or 2N5088 transistors (higher gain than BC239)
  - Slightly different feedback resistor values in clipping stages
  - 150 ohm emitter degeneration resistors (same as all versions)
  - The "Violet" Ram's Head sub-variant is the most prized

**B. MXR Dyna Comp**
- Settings: Output=7, Sensitivity=4
- Circuit: OTA-based (CA3080) compression
- Components: Q1-Q5 = 2N3904, U1 = CA3080, D1-D2 = 1N914
- The Dyna Comp adds sustain and evens out dynamics before the Big Muff
- At Output=7, Sensitivity=4: moderate compression with ~10dB gain reduction on peaks
- The CA3080 OTA has a distinctive "squash" character -- more aggressive and less transparent than modern VCA compressors

**C. Electro-Harmonix Electric Mistress Flanger (not used on Comfortably Numb studio version, but used live)**
- SAD1024 BBD chip (dual 512-stage)
- Two BBDs run in parallel multiplex for lower noise
- Through-zero flanging capability
- Controls: Rate, Range, Color (feedback)

**Rotating Speaker:**
- Yamaha RA-200 rotating speaker cabinet
- Provides subtle Leslie-like modulation
- Adds "body to the throaty midrange tones" from the Big Muff
- This is a key element often overlooked -- the RA-200 adds width and movement

**Amplifier:**
- **Hiwatt DR103 Custom 100**
- Settings: Linked input (Normal + Brilliant channels), Normal Volume=7, Brilliant Volume=4.5, Bass=6, Treble=5, Middle=4, Presence=6, Master Volume=5
- The Hiwatt is VERY different from Marshall:
  - 4x 12AX7 preamp tubes (vs 3 in Marshall Plexi)
  - 4x EL34 power tubes
  - **Partridge transformers**: extremely rigid, tight power supply with minimal sag
  - **Higher negative feedback** than Marshall: This means the amp stays cleaner longer, has more headroom, and when it does distort, the distortion is tighter and more defined (less "woolly")
  - **Different tone stack**: Hiwatt TMB has a 100K mid pot (vs Marshall's 25K, Fender's 10K) -- gives much wider range of mid control
  - The Hiwatt has 2 gain stages before the tone stack, 1 after
  - **Fixed-biased phase inverter** (Marshall uses self-biased)
  - GZ34 rectifier tube (earlier versions) provides slight "sag" on transients
- The Hiwatt's character: extremely loud, very clean until pushed hard, then tight/focused distortion with excellent note definition even at high gain
- At Gilmour's settings (Master=5): the amp is moderately driven, relying on the Big Muff for most of the distortion

**Preamp:**
- Alembic F-2B tube preamp: Input 1, Bright=On, Volume=3, Bass=4, Middle=4, Treble=5
- This adds an extra gain stage and tonal shaping between the pedals and the amp

**Cabinet:**
- **WEM Starfinder cabinets with Fane Crescendo speakers** (NOT Celestion)
- The Fane Crescendo 12" speakers have a different frequency response from Celestion -- flatter, more hi-fi, with extended high-frequency response and tighter bass
- This contributes to the "defined" quality of Gilmour's tone -- the speaker does not add its own coloration the way a Greenback or Alnico Blue does

### Signal Chain for "Comfortably Numb" Second Solo

```
Guitar (Black Strat, bridge FS-1) -> MXR Dyna Comp (Out=7, Sens=4) -> EHX Big Muff (Vol=4, Tone=6, Sust=6) -> Alembic F-2B preamp -> [signal split] -> Hiwatt DR103 (linked, settings above) + Yamaha RA-200 rotating speaker
```

### The "Secret" of the Tone

The magic of the Comfortably Numb solo tone is the **stacking of moderate gain stages**:

1. The Dyna Comp provides ~10dB of compression and sustain
2. The Big Muff provides heavy fuzz (but at Sustain=6, not maxed)
3. The F-2B tube preamp adds another layer of warmth
4. The Hiwatt at Master=5 adds power amp warmth
5. The Yamaha RA-200 rotating speaker adds spatial movement and smoothing

No single element is pushed to its extreme. Each adds a moderate amount of saturation and compression. The result is a singing, sustained tone with extraordinary note definition -- you can hear every note clearly even through all the distortion, because the Hiwatt and Fane speakers keep everything tight and defined.

### What Our App Can Already Replicate
- Compressor (index 1) -- can approximate Dyna Comp
- AmpModel (index 5) -- Clean mode with high gain could approximate Hiwatt character
- CabinetSim (index 6) -- no WEM/Fane IR available
- Chorus (index 8) -- could approximate the RA-200 rotary effect in a crude way
- Overdrive (index 4) -- could approximate Big Muff in a limited way

### What New Components Are Needed
- **Big Muff Pi (Ram's Head)**: Same as above -- we need this pedal
- **Hiwatt DR103 Amp Model**: Fundamentally different from Marshall. Higher headroom, tighter distortion, different tone stack. Would need:
  - 4-stage preamp (2 before tone stack, 1 after, plus cathode follower)
  - Hiwatt TMB tone stack (100K mid pot, additional capacitors)
  - Higher negative feedback (tighter response)
  - Fixed-biased phase inverter
  - Partridge transformer characteristics (tighter, less sag)
- **Yamaha RA-200 Rotary Speaker**: Leslie-type effect with speed control. Could be implemented as a combination of amplitude modulation, frequency modulation, and spatial movement.
- **WEM Starfinder / Fane Crescendo Cabinet IR**: Specific impulse response needed
- **Tube Preamp Stage**: The Alembic F-2B could be approximated by an additional gain stage in the signal chain

---

## 6. Eddie Van Halen -- "Eruption" / "Brown Sound" (1978-84)

### Exact Gear

**Guitar:**
- "Frankenstrat": Fender Stratocaster body (from a Charvel-built partscaster) + Gibson PAF humbucker in bridge position
- The PAF was taken from a 1960s ES-335 and rewound to EVH's specs
- Potted in surf wax to eliminate microphonic feedback
- Single pickup, single volume control, no tone control
- The combination of a bright Fender body/neck with a warm Gibson humbucker creates a unique tonal balance

**The Variac Technique (KEY to the "Brown Sound"):**
- A Variac is a variable autotransformer that allows you to adjust the AC mains voltage feeding the amplifier
- EVH used it to REDUCE the voltage from 110VAC down to 60-100VAC (depending on venue size)
- **What reduced voltage does to a tube amp:**
  - Lower plate voltage on the power tubes -> tubes saturate earlier at lower volume
  - Lower heater voltage -> tubes run cooler, slightly different emission characteristics
  - The power supply sags MORE under load (less headroom)
  - The amp clips more symmetrically at reduced voltage (less harsh)
  - The overall effect: the amp sounds "cranked to 10" at a LOWER volume level
  - EVH's sweet spot for recording: 89 volts (vs normal 110V)
  - For small clubs: as low as 60 volts
- **The name "Brown Sound"**: comes from the visual analogy of a light bulb dimming to a brown/amber color when voltage is reduced -- the amp similarly "dims" and compresses

**Amplifier:**
- Marshall 1959 Super Lead 100W (1967 or 1968 vintage)
- **All controls on 10** (Presence, Bass, Middle, Treble, both volume controls)
- Fed through the Variac at reduced voltage
- Serial number #12301 (whether it was stock or modified is debated)
- The Jose Arredondo modifications: reportedly included cascading gain stages and removing some negative feedback, though EVH himself was not always forthcoming about modifications
- When running at 89V with everything on 10, the amp produces a thick, compressed, harmonically rich distortion with extreme sustain but without harsh "fizz"

**Pedals:**

**A. MXR Phase 90 (Script Logo version)**
- EVH's most consistent effect
- Settings: Speed at approximately 25% (slow, subtle)
- Circuit: 4-stage allpass phaser
  - Op-amps: Originally 741s (TL061 in later versions)
  - JFETs: 2N5952 (4 used as voltage-controlled resistors)
  - Allpass capacitors: C2-C5 = 47nF each (MATCHED -- unlike Uni-Vibe)
  - LFO: Schmitt Trigger-Integrator, generating triangular wave
  - Speed pot: 250K (controls LFO charge/discharge rate)
  - Other key components: C1/C6/C9=47nF, C5/C7=0.01uF, C8/C10=15uF, R1/R3/R5/R10-R13/R15/R17-R18=10K, R2/R7/R8/R16/R19/R24=150K, R4=56K, R6/R23/R25/R26=24K, R14/R21/R36=470K, R20=1M, R22=3.9M, Rtrimmer=250K
  - Frequency sweep: creates 2 notches (at 58.5Hz and 340.8Hz baseline) that sweep through the spectrum
  - Script logo version is LESS aggressive than later block logo (the block logo added a feedback resistor that makes the effect more pronounced)
- The Phase 90 at slow speed adds a subtle, swirling quality that thickens the sound without being obviously a "phaser effect"

**B. Maestro Echoplex EP-3**
- Used primarily for delay, BUT the preamp section is a huge part of the tone
- The EP-3 preamp uses JFET transistors (original: TIS58, modern equivalent: 2N5457)
- Preamp component values:
  - Input cap: 22nF
  - Source bypass cap: 22nF (boosts high-frequency gain)
  - Output cap: 100nF
  - Operating voltage: ~22VDC (from internal power supply)
  - JFET bias: IDSS ~3mA
  - Provides up to 11dB of gain
- The preamp adds a slight presence boost and harmonic richness that many players describe as making the guitar "come alive"
- EVH used this as an always-on effect -- the delay was secondary

**C. MXR Flanger (occasionally)**
- Used for specific effects (the intro of "Unchained" uses the flanger)

**D. MXR EQ (used as boost)**
- Used to boost mids and as a level booster depending on which guitar

**Cabinet:**
- Marshall 1960A 4x12 with **Celestion G12-65** speakers (NOT Greenbacks)
- The G12-65 (65W) is a higher-power speaker with a more "modern" sound than the Greenback
- Flatter frequency response, less midrange coloration, handles the high output levels better

### Signal Chain for Key Songs

**"Eruption" (1978):**
```
Guitar (Frankenstrat, PAF humbucker) -> MXR Phase 90 (speed ~25%) -> Echoplex EP-3 (preamp always on, delay off or short) -> Marshall 1959 via Variac at ~89V (all controls on 10) -> Marshall 1960A (G12-65)
```
- The Phase 90 adds subtle movement and thickness
- The EP-3 preamp adds presence and gain
- The Variac-reduced Marshall provides the compressed, singing sustain
- The tapping section relies on the extreme sustain of this signal chain

**"Ain't Talkin' 'Bout Love" (1978):**
```
Guitar (Frankenstrat) -> MXR Phase 90 (speed ~30%) -> Echoplex EP-3 -> Marshall 1959 via Variac -> Marshall 1960A
```
- Same basic chain
- The Phase 90 is slightly more noticeable on this track
- The arpeggiated chords benefit from the phase-shifted quality

**"Panama" (1984):**
```
Guitar -> [MXR Phase 90 OFF] -> [MXR Flanger OFF] -> Echoplex EP-3 -> Marshall 1959 -> Marshall 1960A
```
- By 1984, EVH was often running the amp at closer to normal voltage
- The 5150 amp was being developed during this period
- Less Phase 90 usage on the later albums

### The "Secret" of the Brown Sound

The Brown Sound is the combination of:

1. **High-output humbucker** (PAF rewound, potted) into...
2. **Echoplex EP-3 preamp** (adds ~11dB of presence-boosted gain) into...
3. **Marshall Plexi with everything on 10** (maximum saturation) BUT...
4. **Fed reduced voltage via Variac** (89V instead of 110V), which makes the amp distort more evenly, with more compression and less harsh high-frequency content, AND...
5. **MXR Phase 90 at slow speed** (adds subtle harmonic movement and thickness)

Remove any one element and you lose the magic. The Variac is the most commonly overlooked element -- it transforms a "too loud and harsh" cranked Plexi into a "singing, compressed, brown" tone.

### What Our App Can Already Replicate
- Phaser (index 10) -- our 4-stage allpass phaser can approximate the Phase 90
- Delay (index 12) -- can approximate Echoplex delay (but not the preamp)
- AmpModel (index 5) -- Crunch mode for Marshall approximation
- CabinetSim (index 6) -- 4x12 closed
- Boost (index 3) -- can approximate EP-3 preamp gain

### What New Components Are Needed
- **MXR Phase 90 (Script Logo)**: Our Phaser is already a 4-stage allpass, but it uses matched frequency sweep. The Phase 90 has specific characteristics:
  - 2N5952 JFETs as variable resistors (not linear sweep)
  - 47nF matched capacitors in all stages
  - Triangular LFO (not sine)
  - No feedback path in Script version (the Block logo added this)
  - Our Phaser could be modified by changing the LFO to triangle and adjusting the frequency sweep range

- **Echoplex EP-3 Preamp**: Simple JFET boost with tone-shaping character. Could be implemented as a variant of our Boost effect (index 3) with:
  - JFET gain stage (~11dB)
  - Input cap 22nF (high-pass at ~72Hz)
  - High-frequency boost from source bypass cap
  - This is a relatively simple addition

- **Variac / Voltage Starve Simulation**: This is the hardest part. Reduced voltage affects:
  - Power tube bias and operating point
  - Power supply sag behavior
  - Clipping symmetry
  - For our DSP: model as reduced headroom in the amp's clipping stage, more power supply "sag" (dynamic compression), and softer clipping characteristics. Could be a parameter on the AmpModel (e.g., "sag" or "voltage" knob).

- **Marshall Plexi 1959 (with Variac)**: Same as Hendrix -- we need this amp model. The Variac effect could be a parameter that modifies the amp's bias point and headroom.

- **G12-65 Cabinet IR**: Different from both Greenback and G12H-30

---

# PART 2: COMPONENT REFERENCE

---

## AMPLIFIERS

### Marshall 1959 Super Lead 100W "Plexi"

**Used by:** Jimi Hendrix, Eddie Van Halen

**Circuit Topology:**
- **Preamp:** 3x ECC83 (12AX7), 4 gain stages total
  - V1A (Normal channel): 820 ohm cathode R, 330uF bypass, 0.022uF coupling
  - V1B (Bright channel): 2.7K cathode R, 0.68uF bypass, 0.0022uF coupling
  - V2A (Second gain): 470K mixer resistors, 470pF bright cap
  - V2B: Cathode follower (drives tone stack, first to distort)
- **Tone Stack:** Marshall TMB (passive, Treble-Mid-Bass)
  - Based on Fender TMB but with different component values
  - Cannot boost, only cut -- but the "flat" position already has significant mid-scoop
- **Phase Inverter:** Long-tail pair (V3)
- **Power Amp:** 4x EL34, push-pull Class AB
- **Negative Feedback:** 56K resistor from 2-ohm speaker tap (~9.5V AC RMS feedback)
  - Moderate NFB -- less than Hiwatt, more than Vox AC30
- **Rectifier:** Solid-state diode bridge (no tube rectifier sag)
- **Bright Cap:** 5000pF (bright channel), 500pF (normal channel volume pot)

**What Makes It Unique:**
- The cathode follower (V2B) is the first stage to distort as the amp is turned up -- this gives Plexi overdrive its characteristic "spongy" quality
- When channels are "jumped" (linked), both channels' gain stages are driven simultaneously
- EL34 power tubes produce a pronounced midrange "bark" when pushed
- Moderate negative feedback means the amp distorts relatively easily
- The bright cap creates a treble boost at lower volume settings that disappears at full volume

**DSP Implementation:**
- 4 cascaded waveshaping stages with coupling cap high-pass between each
- Marshall TMB tone stack (can be modeled as a set of passive filters)
- Power amp section: additional waveshaping with different character (class AB crossover distortion)
- NFB loop simulation (reduces distortion and changes frequency response)
- Variac simulation: reduce the headroom of each stage proportionally

**Open Source References:**
- robrobinette.com complete circuit analysis
- SPICE models available
- Fractal Audio's Brit Plexi model documentation
- LTSpice simulation files on various forums

---

### Hiwatt DR103 Custom 100

**Used by:** David Gilmour

**Circuit Topology:**
- **Preamp:** 4x ECC83 (12AX7) -- one MORE preamp tube than Marshall
  - 2 gain stages BEFORE tone stack
  - 1 gain stage AFTER tone stack
  - Cathode follower between preamp and tone stack
- **Tone Stack:** Modified TMB type
  - 100K mid pot (vs Marshall 25K, Fender 10K) -- much wider mid range
  - Additional capacitors compared to Marshall
  - More mids available than Marshall or Fender
  - Lower input impedance than Marshall tone stack
- **Phase Inverter:** Fixed-biased (NOT self-biased like Marshall) -- more consistent, tighter
- **Power Amp:** 4x EL34, push-pull Class AB
- **Negative Feedback:** SIGNIFICANTLY more than Marshall
  - This is THE key difference -- more NFB = more headroom, cleaner at higher volumes
  - When the Hiwatt does distort, it is tighter and more defined
- **Rectifier:** GZ34 tube rectifier (in original models) -- provides slight "sag" on transients
- **Transformers:** Partridge (extremely rigid, tight power supply)
- **Presence Circuit:** Different from Marshall -- "Full-up boosts treble by reducing feedback at highs. Full-down reduces treble both through feedback and with a cap-to-ground shunting highs"

**What Makes It Unique:**
- Extreme headroom -- can be MUCH louder than a Marshall before distorting
- When it does break up, the distortion is tight, defined, and articulate
- Excellent note separation even at high gain (you can play chords through heavy distortion and hear each note)
- The Partridge transformers contribute to the tight, authoritative low-end
- Overall character: "a PA system" -- very hi-fi and neutral compared to the colorful Marshall

**DSP Implementation:**
- Similar to Plexi but with more gain stages and more NFB
- The tone stack needs a dedicated model (different pot values and additional capacitors)
- NFB loop must be accurately modeled -- this is what makes it sound different from Marshall
- Sag behavior: slight sag from GZ34 rectifier (less than an amp with tube rectifier throughout, but more than solid-state rectified)

---

### Marshall Silver Jubilee 2555

**Used by:** John Frusciante

**Circuit Topology:**
- Based on JCM800 with enhanced preamp section
- 3x ECC83 preamp tubes, 4x EL34 power tubes
- Three gain modes: Clean, Rhythm Clip, Lead
- Rhythm Clip adds an extra gain stage via pulled knob
- Smoother, darker distortion than JCM800
- Presence, Bass, Middle, Treble, Output Master, Lead Master, Input Gain controls

**What Makes It Unique:**
- More gain available than Plexi or JCM800
- Smoother distortion character
- Three-mode versatility
- The mid-forward voicing cuts through a band mix

---

### Silvertone 1485

**Used by:** Jack White

**Circuit Topology:**
- 4x 6L6GC power tubes (120W)
- 3x 12AX7 preamp tubes
- 1x 12AX7 tremolo circuit
- 2x 6CQ7 (phase inverter + reverb driver)
- Two channels, built-in reverb and tremolo

**What Makes It Unique:**
- 6L6GC tubes give a "rounder," less midrange-aggressive character than EL34
- Low-budget construction = less headroom, earlier breakup, "trashy" quality
- 6x10" Jensen speakers: focused midrange, less bass extension than 12" speakers

---

## PEDALS (Not Yet in Our Library)

### Dallas Arbiter Fuzz Face (Germanium PNP)

**Circuit:** 2-transistor common-emitter with shunt-series feedback

**Components:**
| Part | Value | Role |
|------|-------|------|
| C1 | 2.2uF | Input coupling |
| C2 | 20uF | Emitter bypass |
| C3 | 0.01uF (10nF) | Output coupling |
| R1 | 33K | Input bias |
| R2 | 470 ohm | Q2 collector |
| R3 | 8.2K | Q1 collector |
| R4 | 100K | Feedback |
| Rfuzz | 1K linear pot | Fuzz control |
| Rvol | 500K audio pot | Volume control |
| Q1 | AC128 (PNP Ge) | Low gain, beta=70-80 |
| Q2 | AC128 (PNP Ge) | High gain, beta=110-130 |

**Bias:** Q1 Vc=-0.6V, Q2 Vc=-4.5V

**What makes it unique:** Temperature-sensitive germanium transistors, low input impedance interacts with guitar volume, feedback path creates "sputtery" gated quality

**DSP approach:** WDF or nodal analysis of 2-transistor feedback circuit. Must model nonlinear Ge transistor behavior. chowdsp_wdf can handle this.

**Open source:** ElectroSmash analysis, SPICE models, DAFx 2008 paper by Yeh & Smith

---

### Electro-Harmonix Big Muff Pi

**Circuit:** 4-stage cascaded CE amplifiers with symmetric hard clipping + passive mid-scoop tone

**Components (NYC/general):**
| Part | Value | Role |
|------|-------|------|
| Q1-Q4 | 2N5088/BC239 NPN | Gain stages |
| D1-D4 | 1N914 | Symmetric hard clip per pair |
| Collector R | 10K | Per clipping stage |
| Feedback R | 100K | Per clipping stage |
| Coupling C | 100nF | Between stages |
| Emitter R | 150 ohm | Degeneration |
| Tone R1 | 39K | Tone stack |
| Tone R2 | 22K | Tone stack |
| Tone pot | 100K | Tone sweep |
| Tone C1 | 10nF | HP path |
| Tone C2 | 3.9nF | LP path |

**Stage gains:** 19.6dB, 23dB, 25dB, 13dB

**Version Differences:**

| Version | Transistors | Clip Caps | Tone Caps | Character |
|---------|------------|-----------|-----------|-----------|
| Triangle (1969-73) | Various, inconsistent | 100nF | Variable | Brightest, most aggressive |
| Ram's Head (1973-77) | 2N5133/2N5088 | 100nF | Standard | Smooth, sustaining, "sweet" |
| Green Russian (1990s) | Various | 470-500pF | Different | Smooth, woolly, fat low-mids |
| NYC (2000s) | 2N5088 | 100nF | Standard | Similar to Ram's Head, less magic |

**DSP approach:** 4 cascaded soft/hard clipping stages with coupling caps + passive tone filter. Similar architecture to our HM-2 (which already does 3-stage clipping). WDF for each stage, or direct waveshaping with proper component-derived transfer curves.

---

### MXR Phase 90 (Script Logo)

**Circuit:** 4-stage allpass phaser with JFET variable resistors

**Components:**
| Part | Value | Role |
|------|-------|------|
| C2-C5 | 47nF | Allpass filter caps (MATCHED) |
| C1, C6, C9 | 47nF | Various |
| C5, C7 | 0.01uF | Various |
| C8, C10 | 15uF | Various |
| Q2-Q5 | 2N5952 JFET | Variable resistors in allpass |
| Q1 | 2N4125 PNP | Output mixer |
| R1,R3,R5,R10-R13,R15,R17-R18 | 10K | Various |
| R2,R7,R8,R16,R19,R24 | 150K | Various |
| R4 | 56K | Various |
| R6,R23,R25,R26 | 24K | Various |
| R14,R21,R36 | 470K | Various |
| R20 | 1M | Various |
| R22 | 3.9M | Various |
| Rtrimmer | 250K | JFET bias adjustment |
| Speed pot | 250K | LFO rate |

**LFO:** Triangular waveform via Schmitt Trigger-Integrator

**Sweep:** 2 notches (at 58.5Hz and 340.8Hz baseline) swept by JFET resistance modulation

**What makes it unique:** No feedback in script version (smoother than block logo), triangular LFO (vs sine), 2N5952 JFETs as variable resistors

**DSP approach:** Our existing Phaser is already close. Modify to use triangle LFO, specific frequency range, and optional feedback for block logo variant.

---

### MXR Micro Amp

**Circuit:** Single TL061 non-inverting op-amp boost

**Components:**
| Part | Value | Role |
|------|-------|------|
| R1 | 22M | Input pull-down |
| R2 | 10M | Bias |
| R3 | 1K | Various |
| R4 | 56K | Feedback |
| R5 | 500K pot (reverse log) | Gain control |
| R6 | 2.7K | Feedback |
| R7, R8 | 100K | Voltage divider |
| R9 | 470 ohm | Output limiting |
| R10 | 10K | Output |
| C1 | 0.1uF | Input HPF |
| C2 | 47pF | Feedback LPF (~60.4kHz) |
| C3 | 4.7uF | HPF |
| C4 | 1uF | Bias decoupling |
| C5 | 15uF | Output HPF |
| U1 | TL061 | Op-amp |
| D1 | 1N4001 | Reverse polarity protection |

**Gain:** 0 to 26.2dB. Formula: Gv = (1 + R4/(R5+R6)) * (R10/(R9+R10))

**What makes it unique:** Transparent, flat frequency response boost

**DSP approach:** Our Boost effect (index 3) already does this. Verify gain range matches.

---

### Boss CE-1 Chorus Ensemble

**Circuit:** BBD-based chorus with MN3002 (512-stage) + preamp section

**Key specs:**
- BBD: MN3002, delay range 0.32-25.6ms
- Preamp: Op-amp preamp + optional transistor gain stage (high mode)
- Modes: Chorus (dry+wet) and Vibrato (wet only)

**What makes it unique:** The preamp section adds warmth; BBD character (clock noise, bandwidth limiting, subtle distortion from sampling)

**DSP approach:** Our Chorus already handles basic chorus. Need to add BBD emulation (sample-and-hold with reconstruction filter) and preamp section. The BBD adds subtle aliasing and harmonic content that digital delay lines do not naturally produce.

---

### Echoplex EP-3 Preamp

**Circuit:** Single JFET gain stage

**Components:**
| Part | Value | Role |
|------|-------|------|
| Input cap | 22nF | AC coupling / HPF |
| Source bypass | 22nF | HF gain boost |
| Output cap | 100nF | AC coupling |
| JFET | TIS58 (modern: 2N5457/BF245) | Gain element |

**Gain:** Up to 11dB

**Operating voltage:** ~22VDC

**What makes it unique:** Slight presence boost from source bypass cap, JFET harmonic character

**DSP approach:** Simple JFET gain stage model. Could be added as a variant of our Boost effect. Model the JFET's nonlinear transfer characteristic (square-law) for accurate harmonic generation.

---

### Vox Clyde McCoy Wah

**Circuit:** Inductor-based bandpass filter

**Components:**
| Part | Value | Role |
|------|-------|------|
| L1 | 500mH (Halo inductor) | Resonant element |
| C1, C2 | 0.01uF | Various |
| C3 | 4.7uF | Various |
| C4, C5 | 0.22uF | Various |
| R1 | 68K | Various |
| R2 | 1.5K | Various |
| R3 | 22K | Various |
| Q | BC109 | Gain stage |

**Sweep range:** 450Hz to 1.6kHz

**Pot taper:** ICAR (specialized)

**What makes it unique:** The 500mH inductor creates a resonant bandpass with a specific Q that moves with the pedal

**DSP approach:** Adapt our existing Wah with manual sweep mode and inductor-simulated resonant bandpass. The inductor can be modeled as a gyrator (op-amp + RC) in the digital domain.

---

### Roger Mayer Octavia

**Circuit:** Full-wave rectifier octave-up fuzz

**Transistors:** 3x silicon (all-silicon version)

**Diodes:** Germanium (BAT46 modern replacement)

**How it works:** Signal split -> in-phase + inverted paths -> half-wave rectify each -> sum = doubles fundamental

**What makes it unique:** Octave-up effect that works best above 12th fret; becomes ring-modulator-like on low notes

**DSP approach:** Full-wave rectification + fuzz. Can be implemented as: abs(x) for the octave component, mixed with the original fuzzed signal. Anti-aliasing needed (oversampling) because rectification generates lots of harmonics.

---

### MXR Dyna Comp

**Circuit:** OTA-based compressor (CA3080)

**Components:**
| Part | Value | Role |
|------|-------|------|
| Q1-Q5 | 2N3904 | Various |
| U1 | CA3080 | OTA (gain control) |
| D1-D2 | 1N914 | Envelope detection |
| RV1 | 500K | Output level |
| RV2 | 50K | Sensitivity |
| R-trimmer | 2K | Bias |

**What makes it unique:** The CA3080 OTA's "squash" character -- more aggressive than VCA compressors

**DSP approach:** Our Compressor (index 1) handles generic compression. For Dyna Comp accuracy, model the OTA's nonlinear gain control and the specific attack/release characteristics from the passive envelope detector.

---

### Ibanez WH-10 Wah

**Circuit:** Multiple feedback op-amp bandpass filter (NO inductor)

**Pots:** 50K linear (frequency), 500K log (gain correction)

**What makes it unique:** Wider frequency range than Vox, more "vocal" and "quacky," consistent gain across sweep

**DSP approach:** Adapt our Wah by changing the filter to a multiple-feedback bandpass topology. The key difference from Vox is the wider frequency range and consistent gain.

---

### DigiTech Whammy (Pitch Shifter)

**Algorithm:** Time-domain SOLA (Synchronous Overlap-Add)

**How it works:** Input sampled into buffer -> pitch detection -> resampled at modified rate -> windowed overlap-add for smooth output

**What makes it unique:** Characteristic "warble" and slight tracking latency; artifacts are part of the sound

**DSP approach:** Implement SOLA/PSOLA pitch shifting with autocorrelation-based pitch detection. This is a major new feature requiring its own sprint.

---

### Dallas Rangemaster Treble Booster

**Circuit:** Single germanium transistor (OC44) common-emitter booster

**Components:**
| Part | Value | Role |
|------|-------|------|
| R1 | 1M | Input bias |
| R2 | 3.9K | Collector load |
| R3 | 470K | Bias |
| R4 | 1M | Various |
| C1 | 4.7nF | Input HPF (treble emphasis) |
| C2 | 47uF | Emitter bypass |
| C3 | 47uF | Various |
| C4 | 10nF | Output coupling |
| Bias trimmer | 100K | Collector voltage adjust |
| Q1 | OC44 (Ge PNP) | Gain element |

**Bias:** Q1 collector at +2V (referenced to ground, 9V supply)

**What makes it unique:** Boosts treble frequencies while leaving bass relatively flat; the germanium transistor adds subtle harmonic distortion

**DSP approach:** Simple single-transistor booster. High-pass filter at input (C1=4.7nF) sets the treble emphasis. Could be a variant of our Boost effect with frequency shaping.

---

## CABINETS

### Marshall 1960A/B

**Speakers vary by era:**

| Era | Speaker | Power | Character |
|-----|---------|-------|-----------|
| 1960s-70s | Celestion G12M-25 "Greenback" | 25W | Warm, vintage, treble rolloff above 4kHz, presence peak ~2.5kHz |
| 1970s | Celestion G12H-30 | 30W | Tighter lows, more prominent upper mids than Greenback |
| 1980s | Celestion G12-65 | 65W | Flatter, more modern, handles high output, less midrange color |

**1960A** = angled (slant) front -- slight treble boost due to speaker angle

**1960B** = straight front -- more focused, slightly less treble dispersion

### WEM Starfinder with Fane Crescendo

**Used by:** David Gilmour

**Character:** Flat, hi-fi, extended high-frequency response, tight bass -- like a PA speaker compared to the "colored" Celestion sound

### Fender Twin Reverb 2x12

**Speakers:** Jensen C12N (ceramic) or JBL D120F (aluminum dome)

**Character:** Clean, extended high-frequency response, deep bass from open-back design. JBL D120F is much more hi-fi than Jensen, with almost flat response and extreme high-end clarity.

### Vox AC30 2x12

**Speakers:** Celestion Alnico Blue (15W)

**Character:** Chimey, bright, with a prominent "jangle" in the upper midrange. The Alnico Blue's resonant frequency emphasis creates the classic Vox shimmer.

---

# PART 3: IMPLEMENTATION PRIORITY

## Priority Ranking (by impact and feasibility)

### HIGH PRIORITY (would enable multiple presets)

1. **Big Muff Pi** -- Needed for Gilmour, Frusciante, Jack White. Similar architecture to our existing HM-2 (cascaded clipping stages). WDF implementation using chowdsp_wdf. Estimated: 1-2 sprints.

2. **Marshall Plexi 1959 Amp Model** -- Needed for Hendrix, EVH. Replace our generic "Crunch" mode with a proper Plexi model including correct tone stack and gain staging. Estimated: 1 sprint to add as a new amp model type.

3. **Fuzz Face (Germanium)** -- Needed for Hendrix. Simple 2-transistor circuit but nonlinear feedback makes WDF modeling interesting. Estimated: 1 sprint.

4. **Hiwatt DR103 Amp Model** -- Needed for Gilmour. Different enough from Marshall to need its own model (more NFB, different tone stack, more headroom). Estimated: 1 sprint.

### MEDIUM PRIORITY (nice to have, fewer presets affected)

5. **MXR Phase 90 optimization** -- Our Phaser is close. Needs triangle LFO, specific frequency range, script vs block logo feedback option. Estimated: partial sprint.

6. **Echoplex EP-3 Preamp** -- Simple JFET boost. Could be a preset on our Boost effect or a minor addition. Estimated: partial sprint.

7. **Vox Clyde McCoy Wah (manual mode)** -- Add manual sweep parameter to our existing Wah. Estimated: partial sprint.

8. **Variac / Voltage Sag simulation** -- Add "sag" parameter to AmpModel that simulates reduced voltage operation. Estimated: partial sprint.

### LOWER PRIORITY (specialized or complex)

9. **Boss CE-1 Chorus Ensemble** -- BBD emulation is complex. Our Chorus handles the basic effect. Estimated: 1 sprint.

10. **Ibanez WH-10 Wah** -- Different topology from Vox. Could be a mode on our existing Wah. Estimated: partial sprint.

11. **DigiTech Whammy (Pitch Shifter)** -- Major new capability. SOLA implementation is significant. Estimated: 2 sprints.

12. **Roger Mayer Octavia** -- Octave-up fuzz. Interesting but niche. Estimated: 1 sprint.

13. **Silvertone 1485 Amp Model** -- Only needed for Jack White. Estimated: 1 sprint.

14. **Dallas Rangemaster** -- Simple treble boost. Could be a mode on our Boost. Estimated: partial sprint.

15. **MXR Dyna Comp** -- Our Compressor handles generic compression. Dyna Comp specifics are minor. Estimated: partial sprint.

16. **Yamaha RA-200 Rotary** -- Leslie-type effect. New modulation category. Estimated: 1 sprint.

17. **Additional Cabinet IRs** -- Greenback, G12H-30, G12-65, Fane Crescendo, Alnico Blue, Jensen C10Q. These are data (IR files), not code. Can be added incrementally.

---

# PART 4: PRESET DEFINITIONS

These presets use ONLY our current 21 effects to approximate each iconic tone as closely as possible.

### Preset 1: "Clapton Woman Tone"
```json
{
  "name": "Clapton Woman Tone",
  "category": "Crunch",
  "effects": {
    "7": { "enabled": true, "params": {"freq1": 900, "gain1": -15, "Q1": 1.5, "freq2": 3500, "gain2": -18, "Q2": 0.7} },
    "5": { "enabled": true, "params": {"inputGain": 2.0, "outputLevel": 0.7, "modelType": 1} },
    "6": { "enabled": true, "params": {"cabinetType": 1} }
  },
  "effectOrder": [7, 5, 6]
}
```

### Preset 2: "Hendrix Purple Haze"
```json
{
  "name": "Hendrix Purple Haze",
  "category": "Heavy",
  "effects": {
    "4": { "enabled": true, "params": {"drive": 0.85, "tone": 0.4, "level": 0.8} },
    "5": { "enabled": true, "params": {"inputGain": 2.0, "outputLevel": 0.6, "modelType": 1} },
    "6": { "enabled": true, "params": {"cabinetType": 1} }
  },
  "effectOrder": [4, 5, 6]
}
```

### Preset 3: "Hendrix Voodoo Child"
```json
{
  "name": "Hendrix Voodoo Child",
  "category": "Heavy",
  "effects": {
    "2": { "enabled": true, "params": {"sensitivity": 0.8, "frequency": 500, "resonance": 5.0, "mode": 0} },
    "4": { "enabled": true, "params": {"drive": 0.8, "tone": 0.5, "level": 0.75} },
    "5": { "enabled": true, "params": {"inputGain": 2.0, "outputLevel": 0.6, "modelType": 1} },
    "6": { "enabled": true, "params": {"cabinetType": 1} }
  },
  "effectOrder": [2, 4, 5, 6]
}
```

### Preset 4: "Seven Nation Army"
```json
{
  "name": "Seven Nation Army",
  "category": "Crunch",
  "effects": {
    "3": { "enabled": true, "params": {"gain": 0.6} },
    "7": { "enabled": true, "params": {"freq1": 250, "gain1": -8, "Q1": 1.0, "freq2": 2000, "gain2": 4, "Q2": 0.7} },
    "5": { "enabled": true, "params": {"inputGain": 1.2, "outputLevel": 0.7, "modelType": 0} },
    "6": { "enabled": true, "params": {"cabinetType": 0} }
  },
  "effectOrder": [3, 7, 5, 6],
  "note": "Cannot replicate octave-down Whammy effect with current effects"
}
```

### Preset 5: "Frusciante Under the Bridge"
```json
{
  "name": "Frusciante Under the Bridge",
  "category": "Clean",
  "effects": {
    "8": { "enabled": true, "params": {"rate": 1.2, "depth": 0.4} },
    "5": { "enabled": true, "params": {"inputGain": 0.8, "outputLevel": 0.7, "modelType": 0} },
    "6": { "enabled": true, "params": {"cabinetType": 0} }
  },
  "effectOrder": [8, 5, 6]
}
```

### Preset 6: "Frusciante Dani California"
```json
{
  "name": "Frusciante Dani California",
  "category": "Heavy",
  "effects": {
    "18": { "enabled": true, "params": {"dist": 0.6, "tone": 0.7, "level": 0.7, "mode": 1} },
    "5": { "enabled": true, "params": {"inputGain": 1.5, "outputLevel": 0.65, "modelType": 1} },
    "6": { "enabled": true, "params": {"cabinetType": 1} }
  },
  "effectOrder": [18, 5, 6]
}
```

### Preset 7: "Gilmour Comfortably Numb"
```json
{
  "name": "Gilmour Comfortably Numb",
  "category": "Heavy",
  "effects": {
    "1": { "enabled": true, "params": {"threshold": -15, "ratio": 4.0, "attack": 10, "release": 200, "makeupGain": 6, "kneeWidth": 6} },
    "4": { "enabled": true, "params": {"drive": 0.7, "tone": 0.6, "level": 0.6} },
    "5": { "enabled": true, "params": {"inputGain": 1.3, "outputLevel": 0.7, "modelType": 0} },
    "6": { "enabled": true, "params": {"cabinetType": 1} },
    "13": { "enabled": true, "params": {"decay": 0.4, "damping": 0.5, "mix": 0.15} }
  },
  "effectOrder": [1, 4, 5, 6, 13]
}
```

### Preset 8: "EVH Brown Sound"
```json
{
  "name": "EVH Brown Sound",
  "category": "Heavy",
  "effects": {
    "10": { "enabled": true, "params": {"rate": 0.3, "depth": 0.5, "feedback": 0.0, "stages": 4} },
    "3": { "enabled": true, "params": {"gain": 0.7} },
    "5": { "enabled": true, "params": {"inputGain": 2.0, "outputLevel": 0.65, "modelType": 1} },
    "6": { "enabled": true, "params": {"cabinetType": 1} }
  },
  "effectOrder": [10, 3, 5, 6]
}
```

---

# PART 5: GAP ANALYSIS SUMMARY

## Effects We Have vs. What We Need

| Component | Have? | Current Index | Gap |
|-----------|-------|---------------|-----|
| Fuzz Face | NO | -- | Need WDF 2-transistor Ge fuzz |
| Big Muff Pi | NO | -- | Need 4-stage clipping + tone stack |
| MXR Phase 90 | PARTIAL | 10 (Phaser) | Need triangle LFO, specific range |
| MXR Micro Amp | YES | 3 (Boost) | Close enough, verify gain range |
| Boss CE-1 | PARTIAL | 8 (Chorus) | Need BBD emulation + preamp |
| Boss DS-2 | YES | 18 | Full WDF implementation |
| Vox Wah | PARTIAL | 2 (Wah) | Need manual sweep + inductor model |
| Ibanez WH-10 | PARTIAL | 2 (Wah) | Need op-amp bandpass mode |
| Octavia | NO | -- | Need octave-up fuzz |
| Echoplex Preamp | PARTIAL | 3 (Boost) | Need JFET character + tone shaping |
| Rangemaster | NO | -- | Need treble boost with Ge character |
| Dyna Comp | PARTIAL | 1 (Compressor) | Need OTA character |
| Electric Mistress | PARTIAL | 11 (Flanger) | Need BBD character |
| DigiTech Whammy | NO | -- | Need pitch shifter (major feature) |
| Marshall Plexi | PARTIAL | 5 (AmpModel crunch) | Need proper Plexi model |
| Hiwatt DR103 | NO | -- | Need new amp model |
| Marshall Jubilee | PARTIAL | 5 (AmpModel crunch) | Need Jubilee variant |
| Silvertone 1485 | NO | -- | Need new amp model |
| Rotary Speaker | NO | -- | Need Leslie-type effect |
| Greenback IR | NO | 6 (CabinetSim) | Need IR data |
| G12H-30 IR | NO | 6 (CabinetSim) | Need IR data |
| G12-65 IR | NO | 6 (CabinetSim) | Need IR data |
| Fane Crescendo IR | NO | 6 (CabinetSim) | Need IR data |

## New Effects Count

- **Must build (new effect slots):** 4 (Fuzz Face, Big Muff, Octavia, Pitch Shifter)
- **New amp models (within existing AmpModel):** 3 (Plexi, Hiwatt, Silvertone)
- **Modifications to existing effects:** 4 (Phaser, Wah, Boost, Compressor)
- **New modulation effect:** 1 (Rotary Speaker)
- **Cabinet IRs needed:** 5+ additional speaker types

---

## Sources and References

### Artist Tone Research
- [Killer Guitar Rigs - Woman Tone](https://killerguitarrigs.com/what-is-the-woman-tone/)
- [ElectroSmash - Fuzz Face Analysis](https://www.electrosmash.com/fuzz-face)
- [ElectroSmash - Germanium Fuzz Face](https://www.electrosmash.com/germanium-fuzz)
- [Fuzz Face - Wikipedia](https://en.wikipedia.org/wiki/Fuzz_Face)
- [Custom Boards Finland - Hendrix Signal Chain](https://en.customboards.fi/blogs/articles/a-detailed-look-at-jimi-hendrix-pedals-and-signal-chain)
- [guitar.com - Van Halen I Gear](https://guitar.com/features/artist-rigs/the-gear-used-by-eddie-van-halen-on-van-halen-i/)
- [Guitarriego - Brown Sound](https://guitarriego.com/en-us/guitar/brown-sound-the-secret-of-van-halens-tone-myths-and-truths/)
- [VHND - Tone Chaser](https://www.vhnd.com/2022/07/08/tone-chaser-one-mans-pursuit-of-the-classic-eddie-van-halen-sound/)
- [Gilmourish - The Wall Settings](https://www.gilmourish.com/?page_id=50)
- [Kit Rae - Comfortably Numb Sound](http://www.kitrae.net/music/Music_mp3_Comfortably_Numb_Sound.html)
- [Music Strive - Frusciante Amp Settings](https://musicstrive.com/john-frusciante-amp-settings/)
- [Rock Guitar Universe - Frusciante Gear Guide](https://rockguitaruniverse.com/john-frusciante-gear-effects-tone/)

### Circuit Analysis
- [ElectroSmash - Big Muff Pi Analysis](https://www.electrosmash.com/big-muff-pi-analysis)
- [ElectroSmash - MXR Phase 90 Analysis](https://www.electrosmash.com/mxr-phase90)
- [ElectroSmash - MXR Micro Amp Analysis](https://www.electrosmash.com/mxr-microamp)
- [ElectroSmash - MXR Dyna Comp Analysis](https://www.electrosmash.com/mxr-dyna-comp-analysis)
- [ElectroSmash - Vox V847 Wah Analysis](https://www.electrosmash.com/vox-v847-analysis)
- [ElectroSmash - Dallas Rangemaster Analysis](https://www.electrosmash.com/dallas-rangemaster)
- [Rob Robinette - How Marshall JCM800 Works](https://robrobinette.com/How_the_Marshall_JCM800_Works.htm)
- [Kit Rae - Big Muff Page](http://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part3.html)

### Amplifier Technical References
- [Hiwatt.org - Tech Info](https://hiwatt.org/tech2.html)
- [Legendary Tones - Hiwatt DR103](https://legendarytones.com/hiwatt-custom-100-dr103/)
- [Hiwatt Schematics](https://el34world.com/charts/Schematics/Files/Hiwatt/Hiwatt_Schematics.htm)
- [Fractal Audio - Hipower (Hiwatt DR103) Model](https://forum.fractalaudio.com/threads/fractal-audio-amp-models-hipower-hiwatt-dr103.113789/)
- [Ampbooks - Hiwatt CP103 Tone Stack](https://www.ampbooks.com/mobile/classic-circuits/cp103-tonestack/)

### Pedal Circuit References
- [Catalinbread - Boss CE-1](https://catalinbread.com/blogs/kulas-cabinet/boss-ce-1-chorus-ensemble)
- [Aion FX - Echoplex EP-3 Preamp](https://aionfx.com/app/files/docs/ares_legacy_documentation.pdf)
- [Fuzz Central - Vox Clyde McCoy Wah](https://fuzzcentral.ssguitar.com/mccoy.php)
- [Fuzz Central - Tycobrahe Octavia](https://fuzzcentral.ssguitar.com/octavia.php)
- [Aion FX - Octahedron (Roger Mayer Octavia)](https://aionfx.com/project/octahedron-octave-fuzz/)
- [ElectroSmash - Boss CE-2 Analysis](https://www.electrosmash.com/boss-ce-2-analysis)

### Academic / DSP References
- Yeh & Smith, "Simulating Guitar Distortion Circuits Using Wave Digital and Nonlinear State-Space Formulations" (DAFx 2008)
- CCRMA Stanford, "Digital Grey Box Model of the Uni-Vibe" (DAFx 2019)
- Chowdhury, "Real-Time Physical Modelling for Analog Audio Effects" (DAFx 2020)
- [DigiTech Whammy - Wikipedia](https://en.wikipedia.org/wiki/DigiTech_Whammy)

### Existing Project Documentation (in this repo)
- docs/pedal-circuit-analysis.md -- DS-1, DS-2, RAT, HM-2, Univibe full circuit analysis
- docs/implementation-research.md -- chowdsp_wdf library, WDF implementation patterns
- docs/dsp-techniques-research.md -- WDF theory, ADAA, diode math, LDR modeling
