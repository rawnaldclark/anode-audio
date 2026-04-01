# Iron Butterfly / Mosrite Fuzzrite Tone Research

## Part 1: Erik Brann's Rig (1967-1970)

### Guitar
- **Mosrite Ventures Mark I** (sunburst finish)
- Purchased from Iron Butterfly's original guitarist Danny Weis when Brann joined in late 1967
- German-carved body, slim neck
- **Single-coil pickups** (Mosrite's own design — high-output for the era, ~7-8K ohm DC resistance)
- Standard Mosrite vibrato tailpiece
- Brann was 17 years old when he recorded "In-A-Gadda-Da-Vida"

### Amplifier: Vox Super Beatle V1141/V1142
- **Solid-state** amplifier (NOT tube) — this is critical for understanding the tone
- **120W RMS / 240W peak** into 2-ohm load
- Three channels:
  - **Normal**: Tremolo, Top Boost rocker switch
  - **Brilliant**: Reverb (selectable), MRB (Mid Resonant Boost)
  - **Bass**: Tone-X (sweepable frequency tone control)
- **Speaker cab (V414)**: 4x 12" Celestion G12M Alnico speakers + 2x Midax horns
- **Built-in Distortion Booster**: Present in circuitry of both V1141 and V1142. V1141 shipped with 4-button footswitch (reverb, tremolo, MRB, distortion). V1142 shipped with 3-button footswitch (no distortion button) but the circuit was still on the PCB
- **Key tone fact**: The Brilliant channel + MRB provides a scooped-mid, bright, slightly compressed solid-state clean that is the FOUNDATION of the Iron Butterfly sound. The amp does NOT break up like a tube amp — any distortion comes from the Distortion Booster circuit or external pedals

### The Fuzz Debate: Fuzzrite vs. Amp Distortion

There is genuine disagreement among knowledgeable sources:

**Theory 1: Mosrite Fuzzrite pedal** (majority view)
- Most gear historians and forum consensus identifies the Fuzzrite as the primary fuzz source
- Brann used all-Mosrite gear (guitar + fuzz) which was common for Mosrite endorsees
- The "spitty, buzzy" character of the IAGDV riff matches Fuzzrite characteristics

**Theory 2: Vox Super Beatle built-in Distortion Booster** (minority but credible)
- One experienced player on The Gear Page who played the song professionally claims: "plugging straight into the amp's Brilliant channel...and using the distinctive internal Vox Super-Beatle distortion IS the sound of Erik B"
- The V1141's distortion booster was germanium-based
- This person achieved comparable results "with the guitar I used before the Mosrite"

**Theory 3: Both**
- Most likely scenario: Fuzzrite into Brilliant channel with Distortion Booster engaged for extra saturation and sustain
- The combination of a buzzy fuzz into a solid-state amp with its own distortion circuit would explain the unique "metallic grind" quality

**Theory 4: Different pedals on different albums**
- "Heavy" (1968, first album with Danny Weis): Possibly Tonebender MKI (described as "smoother with very long sustain")
- "In-A-Gadda-Da-Vida" (1968, second album with Brann): Fuzzrite
- "Ball" (1969, third album): Possibly Gibson Maestro or continued Fuzzrite use
- This is plausible since Brann inherited gear from Weis and may have also acquired his own

### Which Fuzzrite Version?

**Critical timeline**: The germanium Fuzzrite was manufactured from mid-1966 to early 1968. Only ~200-250 germanium units were made. The silicon version replaced it in 1968.

**"In-A-Gadda-Da-Vida" was recorded in May 1968** — right at the transition point.

If Brann inherited/purchased the Fuzzrite from Danny Weis (or acquired one separately pre-1968), it would almost certainly be a **germanium version**. The silicon version had barely entered production by May 1968.

**Verdict: Most likely germanium**, based on:
1. Timeline (Ge production peaked 1966-1967, Weis would have had one from this era)
2. Tonal characteristics (the warm, temperature-sensitive, slightly gated quality matches Ge)
3. Forum consensus specifically calls out "the first version of the fuzzrite, as used by Iron Butterfly, was germanium"

### Other Effects
- **Fender Reverb Tank**: External spring reverb unit (likely a Fender 6G15 standalone reverb)
- **Vox Wah-Wah**: Used for the solo section of IAGDV (the "watery cry baby solo")
- **Built-in Vox Reverb**: Available on the Brilliant channel
- **Built-in Vox Tremolo**: Available on the Normal channel

### Brann's Known Signal Chain (IAGDV era)
```
Mosrite Ventures Mark I -> [Vox Wah] -> [Mosrite Fuzzrite Ge] -> Vox Super Beatle V1141 Brilliant Channel
                                                                  (with Distortion Booster + Reverb engaged)
```

---

## Part 2: Mosrite Fuzzrite Circuit — Complete Technical Analysis

### Designer
Ed Sanner, Mosrite's electronics engineer. The Fuzzrite was Mosrite's answer to the British fuzz pedals (Tone Bender, Fuzz Face) flooding the US market in 1966.

### Circuit Topology
Two cascaded common-emitter gain stages with a unique output BLEND control ("Depth") rather than a simple gain/fuzz knob.

**Signal flow**:
1. Input -> C1 (47nF coupling cap) -> Q1 base
2. Q1 collector -> C2 (2.2nF coupling cap) -> Depth pot wiper
3. Depth pot blends between Q1 output (clean-ish, fat) and Q2 output (saturated, buzzy)
4. Q1 collector also -> C4 (47nF coupling cap) -> Q2 base (direct coupling to second stage)
5. Q2 collector -> C3 (2.2nF coupling cap) -> Depth pot lug
6. Depth pot output -> R_HP (22K) -> ground (forms HPF with C2/C3)
7. After Depth pot -> Volume pot -> Output

### Complete BOM — Germanium Version (1966-1967)

**Transistors:**
| Ref | Type | Notes |
|-----|------|-------|
| Q1 | 2N2613 (PNP Ge) | High gain, somewhat leaky (Icbo 250-500uA), hFE 100-150 |
| Q2 | 2N408 (PNP Ge) | Low gain, low leakage (Icbo <80uA), hFE 40-60 |

Original markings were "TZ82" on some units. The asymmetric gain selection (high-gain Q1, low-gain Q2) is deliberate — Q1 provides the initial drive, Q2 provides the buzzy, saturated character.

**Resistors:**
| Ref | Value | Function |
|-----|-------|----------|
| R1 | 1M | Input pulldown / bias path |
| R2 | 470K | Q1 collector to V+ (collector load) |
| R3 | 470K | Q1 collector to Q1 base (DC feedback bias) |
| R4 | 470K | Q2 collector to V+ (collector load) |
| R5 | 470K | Q2 collector to Q2 base (DC feedback bias) |
| R_HP | 22K | HPF with output coupling caps (CRITICAL — often omitted from schematics) |

**Note on the 22K resistor**: This component was physically soldered behind the PCB directly to the Depth potentiometer in original units. Most published schematics omit it. It forms a high-pass filter at ~3.3kHz with the 2.2nF coupling caps, which is KEY to the Fuzzrite's distinctive "buzzy" character — it rolls off bass from the fuzzed signal, creating that thin, cutting, almost insect-like tone.

**Capacitors:**
| Ref | Value | Type | Function |
|-----|-------|------|----------|
| C1 | 47nF (originally 100nF early units) | Film | Input coupling to Q1 |
| C2 | 2.2nF (originally 2nF) | Film | Q1 collector to Depth pot |
| C3 | 2.2nF (originally 2nF) | Film | Q2 collector to Depth pot |
| C4 | 47nF (originally 100nF early units) | Film | Q1 collector to Q2 base (interstage coupling) |
| C5 | 100uF | Electrolytic | Power supply filter |

**Note**: Early units used 100nF for C1 and C4. Later germanium units transitioned to 50nF, then 47nF. The input cap value affects bass response — 100nF lets in more bass (~32Hz -3dB with 1M input R), while 47nF cuts at ~68Hz.

**Potentiometers:**
| Control | Value | Taper | Function |
|---------|-------|-------|----------|
| Depth | 350K | Reverse audio (C) | Blends Q1 and Q2 outputs |
| Volume | 33K | Audio (A) | Master output level |

Modern builds typically substitute 500K + 1.2M parallel (=~415K) for Depth and 50K for Volume.

**Power Supply:**
- 9V battery (PNP circuit — positive ground in germanium version)
- No voltage regulation
- Collector voltages sit at ~0.6V each (heavily saturated by design)

### Complete BOM — Silicon Version (1968)

**Transistors:**
| Ref | Type | Notes |
|-----|------|-------|
| Q1 | 2N2222 / PN2222A / 2N3904 | NPN Si, hFE ~100-200 |
| Q2 | 2N2222 / PN2222A / 2N3904 | NPN Si, hFE ~100-200, ideally lower than Q1 |

Original silicon units used transistors marked "TZ82" (custom Mosrite marking). Some units used orange encapsulated Sprague modules.

**Resistors**: Same values as germanium version. Some silicon units may omit the 22K HPF resistor.

**Capacitors**: Same topology. Silicon units used 47nF for C1/C4, 2.2nF for C2/C3.

**Key difference**: NPN silicon = negative ground (standard modern pedal polarity). The silicon version is described as having "inorganic pick attack" — "fuzz floodgates opening and slamming shut" compared to germanium's more touch-sensitive response.

### Gain Analysis
Each stage provides approximately **44dB of voltage gain** (x158). With two stages cascaded:
- Total available gain: ~88dB (x25,000)
- But Depth control blends rather than cascading, so effective gain ranges from ~44dB (Depth at 0, Q1 only) to the full ~88dB (Depth at max, both stages)
- At low Depth: clean-ish, fat tone (just Q1)
- Mid Depth: voltage-starved gating, pseudo ring-mod octave harmonics
- High Depth: aggressive, thin, fully saturated buzz-saw

### HPF Characteristics
The 22K resistor + 2.2nF capacitor forms a first-order HPF:
- **f_c = 1 / (2 * pi * 22K * 2.2nF) = 3,289 Hz**
- This is surprisingly high — it means the Fuzzrite's fuzzed signal has significant bass rolloff
- This HPF is what gives the Fuzzrite its distinctive "buzzy" rather than "thick" character
- Compare to a Big Muff (tone stack centered around ~1kHz) or Fuzz Face (no HPF, full bass fuzz)

### Biasing Scheme
The 470K collector-to-base feedback resistor sets the DC operating point. With 9V supply:
- Q collector voltage ≈ V+ * R_load / (R_load + R_feedback) ≈ 9 * 470K / (470K + 470K) = ~4.5V (theoretical)
- But with germanium leakage current and low-hFE Q2, actual collector voltages are much lower
- Reports indicate Vc ≈ 0.6V per stage in original units — deeply saturated, voltage-starved
- This "starved" biasing is intentional and contributes to the gated, sputtery character

---

## Part 3: Song-Specific Tone Analysis

### "In-A-Gadda-Da-Vida" (1968)

**Recording details:**
- **Studio**: Ultra Sonic Recording Studios, Hempstead, Long Island, NY
- **Date**: May 27, 1968
- **Engineer**: Don Casale (uncredited on album; Jim Hilton credited as producer/engineer)
- **Recording method**: Single live take. Casale rolled tape during what the band thought was a soundcheck/rehearsal. When the 17-minute take finished, everyone agreed it was good enough
- **Post-production**: Jim Hilton remixed at Gold Star Studios, Los Angeles

**Tone characteristics of the main riff:**
- Buzzy, spitty fuzz with prominent high-frequency content
- Not thick or bassy — the HPF character of the Fuzzrite is clearly audible
- Moderate sustain (not infinite like Big Muff)
- "Percussive click to the front of each note" — characteristic Fuzzrite pick attack
- Clean notes ring through between riff attacks — suggests Depth not fully maxed
- The solid-state Vox amp adds a stiff, uncompressed quality underneath the fuzz
- Spring reverb audible on sustained notes (Vox built-in or Fender reverb tank)

**Estimated settings for the main riff:**
- Fuzzrite Depth: ~6-7/10 (enough fuzz for sustain but retaining note definition)
- Fuzzrite Volume: ~7-8/10 (matching amp channel volume)
- Vox Brilliant channel: Volume ~5-6, Treble ~6-7, Bass ~4-5
- MRB: Possibly engaged (adds mid-frequency emphasis)
- Reverb: Light, ~3/10

**Solo sections:**
- Wah engaged (Vox wah pedal)
- Same fuzz settings, possibly Depth increased to ~8/10 for more sustain
- More reverb for the "watery" quality noted by listeners

### "Iron Butterfly Theme" (from "Heavy", 1968)

**Note**: This was recorded with Danny Weis on guitar, NOT Erik Brann.
- Feedback-laden fuzz guitar
- More chaotic, less controlled than IAGDV
- Weis used the same Vox Super Beatle
- Fuzz source less certain — possibly Tonebender MKI rather than Fuzzrite

### "Unconscious Power" / "Possession" (from "Heavy", 1968)

- Also Danny Weis on guitar
- Released as a double-A-side single
- Tighter, more controlled fuzz than "Iron Butterfly Theme"
- Less reverb, more upfront guitar tone

### "It Must Be Love" (from "Ball", 1969)

- Erik Brann on guitar
- "Grinding" metallic fuzz quality on the solo
- Forum debate between Fuzzrite (Ge), Gibson Maestro FZ-1A, and Shin-ei Companion FY-2
- The "metallic" quality suggests possible guitar volume rollback on a germanium fuzz (Ge circuits clean up uniquely with volume knob interaction)
- Could also be the Vox amp's Distortion Booster alone, creating a different character than the Fuzzrite

### "Termination" (from "In-A-Gadda-Da-Vida" album, 1968)

- Specifically identified as using the Mosrite Fuzzrite
- Full-on buzz-saw fuzz tone
- More aggressive than the title track — Depth likely maxed

---

## Part 4: John Frusciante's Fuzzrite Usage

### The Pedal
- **Mosrite Fuzzrite V1** — confirmed **germanium** version
- Only ~200-250 germanium units were ever made, making this a rare vintage piece
- Used during **Stadium Arcadium tour (2006-2007)** and on **The Empyrean (2009)**

### Frusciante's Settings (CRITICAL FINDING)
**Depth: 0 (fully counterclockwise)**
**Volume: 8-10/10**

This is a highly significant and counterintuitive setting:
- At Depth = 0, you hear ONLY Q1 (the first transistor stage)
- No Q2 blending = no buzz-saw character
- Result: "very controllable, dynamic, and defined" tone
- The Volume knob functions essentially as a gain control because Frusciante's Marshall amps were already extremely loud
- Depth adds treble frequencies that reduce definition in a loud band mix — at zero, frequencies stay balanced

### Signal Chain Position
Based on the JFX Pedals Empyrean layout (which replicates Frusciante's chain):
```
Guitar -> [Fuzz Factory] -> DS-2 -> Fuzzrite -> [Micro Amp] -> [Big Muff] -> WH-10 Wah -> Amps
```

The Fuzzrite sits AFTER the DS-2 but BEFORE the wah. This is significant — the DS-2 can push the Fuzzrite harder, and the wah shapes the fuzzed signal.

### Songs Featuring the Fuzzrite

**Stadium Arcadium Tour (2006-2007):**
- **"Dani California" solo** (live versions) — Fuzzrite for the solo's aggressive fuzz
- **La Cigale 2006 intro improvisation** — Fuzzrite prominently used
- **"Stadium Arcadium" solo** (Rock In Rio Portugal) — subtler Fuzzrite use
- Various improvisations and extended jams — "responsible for most of the distorted sounds of improvisations and solos" in the first year of the tour

**The Empyrean (2009):**
- **"Before the Beginning"** — Fuzzrite combined with DS-2 and guitar volume control for sounds ranging from "slightly distorted to explosive"
- Also used Ibanez WH-10 wah for feedback at different frequencies on this track

**Note**: The Fuzzrite was NOT used on the Stadium Arcadium studio album itself. Studio fuzz/distortion came from Big Muff Pi, English Muff'n, and DS-2. The Fuzzrite was adopted for the subsequent tour and solo work.

### Frusciante's Amp Settings (Marshall Silver Jubilee)
- Presence: 4-5
- Bass: 3
- Middle: 3
- Treble: 7-8
- Output Master: 7-8
- Lead Master: 7-8
- Input Gain: <5
- These are Marshall Jubilee settings — our app has a Jubilee voicing in AmpModel

---

## Part 5: Tone Mapping to Our App's Signal Chain

### Available Effects (relevant to Iron Butterfly / Fuzzrite tones)

We do NOT have a dedicated Fuzzrite effect. The closest options:

| Real Gear | Our Closest Effect | Match Quality | Notes |
|-----------|-------------------|---------------|-------|
| Mosrite Fuzzrite (Ge) | FuzzFace (idx 21) | **MODERATE** | Both are 2-transistor Ge fuzz, but VERY different topology |
| Mosrite Fuzzrite (Si) | FuzzFace (idx 21) | **LOW** | FuzzFace has no Depth blend control |
| Vox Super Beatle | AmpModel Twin (idx 5, voicing=7) | **MODERATE** | Twin is closest solid-state-like clean voicing |
| Vox Wah | Wah (idx 2) | **GOOD** | Generic wah approximation |
| Fender Reverb Tank | Reverb (idx 13) | **GOOD** | Spring reverb setting |
| Marshall Jubilee | AmpModel Jubilee (idx 5, voicing=5) | **EXCELLENT** | Direct match for Frusciante |

### Why FuzzFace is NOT a Good Fuzzrite Substitute

| Characteristic | Fuzz Face | Fuzzrite |
|---------------|-----------|----------|
| Topology | 2-transistor with feedback loop (Q2 collector -> Q1 base) | 2 cascaded CE stages, NO feedback between stages |
| Controls | Fuzz (gain), Volume | Depth (BLEND of stages), Volume |
| Depth behavior | More fuzz = more gain | More depth = more Q2 in mix (unique!) |
| Bass content | Very bassy, full | Thin, HPF at ~3.3kHz on fuzz signal |
| Pick attack | Smooth, compressed | Percussive, "floodgate" opening |
| Cleanup with volume | Excellent (Ge) | Different character — Q1 output with no blend |
| Germanium character | Warm, round, vocal | Buzzy, spitty, insect-like |

**Recommendation**: The Fuzzrite deserves its own dedicated effect slot if we want accurate Iron Butterfly or Frusciante-Fuzzrite presets.

### Fuzzrite Implementation Complexity

**EASY to implement** compared to Fuzz Face:
- No feedback path between transistors (no WDF decomposition needed)
- Two independent CE stages = straightforward cascaded gain with clipping
- Depth pot = simple crossfade between two outputs
- HPF is a simple first-order filter
- No guitar impedance interaction complexity (unlike Fuzz Face)

**Proposed DSP approach** (non-WDF, direct analog modeling):
1. Input coupling HPF: C1 (47nF) + R1 (1M) = 3.4Hz cutoff
2. Stage 1: Gain of ~158x (44dB), soft-clip via Ge transistor model (tanh with asymmetric bias)
3. Coupling HPF: C2 (2.2nF) into Depth blend
4. Stage 2: Same topology as Stage 1 but with lower-gain transistor characteristics
5. Coupling HPF: C3 (2.2nF) into Depth blend
6. Depth crossfade: dry=Q1 output (through C2), wet=Q2 output (through C3)
7. Output HPF: 22K + effective C from blend = ~3.3kHz (this shapes the buzz-saw character)
8. Volume pot: Simple attenuation through 33K audio pot
9. Output coupling

**Anti-aliasing**: 2x oversampling sufficient (Ge clipping is smooth, no hard discontinuities)

### Preset Proposals

#### Preset 1: "Psychedelic Fuzz" (In-A-Gadda-Da-Vida main riff)

Using FuzzFace as closest available substitute:

| Effect | Param | Value | Notes |
|--------|-------|-------|-------|
| NoiseGate (0) | threshold | 0.3 | Light gate for single-coil noise |
| FuzzFace (21) | fuzzDrive | 0.6 | Moderate fuzz, not maxed |
| FuzzFace (21) | volume | 0.7 | |
| FuzzFace (21) | guitarVolume | 0.8 | Slight rollback for definition |
| FuzzFace (21) | toneBalance | 0.3 | Roll off bass (Fuzzrite is thin) |
| AmpModel (5) | voicing | 7 (Twin) | Closest to solid-state Vox clean |
| AmpModel (5) | inputGain | 0.6 | Moderate — amp shouldn't add much dirt |
| AmpModel (5) | bass | 0.45 | Slightly scooped |
| AmpModel (5) | mid | 0.55 | Slight mid emphasis |
| AmpModel (5) | treble | 0.65 | Bright Vox character |
| AmpModel (5) | presence | 0.6 | |
| CabinetSim (6) | cabinetType | 7 (Alnico) | Celestion Alnico closest to Vox speakers |
| ParametricEQ (7) | midFreq | 3300 | Simulate Fuzzrite HPF character |
| ParametricEQ (7) | midGain | -3.0 | Slight cut below 3.3kHz relative |
| Reverb (13) | decay | 0.35 | Light spring reverb |
| Reverb (13) | mix | 0.25 | Subtle |
| Reverb (13) | damping | 0.5 | |

**Category**: HEAVY

#### Preset 2: "Frusciante Fuzzrite" (Stadium Arcadium tour / The Empyrean)

| Effect | Param | Value | Notes |
|--------|-------|-------|-------|
| Boost (3) | gain | 0.4 | Subtle DS-2-like push (DS-2 before Fuzzrite) |
| Boost (3) | mode | 0 | Clean boost mode |
| FuzzFace (21) | fuzzDrive | 0.35 | LOW — Frusciante ran Depth at 0 (Q1 only) |
| FuzzFace (21) | volume | 0.85 | Volume 8-10 on real unit |
| FuzzFace (21) | guitarVolume | 1.0 | Full guitar volume |
| FuzzFace (21) | toneBalance | 0.5 | Balanced (Depth=0 means no HPF emphasis) |
| AmpModel (5) | voicing | 5 (Jubilee) | Marshall Silver Jubilee |
| AmpModel (5) | inputGain | 0.5 | Input gain <5 |
| AmpModel (5) | bass | 0.3 | Bass at 3 |
| AmpModel (5) | mid | 0.3 | Middle at 3 |
| AmpModel (5) | treble | 0.75 | Treble at 7-8 |
| AmpModel (5) | presence | 0.45 | Presence at 4-5 |
| CabinetSim (6) | cabinetType | 4 (G12H-30) | Marshall 4x12 |
| Delay (12) | time | 0.35 | Light delay for space |
| Delay (12) | feedback | 0.2 | |
| Delay (12) | mix | 0.15 | Subtle |
| Reverb (13) | decay | 0.4 | Medium room |
| Reverb (13) | mix | 0.2 | |

**Category**: HEAVY

---

## Part 6: Fuzzrite as New Effect — Implementation Specification

If we decide to add a dedicated Fuzzrite effect (recommended for accuracy):

### Effect Index
Would be index **26** (next available slot)

### Parameters
| ID | Name | Range | Default | Notes |
|----|------|-------|---------|-------|
| 0 | depth | 0.0-1.0 | 0.5 | Blend between Q1 (0) and Q2 (1) outputs |
| 1 | volume | 0.0-1.0 | 0.7 | Output level |
| 2 | variant | 0-1 | 0 | 0=Germanium, 1=Silicon |
| 3 | hpfFreq | 1000-5000 | 3300 | The critical HPF frequency (22K * 2.2nF = 3289Hz) |

### DSP Architecture (C++ header-only)

```
Input -> InputCouplingHPF (3.4Hz) ->
  Stage1 {
    gain: 158x (44dB)
    clip: asymmetric_ge_tanh(x, Is=5e-6, Vt=26e-3) [Ge variant]
          symmetric_si_tanh(x) [Si variant]
    bias: Vc ~0.6V (starved)
  } ->
  CouplingCap1 (2.2nF model = HPF at ~720Hz with 100K load) ->

  [DEPTH CROSSFADE]
     dry path: Stage1 output (through coupling cap)
     wet path: Stage2 {
       gain: 100x (~40dB, lower gain Q2)
       clip: same model as Stage1 but with lower hFE
       bias: Vc ~0.6V
     } -> CouplingCap2 (2.2nF) ->

  OutputHPF (22K + 2.2nF = 3289Hz) ->
  VolumePot (33K audio taper model) ->
  Output
```

### Ge vs Si Differences in DSP
| Parameter | Germanium | Silicon |
|-----------|-----------|---------|
| Q1 gain | ~150 (hFE 100-150) | ~200 (hFE ~200) |
| Q2 gain | ~50 (hFE 40-60) | ~150 (hFE ~150) |
| Clipping character | Soft, asymmetric, temperature-dependent | Hard, symmetric, consistent |
| Leakage current | 250-500uA (Q1), <80uA (Q2) | Negligible |
| Bias point | Lower Vc due to leakage (more starved) | Higher Vc (less starved) |
| Pick attack | Smooth, touch-sensitive | Percussive, "floodgate" |
| Gating | Yes, especially at mid-Depth | Less pronounced |
| Polarity | PNP, positive ground | NPN, negative ground |

### CPU Budget Estimate
- 2x oversampling: ~2x base cost
- Per sample: 2 tanh (or fast_tanh) + 3 HPF biquads + 1 crossfade = ~30 FLOPS
- Total: ~60 FLOPS/sample at 2x OS
- Very lightweight — comparable to Rangemaster

---

## Part 7: Key Psychoacoustic Findings

### What Makes the Fuzzrite Sound Different from ALL Other Fuzzes

1. **The 3.3kHz HPF**: This is the single most distinctive element. No other classic fuzz has this aggressive bass rolloff on the fuzzed signal. It creates the "buzzy, insect-like" quality. Without modeling this HPF, any Fuzzrite emulation will sound like a generic fuzz.

2. **The Depth Blend (not Gain)**: The Fuzzrite's Depth control does NOT increase gain like a Fuzz Face's Fuzz knob. It CROSSFADES between two different distortion characters. This is unique among 1960s fuzzes and is the primary interaction control.

3. **No Feedback Path**: Unlike Fuzz Face (Q2->Q1 feedback) or Tone Bender (various feedback topologies), the Fuzzrite has NO inter-stage feedback. Each stage is independently biased. This makes the distortion character more "stiff" and less dynamically responsive than a Fuzz Face.

4. **Starved Biasing**: Both stages sitting at ~0.6V collector voltage means the transistors are operating in deep saturation with very little headroom. This contributes to the gated, sputtery quality at high Depth settings.

5. **Frusciante's Depth=0 Trick**: Using only Q1 (Depth at 0) essentially turns the Fuzzrite into a single-transistor overdrive/booster — similar in concept to a Rangemaster but without the treble-emphasis input cap. At Volume 8-10, Q1's output drives the amp into its own distortion.

### Tonal Signatures to Listen For (A/B Testing)

When comparing our emulation against reference recordings:
- **IAGDV main riff**: Listen for the "zzz" quality on sustained notes — that high-frequency buzz is the HPF at work
- **Note decay**: Fuzzrite has moderate sustain, NOT infinite. Notes should decay with a slight "gate" at the end
- **Pick attack**: Should be percussive, almost clicking, not smooth and compressed like a Fuzz Face
- **Low end**: Should be THIN compared to Fuzz Face or Big Muff. If your emulation sounds thick and bassy, the HPF is wrong
- **Depth at 0**: Should sound like a clean-ish, slightly hairy boost — not a full fuzz
- **Depth at 5**: Should have octave-up artifacts and pseudo ring-mod qualities
- **Depth at 10**: Full buzz-saw, thin and aggressive

---

## Sources

### Primary Research Sources
- [Fuzzboxes.org - Mosrite Fuzzrite Detailed History](https://fuzzboxes.org/features/fuzzrite)
- [Small Bear Electronics - Fur's Rite Clone Documentation](https://diy.smallbearelec.com/Projects/FursRite/FursRite.htm)
- [Homo Electromagneticus - Cloning and Modifying a Fuzzrite](https://homoelectromagneticus.com/projects/fuzzrite.php)
- [Tagboard Effects - Fuzzrite Germanium Layout](https://tagboardeffects.blogspot.com/2015/08/mosrite-fuzzrite-germanium.html)
- [Tagboard Effects - Fuzzrite Silicon Layout](https://tagboardeffects.blogspot.com/2010/02/mosrite-fuzzrite.html)
- [Aion FX - Orpheus Germanium Fuzz](https://aionfx.com/project/orpheus-germanium-fuzz/)
- [Aion FX - Orpheus Silicon Fuzz](https://aionfx.com/app/files/docs/orpheus_si_documentation.pdf)
- [PedalPCB - Starboard Fuzz Tutorial](https://forum.pedalpcb.com/threads/starboard-fuzz-mossrite-fuzzrite.10428/)
- [Vero P2P - Fuzzrite Silicon Version](https://vero-p2p.blogspot.com/2022/10/mosrite-fuzzrite-silicon-version.html)

### Amp & Recording Sources
- [Vox Showroom - Super Beatle Specs](https://www.voxshowroom.com/us/amp/beat_head.html)
- [Vox Showroom - Super Beatle Overview](https://www.voxshowroom.com/us/amp/beat.html)
- [Songfacts - In-A-Gadda-Da-Vida Recording](https://www.songfacts.com/facts/iron-butterfly/in-a-gadda-da-vida)
- [Wikipedia - In-A-Gadda-Da-Vida](https://en.wikipedia.org/wiki/In-A-Gadda-Da-Vida)
- [Wikipedia - Erik Brann](https://en.wikipedia.org/wiki/Erik_Brann)

### Gear Discussion Sources
- [Marshall Forum - Iron Butterfly Guitar Sound](https://marshallforum.com/threads/iron-butterfly-guitar-sound.81477/)
- [The Gear Page - In A Gadda Da Vida](https://www.thegearpage.net/board/index.php?threads/in-a-gadda-da-vida.356470/)
- [Seymour Duncan Forum - Iron Butterfly Fuzz Pedal](https://forum.seymourduncan.com/threads/fuzz-pedal-iron-butterfly.309679/)
- [TDPRI - Iron Butterfly It Must Be Love Fuzz Tone](https://www.tdpri.com/threads/iron-butterfly-it-must-be-love-fuzz-tone.771398/)

### Frusciante Fuzzrite Sources
- [JFX Pedals - Empyrean](https://jfxpedals.com/products/empyrean)
- [Frusrite - Frus Rite Deluxe](https://www.frusrite.com/p/frus-rite-deluxe.html)
- [Ground Guitar - Frusciante Stadium Arcadium Gear](https://www.groundguitar.com/tone-breakdown/john-frusciantes-gear-on-stadium-arcadium-guitars-amps-pedals-and-effects-explained/)
- [Catalinbread - Fuzzrite Germanium](https://catalinbread.com/products/fuzzrite-germanium)
- [Triungulo Lab - JF Fuzz](https://triungulolab.com/products/jf-fuzz-germanium-fuzzrite-reissue)
- [JF Effects Brazil - Mosrite Fuzzrite](http://www.jfeffects.com.br/2020/07/mosrite-fuzzrite-o-fuzz-da-turne-de.html)
