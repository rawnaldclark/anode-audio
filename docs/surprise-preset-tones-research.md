# Surprise Preset Tones Research
## 4 Iconic Guitar Tones for Factory Presets

Research date: 2026-03-21

---

## Table of Contents
1. [Tone A: Shoegaze Wall of Sound (Kevin Shields / MBV)](#tone-a-shoegaze-wall-of-sound)
2. [Tone B: Country Chicken Pickin' (Brad Paisley / Brian Setzer)](#tone-b-country-chicken-pickin)
3. [Tone C: Reggae Skank (Bob Marley "Stir It Up")](#tone-c-reggae-skank)
4. [Tone D: Post-Punk Dotted Delay (The Edge / U2 "Streets")](#tone-d-post-punk-dotted-delay)
5. [Implementation: Kotlin Preset Definitions](#implementation-kotlin-preset-definitions)

---

## Tone A: Shoegaze Wall of Sound

### Reference: Kevin Shields / My Bloody Valentine "Loveless" (1991)

### What Makes It Iconic
The MBV "Loveless" sound is the defining shoegaze tone -- a massive, enveloping
wall of guitar that blurs the line between melody and texture. Instantly
recognizable from the opening of "Only Shallow" or the dense wash of "To Here
Knows When." It defined an entire genre.

### Original Gear (Research Summary)
- **Guitar**: Fender Jazzmaster/Jaguar with tremolo arm (tape-wrapped for looseness)
- **Amps**: Marshall JCM800 cranked for natural tube saturation + Vox AC30 for chimey
  character. Maximum gain at maximum volume.
- **Key Pedals**:
  - Roger Mayer Axis Fuzz (silicon fuzz, not a standard Fuzz Face)
  - Yamaha SPX90 reverse reverb program (THE secret weapon)
  - Alesis Midiverb II (additional reverb layers)
  - Various modulation (chorus, vibrato from tremolo arm)
- **Technique**: "Glide guitar" -- constant tremolo arm manipulation while strumming
  fast 16th notes, bending pitch into a reverse reverb program. Tone knob rolled
  all the way down for a dull, dark base tone.

### Key Sonic Characteristics
1. **Massive saturation**: Fuzz into cranked amp -- not just distortion but
   COMPRESSION from the amp tubes being fully saturated
2. **Dense modulation**: Pitch instability from tremolo arm creates a natural
   chorus/vibrato effect; stacked with electronic chorus for extra width
3. **Reverse reverb wash**: The SPX90's reverse reverb creates swells that build
   INTO notes rather than decaying after them -- the signature MBV quality
4. **Dark, filtered tone**: Guitar tone knob at 0, creating a LPF around 800-1000Hz
   that removes pick attack harshness and creates a "vowel" quality
5. **Wall of feedback**: Multiple overdubbed guitars at high volume creating
   sympathetic resonance

### How to Replicate With Our Effects

Since we don't have a true "reverse reverb" mode, we approximate the dense wash
using heavy reverb with long pre-delay + chorus modulation + fuzz saturation.
The Big Muff is the closest to the shoegaze fuzz sound (EHX Big Muff Pi is
literally called "the shoegaze pedal").

**Signal chain (enabled effects):**
```
NoiseGate(off) -> Compressor(on, light) -> BigMuff(on, heavy sustain) ->
AmpModel(on, Crunch, low input gain) -> CabinetSim(on, British 4x12) ->
Chorus(on, slow/deep) -> Flanger(on, subtle) ->
Delay(on, medium, high feedback) -> Reverb(on, massive)
```

### Exact Parameter Settings

#### Compressor (index 1) -- Light sustain, not squash
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -20 dB | Gentle compression to sustain notes |
| Ratio | 1 | 3.0 | Low ratio -- we want dynamics within the wall |
| Attack | 2 | 20 ms | Let transients through for pick definition |
| Release | 3 | 200 ms | Medium release for smooth sustain |
| Makeup Gain | 4 | 4 dB | Compensate for compression |
| Knee Width | 5 | 8 dB | Soft knee for transparency |
| Character | 6 | 0 (VCA) | Clean compression, fuzz provides the color |

#### Big Muff Pi (index 23) -- THE shoegaze fuzz
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Sustain | 0 | 0.85 | High sustain for dense, singing fuzz |
| Tone | 1 | 0.3 | Rolled back -- Shields runs tone knob near zero. Dark, thick, woolly |
| Volume | 2 | 0.6 | Moderate volume, amp does the rest |
| Variant | 3 | 1 (Ram's Head) | Ram's Head has the most mid-scoop, classic for shoegaze |

*Why Ram's Head variant*: The Ram's Head Big Muff (1973-77) has a deeper mid-scoop
and more open, "swooshy" sustain compared to the NYC version. This is THE shoegaze
fuzz variant -- it creates that "wall of bees" texture when combined with reverb.

#### AmpModel (index 5) -- Cranked Marshall character
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Input Gain | 0 | 0.7 | Post-fuzz, moderate (fuzz already provides saturation) |
| Output Level | 1 | 0.6 | Leave headroom for reverb/delay buildup |
| Model Type | 2 | 1 (Crunch) | Marshall-style breakup character |
| Variac | 3 | 0.2 | Slight sag for extra compression/sustain |

*Why Crunch and not Plexi*: The BigMuff already provides massive gain. Crunch gives
medium saturation that adds tube warmth without turning everything into a square wave.
InputGain at 0.7 post-fuzz is safe per our gain staging audit (fuzz provides
saturation, amp adds character).

#### CabinetSim (index 6) -- British 4x12 for Marshall character
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Cabinet Type | 0 | 1 (4x12 British) | Marshall 4x12 cab character |
| Mix | 1 | 1.0 | Full cabinet simulation |

#### Chorus (index 8) -- Slow, deep, wide
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Rate | 0 | 0.4 Hz | Very slow -- creates gentle pitch undulation |
| Depth | 1 | 0.8 | Deep modulation for width and shimmer |
| Mode | 2 | 0 (Standard) | 3-voice for maximum width (not CE-1 single voice) |

*Why Standard, not CE-1*: CE-1 mode is a single-voice chorus with BBD warmth.
For shoegaze, we want the 3-voice Standard mode which creates a wider, more
diffuse modulation -- closer to the multi-layered guitar overdubs of Loveless.

#### Flanger (index 11) -- Subtle jet wash underneath
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Rate | 0 | 0.2 Hz | Very slow sweep |
| Depth | 1 | 0.3 | Subtle -- adds metallic shimmer without being obvious |
| Feedback | 2 | 0.4 | Moderate resonance for comb filter color |

#### Delay (index 12) -- Medium delay, high feedback for density
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Delay Time | 0 | 450 ms | Medium delay creates wash without distinct echoes |
| Feedback | 1 | 0.7 | High feedback -- echoes build into a dense cloud |

*Wet/dry: 0.5* -- Equal mix blurs boundary between direct and delayed sound.

#### Reverb (index 13) -- MASSIVE hall/plate
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Decay | 0 | 7.0 s | Very long tail -- creates the "wall" |
| Damping | 1 | 0.6 | Moderate damping -- dark but not mud |
| Pre-Delay | 2 | 40 ms | Some pre-delay separates attack from wash |
| Size | 3 | 0.9 | Near-maximum room size |

*Wet/dry: 0.6* -- Heavy reverb mix. The reverb IS the tone.

### Master Gains
- **inputGain**: 1.0 (unity -- BigMuff provides all the gain we need)
- **outputGain**: 0.7 (tame the buildup from reverb+delay feedback)

### Preset Name: "Loveless Wash"
### Category: AMBIENT

---

## Tone B: Country Chicken Pickin'

### Reference: Brad Paisley meets Brian Setzer -- Telecaster twang with slapback

### What Makes It Iconic
The snappy, percussive, twangy guitar sound that defines country and rockabilly.
Instantly recognizable from Brad Paisley's lightning-fast leads, Chet Atkins'
fingerpicking, or Brian Setzer's rockabilly riffs. The "chicken pickin'" technique
(hybrid picking with fingers snapping strings against frets) requires a specific
tone that emphasizes the pick attack "snap" while keeping the body clean and warm.

### Original Gear (Research Summary)

**Brad Paisley's Rig:**
- **Guitar**: Fender Telecaster (bridge pickup for snap, neck for warmth)
- **Amp**: Dr. Z Maz 18 / Trainwreck -- pushed to edge of breakup
- **Compressor**: Wampler Ego Compressor (but Paisley prefers natural amp compression)
- **Delay**: Boss DD-3 slapback (125-175ms)
- **Key insight**: Paisley does NOT use heavy compression. He relies on the natural
  compression from a tube amp pushed to the edge of breakup.
- **Amp settings**: Gain 3, Bass 5, Mids 9, Treble 7, Reverb 2

**Brian Setzer's Rig:**
- **Guitar**: 1959 Gretsch 6120 with TV Jones TV Classic pickups
- **Amp**: 1963 Fender Bassman pushed to edge of clean headroom
- **Delay**: Roland RE-301 Space Echo, slapback 60-120ms, single repeat
- **Reverb**: Minimal or off -- slapback provides all the ambience

### Key Sonic Characteristics
1. **Clean-to-edge-of-breakup amp**: NOT pristine clean, but right at the point
   where digging in causes slight tube compression. This is the "living" quality.
2. **Slapback delay**: 80-150ms, single or near-single repeat. Creates the
   classic rockabilly "doubling" effect that makes single notes sound huge.
3. **Pick attack preserved**: Compression (if any) must have SLOW attack to let
   the "snap" and "cluck" of hybrid picking come through.
4. **Mid-forward EQ**: Boosted mids (around 1-3kHz) for cut and presence.
   The "twang" lives in the upper mids (2-4kHz).
5. **Minimal reverb**: Unlike surf or ambient styles, country/rockabilly is DRY.
   Slapback delay provides the sense of space.

### How to Replicate With Our Effects

**Signal chain (enabled effects):**
```
NoiseGate(on, light) -> Compressor(on, slow attack OTA) ->
AmpModel(on, Twin Reverb, edge of breakup) -> CabinetSim(on, Jensen C10Q) ->
ParametricEQ(on, mid boost) -> Delay(on, slapback) -> Reverb(on, tiny)
```

### Exact Parameter Settings

#### NoiseGate (index 0) -- Light gate, country is quiet
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -50 dB | Very light -- don't choke pick ghost notes |
| Hysteresis | 1 | 8 dB | Wide hysteresis to avoid flutter on quiet passages |
| Attack | 2 | 0.5 ms | Fast open to catch every pick transient |
| Release | 3 | 80 ms | Quick release for staccato playing |

#### Compressor (index 1) -- Slow attack to preserve snap
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -15 dB | Moderate threshold |
| Ratio | 1 | 4.0 | Medium compression -- sustain without squash |
| Attack | 2 | 30 ms | SLOW attack -- this is THE key setting. Lets the pick transient "snap" through before compression kicks in |
| Release | 3 | 150 ms | Medium release for note-to-note tracking |
| Makeup Gain | 4 | 3 dB | Light compensation |
| Knee Width | 5 | 6 dB | Soft knee for natural feel |
| Character | 6 | 1 (OTA) | OTA/Dyna Comp character adds slight color and treble emphasis |

*Why OTA character*: The MXR Dyna Comp is a classic country compressor. The OTA
character's treble-emphasis sidechain responds more to pick attack frequencies,
and the slight even-harmonic coloration adds warmth. Brad Paisley used the
Wampler Ego but the Dyna Comp character is more universally "country."

#### AmpModel (index 5) -- Fender Twin clean, pushed
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Input Gain | 0 | 0.9 | Pushed to edge of breakup -- dig in and it crunches slightly |
| Output Level | 1 | 0.7 | Full clean volume |
| Model Type | 2 | 7 (Twin Reverb) | Fender Twin = THE country amp. Maximum clean headroom, scooped mids, deep bass |
| Variac | 3 | 0.0 | No sag -- country needs tight, immediate response |

*Why Twin Reverb*: Brad Paisley uses boutique amps with Fender lineage. Brian Setzer
uses a Fender Bassman. The Fender Twin Reverb (6L6GC, scooped mids, massive headroom)
is the archetypal country amp -- clean, loud, and authoritative. Our Twin voicing
models exactly this.

#### CabinetSim (index 6) -- Jensen C10Q for vintage Fender character
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Cabinet Type | 0 | 9 (Jensen C10Q) | Classic Fender speaker -- papery, bright, snappy |
| Mix | 1 | 1.0 | Full cab sim |

*Why Jensen C10Q*: The Jensen C10Q (or similar alnico speakers) is what came stock
in vintage Fender amps. It has a papery, slightly compressed midrange breakup and
bright, snappy treble response that defines the Fender country sound. Way better
fit than any British Celestion for this tone.

#### ParametricEQ (index 7) -- Mid-forward "twang" boost
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Low Freq | 0 | 100 Hz | Low shelf frequency |
| Low Gain | 1 | -2 dB | Slight bass cut -- keep it tight for chicken pickin' |
| Mid Freq | 2 | 2500 Hz | The "twang" and "snap" frequency |
| Mid Gain | 3 | 3 dB | Boost for presence and cut |
| Mid Q | 4 | 1.5 | Moderate Q -- broad enough to sound natural |
| High Freq | 5 | 6000 Hz | Treble shelf |
| High Gain | 6 | 2 dB | Slight treble lift for sparkle |

#### Delay (index 12) -- Classic slapback
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Delay Time | 0 | 120 ms | Classic slapback range (80-150ms). 120ms is right in the sweet spot |
| Feedback | 1 | 0.05 | Near-zero feedback -- single repeat only |

*Wet/dry: 0.3* -- Slapback should be audible but not dominant. 30% wet gives the
"doubling" effect without muddying the attack.

#### Reverb (index 13) -- Tiny room, barely there
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Decay | 0 | 0.8 s | Very short -- just a hint of space |
| Damping | 1 | 0.3 | Bright reverb (country rooms are lively) |
| Pre-Delay | 2 | 5 ms | Minimal pre-delay |
| Size | 3 | 0.2 | Small room |

*Wet/dry: 0.15* -- Barely perceptible. The slapback delay is the main ambience;
reverb just adds air.

### Master Gains
- **inputGain**: 1.0 (unity -- let the amp model do the work)
- **outputGain**: 1.0 (clean tones don't need output taming)

### Preset Name: "Chicken Pickin'"
### Category: CLEAN

---

## Tone C: Reggae Skank

### Reference: Bob Marley & The Wailers "Stir It Up" / "Three Little Birds"

### What Makes It Iconic
The reggae guitar "skank" is one of the most recognizable guitar sounds in music --
sharp, bright, staccato chord stabs on the offbeat (beats 2 and 4, or the "and"
of every beat). From Bob Marley's Wailers to Steel Pulse to Sublime, this clean,
percussive guitar style is the rhythmic backbone of reggae.

### Original Gear (Research Summary)
- **Guitar**: Bob Marley used a Gibson Les Paul Special with P-90 pickups.
  Al Anderson (Wailers lead) also used Strats and various guitars.
- **Amp**: Fender Twin Reverb -- clean, loud, with built-in spring reverb
- **Effects**: Minimal -- clean amp with spring reverb is the whole chain
- **Studio processing**: HPF to cut bass below ~100Hz, keeping the guitar
  out of the bass guitar/kick drum territory. Slight compression for consistency.
- **Technique**: "The skank" -- downstroke staccato chords, immediate fretting-hand
  muting after each strum. Off-beat timing. Minimal sustain.

### Key Sonic Characteristics
1. **Pristine clean tone**: Zero distortion. The guitar must be percussive and
   transparent, like a rhythmic click with pitch.
2. **Bright, present top end**: Treble is boosted, bass is cut. The guitar sits
   ABOVE the bass and drums in frequency, acting as a hi-hat equivalent.
3. **Staccato/short notes**: Muted immediately after strumming. The "chop" quality.
   Compression helps maintain consistent volume across these short bursts.
4. **Spring reverb**: A touch of spring reverb (from the Fender Twin) adds warmth
   and depth without washing out the staccato attack. In dub reggae, the reverb
   is cranked much higher.
5. **Mid-scoop with treble emphasis**: EQ-wise, the guitar is bright and thin
   on purpose -- it needs to cut through a dense mix of bass, drums, keyboards.
6. **HPF around 100-200Hz**: Bass frequencies are aggressively filtered out.

### How to Replicate With Our Effects

**Signal chain (enabled effects):**
```
NoiseGate(on, moderate) -> Compressor(on, fast, consistent) ->
AmpModel(on, Twin Reverb, clean) -> CabinetSim(on, Jensen C10Q) ->
ParametricEQ(on, bass cut, treble boost) -> Reverb(on, spring character)
```

### Exact Parameter Settings

#### NoiseGate (index 0) -- Clean up between staccato chops
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -45 dB | Moderate -- cleans up between chord stabs |
| Hysteresis | 1 | 6 dB | Standard |
| Attack | 2 | 0.5 ms | Fast -- don't choke the staccato attack |
| Release | 3 | 40 ms | Fast release -- staccato needs quick muting |

#### Compressor (index 1) -- Consistent stab volume
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -18 dB | Moderate compression |
| Ratio | 1 | 5.0 | Medium-high ratio for consistent chord volume |
| Attack | 2 | 5 ms | Fast attack -- we want to TAME the transient for consistency (unlike country where we preserve it) |
| Release | 3 | 80 ms | Fast release -- chords are very short |
| Makeup Gain | 4 | 4 dB | Compensate for squashed dynamics |
| Knee Width | 5 | 4 dB | Harder knee for more obvious compression |
| Character | 6 | 0 (VCA) | Clean VCA -- no coloration, reggae needs transparency |

*Why fast attack (unlike country)*: In reggae, every chord stab should be the SAME
volume. We don't want pick dynamics -- we want machine-like consistency. Fast attack
clamps down on every stab equally, creating the "typewriter" quality of reggae rhythm.

#### AmpModel (index 5) -- Fender Twin, dead clean
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Input Gain | 0 | 0.3 | Very low gain -- absolutely no breakup |
| Output Level | 1 | 0.65 | Moderate output |
| Model Type | 2 | 7 (Twin Reverb) | Bob Marley's actual amp. Clean, loud, bright |
| Variac | 3 | 0.0 | No sag -- clean and immediate |

*Why input gain 0.3*: Reggae demands pristine clean. Even 0.5 on the Twin can
introduce slight compression/saturation. At 0.3, the signal stays well below the
clipping threshold of even the first triode stage.

#### CabinetSim (index 6) -- Jensen for Fender character
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Cabinet Type | 0 | 9 (Jensen C10Q) | Fender Twin speaker -- bright, clean |
| Mix | 1 | 1.0 | Full cab sim |

#### ParametricEQ (index 7) -- Bass cut, treble boost, mid scoop
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Low Freq | 0 | 150 Hz | HPF point -- cut everything below to stay out of bass guitar territory |
| Low Gain | 1 | -8 dB | Aggressive bass cut |
| Mid Freq | 2 | 800 Hz | Low-mid area |
| Mid Gain | 3 | -3 dB | Slight mid scoop for brightness |
| Mid Q | 4 | 1.0 | Broad scoop |
| High Freq | 5 | 4000 Hz | Presence/treble |
| High Gain | 6 | 4 dB | Boost treble for brightness and snap |

*Why aggressive bass cut*: In a reggae mix, the bass guitar and kick drum own
everything below 200Hz. The guitar must be bright and thin to sit in its own
frequency pocket. This EQ mimics the console HPF that every reggae engineer
applies to the rhythm guitar channel.

#### Reverb (index 13) -- Spring reverb character
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Decay | 0 | 1.5 s | Short-medium -- spring reverb character |
| Damping | 1 | 0.3 | Bright, splashy (spring reverbs are bright) |
| Pre-Delay | 2 | 8 ms | Short pre-delay |
| Size | 3 | 0.3 | Small -- spring tanks are physically small |

*Wet/dry: 0.25* -- A TOUCH of spring reverb. Enough to add depth to the staccato
chords without washing them out. In dub reggae (Lee "Scratch" Perry, King Tubby),
this would be cranked to 0.6+, but for classic roots reggae, keep it subtle.

*Note on spring reverb approximation*: Our Dattorro plate reverb is not a spring
reverb model. However, with short decay (1.5s), bright damping (0.3), and small
size (0.3), it approximates the quick, splashy character of a Fender spring tank.
A true spring would have the characteristic "drip" and "boing" from the spring's
mechanical resonances, but this gets us in the ballpark.

### Master Gains
- **inputGain**: 0.9 (slightly below unity -- keep it clean)
- **outputGain**: 1.0 (clean tone, no buildup to tame)

### Preset Name: "Reggae Skank"
### Category: CLEAN

---

## Tone D: Post-Punk Dotted Delay

### Reference: The Edge / U2 "Where The Streets Have No Name" (1987)

### What Makes It Iconic
The Edge's use of rhythmic delay is arguably the most influential guitar technique
of the 1980s. The "dotted eighth note delay" creates a hypnotic, cascading pattern
where the delays interlock with the picking pattern to create a rhythm far more
complex than what the guitarist is actually playing. "Where the Streets Have No Name"
is the quintessential example -- a simple 16th-note picked arpeggio that becomes
a shimmering, complex tapestry through delay.

### Original Gear (Research Summary)
- **Guitar**: Fender Stratocaster (bridge pickup) or Gibson Explorer
- **Amp**: 1964 Vox AC30 Top Boost with Celestion Alnico Blue speakers.
  Settings kept constant; plugged into Low Gain, Brilliant input.
  Normal ~12:30, Brilliant ~10:30, Treble ~11:00, Bass ~1:00, Cut ~8:00
- **Delay**: Korg SDD-3000 digital delay (THE defining piece of gear)
  - Primary delay: 359-365ms (dotted eighth at ~126 BPM)
  - Secondary delay: 120ms (slapback, adds thickness)
  - Modulation: Rate 5Hz, subtle depth (adds warmth to repeats)
  - Feedback: 4 strong repeats, decaying
  - The SDD-3000's preamp adds ~10dB of gain, pushing the AC30 harder
- **Compressor**: MXR Dyna Comp (used occasionally, especially with slide)
- **Modulation**: The Korg SDD-3000's built-in modulation adds chorus-like
  movement to the delay repeats. Additional chorus/modulation used on some songs.

### Key Sonic Characteristics
1. **Dotted eighth delay**: At 126 BPM, a dotted eighth = 3/16 of a bar.
   In milliseconds: (60000 / 126) * 0.75 = ~357ms. This creates a pattern
   where the delay falls between the played notes, making a simple rhythm
   sound intricate and cascading.
2. **Clean-to-slightly-crunchy amp**: The AC30 Top Boost is set for chimey
   clean with just a hint of breakup when played hard. Not distorted, but
   not sterile either.
3. **Bright, jangly tone**: The AC30's Top Boost circuit (built-in treble
   booster) adds sparkle. Bridge pickup Strat into AC30 = maximum jangle.
4. **Delay modulation**: The subtle pitch modulation on the delay repeats
   (from the SDD-3000) adds warmth and prevents the delays from sounding
   sterile or "digital."
5. **Compression for evenness**: Light compression ensures every picked note
   feeds the delay evenly. Uneven dynamics would make the delay pattern
   rhythmically uneven.

### Delay Time Calculation
```
BPM = 126
Quarter note = 60000 / 126 = 476.2 ms
Dotted eighth = Quarter * 0.75 = 357.1 ms
```
We'll use **360ms** (the commonly documented setting, and practically identical
to the calculated 357ms -- the 3ms difference is inaudible).

### How to Replicate With Our Effects

**Signal chain (enabled effects):**
```
NoiseGate(on, light) -> Compressor(on, even dynamics) ->
AmpModel(on, AC30, chimey breakup) -> CabinetSim(on, Alnico Blue) ->
Chorus(on, subtle warmth) -> Delay(on, dotted eighth, high feedback) ->
Reverb(on, subtle ambience)
```

### Exact Parameter Settings

#### NoiseGate (index 0) -- Light cleanup
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -48 dB | Very light -- don't choke quiet notes |
| Hysteresis | 1 | 6 dB | Standard |
| Attack | 2 | 0.5 ms | Fast |
| Release | 3 | 60 ms | Medium -- let delay tails ring |

#### Compressor (index 1) -- Even dynamics for consistent delay feed
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Threshold | 0 | -20 dB | Moderate |
| Ratio | 1 | 4.0 | Medium compression |
| Attack | 2 | 8 ms | Medium-fast -- slight transient preservation but mostly even |
| Release | 3 | 120 ms | Medium release |
| Makeup Gain | 4 | 3 dB | Light compensation |
| Knee Width | 5 | 6 dB | Soft knee |
| Character | 6 | 1 (OTA) | OTA/Dyna Comp -- The Edge actually uses the MXR Dyna Comp |

*Why OTA character*: The Edge literally uses an MXR Dyna Comp. The OTA character
models the CA3080 OTA gain element with faster attack response and treble-emphasis
sidechain. This is historically accurate.

#### AmpModel (index 5) -- Vox AC30 Top Boost
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Input Gain | 0 | 0.8 | Pushed but not overdriven -- chimey breakup when picking hard |
| Output Level | 1 | 0.6 | Leave headroom for delay buildup |
| Model Type | 2 | 6 (AC30) | The Edge's ACTUAL amp. Chimey, bright, jangly, EL84 breakup |
| Variac | 3 | 0.0 | No sag -- AC30s have immediate response |

*Why inputGain 0.8*: The AC30 voicing is our brightest, most chimey model. At 0.8,
it's in the "edge of breakup" territory where picking dynamics control whether you
get clean jangle or slight crunch. This is exactly the Edge's sweet spot.

#### CabinetSim (index 6) -- Alnico Blue (AC30's stock speaker)
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Cabinet Type | 0 | 8 (Alnico Blue) | Celestion Blue is THE AC30 speaker. Bright, complex, chimey |
| Mix | 1 | 1.0 | Full cab sim |

*Why Alnico Blue*: The Edge's 1964 AC30 came with Celestion Alnico Blue speakers.
These have a pronounced upper-midrange presence peak around 2-3kHz and a complex,
breaking-up quality that defines the "chimey" AC30 sound. Our Alnico Blue IR
is the historically correct match.

#### Chorus (index 8) -- Subtle modulation mimicking SDD-3000
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Rate | 0 | 1.5 Hz | Moderate speed -- mimics the SDD-3000's modulation |
| Depth | 1 | 0.25 | Subtle -- just adds shimmer and warmth to the signal |
| Mode | 2 | 1 (CE-1) | CE-1 single-voice for subtle warmth (not 3-voice shimmer) |

*Why CE-1 mode*: The Edge's modulation comes from the SDD-3000's built-in modulation
on the delay repeats, which is a single-path modulation (not multi-voice). The CE-1
mode's single-voice triangle LFO with BBD warmth more closely approximates this than
the 3-voice standard chorus. We're using it at low depth as a "thickener."

*Wet/dry: 0.35* -- Subtle chorus, not dominant.

#### Delay (index 12) -- THE defining effect: dotted eighth
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Delay Time | 0 | 360 ms | Dotted eighth at 126 BPM. The documented setting from the "Play Guitar with U2" book |
| Feedback | 1 | 0.55 | 4-5 audible repeats, decaying. High enough to create the cascading rhythm, low enough that repeats don't build up into mush |

*Wet/dry: 0.45* -- The delay is prominent but the dry signal must still be clearly
defined. At 0.45, the rhythmic interplay between dry and delayed notes is clear.

*Why feedback 0.55*: At 360ms delay with 0.55 feedback, each repeat is ~5dB quieter
than the previous. This gives roughly 4 clearly audible repeats before they fade
below the noise floor. This matches The Edge's documented preference for "4 strong
repeats."

Calculating: 20*log10(0.55) = -5.2dB per repeat
- Repeat 1: -5.2 dB
- Repeat 2: -10.4 dB
- Repeat 3: -15.6 dB
- Repeat 4: -20.8 dB (still audible)
- Repeat 5: -26.0 dB (barely perceptible)

#### Reverb (index 13) -- Subtle ambience, not wash
| Param | ID | Value | Rationale |
|-------|------|-------|-----------|
| Decay | 0 | 2.0 s | Medium decay |
| Damping | 1 | 0.4 | Moderate -- bright enough to complement the AC30 jangle |
| Pre-Delay | 2 | 25 ms | Some separation from dry signal |
| Size | 3 | 0.5 | Medium room |

*Wet/dry: 0.2* -- Very subtle. The delay is the star; reverb just adds air and
prevents the sound from feeling "small." Too much reverb would smear the
precisely-timed delay pattern.

### Master Gains
- **inputGain**: 1.0 (unity)
- **outputGain**: 0.8 (slight reduction to prevent delay feedback buildup from clipping)

### Preset Name: "Dotted Delay"
### Category: AMBIENT

---

## Implementation: Kotlin Preset Definitions

Below are the complete preset definitions ready to add to `FactoryPresets.kt`.

### Tone A: "Loveless Wash"

```kotlin
/**
 * Factory Preset: "Loveless Wash"
 * Shoegaze wall-of-sound inspired by Kevin Shields / My Bloody Valentine.
 * Big Muff fuzz -> cranked Crunch amp -> dense chorus + flanger + delay + massive reverb.
 * The Big Muff (Ram's Head variant) with tone rolled back creates the signature
 * woolly, mid-scooped sustain, while stacked modulation and long reverb/delay
 * create the "wall of sound" texture.
 */
private fun createLovelessWash(): Preset {
    val enabled = mapOf(
        0 to false,  // NoiseGate -- off (fuzz noise IS the texture)
        1 to true,   // Compressor
        2 to false,  // Wah
        3 to false,  // Boost
        4 to false,  // Overdrive
        5 to true,   // AmpModel (Crunch)
        6 to true,   // CabinetSim (4x12 British)
        7 to false,  // ParametricEQ
        8 to true,   // Chorus (slow, deep, 3-voice)
        9 to false,  // Vibrato
        10 to false, // Phaser
        11 to true,  // Flanger (subtle wash)
        12 to true,  // Delay (medium, high feedback)
        13 to true,  // Reverb (massive)
        14 to true,  // Tuner
        15 to false, // Tremolo
        16 to false, // Boss DS-1
        17 to false, // ProCo RAT
        18 to false, // Boss DS-2
        19 to false, // Boss HM-2
        20 to false, // Univibe
        21 to false, // Fuzz Face
        22 to false, // Rangemaster
        23 to true,  // Big Muff Pi (Ram's Head)
        24 to false, // Octavia
        25 to false  // Rotary Speaker
    )

    val params = mapOf(
        // Compressor: light sustain
        Pair(1, 0) to -20f,   // threshold
        Pair(1, 1) to 3f,     // ratio
        Pair(1, 2) to 20f,    // attack
        Pair(1, 3) to 200f,   // release
        Pair(1, 4) to 4f,     // makeupGain
        Pair(1, 5) to 8f,     // kneeWidth
        Pair(1, 6) to 0f,     // character (VCA)
        // Big Muff Pi: heavy sustain, dark tone, Ram's Head variant
        Pair(23, 0) to 0.85f, // sustain (high)
        Pair(23, 1) to 0.3f,  // tone (dark -- Shields runs tone near zero)
        Pair(23, 2) to 0.6f,  // volume
        Pair(23, 3) to 1f,    // variant (Ram's Head)
        // AmpModel: Crunch, moderate post-fuzz gain
        Pair(5, 0) to 0.7f,   // inputGain (post-fuzz safe range)
        Pair(5, 1) to 0.6f,   // outputLevel
        Pair(5, 2) to 1f,     // modelType (Crunch)
        Pair(5, 3) to 0.2f,   // variac (slight sag)
        // CabinetSim: British 4x12 (Marshall character)
        Pair(6, 0) to 1f,     // cabinetType (4x12 British)
        Pair(6, 1) to 1.0f,   // mix
        // Chorus: slow, deep, 3-voice
        Pair(8, 0) to 0.4f,   // rate (Hz)
        Pair(8, 1) to 0.8f,   // depth
        Pair(8, 2) to 0f,     // mode (Standard 3-voice)
        // Flanger: subtle jet wash
        Pair(11, 0) to 0.2f,  // rate (Hz)
        Pair(11, 1) to 0.3f,  // depth
        Pair(11, 2) to 0.4f,  // feedback
        // Delay: medium time, high feedback for density
        Pair(12, 0) to 450f,  // delayTimeMs
        Pair(12, 1) to 0.7f,  // feedback
        // Reverb: massive hall
        Pair(13, 0) to 7.0f,  // decay (seconds)
        Pair(13, 1) to 0.6f,  // damping
        Pair(13, 2) to 40f,   // preDelay (ms)
        Pair(13, 3) to 0.9f   // size
    )

    val wetDry = mapOf(
        8 to 0.5f,   // Chorus: 50/50 for wide modulation
        11 to 0.25f, // Flanger: subtle underneath
        12 to 0.5f,  // Delay: equal mix blurs dry/wet boundary
        13 to 0.6f   // Reverb: heavy -- reverb IS the tone
    )

    return Preset(
        id = "factory-loveless-wash",
        name = "Loveless Wash",
        author = "Factory",
        category = PresetCategory.AMBIENT,
        effects = buildEffectChain(enabled, params, wetDry),
        isFactory = true,
        inputGain = 1.0f,
        outputGain = 0.7f
    )
}
```

### Tone B: "Chicken Pickin'"

```kotlin
/**
 * Factory Preset: "Chicken Pickin'"
 * Country/rockabilly tone inspired by Brad Paisley and Brian Setzer.
 * Clean Fender Twin at edge of breakup + OTA compression with slow attack
 * (preserves pick snap) + classic 120ms slapback delay.
 * Jensen C10Q speaker captures the papery Fender twang.
 */
private fun createChickenPickin(): Preset {
    val enabled = mapOf(
        0 to true,   // NoiseGate (light)
        1 to true,   // Compressor (OTA, slow attack for snap)
        2 to false,  // Wah
        3 to false,  // Boost
        4 to false,  // Overdrive
        5 to true,   // AmpModel (Twin Reverb)
        6 to true,   // CabinetSim (Jensen C10Q)
        7 to true,   // ParametricEQ (mid boost for twang)
        8 to false,  // Chorus
        9 to false,  // Vibrato
        10 to false, // Phaser
        11 to false, // Flanger
        12 to true,  // Delay (slapback)
        13 to true,  // Reverb (tiny room)
        14 to true,  // Tuner
        15 to false, // Tremolo
        16 to false, // Boss DS-1
        17 to false, // ProCo RAT
        18 to false, // Boss DS-2
        19 to false, // Boss HM-2
        20 to false, // Univibe
        21 to false, // Fuzz Face
        22 to false, // Rangemaster
        23 to false, // Big Muff Pi
        24 to false, // Octavia
        25 to false  // Rotary Speaker
    )

    val params = mapOf(
        // NoiseGate: very light
        Pair(0, 0) to -50f,   // threshold (low)
        Pair(0, 1) to 8f,     // hysteresis
        Pair(0, 2) to 0.5f,   // attack (fast)
        Pair(0, 3) to 80f,    // release
        // Compressor: OTA character, SLOW attack for pick snap
        Pair(1, 0) to -15f,   // threshold
        Pair(1, 1) to 4f,     // ratio
        Pair(1, 2) to 30f,    // attack (SLOW -- lets transient through)
        Pair(1, 3) to 150f,   // release
        Pair(1, 4) to 3f,     // makeupGain
        Pair(1, 5) to 6f,     // kneeWidth
        Pair(1, 6) to 1f,     // character (OTA/Dyna Comp)
        // AmpModel: Fender Twin Reverb, edge of breakup
        Pair(5, 0) to 0.9f,   // inputGain (pushed to edge)
        Pair(5, 1) to 0.7f,   // outputLevel
        Pair(5, 2) to 7f,     // modelType (Twin Reverb)
        Pair(5, 3) to 0f,     // variac (off)
        // CabinetSim: Jensen C10Q
        Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
        Pair(6, 1) to 1.0f,   // mix
        // ParametricEQ: mid-forward twang
        Pair(7, 0) to 100f,   // lowFreq
        Pair(7, 1) to -2f,    // lowGain (slight bass cut)
        Pair(7, 2) to 2500f,  // midFreq (twang frequency)
        Pair(7, 3) to 3f,     // midGain (boost)
        Pair(7, 4) to 1.5f,   // midQ
        Pair(7, 5) to 6000f,  // highFreq
        Pair(7, 6) to 2f,     // highGain (sparkle)
        // Delay: classic slapback
        Pair(12, 0) to 120f,  // delayTimeMs (slapback sweet spot)
        Pair(12, 1) to 0.05f, // feedback (single repeat)
        // Reverb: tiny room
        Pair(13, 0) to 0.8f,  // decay
        Pair(13, 1) to 0.3f,  // damping
        Pair(13, 2) to 5f,    // preDelay
        Pair(13, 3) to 0.2f   // size
    )

    val wetDry = mapOf(
        12 to 0.3f,  // Delay: slapback audible but not dominant
        13 to 0.15f  // Reverb: barely there
    )

    return Preset(
        id = "factory-chicken-pickin",
        name = "Chicken Pickin'",
        author = "Factory",
        category = PresetCategory.CLEAN,
        effects = buildEffectChain(enabled, params, wetDry),
        isFactory = true,
        inputGain = 1.0f,
        outputGain = 1.0f
    )
}
```

### Tone C: "Reggae Skank"

```kotlin
/**
 * Factory Preset: "Reggae Skank"
 * Classic roots reggae rhythm guitar inspired by Bob Marley & The Wailers.
 * Dead-clean Fender Twin + fast compression for consistent chord stabs +
 * aggressive EQ (bass cut, treble boost) to sit above the bass/drums +
 * touch of bright reverb for depth.
 */
private fun createReggaeSkank(): Preset {
    val enabled = mapOf(
        0 to true,   // NoiseGate (clean between stabs)
        1 to true,   // Compressor (fast, consistent)
        2 to false,  // Wah
        3 to false,  // Boost
        4 to false,  // Overdrive
        5 to true,   // AmpModel (Twin Reverb, pristine clean)
        6 to true,   // CabinetSim (Jensen C10Q)
        7 to true,   // ParametricEQ (bass cut, treble boost)
        8 to false,  // Chorus
        9 to false,  // Vibrato
        10 to false, // Phaser
        11 to false, // Flanger
        12 to false, // Delay
        13 to true,  // Reverb (spring character)
        14 to true,  // Tuner
        15 to false, // Tremolo
        16 to false, // Boss DS-1
        17 to false, // ProCo RAT
        18 to false, // Boss DS-2
        19 to false, // Boss HM-2
        20 to false, // Univibe
        21 to false, // Fuzz Face
        22 to false, // Rangemaster
        23 to false, // Big Muff Pi
        24 to false, // Octavia
        25 to false  // Rotary Speaker
    )

    val params = mapOf(
        // NoiseGate: cleans up between staccato chords
        Pair(0, 0) to -45f,   // threshold
        Pair(0, 1) to 6f,     // hysteresis
        Pair(0, 2) to 0.5f,   // attack
        Pair(0, 3) to 40f,    // release (fast for staccato)
        // Compressor: fast attack for consistent volume
        Pair(1, 0) to -18f,   // threshold
        Pair(1, 1) to 5f,     // ratio (medium-high)
        Pair(1, 2) to 5f,     // attack (FAST -- tame every stab equally)
        Pair(1, 3) to 80f,    // release (fast for short chords)
        Pair(1, 4) to 4f,     // makeupGain
        Pair(1, 5) to 4f,     // kneeWidth (harder knee)
        Pair(1, 6) to 0f,     // character (VCA -- transparent)
        // AmpModel: Fender Twin, dead clean
        Pair(5, 0) to 0.3f,   // inputGain (very low -- pristine clean)
        Pair(5, 1) to 0.65f,  // outputLevel
        Pair(5, 2) to 7f,     // modelType (Twin Reverb)
        Pair(5, 3) to 0f,     // variac (off)
        // CabinetSim: Jensen C10Q
        Pair(6, 0) to 9f,     // cabinetType (Jensen C10Q)
        Pair(6, 1) to 1.0f,   // mix
        // ParametricEQ: bass cut, mid scoop, treble boost
        Pair(7, 0) to 150f,   // lowFreq
        Pair(7, 1) to -8f,    // lowGain (aggressive bass cut)
        Pair(7, 2) to 800f,   // midFreq
        Pair(7, 3) to -3f,    // midGain (slight scoop)
        Pair(7, 4) to 1.0f,   // midQ
        Pair(7, 5) to 4000f,  // highFreq
        Pair(7, 6) to 4f,     // highGain (treble boost for brightness)
        // Reverb: spring character (short, bright, splashy)
        Pair(13, 0) to 1.5f,  // decay
        Pair(13, 1) to 0.3f,  // damping (bright)
        Pair(13, 2) to 8f,    // preDelay
        Pair(13, 3) to 0.3f   // size (small -- spring tank)
    )

    val wetDry = mapOf(
        13 to 0.25f  // Reverb: touch of spring
    )

    return Preset(
        id = "factory-reggae-skank",
        name = "Reggae Skank",
        author = "Factory",
        category = PresetCategory.CLEAN,
        effects = buildEffectChain(enabled, params, wetDry),
        isFactory = true,
        inputGain = 0.9f,
        outputGain = 1.0f
    )
}
```

### Tone D: "Dotted Delay"

```kotlin
/**
 * Factory Preset: "Dotted Delay"
 * Post-punk rhythmic delay inspired by The Edge / U2 "Where The Streets Have No Name."
 * Vox AC30 at chimey breakup + OTA compression (Dyna Comp) + CE-1 chorus for
 * subtle warmth + 360ms dotted eighth delay with ~4 repeats + Alnico Blue cab.
 * The delay creates complex rhythmic patterns from simple picked arpeggios.
 */
private fun createDottedDelay(): Preset {
    val enabled = mapOf(
        0 to true,   // NoiseGate (light)
        1 to true,   // Compressor (OTA/Dyna Comp)
        2 to false,  // Wah
        3 to false,  // Boost
        4 to false,  // Overdrive
        5 to true,   // AmpModel (AC30)
        6 to true,   // CabinetSim (Alnico Blue)
        7 to false,  // ParametricEQ
        8 to true,   // Chorus (CE-1, subtle)
        9 to false,  // Vibrato
        10 to false, // Phaser
        11 to false, // Flanger
        12 to true,  // Delay (360ms dotted eighth)
        13 to true,  // Reverb (subtle ambience)
        14 to true,  // Tuner
        15 to false, // Tremolo
        16 to false, // Boss DS-1
        17 to false, // ProCo RAT
        18 to false, // Boss DS-2
        19 to false, // Boss HM-2
        20 to false, // Univibe
        21 to false, // Fuzz Face
        22 to false, // Rangemaster
        23 to false, // Big Muff Pi
        24 to false, // Octavia
        25 to false  // Rotary Speaker
    )

    val params = mapOf(
        // NoiseGate: very light
        Pair(0, 0) to -48f,   // threshold
        Pair(0, 1) to 6f,     // hysteresis
        Pair(0, 2) to 0.5f,   // attack
        Pair(0, 3) to 60f,    // release
        // Compressor: OTA (Dyna Comp -- Edge's actual pedal)
        Pair(1, 0) to -20f,   // threshold
        Pair(1, 1) to 4f,     // ratio
        Pair(1, 2) to 8f,     // attack (medium-fast for even delay feed)
        Pair(1, 3) to 120f,   // release
        Pair(1, 4) to 3f,     // makeupGain
        Pair(1, 5) to 6f,     // kneeWidth
        Pair(1, 6) to 1f,     // character (OTA -- historically accurate)
        // AmpModel: Vox AC30 Top Boost
        Pair(5, 0) to 0.8f,   // inputGain (chimey breakup)
        Pair(5, 1) to 0.6f,   // outputLevel (headroom for delay buildup)
        Pair(5, 2) to 6f,     // modelType (AC30)
        Pair(5, 3) to 0f,     // variac (off)
        // CabinetSim: Alnico Blue
        Pair(6, 0) to 8f,     // cabinetType (Alnico Blue)
        Pair(6, 1) to 1.0f,   // mix
        // Chorus: CE-1 mode, subtle warmth
        Pair(8, 0) to 1.5f,   // rate (Hz)
        Pair(8, 1) to 0.25f,  // depth (subtle)
        Pair(8, 2) to 1f,     // mode (CE-1)
        // Delay: dotted eighth at 126 BPM
        Pair(12, 0) to 360f,  // delayTimeMs (dotted eighth)
        Pair(12, 1) to 0.55f, // feedback (~4 repeats)
        // Reverb: subtle ambience
        Pair(13, 0) to 2.0f,  // decay
        Pair(13, 1) to 0.4f,  // damping
        Pair(13, 2) to 25f,   // preDelay
        Pair(13, 3) to 0.5f   // size
    )

    val wetDry = mapOf(
        8 to 0.35f,  // Chorus: subtle warmth
        12 to 0.45f, // Delay: prominent but dry signal still clear
        13 to 0.2f   // Reverb: just air
    )

    return Preset(
        id = "factory-dotted-delay",
        name = "Dotted Delay",
        author = "Factory",
        category = PresetCategory.AMBIENT,
        effects = buildEffectChain(enabled, params, wetDry),
        isFactory = true,
        inputGain = 1.0f,
        outputGain = 0.8f
    )
}
```

---

## Summary Comparison

| Aspect | Loveless Wash | Chicken Pickin' | Reggae Skank | Dotted Delay |
|--------|--------------|-----------------|--------------|--------------|
| **Amp** | Crunch (1) | Twin Reverb (7) | Twin Reverb (7) | AC30 (6) |
| **Cab** | 4x12 British (1) | Jensen C10Q (9) | Jensen C10Q (9) | Alnico Blue (8) |
| **Key Pedal** | Big Muff Pi | -- | -- | -- |
| **Compressor** | VCA, light | OTA, slow attack | VCA, fast attack | OTA (Dyna Comp) |
| **Modulation** | Chorus + Flanger | None | None | CE-1 Chorus |
| **Delay** | 450ms, fb=0.7 | 120ms slapback | None | 360ms dotted 8th |
| **Reverb** | Massive (7s) | Tiny (0.8s) | Spring-like (1.5s) | Subtle (2s) |
| **Category** | AMBIENT | CLEAN | CLEAN | AMBIENT |
| **Effects Used** | 8 | 7 | 6 | 7 |
| **Unique Showcase** | BigMuff+Chorus+Flanger | OTA comp+EQ+slapback | EQ sculpting+clean amp | Dotted delay+AC30+CE-1 |

### Genre Diversity
- **Shoegaze/Dreampop**: Dense, saturated, modulated, reverb-heavy
- **Country/Rockabilly**: Clean, snappy, percussive, slapback
- **Reggae**: Bright, staccato, compressed, minimal effects
- **Post-Punk/New Wave**: Chimey, rhythmic delay, subtle modulation

### Effect Coverage
These 4 presets collectively showcase:
- **Big Muff Pi** (only used in existing Gilmour/Frusciante presets -- now in shoegaze context)
- **Flanger** (not used in any existing factory preset)
- **ParametricEQ** (not used in any existing factory preset)
- **CE-1 Chorus mode** (only in Frusciante UTB -- now in Edge context)
- **OTA Compressor character** (only in Gilmour presets -- now in country/Edge)
- **AC30 amp voicing** (only in Vox Chime -- now in post-punk context)
- **Twin Reverb voicing** (not used in any existing factory preset)
- **Jensen C10Q cabinet** (not used in any existing factory preset)
- **Alnico Blue cabinet** (not used in any existing factory preset)

---

## Sources

### Kevin Shields / My Bloody Valentine
- [My Bloody Valentine Loveless 30 years on -- gearnews.com](https://www.gearnews.com/my-bloody-valentine-loveless-30-years-on-the-gear-and-how-to-glide/)
- [4 Shoegaze Secrets from Kevin Shields Pedalboard -- The Bold Musician](https://www.theboldmusician.com/my-bloody-valentine-guitar-sound-kevin-shields-pedalboard/)
- [Kevin Shields MBV guitar technique -- Far Out Magazine](https://faroutmagazine.co.uk/kevin-shields-my-bloody-valentine-explains-his-guitar-technique/)
- [Gear Rundown: Kevin Shields -- Mixdown](https://mixdownmag.com.au/features/my-bloody-valentine-kevin-shields/)
- [Kevin Shields Equipboard](https://equipboard.com/pros/kevin-shields)
- [Engineering Loveless -- Happy Mag](https://happymag.tv/engineering-the-sound-my-bloody-valentines-loveless/)
- [Guide to Shoegaze Tones -- Sweetwater](https://www.sweetwater.com/insync/guide-to-shoegaze-tones-using-pedals/)
- [Shoegaze Guitar Pedals -- Raccoon Point Studios](https://www.rpmusicstudios.com/blog/guide-to-shoegaze-guitar-pedals)

### Brad Paisley / Brian Setzer
- [Brad Paisley on compressor pedals -- MusicRadar](https://www.musicradar.com/artists/brad-paisley-country-guitar-compressor-pedals-and-why-he-prefers-slapback-to-reverb)
- [Brad Paisley Amp Settings -- Guitar Chalk](https://www.guitarchalk.com/amp-settings-brad-paisley/)
- [Brian Setzer Rock This Town secrets -- Guitar World](https://www.guitarworld.com/artists/the-secrets-behind-brian-setzers-guitar-sound-on-stray-cats-rock-this-town)
- [Rock this Tone: Brian Setzer -- Reverb.com](https://reverb.com/news/rock-this-tone-the-gear-and-sound-of-brian-setzer)
- [Brad Paisley Equipboard](https://equipboard.com/pros/brad-paisley)
- [Brian Setzer Equipboard](https://equipboard.com/pros/brian-setzer)

### Bob Marley / Reggae
- [Bob Marley's essential reggae rhythm styles -- Guitar World](https://www.guitarworld.com/lessons/bob-marley-reggae-rhythm-guitar)
- [How to Get a Reggae Guitar Tone -- Riffhard](https://www.riffhard.com/how-to-get-a-reggae-guitar-tone/)
- [Reggae Guitar Tone -- Gear Aficionado](https://gearaficionado.com/blog/how-to-get-a-reggae-guitar-tone-amp-settings-and-fx/)
- [The Reggae Guitar Skank -- Bass Culture/Dubmatix](https://bassculture.substack.com/p/the-reggae-guitar-skank)
- [Essential Guitar Effects Rig for Reggae -- Kuassa](https://www.kuassa.com/the-essential-guitar-effects-rig-for-reggae/)

### The Edge / U2
- [A Study of The Edge's Guitar Delay -- amnesta.net](https://www.amnesta.net/edge_delay/)
- [Where the Streets Have No Name delay analysis -- amnesta.net](https://www.amnesta.net/edge_delay/streets.html)
- [Patch Work: Streets -- BOSS Articles](https://articles.boss.info/patch-work-where-the-streets-have-no-name-by-u2/)
- [How to Sound Like The Edge -- Rotosound](https://www.rotosound.com/blog/advice/how-to-sound-like-u2s-the-edge/)
- [Edge Amp Settings -- Guitar Chalk](https://www.guitarchalk.com/amp-settings-for-edge-david-evans-of-u2/)
