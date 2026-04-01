# Peter Green Fleetwood Mac Era Tones (1967-1970) -- Comprehensive Research

Deep research for building 3 factory presets capturing Peter Green's legendary Fleetwood Mac-era guitar tones. Covers exact gear, the famous reversed-magnet Les Paul, circuit-level analysis, and DSP implementation strategy.

---

## TABLE OF CONTENTS

1. [Peter Green's Rig: Core Components](#1-peter-greens-rig-core-components)
2. [The Reversed Pickup Magnet: Complete Technical Analysis](#2-the-reversed-pickup-magnet)
3. [Tone 1: "The Green Manalishi" (1970)](#3-tone-1-the-green-manalishi-1970)
4. [Tone 2: "Albatross" (1968)](#4-tone-2-albatross-1968)
5. [Tone 3: "Need Your Love So Bad" / "Man of the World"](#5-tone-3-need-your-love-so-bad--man-of-the-world)
6. [Peter Green's Playing Technique](#6-peter-greens-playing-technique)
7. [Gap Analysis: What Our App Needs](#7-gap-analysis-what-our-app-needs)
8. [Preset Specifications](#8-preset-specifications)

---

## 1. PETER GREEN'S RIG: CORE COMPONENTS

### 1.1 The Guitar: 1959 Gibson Les Paul Standard #9xxxx ("Greeny"/"The Peter Green Les Paul")

**Specifications:**
- Year: 1959 (serial number in the 9-xxxx range, placing it in late 1958/early 1959 production)
- Finish: Heritage Cherry Sunburst, heavily faded to a honey/lemon drop by the time Green used it
- Neck profile: 1959 "fat" profile, approximately 0.87" at 1st fret, 0.95" at 12th fret
- Fretboard: Brazilian rosewood, 22 frets
- Scale length: 24.75" (standard Gibson)
- Pickups: Original Gibson PAF (Patent Applied For) humbuckers
  - Neck: PAF, approximately 7.5K-8.2K ohm DC resistance (original PAFs varied significantly)
  - Bridge: PAF, approximately 7.8K-8.5K ohm DC resistance
  - **THE CRITICAL MODIFICATION**: The neck pickup's magnet is installed reversed (more on this below)
- Bridge: ABR-1 Tune-o-Matic with aluminum stop tailpiece
- Pots: 500K audio taper CTS pots (original 1959 spec)
- Tone capacitors: 0.022uF "Bumblebee" (Sprague Black Beauty) capacitors on each pickup
- Strings: Very light gauge, reportedly .009-.042 (some sources say .008-.038 in later years)
- Action: Very low action, contributing to his light touch

**History of the guitar:**
Green acquired this Les Paul around 1965-1966. It was later sold to Gary Moore in 1970 for approximately 300 GBP (some accounts say Moore was gifted it, others say a nominal fee). Moore used it extensively from the mid-1990s through his career. After Moore's death in 2011, Kirk Hammett (Metallica) purchased it for a reported $2 million, making it one of the most expensive guitars ever sold.

### 1.2 The Amplifiers

Peter Green's amplifier setup evolved during his Fleetwood Mac tenure, but the core amps were:

**Primary: Marshall JTM45 (1965-1967 era) / Marshall 1959 "Plexi" Super Lead 100**

The JTM45 was Green's primary amp for the Bluesbreakers period (late 1966, where he replaced Clapton) and early Fleetwood Mac:

- **JTM45 Circuit Details:**
  - Power tubes: 2x KT66 (early JTM45) or 2x EL34 (later JTM45 and 1959)
  - Preamp tubes: 3x ECC83 (12AX7)
  - Rectifier: GZ34 (tube rectifier in early models, adding sag)
  - Output power: ~45W (KT66 version), ~50W (EL34 version), 100W (1959 Super Lead)
  - Channels: Two-channel (Normal + High Treble/Bright)
  - Normal channel: single 68K grid resistor, 470pF + 0.022uF parallel coupling to second stage
  - High Treble channel: 68K grid + 56K resistor, bright cap bypasses tone controls
  - Tone stack: Marshall TMB (Treble/Middle/Bass) derived from Fender 5F6-A Bassman
    - Treble: 250pF + 250K pot
    - Mid: 25K pot
    - Bass: 56K + 0.022uF + 1M pot

- **Green's reported settings (from crew and contemporaries):**
  - Volume: 7-10 (depending on venue size and song)
  - Treble: 4-6 (NOT cranked like Clapton -- Green preferred warmer tones)
  - Middle: 6-8 (higher mid emphasis for the vocal quality)
  - Bass: 4-5 (moderate, avoiding muddiness)
  - Presence: 3-5 (moderate -- Green's tone is never harsh)

**Secondary: Marshall 1962 "Bluesbreaker" Combo (2x12, 30W)**

For some studio recordings and smaller venues, Green used the Marshall 1962 Bluesbreaker combo:
- 2x EL34 power tubes (some early models used 5881/6L6)
- 2x 12" Celestion speakers (G12M-25 Greenbacks in most versions)
- 30 watts, which breaks up earlier than the JTM45/100W heads
- Open-back design creates different dispersion than closed-back 4x12
- This amp was particularly important for the more intimate, bluesy tones

**Occasional: Fender Dual Showman / Fender Twin Reverb**

Several photographs from 1968-1969 show Green playing through Fender amps (likely a Dual Showman Reverb or Twin Reverb) alongside his Marshall stack. This is significant because:
- The Fender's cleaner headroom and different EQ character (6L6GC, scooped mids) would have been used for clean passages
- The "Albatross" recording tone has qualities consistent with a Fender-type clean channel: glassy, with scoop and shimmer
- However, most sources attribute the core of Green's tone to Marshall amplification

### 1.3 Speaker Cabinets

**Marshall 1960A/1960B 4x12 Cabinets:**
- Speakers: Celestion G12M-25 "Greenback" (most common in this era)
  - Ceramic magnet, 25W power handling
  - Resonant frequency: approximately 75Hz
  - Frequency response: strong midrange presence around 1.5-3kHz, natural treble rolloff above 4-5kHz
  - The Greenback's character is warmer and more "compressed" sounding than later Celestion models (G12H-30, V30)
- Some reports suggest Green also used cabinets with Celestion G12H-30 "Heritage" speakers:
  - Heavier magnet, 30W power handling
  - Tighter bass, more defined midrange attack
  - Slightly more "aggressive" than the G12M-25 Greenback

**Marshall 1962 Bluesbreaker Combo speakers:**
- 2x Celestion G12M-25 Greenback speakers in open-back configuration
- The open back creates a more "airy" and dispersed sound compared to the sealed 4x12
- Less bass buildup, more even frequency response off-axis

### 1.4 Effects: EXTREMELY Minimal

Peter Green was one of the most effects-averse players of his era. His signal chain was remarkably simple:

**Confirmed effects:**
- **NONE for most recordings.** Green's tone was fundamentally guitar-into-amplifier.

**Possible/disputed effects:**
- **Dallas Rangemaster Treble Booster**: Some sources suggest Green may have occasionally used a Rangemaster, particularly for the hotter tones on "The Green Manalishi." However, unlike Brian May or Tony Iommi who were confirmed Rangemaster users, photographic or first-hand evidence for Green is thin. The Rangemaster would have:
  - Added +20-25dB of gain centered above 1kHz (OC44 germanium transistor, 4.7nF input cap)
  - Driven the Marshall's preamp harder, creating more sustain and saturation
  - Boosted the upper midrange, which would explain some of the "cutting" quality in Manalishi

- **Amp tremolo**: The Marshall JTM45 does NOT have built-in tremolo. However, the Marshall 1962 Bluesbreaker combo DOES have a tremolo circuit (speed and intensity controls). If Green used a Bluesbreaker for any recordings, he had access to built-in tremolo. Additionally, some Fender amps he may have used (Twin Reverb, etc.) have built-in tremolo.

- **Studio effects**: On "Albatross," there appears to be subtle reverb (likely studio room ambience or a plate reverb applied during mixing by producer Mike Vernon at Blue Horizon Studios). There may also be a subtle tremolo effect on some recordings, but opinions differ on whether this was an amp effect, studio effect, or Green's own vibrato technique being mistaken for tremolo.

### 1.5 Strings and Picks

- **Strings**: Light gauge, .009-.042 or possibly .008s. Green's extremely light touch and bending facility suggest very light strings. Light strings also contribute to the "singing" quality of his tone -- less mass = faster attack, more overtone content relative to fundamental, and easier wide bends.
- **Pick**: Standard medium picks (various accounts; not a signature pick user)
- **Fingers**: Green occasionally played fingerstyle, particularly for soft, clean passages. His right-hand technique was extremely controlled.

---

## 2. THE REVERSED PICKUP MAGNET: COMPLETE TECHNICAL ANALYSIS

This is THE defining characteristic of Peter Green's Les Paul tone and requires thorough analysis.

### 2.1 What Happened Physically

The neck pickup in Green's 1959 Les Paul has its Alnico V bar magnet installed 180 degrees rotated (flipped) compared to the bridge pickup. In a standard Les Paul:

- Both pickups have magnets oriented the same way (e.g., North facing the strings)
- Both pickups are wound in the same direction
- When both pickups are selected (middle toggle position), the signals ADD constructively = full, fat, loud tone

With Green's guitar, the neck pickup magnet is reversed:

- Neck pickup: South pole facing strings (REVERSED)
- Bridge pickup: North pole facing strings (NORMAL)
- Both pickups wound in the same direction (winding was NOT changed)
- Result: When both pickups are selected, they are **magnetically out of phase**

### 2.2 How This Differs from Electrical Out-of-Phase

It is critical to understand the difference between magnetic phase reversal and electrical phase reversal:

**Electrical phase reversal** (swapping hot/ground wires on one pickup):
- Reverses the phase of the ENTIRE signal -- both the string vibration signal AND the electromagnetic noise (hum)
- Result: Thin, nasal tone + INCREASED hum (because the hum-cancelling property of the humbucker is also reversed, and the two humbuckers no longer cancel each other's hum)

**Magnetic phase reversal** (flipping the magnet):
- Reverses the phase of the STRING SIGNAL ONLY
- The electromagnetic noise pickup is determined by coil winding direction, which is unchanged
- Result: Thin, nasal tone + HUM IS STILL CANCELLED between the two pickups
- This is why Green's middle position sounds characteristically thin/hollow but remains quiet (no excess hum)

In Green's guitar, the practical effect is:
- Neck position (toggle down): Normal neck humbucker tone -- warm, full, round
- Bridge position (toggle up): Normal bridge humbucker tone -- bright, biting, percussive
- **Middle position (both pickups): OUT-OF-PHASE TONE** -- the defining "Peter Green sound"

### 2.3 The Out-of-Phase Frequency Response: What Actually Gets Cancelled

When two pickups are summed out of phase, the frequencies that cancel most completely are those where both pickups produce signals of nearly equal amplitude. The frequencies that survive are those where the two pickups produce signals of significantly different amplitude.

**Key physics:**

The neck and bridge pickups "see" different points on the vibrating string:
- **Neck pickup**: Closer to the string's midpoint (12th fret) where the fundamental and odd harmonics have large amplitude, but even harmonics may be near a node
- **Bridge pickup**: Closer to the bridge where the string has less fundamental amplitude but proportionally more high harmonics

For a standard Les Paul with pickup positions approximately at the 22nd fret area (neck) and near the bridge:

**Frequencies that cancel (nearly equal amplitude at both pickups):**
- The fundamental and low harmonics (below ~500Hz): Both pickups see these strongly, so they cancel significantly
- Upper harmonics in certain bands where the two pickup positions happen to see similar amplitudes

**Frequencies that survive (unequal amplitude at both pickups):**
- Mid-range frequencies (~500Hz-2kHz) where the spatial filtering of the two pickup positions creates the largest amplitude difference between the pickups
- Very high frequencies (>3kHz) where the bridge pickup dominates due to its proximity to the bridge

**The resulting frequency response is:**
- Dramatically reduced bass/low-mids (the "thin" quality)
- A prominent nasal/honky peak in the midrange, typically centered around 600Hz-1.2kHz (string and pickup position dependent)
- Some high-frequency presence that survives due to the bridge pickup's dominance in that range
- Overall output level is significantly lower than either pickup alone (the cancellation removes a lot of energy)

### 2.4 The Psychoacoustic Character

The out-of-phase middle position produces a tone that is:

1. **Thin/hollow**: Massive bass cancellation removes the body and fullness
2. **Nasal/honky**: The surviving midrange peak sounds like a voice speaking through the nose, or a wah pedal parked at a specific position (~800Hz-1kHz)
3. **Quacky**: On clean tones, there is a "quack" quality similar to a Stratocaster's positions 2 and 4 (which are also out-of-phase combinations, though between single coils with different pole spacing)
4. **Sustaining and vocal**: The reduced harmonic content means the remaining midrange sustains more evenly, creating a singing, almost vocal quality
5. **Extremely dynamic**: Because the tone is so focused in a narrow band, it responds dramatically to playing dynamics, pickup height adjustments, and volume/tone pot positions
6. **Distinctive overtone structure**: The phase cancellation creates a unique harmonic spectrum that does not exist in any single-pickup configuration -- it is audibly different from simply EQing a normal pickup to have less bass and more mids

### 2.5 DSP Implementation of the Out-of-Phase Effect

To emulate this in our app, we need a filter that approximates the frequency-cancellation pattern. This is NOT a simple EQ -- it has a specific comb-filter-like character due to the spatial relationship between pickups.

**Approach 1: Parametric EQ approximation (simpler, good enough)**

The out-of-phase cancellation can be approximated with:
1. High-pass filter at ~200Hz (12dB/oct) -- removes the bass that gets cancelled
2. Bandpass emphasis centered at ~800Hz, Q of approximately 2-3 -- the nasal peak
3. Low-pass rolloff above ~4kHz (6dB/oct) -- the remaining treble rolls off due to partial cancellation
4. Overall level reduction of -6 to -10dB (the cancellation reduces total output significantly)

This can be achieved with our ParametricEQ (index 7) configured as:
- Band 1: Low shelf at 200Hz, -12dB (simulate bass cancellation)
- Band 2: Peak at 800Hz, +8dB, Q=2.5 (the nasal honk)
- Band 3: High shelf at 4kHz, -6dB (treble rolloff)

**Approach 2: Comb filter (more accurate)**

The true out-of-phase cancellation is actually a comb filter pattern determined by the time delay between the two pickup positions. The distance between a Les Paul's neck and bridge pickups is approximately 4.5 inches (11.4cm). The speed of transverse wave propagation on a guitar string varies with frequency, so the comb filter is not perfectly periodic, but a first approximation:

- Delay between pickups (at fundamental): approximately 0.5-1.5ms depending on string and fret
- This creates a comb filter with notches at approximately every 700-2000Hz
- The first notch (and deepest) is in the low frequencies, which matches the bass cancellation we hear

For our app, Approach 1 (parametric EQ) is sufficient and CPU-efficient. The psychoacoustic difference between a true spatial comb filter and a well-tuned EQ approximation is minimal, especially once amp distortion is applied downstream.

### 2.6 Volume and Tone Pot Interaction

Green was masterful at using his guitar's controls to shape the out-of-phase tone:

- **Volume rolled back (7-8)**: Cleans up the out-of-phase tone, making it more glassy and delicate. The reduced signal hits the amp's clean region. This was used for "Albatross."
- **Volume full (10)**: Maximum signal into the amp, with the out-of-phase cancellation providing the characteristic mid-focused drive. Used for "Man of the World" lead passages.
- **Tone rolled back (3-5)**: On the middle position, rolling the tone back further narrows the surviving frequency band, creating an even more focused, vocal midrange. This is how Green achieved his most distinctive "woman tone"-adjacent sounds -- but with the out-of-phase character adding hollowness that Clapton's straight neck pickup does not have.
- **Tone full (10)**: Lets through whatever high harmonics survive the phase cancellation, adding a bit of edge and definition.

---

## 3. TONE 1: "THE GREEN MANALISHI (WITH THE TWO PRONG CROWN)" (1970)

### 3.1 Context

"The Green Manalishi" was Fleetwood Mac's last single with Peter Green, released in May 1970. It is widely regarded as one of the earliest proto-metal/proto-punk recordings. The tone is dark, aggressive, almost menacing -- a radical departure from Green's typically refined blues playing.

The song was inspired by Green's deteriorating mental state (LSD use, mounting financial guilt) and reportedly came from a nightmare about money ("the green" = money, "manalishi" = possibly a portmanteau). The musical intensity reflects this inner turmoil.

### 3.2 The Recording Setup

**Studio**: Warner/Reprise Studios, Hollywood, California (some sources say Kingsway Studios, London -- the exact studio is debated)

**Guitar**: 1959 Les Paul, MIDDLE POSITION (out-of-phase)
- This is critical: the nasal, cutting quality of the Manalishi riff is the out-of-phase pickup sound being driven hard
- Volume: Full (10)
- Tone: Approximately 5-7 (some treble present but not wide open)

**Amplifier**: Marshall (most likely the 1959 100W Super Lead at this point in his career)
- Volume: HIGH -- the amp is being pushed into significant breakup
- Treble: 5-6 (moderate -- the tone is dark, not bright)
- Middle: 7-8 (the mids are prominent and aggressive)
- Bass: 4-5
- Presence: 3-4 (low -- Green wanted darkness, not presence)

**Possible boost**: Some historians argue Green used a Dallas Rangemaster or similar treble booster to push the amp harder for this recording. The level of sustain and saturation on the main riff suggests either:
- A very cranked amp (possible at a Hollywood recording studio)
- Some form of boost pedal
- Or simply the natural sustain of the Les Paul into a cranked Marshall

**Speaker cabinet**: Marshall 4x12 with Celestion Greenbacks (G12M-25)

**Effects**:
- None confirmed in the guitar signal chain
- Studio reverb is audible on the recording (likely EMT plate reverb or studio room, added during mixing)
- Danny Kirwan's rhythm guitar (also present) provides some of the texture

### 3.3 What Creates the Manalishi Tone

The "Green Manalishi" tone is the result of these interacting elements:

1. **Out-of-phase pickups (middle position)**: Creates the nasal, hollow, cutting midrange that gives the riff its menacing character. Without the phase cancellation, the same riff through a normal Les Paul sounds fuller and less threatening.

2. **Cranked Marshall**: The heavily saturated amp compresses the already-focused midrange, creating a thick, sustaining distortion that is entirely in the mid frequencies. Because the out-of-phase pickup has already removed most bass and treble, the amp distortion generates harmonics almost exclusively in the midrange -- this is what gives the tone its "nasal buzz saw" quality.

3. **The riff itself**: The main riff sits in the low-mid register (open A string = 110Hz), but the out-of-phase pickup filters this into a mid-focused sound. The combination of a low riff with a mid-heavy tone is what creates the dark, aggressive character.

4. **Playing attack**: Green hits the strings hard on this recording, driving the amp harder and creating more saturation. This is unusual for Green, who was typically a light-touch player -- the aggression in the performance is part of the tone.

5. **No reverb in the guitar chain**: The tone is dry and in-your-face, with any reverb being studio ambience or mixing desk processing.

### 3.4 Frequency Analysis

The Manalishi guitar tone occupies approximately:
- **Low end**: Significantly rolled off below 200Hz (out-of-phase cancellation)
- **Primary energy**: 400Hz-2kHz (the nasal peak centered around 700-1kHz)
- **Distortion harmonics**: 1-3kHz (generated by the amp clipping the midrange)
- **High end**: Rolling off above 3-4kHz (Greenback speakers + reduced treble from out-of-phase)
- **Overall character**: Narrow-band, mid-focused, heavily saturated, dark

---

## 4. TONE 2: "ALBATROSS" (1968)

### 4.1 Context

"Albatross" was Fleetwood Mac's only UK #1 hit single, released in November 1968. It is an instrumental that showcases Green's melodic genius and his ability to create deeply emotional music with an almost ethereally clean tone. The song was reportedly inspired by Santo & Johnny's "Sleep Walk" (1959) and has a dreamy, oceanic quality.

### 4.2 The Recording Setup

**Studio**: CBS Studios, New Bond Street, London (produced by Mike Vernon for Blue Horizon Records)

**Guitar**: 1959 Les Paul
- **Pickup position**: NECK PICKUP (toggle down) -- NOT the middle out-of-phase position
- This is important: "Albatross" uses the warm, full neck pickup tone, NOT the out-of-phase sound
- **Volume**: Rolled back to approximately 5-7 (clean signal, well below amp breakup threshold)
- **Tone**: Approximately 5-6 (slightly rolled back for warmth, but not fully dark)

The melody is played on the neck pickup with the guitar volume rolled back enough to keep the amp clean. This produces the warm, sustaining, slightly compressed clean tone that defines the track.

**Amplifier**: Marshall (likely JTM45 or early Plexi)
- The key insight: the amp settings for "Albatross" are MODERATE, not cranked
- Volume: 4-5 (enough for some warmth and compression, but below breakup)
- Treble: 5-6
- Middle: 5-6
- Bass: 5-6
- Presence: 3-4
- The amp provides warmth and subtle tube compression without distortion

Alternative theory: Some historians believe Green may have used a Fender amplifier for "Albatross" (possibly a Dual Showman or Twin Reverb). The clean, glassy, slightly scooped quality of the tone has characteristics more associated with Fender clean channels than Marshall. If a Fender was used:
- The 6L6GC power tubes would provide more headroom and a cleaner breakup character
- The Fender's scooped mid EQ would explain the "glassy" quality
- The built-in reverb (spring) could explain the subtle reverb on the recording

**Speaker cabinet**: Either Marshall 4x12 (Greenback) or Fender speaker (2x15 JBL D130F in a Dual Showman, or 2x12 Oxford/Jensen in a Twin)

**Effects**:
- **Reverb**: There is subtle reverb on the recording. This could be:
  - Studio plate reverb (EMT 140 was the standard at CBS Studios)
  - Fender amp spring reverb (if a Fender amp was used)
  - Room ambience from the studio
  - The reverb is subtle but crucial -- it provides the "oceanic," floating quality

- **Tremolo**: DISPUTED. Some listeners perceive a subtle tremolo/pulsing in the sustaining notes. This could be:
  - Green's own vibrato technique (hand vibrato producing amplitude modulation)
  - A very subtle amp tremolo (if a Fender amp was used)
  - A studio effect added during mixing
  - Most likely Green's vibrato technique, not an effect

### 4.3 What Creates the Albatross Tone

1. **Neck pickup**: Warm, full fundamental tone with rolled-off highs. The PAF humbucker in the neck position emphasizes the fundamental and low harmonics, creating a round, singing tone.

2. **Volume rolled back**: By reducing the guitar volume to ~5-7, Green keeps the signal well below the amp's breakup threshold. This produces a clean but not sterile tone -- the amp adds subtle tube compression and warmth without distortion.

3. **Moderate amp settings**: The Marshall (or Fender) is set for a balanced, warm clean tone. Nothing is extreme -- everything is moderate and musical.

4. **Subtle reverb**: Whether from a plate reverb or spring reverb, the subtle wash adds the ethereal, floating quality. It is not a heavy reverb -- just enough to create a sense of space around the notes.

5. **Playing technique**: Green's vibrato on "Albatross" is slow, wide, and perfectly controlled. This adds natural sustain and a pulsing quality that might be mistaken for tremolo. His touch is extremely light, allowing notes to ring and sustain naturally.

6. **Light strings**: Green's light-gauge strings (.009s or lighter) allow the notes to sustain longer and produce a more "singing" quality, with more harmonic content relative to the fundamental.

### 4.4 Frequency Analysis

The Albatross tone occupies approximately:
- **Low end**: Full and warm, with gentle rolloff below 80-100Hz
- **Low mids**: Prominent 200-500Hz warmth from the neck pickup
- **Midrange**: Smooth 500Hz-2kHz, without the nasal peak of the out-of-phase sound
- **Presence**: Gentle rolloff above 3kHz (neck pickup + tone pot slightly rolled back)
- **High end**: Soft, without sparkle or bite -- the treble is there but subdued
- **Overall character**: Warm, round, sustained, with subtle reverb wash

---

## 5. TONE 3: "NEED YOUR LOVE SO BAD" / "MAN OF THE WORLD"

### 5.1 Context

These two songs represent the emotional core of Peter Green's playing: deeply expressive blues guitar with a tone that seems to sing, cry, and speak. "Need Your Love So Bad" (originally by Little Willie John, covered by Fleetwood Mac in 1968 on the "English Rose" album and as a 1969 single) features one of the most emotionally devastating guitar tones in blues history. "Man of the World" (1969 single) showcases a similar emotional depth with Green's increasingly personal and melancholic songwriting.

### 5.2 The Recording Setup -- "Need Your Love So Bad"

**Guitar**: 1959 Les Paul
- **Pickup position**: MIDDLE POSITION (out-of-phase) for the intro melody and solos
  - The distinctive nasal, vocal quality of the intro is unmistakably the out-of-phase sound
  - This is Green's signature: using the out-of-phase tone not for aggression (as in Manalishi) but for emotional expressiveness
- **Volume**: Approximately 7-8 (enough signal for sustain but not full breakup)
- **Tone**: Approximately 4-6 (moderately rolled back, adding warmth to the out-of-phase nasal tone)

**Amplifier**: Marshall (JTM45 or early Plexi)
- Volume: 5-7 (on the edge of breakup -- notes sustain when played firmly, clean up on softer passages)
- Treble: 4-5 (restrained -- Green is going for warmth)
- Middle: 7-8 (emphasizing the vocal midrange)
- Bass: 4-5
- Presence: 2-4 (very low -- this is the warmest of Green's tones)

**Effects**: None confirmed. The recording has a natural room reverb quality.

### 5.3 The Recording Setup -- "Man of the World"

**Guitar**: 1959 Les Paul
- **Pickup position**: Primarily MIDDLE POSITION (out-of-phase) for the distinctive vocal quality
  - Some passages may use the neck pickup alone for a fuller, warmer tone during quieter sections
- **Volume**: 7-9 (varies with the song's dynamics)
- **Tone**: 5-7

**Amplifier**: Marshall, similar settings to "Need Your Love So Bad" but potentially slightly more gain:
- Volume: 6-7
- Treble: 4-5
- Middle: 7
- Bass: 4-5
- Presence: 3

**Effects**: None confirmed.

### 5.4 The "Woman Tone" Connection

Peter Green's emotional blues tone is often compared to Clapton's "woman tone," and for good reason -- both players achieve a vocal, singing guitar quality by emphasizing midrange and rolling off treble. However, there are critical differences:

**Clapton's Woman Tone:**
- Neck pickup ONLY (full, round output)
- Tone pot at 0 (total treble removal, cutoff ~900Hz with 0.022uF cap)
- Amp cranked to maximum (everything on 10)
- Result: Extremely thick, warm, vocal, with NO nasal quality

**Green's Emotional Blues Tone:**
- MIDDLE position (out-of-phase) -- this adds the nasal, hollow quality
- Tone pot partially rolled back (4-6, not fully off)
- Amp at moderate-to-high volume (not fully cranked)
- Result: Vocal quality but with a thinner, more nasal, more "human voice" character

The key distinction is that Green's out-of-phase tone has an inherent HOLLOW quality that Clapton's neck-pickup tone lacks. This hollowness is what makes Green's tone sound like a person singing with emotion -- the voice-like midrange surrounded by emptiness.

Green's tone on these songs also responds incredibly dynamically to his touch:
- Soft picking: Clean, gentle, whispering
- Medium picking: Warm sustain, just touching breakup
- Hard picking: Biting, crying, with momentary distortion that cleans up immediately
- Vibrato: Slow, wide, deliberate -- adding natural amplitude and pitch modulation

### 5.5 Frequency Analysis

The emotional blues tone occupies approximately:
- **Low end**: Reduced by out-of-phase cancellation, but not as dramatically as Manalishi (the amp is not as saturated, so the bass is more present pre-distortion)
- **Primary energy**: 400Hz-2kHz with the nasal peak at ~800Hz-1kHz
- **Presence**: More treble than Manalishi (tone pot less rolled back), some shimmer up to 3-4kHz
- **High end**: Gentle rolloff above 4kHz
- **Dynamic range**: HUGE -- the tone covers a wide range from whisper-clean to singing sustain
- **Overall character**: Nasal, vocal, hollow, emotionally expressive, dynamically responsive

---

## 6. PETER GREEN'S PLAYING TECHNIQUE

Green's technique is inseparable from his tone. Without the technique, even perfect gear emulation will not sound like Peter Green.

### 6.1 Touch Sensitivity

Green had an extraordinarily light touch. He rarely struck the strings hard (the Manalishi being a notable exception). This light touch:
- Kept the signal in the amp's clean-to-edge-of-breakup zone
- Allowed maximum dynamic range
- Produced a "singing" quality where the fundamental dominates over the attack transient
- Is why Green's tone sounds so different from, say, Hendrix through similar gear -- Hendrix hit hard, Green touched gently

### 6.2 Vibrato

Green's vibrato is one of the most recognizable in guitar history:
- **Speed**: Slow to moderate (approximately 4-6 Hz)
- **Width**: Wide (approximately 1/4 to 1/2 step pitch bend)
- **Type**: Wrist vibrato (classical-influenced), not finger vibrato
- **Character**: Even, controlled, singing -- never rushed or nervous
- **Application**: Green applied vibrato to sustained notes almost immediately, creating a vocal quality
- **Amplitude modulation**: His vibrato also creates natural amplitude modulation because bending the string changes the pickup's output slightly -- this is what some people mistake for tremolo

### 6.3 Bending

Green was a master of string bending:
- Slow, precise bends -- often from a half step to a full step
- "Pre-bent" notes released slowly (bending to pitch then releasing, creating a crying quality)
- Ghost bends and micro-bends (quarter-step bends for emotional inflection)
- Very accurate intonation on bends -- Green always hit the target pitch precisely

### 6.4 Phrasing and Space

Green understood that silence is as important as notes:
- Long pauses between phrases (letting notes ring and decay naturally)
- Relatively few notes per measure compared to contemporaries
- Each note chosen for maximum emotional impact
- "Conversational" phrasing -- like call and response within a single guitar part

### 6.5 Dynamics

Green used dynamics as an expressive tool more than almost any other rock guitarist:
- Whisper-soft passages that barely register on the amp
- Gradual swells using pick attack, not volume knob (though he used the volume knob too)
- Sudden dynamic contrasts for emotional effect
- This is why his amp was not fully cranked -- he needed headroom for dynamics

---

## 7. GAP ANALYSIS: WHAT OUR APP NEEDS

### 7.1 What We Already Have

| Component | App Effect | Adequacy |
|-----------|-----------|----------|
| Marshall JTM45/Plexi amp | AmpModel (Plexi, index 5, modelType=3) | Good -- our Plexi model captures the basic character |
| Marshall Crunch | AmpModel (Crunch, index 5, modelType=1) | Good for edge-of-breakup tones |
| Fender Clean | AmpModel (Clean, index 5, modelType=0) | Adequate for Albatross clean |
| Fender Twin | AmpModel (Fender Twin, index 5, modelType=7) | Better for Albatross |
| Greenback 4x12 cab | CabinetSim (Greenback, index 6, type=4) | Good |
| G12H-30 4x12 cab | CabinetSim (G12H-30, index 6, type=5) | Available |
| British 4x12 cab | CabinetSim (British, index 6, type=1) | Alternative |
| Reverb | Reverb (Dattorro plate, index 13) | Excellent for studio plate |
| Tremolo | Tremolo (index 15) | Available if needed |
| Parametric EQ | ParametricEQ (index 7) | Key for out-of-phase simulation |
| Boost (Rangemaster) | Rangemaster (index 22) | Available for Manalishi boost |
| Noise Gate | NoiseGate (index 0) | Standard |
| Compressor | Compressor (index 1) | For sustain simulation |

### 7.2 What We Need (New or Modified)

**CRITICAL: Out-of-Phase Pickup Emulation**

This is the single most important element for Peter Green presets. We need to emulate the frequency response of two PAF humbuckers summed out of phase. Our ParametricEQ can approximate this, but ideally we would want:

Option A (use existing ParametricEQ, index 7):
- Configure the 3-band parametric EQ to create the out-of-phase response:
  - Low shelf: Aggressive cut below 200Hz (-12 to -15dB)
  - Mid peak: Boost at 800-1000Hz (+8 to +10dB, Q=2.0-3.0)
  - High shelf: Cut above 3.5-4kHz (-6 to -8dB)
- PLUS: Reduce the input gain by -6 to -8dB (the out-of-phase cancellation reduces output)
- This is probably our best immediate option

Option B (future: dedicated "Pickup Position" effect):
- A new effect at the front of the chain that models different pickup positions
- Parameters: Position (Neck / Middle Out-of-Phase / Middle In-Phase / Bridge)
- The out-of-phase mode would apply the specific comb-filter-like cancellation pattern
- This would be more accurate than EQ approximation
- Lower priority for now -- the EQ approximation is sufficient

**NICE TO HAVE: Guitar Volume/Tone Simulation**

Green's extensive use of guitar volume and tone controls to shape his sound is difficult to replicate in a preset that has fixed parameters. Ideally:
- A "Guitar Volume" simulation pre-effect that rolls off highs as volume decreases (simulating the interaction between pickup impedance, cable capacitance, and pot position)
- A "Guitar Tone" simulation that applies a first-order LPF with a resonant peak

For now, we can approximate this with ParametricEQ and Boost settings.

### 7.3 Effect Chain Order for Peter Green Presets

The signal chain for Green's tones should be:
```
Guitar signal
  -> ParametricEQ (configured as out-of-phase pickup sim) [for Manalishi and Blues tones]
  -> Rangemaster (optional, for Manalishi) [treble boost into amp]
  -> AmpModel (Plexi or Clean/Twin depending on tone)
  -> CabinetSim (Greenback 4x12)
  -> Reverb (subtle plate, for Albatross)
  -> Tremolo (off for most presets, optional for experimentation)
```

**PROBLEM**: Our signal chain order is fixed: NoiseGate(0) -> ... -> ParametricEQ(7) -> ... -> AmpModel(5). But the ParametricEQ (index 7) comes AFTER the AmpModel (index 5) in our default chain. For the out-of-phase pickup simulation, the EQ needs to be BEFORE the amp model.

**SOLUTION**: We have effect reordering (setEffectOrder). The Peter Green presets should reorder the chain to place ParametricEQ BEFORE AmpModel:
```
0:NoiseGate -> 1:Compressor -> 7:ParametricEQ -> 22:Rangemaster -> 5:AmpModel -> 6:CabinetSim -> 13:Reverb -> 15:Tremolo
```

Wait -- checking the architecture: the effect reordering feature allows arbitrary ordering. We should use this to place the EQ before the amp for the pickup simulation. However, we need to verify that our preset system can store and restore effect order.

---

## 8. PRESET SPECIFICATIONS

### 8.1 Preset 1: "Manalishi" -- Dark Aggressive Proto-Metal

**Concept**: The Green Manalishi riff tone. Out-of-phase pickups into a driven Plexi. Dark, aggressive, nasal, cutting.

**Signal Chain** (reordered):
```
NoiseGate -> Compressor -> ParametricEQ [pickup sim] -> Rangemaster -> AmpModel [Plexi] -> CabinetSim [Greenback] -> (no reverb)
```

**Effect Settings**:

| Effect | Index | Enabled | Parameters |
|--------|-------|---------|------------|
| NoiseGate | 0 | YES | threshold=-45dB, hysteresis=6dB, attack=1ms, release=30ms |
| Compressor | 1 | NO | (off -- Green didn't use compression) |
| Wah | 2 | NO | |
| Boost | 3 | NO | |
| Overdrive | 4 | NO | |
| AmpModel | 5 | YES | inputGain=0.9, outputLevel=0.65, modelType=3 (Plexi), variac=0.0 |
| CabinetSim | 6 | YES | cabinetType=4 (Greenback), mix=1.0 |
| ParametricEQ | 7 | YES | *See out-of-phase config below* |
| Chorus | 8 | NO | |
| Vibrato | 9 | NO | |
| Phaser | 10 | NO | |
| Flanger | 11 | NO | |
| Delay | 12 | NO | |
| Reverb | 13 | NO | (dry tone, no reverb) |
| Tuner | 14 | YES | |
| Tremolo | 15 | NO | |
| BossDS1 | 16 | NO | |
| ProCoRAT | 17 | NO | |
| BossDS2 | 18 | NO | |
| BossHM2 | 19 | NO | |
| Univibe | 20 | NO | |
| FuzzFace | 21 | NO | |
| Rangemaster | 22 | YES | range=0.6, volume=0.65 (moderate treble boost to drive amp harder) |
| BigMuff | 23 | NO | |
| Octavia | 24 | NO | |
| RotarySpeaker | 25 | NO | |

**ParametricEQ "Out-of-Phase" Configuration (param IDs from EffectRegistry):**
- Param 0 (Low Freq): 200Hz
- Param 1 (Low Gain): -12dB (max cut -- simulates fundamental cancellation. Limited by -12dB max)
- Param 2 (Mid Freq): 850Hz
- Param 3 (Mid Gain): +10dB (strong nasal honk peak)
- Param 4 (Mid Q): 2.5 (narrow peak for the characteristic "honk")
- Param 5 (High Freq): 4000Hz
- Param 6 (High Gain): -8dB (treble rolloff from partial cancellation)
NOTE: Low band max cut is -12dB. Real out-of-phase cancellation can be -20dB+ in the bass. This is a limitation but acceptable -- the mid peak emphasis compensates perceptually.

**AmpModel Settings Rationale:**
- inputGain=0.9: The Rangemaster is adding gain, so the amp input doesn't need to be extreme. With Plexi preGain=5.0, effective gain is 5.0 * 0.9 = 4.5x, which through the Rangemaster's additional boost puts us solidly into crunch territory without squaring off (our amp audit found inputGain>1.0 on Plexi = square wave)
- modelType=3 (Plexi): The Marshall 1959 Super Lead is the correct model
- variac=0: Green did not use reduced voltage
- outputLevel=0.65: The out-of-phase EQ reduces overall level; compensate with moderate output

**Rangemaster Settings:**
- Range: 0.6 (moderate treble boost emphasis)
- Volume: 0.65 (enough output to push amp into saturation without squaring off)
- This emulates the possible Rangemaster or the effect of cranking the guitar volume into a hot amp
- The Rangemaster's 4.7nF input cap provides treble emphasis (~4.8kHz HPF) which partially counteracts the out-of-phase treble rolloff, preserving some edge and bite

**Character**: Dark, nasal, aggressive, buzzy midrange distortion, minimal bass, minimal treble. The riff should sit in a narrow frequency band around 500Hz-2kHz with heavy saturation.

### 8.2 Preset 2: "Ethereal Instrumental" -- Clean, Warm, Atmospheric

**Concept**: The "Albatross" instrumental tone. Neck pickup (no out-of-phase), clean amp, warm, glassy, with subtle reverb.

**Signal Chain** (default order, no reordering needed):
```
NoiseGate -> Compressor -> AmpModel [Fender Twin or Clean] -> CabinetSim [Combo or British] -> Reverb
```

**Effect Settings**:

| Effect | Index | Enabled | Parameters |
|--------|-------|---------|------------|
| NoiseGate | 0 | YES | threshold=-50dB, hysteresis=6dB, attack=0.5ms, release=50ms |
| Compressor | 1 | YES | threshold=-22dB, ratio=2.5, attack=20ms, release=200ms, makeup=4dB, knee=8dB, character=OTA (1) |
| Wah | 2 | NO | |
| Boost | 3 | NO | |
| Overdrive | 4 | NO | |
| AmpModel | 5 | YES | inputGain=0.35, outputLevel=0.7, modelType=7 (Fender Twin), variac=0.0 |
| CabinetSim | 6 | YES | cabinetType=0 (Combo 1x12), mix=1.0 |
| ParametricEQ | 7 | YES | *See warm neck pickup config below* |
| Chorus | 8 | NO | |
| Vibrato | 9 | NO | |
| Phaser | 10 | NO | |
| Flanger | 11 | NO | |
| Delay | 12 | NO | |
| Reverb | 13 | YES | decay=3.5s, damping=0.55, preDelay=25ms, size=0.75 |
| Tuner | 14 | YES | |
| Tremolo | 15 | NO | (off -- but could be enabled for experimentation at rate=3Hz, depth=0.15) |
| All pedals 16-25 | | NO | |

**ParametricEQ "Neck Pickup, Volume Rolled Back" Configuration:**
- Param 0 (Low Freq): 120Hz
- Param 1 (Low Gain): +2dB (gentle bass warmth from neck pickup)
- Param 2 (Mid Freq): 2500Hz
- Param 3 (Mid Gain): -3dB (slight presence scoop for glassy quality)
- Param 4 (Mid Q): 1.5 (moderate width)
- Param 5 (High Freq): 5000Hz
- Param 6 (High Gain): -5dB (treble rolloff simulating neck pickup + tone pot at 5-6)

**AmpModel Settings Rationale:**
- inputGain=0.35: Very low gain -- the amp should be clean and warm, no breakup
- modelType=7 (Fender Twin): The glassy, scooped clean character matches "Albatross" better than Marshall voicings. The Twin's 6L6GC headroom and scooped mids produce the right clean tone.
- Alternative: modelType=0 (Clean) could also work, but the Twin's specific EQ curve is a better match

**CabinetSim Settings:**
- cabinetType=0 (Combo 1x12): If Green used a Fender amp, this matches. If Marshall, use type=4 (Greenback). The combo type gives a more open, intimate sound appropriate for the track.
- Alternative: type=2 (Open Back 2x12, Vox-style) could approximate the Bluesbreaker combo's open-back character

**Reverb Settings:**
- decay=3.5s: Medium-long reverb tail for the ethereal, oceanic quality
- damping=0.55: Moderate damping so the reverb is warm, not bright
- preDelay=25ms: Slight pre-delay separates the dry signal from the reverb, keeping clarity
- size=0.75: Large room feel
- wetDry mix=0.30: Subtle! The reverb should be felt, not heard as an obvious effect

**Compressor Settings Rationale:**
- Using OTA character (1) for a smoother, more musical compression that simulates the natural tube compression of a Fender clean channel
- Low ratio (2.5:1) and gentle threshold (-22dB) provide subtle sustain enhancement without obvious squashing
- This helps emulate the natural compression of a tube amp running at moderate volume

**Character**: Warm, glassy, clean, sustaining, with a subtle reverb wash. Notes should ring clearly with a round, singing quality. No distortion, no grit -- pure warm clean tone.

### 8.3 Preset 3: "Emotional Blues" -- Vocal, Expressive, Dynamic

**Concept**: The "Need Your Love So Bad" / "Man of the World" lead tone. Out-of-phase pickups at moderate drive, creating the vocal, crying quality. This is the tone that made Peter Green legendary.

**Signal Chain** (reordered to put EQ before amp):
```
NoiseGate -> ParametricEQ [pickup sim] -> AmpModel [Crunch] -> CabinetSim [Greenback] -> Reverb (subtle)
```

**Effect Settings**:

| Effect | Index | Enabled | Parameters |
|--------|-------|---------|------------|
| NoiseGate | 0 | YES | threshold=-48dB, hysteresis=6dB, attack=0.5ms, release=40ms |
| Compressor | 1 | NO | (no compression -- dynamics are essential for this tone) |
| Wah | 2 | NO | |
| Boost | 3 | NO | |
| Overdrive | 4 | NO | |
| AmpModel | 5 | YES | inputGain=0.65, outputLevel=0.6, modelType=1 (Crunch), variac=0.0 |
| CabinetSim | 6 | YES | cabinetType=4 (Greenback), mix=1.0 |
| ParametricEQ | 7 | YES | *See out-of-phase (warmer variant) config below* |
| Chorus | 8 | NO | |
| Vibrato | 9 | NO | |
| Phaser | 10 | NO | |
| Flanger | 11 | NO | |
| Delay | 12 | NO | |
| Reverb | 13 | YES | decay=2.0s, damping=0.5, preDelay=15ms, size=0.5 |
| Tuner | 14 | YES | |
| All pedals 15-25 | | NO | |

**ParametricEQ "Out-of-Phase Warm Variant" Configuration:**
- Param 0 (Low Freq): 180Hz
- Param 1 (Low Gain): -10dB (less aggressive bass cut than Manalishi -- more warmth retained)
- Param 2 (Mid Freq): 900Hz
- Param 3 (Mid Gain): +7dB (the nasal peak, slightly less extreme, slightly higher center freq)
- Param 4 (Mid Q): 2.0 (slightly wider than Manalishi for more musicality)
- Param 5 (High Freq): 3500Hz
- Param 6 (High Gain): -5dB (moderate treble rolloff -- more treble than Manalishi for expressiveness)

**AmpModel Settings Rationale:**
- inputGain=0.65: Moderate gain -- the amp should be on the edge of breakup. Light picking = clean, hard picking = singing sustain with mild distortion. With Crunch preGain=4.0, effective gain is 4.0 * 0.65 = 2.6x, which is in the sweet spot for edge-of-breakup dynamics.
- modelType=1 (Crunch): The Marshall crunch voicing at moderate gain best captures the edge-of-breakup quality. The Crunch model's mid-forward EQ (+3dB midGainDb) complements the out-of-phase mid peak.
- Alternative: modelType=3 (Plexi) at lower inputGain could also work, but the Crunch voicing's lower preGain (4.0 vs 5.0) gives more dynamic range before full saturation.

**Reverb Settings:**
- decay=2.0s: Short-to-medium reverb -- more subtle than Albatross
- damping=0.5: Warm reverb
- preDelay=15ms
- size=0.5: Medium room
- wetDry mix=0.20: Very subtle -- just a hint of ambience

**Character**: Vocal, nasal, hollow, but with more warmth and dynamic range than Manalishi. Should respond dramatically to playing dynamics: clean up on soft picking, sing with sustain on harder playing. The midrange "honk" should be present but musical, not harsh. This is the most dynamically responsive of the three presets.

---

## APPENDIX A: EFFECT PARAMETER REFERENCE (from EffectRegistry)

### ParametricEQ Parameters (Index 7)
- Param 0: Low Freq (20-500 Hz, default 100Hz) -- low shelving band center
- Param 1: Low Gain (-12 to +12 dB, default 0) -- low shelving gain
- Param 2: Mid Freq (200-5000 Hz, default 1000Hz) -- mid parametric center
- Param 3: Mid Gain (-12 to +12 dB, default 0) -- mid parametric gain
- Param 4: Mid Q (0.1-10, default 1.0) -- mid parametric bandwidth
- Param 5: High Freq (2000-20000 Hz, default 5000Hz) -- high shelving center
- Param 6: High Gain (-12 to +12 dB, default 0) -- high shelving gain
NOTE: Low and High bands appear to be shelving filters (no Q parameter).
Only the Mid band has a Q control for narrow/wide bandwidth.

### AmpModel Parameters (Index 5)
- Param 0: inputGain (0.0-2.0)
- Param 1: outputLevel (0.0-1.0)
- Param 2: modelType (0-8 integer: 0=Clean, 1=Crunch, 3=Plexi, 7=Twin)
- Param 3: variac (0.0-1.0)

### Rangemaster Parameters (Index 22)
- Param 0: Range (0.0-1.0, default 0.5) -- treble boost amount
- Param 1: Volume (0.0-1.0, default 0.5) -- output level

### Reverb Parameters (Index 13)
- Param 0: decay (0.1-10.0 seconds)
- Param 1: damping (0.0-1.0)
- Param 2: preDelay (0-100 ms)
- Param 3: size (0.0-1.0)

### Compressor Parameters (Index 1)
- Param 0: Threshold (-60 to 0 dB)
- Param 1: Ratio (1-20 :1)
- Param 2: Attack (0.1-100 ms)
- Param 3: Release (10-1000 ms)
- Param 4: Makeup Gain (0-30 dB)
- Param 5: Knee Width (0-12 dB)
- Param 6: Character (0=VCA, 1=OTA)

---

## APPENDIX B: COMPARISON WITH OTHER "PETER GREEN TONE" ANALYSES

### Common Misconceptions

1. **"Peter Green used a Fuzz Face"**: FALSE. There is no evidence Green used a Fuzz Face or any fuzz pedal. His distortion came entirely from amp saturation.

2. **"The reversed pickup was an accident at the factory"**: UNCERTAIN. The most common story is that the neck pickup was removed for service/repair and reinstalled with the magnet reversed, either accidentally or deliberately. Some accounts say it was done by a repair technician; others suggest Green discovered the sound by accident and kept it. Regardless of origin, Green clearly loved the sound and made it his signature.

3. **"You need a Les Paul to get Peter Green's tone"**: PARTIALLY TRUE. The out-of-phase sound is specific to a two-humbucker guitar with one magnet reversed. However, the clean tones (Albatross) and the edge-of-breakup quality can be approximated with many guitars. The key is the out-of-phase middle position, which CAN be replicated with a mod to any two-humbucker guitar.

4. **"Peter Green's tone was all in his hands"**: PARTIALLY TRUE. Green's touch, vibrato, and phrasing are irreplaceable. But the out-of-phase pickup creates a specific frequency response that no amount of technique can replicate on a standard guitar. It is both the gear and the player.

5. **"Tremolo is essential to Peter Green's sound"**: FALSE for most of his work. Aside from possible subtle tremolo on a few recordings, Green's tone was tremolo-free. What people perceive as tremolo is usually his controlled vibrato technique.

### How Our Presets Compare to the Real Thing

**What we CAN capture:**
- The out-of-phase frequency response (via ParametricEQ approximation)
- The Marshall amp character (Plexi and Crunch voicings)
- The Greenback speaker character (CabinetSim)
- The reverb ambience (Dattorro plate)
- The gain staging and dynamic response
- The Rangemaster boost character (for Manalishi)

**What we CANNOT fully capture:**
- The exact comb-filter pattern of the out-of-phase pickups (our EQ is an approximation)
- The interaction between guitar volume pot and amp input impedance
- Green's vibrato technique and touch sensitivity
- The PAF humbucker's specific resonant frequency and magnetic field pattern
- The GZ34 tube rectifier sag in early JTM45s (our Plexi model does not model rectifier behavior separately)
- The specific acoustic properties of CBS Studios or Blue Horizon's recording rooms
- Danny Kirwan's complementary rhythm guitar parts that are part of the recorded sound

**Overall accuracy estimate:**
- Manalishi: ~75-80% (the out-of-phase EQ + Rangemaster + Plexi captures the essential character)
- Albatross: ~85% (clean tone with reverb is simpler to replicate; main gap is the specific amp character)
- Emotional Blues: ~70-75% (the out-of-phase + edge-of-breakup is the most demanding to replicate, and the most dependent on playing dynamics and technique)

---

## APPENDIX C: RECOMMENDED EFFECT CHAIN ORDER

For the Peter Green presets that use the out-of-phase pickup simulation, the ParametricEQ MUST be placed before the AmpModel in the signal chain. Our app supports effect reordering via setEffectOrder.

**Recommended reordered chain for Manalishi and Emotional Blues presets:**

```
Position 0:  NoiseGate (index 0)
Position 1:  Compressor (index 1)
Position 2:  ParametricEQ (index 7)     <-- MOVED from position 7
Position 3:  Wah (index 2)
Position 4:  Boost (index 3)
Position 5:  Rangemaster (index 22)     <-- MOVED from position 22
Position 6:  AmpModel (index 5)
Position 7:  CabinetSim (index 6)
Position 8:  Overdrive (index 4)
Position 9:  Chorus (index 8)
Position 10: Vibrato (index 9)
Position 11: Phaser (index 10)
Position 12: Flanger (index 11)
Position 13: Delay (index 12)
Position 14: Reverb (index 13)
Position 15: Tuner (index 14)
Position 16: Tremolo (index 15)
Position 17: BossDS1 (index 16)
Position 18: ProCoRAT (index 17)
Position 19: BossDS2 (index 18)
Position 20: BossHM2 (index 19)
Position 21: Univibe (index 20)
Position 22: FuzzFace (index 21)
Position 23: BigMuff (index 23)
Position 24: Octavia (index 24)
Position 25: RotarySpeaker (index 25)
```

The key moves:
1. ParametricEQ (7) moved to position 2 (before amp, after noise gate/compressor)
2. Rangemaster (22) moved to position 5 (before amp, acting as treble boost into Marshall input)

This correctly models the signal flow: guitar pickup (simulated by EQ) -> treble booster -> amplifier -> cabinet -> studio effects.

---

## APPENDIX D: FUTURE IMPROVEMENTS

### Priority 1: Dedicated Pickup Position Effect
A "Guitar Pickup" effect at position 0 (or -1, before the chain) that models:
- Pickup position: Neck / Middle (In-Phase) / Middle (Out-of-Phase / Peter Green) / Bridge
- Volume pot: 0-10 (with proper impedance interaction and treble bleed)
- Tone pot: 0-10 (with correct resonant peak shift based on 0.022uF cap + pickup impedance)
- This would dramatically improve all presets that depend on guitar control settings

### Priority 2: JTM45 Amp Voicing
A dedicated JTM45 voicing (separate from Plexi) that models:
- KT66 power tubes (different saturation character from EL34)
- GZ34 tube rectifier sag (the voltage sag under load that creates the "spongy" feel)
- Lower power (45W vs 100W) meaning earlier power amp breakup
- This would be more accurate for early Green tones than the 100W Plexi model

### Priority 3: Marshall 1962 Bluesbreaker Voicing
The Bluesbreaker combo is an iconic amplifier in its own right:
- 2x EL34, 30W (very early breakup)
- Open-back 2x12 cabinet (different dispersion from closed 4x12)
- Built-in tremolo circuit
- Used by Clapton (Beano album) and Green for more intimate tones
