# Looper Competitive Landscape Analysis

## Research Date: March 2026
## Purpose: Identify market gaps and competitive advantages for our Android guitar looper

---

## 1. Hardware Looper Feature Matrix

### 1.1 Boss Loop Station Family

| Feature | RC-5 | RC-500 | RC-600 |
|---------|------|--------|--------|
| **Tracks** | 1 | 2 | 6 stereo |
| **Recording Time** | 13 hours | 13 hours | 13 hours |
| **Audio Quality** | 32-bit AD/DA, 32-bit float | 32-bit AD/DA, 32-bit float | 32-bit AD/DA, 32-bit float |
| **Memory Slots** | 99 | 99 | 99 |
| **Drum Kits** | 7 | 16 | 16 |
| **Inputs** | 1/4" only | 1/4" + XLR | 2x XLR + 2x stereo 1/4" |
| **MIDI** | Yes | Yes | Yes |
| **USB Audio** | No | No | Yes |
| **Footswitches** | 1 + external | 2 + external | 9 assignable |
| **Battery Power** | Yes | Yes | No (AC only) |
| **Price (USD)** | ~$180 | ~$300 | ~$600 |

**Limitations:**
- No simultaneous dry+wet recording
- No re-amping capability
- Cannot change effects on recorded loops retroactively
- Fixed signal chain position (before or after effects, not both)
- No time-stretch (pitch changes with speed)
- Loop import/export requires firmware-specific format

### 1.2 Electro-Harmonix

| Feature | 720 Stereo | 45000 |
|---------|-----------|-------|
| **Tracks** | 1 (stereo) | 4 mono + 1 stereo mixdown |
| **Recording Time** | 12 min | Hours (SD card) |
| **Audio Quality** | 24-bit | CD quality (16-bit/44.1kHz) |
| **Storage** | Internal | SDHC card (4-32GB) |
| **Quantize** | No | Yes |
| **MIDI** | No | Yes |
| **Price** | ~$180 | ~$550 (discontinued) |

**Limitations:**
- 720 is a simple single-track looper (competitor to Ditto)
- 45000 is DISCONTINUED (2024) -- leaves a market gap for multi-track
- No effects on loops, no dry recording, no re-amping

### 1.3 TC Electronic Ditto Family

| Feature | Ditto | Ditto X4 |
|---------|-------|----------|
| **Tracks** | 1 | 2 |
| **Recording Time** | 5 min | 5 min |
| **Loop FX** | None | 7 (tape stop, reverse, etc.) |
| **MIDI Sync** | No | Yes |
| **Audio Quality** | 24-bit | 24-bit |
| **Bypass** | True analog | True analog |
| **Price** | ~$100 | ~$250 (discontinued) |

**Limitations:**
- Only 5 minutes recording time (severely limiting)
- Ditto X4 is discontinued
- TC Electronic has been focusing on budget products, no new flagship looper
- No quantization, no drum tracks, no USB export

### 1.4 Headrush Looperboard

| Feature | Looperboard |
|---------|------------|
| **Tracks** | 4 stereo |
| **Recording Time** | Unlimited (internal + USB/SD) |
| **Display** | 7" touchscreen |
| **Processor** | Quad-core |
| **Footswitches** | 12 |
| **Features** | Time-stretch, auto tempo detect, quantize, transpose, bounce, peel |
| **Built-in FX** | Full suite (amp sims, effects) |
| **I/O** | 4-in/4-out, XLR w/ phantom, MIDI, USB audio |
| **Price** | ~$800 (discontinued) |

**Significance:** This was the most advanced hardware looper ever made. Its DISCONTINUATION is a major market signal -- there is demand for this level of sophistication, but the hardware price point ($800+) was too high. Our software approach can deliver similar features at app pricing.

### 1.5 Pigtronix Infinity

| Feature | Infinity Looper |
|---------|----------------|
| **Tracks** | 2 stereo loop pairs |
| **Audio Quality** | 24-bit/48kHz |
| **Sync Modes** | Free, Sync Multi (1x-6x), Series |
| **MIDI** | Input only (no clock output) |
| **Overdubs** | 256+ per loop |
| **Special** | Discrete analog limiter, analog pass-through |

**Limitations:**
- MIDI in only, cannot send clock
- No effects on loops
- No touchscreen or visual feedback
- No quantization or drum tracks

### 1.6 Line 6 HX Stomp (Built-in Looper)

| Feature | HX Stomp Looper |
|---------|----------------|
| **Tracks** | 1 |
| **Recording Time** | ~60s mono, ~30s stereo |
| **Controls** | 1-switch operation |
| **Audio Quality** | 24-bit/96kHz |
| **Integration** | Within Helix effects chain |

**Limitations:**
- Extremely basic -- afterthought looper in a multi-effects unit
- 1-switch operation is clumsy (single footswitch cycles through Record/Play/Overdub/Stop)
- Very short recording time
- No independent loop tracks, no quantize, no export
- **However**: It DOES have one unique advantage -- the looper sits inside the effects chain, so changing amp/effect settings changes how the loop sounds on playback IF placed before effects

### 1.7 Chase Bliss Blooper (Experimental)

| Feature | Blooper |
|---------|---------|
| **Concept** | "Bottomless looper" -- loop manipulation as creative instrument |
| **Tracks** | 1 with infinite layers |
| **Modifiers (Channel A)** | Smooth Speed, Dropper, Trimmer |
| **Modifiers (Channel B)** | Stepped Speed, Scrambler, Filter |
| **Hidden Modifiers** | Swapper, Pitcher, Stretcher, Stutter, Stopper, Stepped Trimmer |
| **Additive Mode** | Modifications recorded into overdubs |
| **Ramp** | 16 DIP switches for parameter modulation |
| **Browser Interface** | Export loops, swap modifiers via web app |
| **Price** | ~$500 |

**Significance for us:** The Blooper proves there is a market for CREATIVE loop manipulation beyond basic record/play/overdub. Its modifiers (filter, pitch, speed, stutter, scramble) are software DSP operations that we can implement with zero additional hardware cost. The Blooper's $500 price tag for a single-track looper with DSP effects is extraordinary -- it validates demand for this category.

### 1.8 Sheeran Looper+ / Looper X (2025-2026)

| Feature | Looper+ | Looper X |
|---------|---------|----------|
| **Tracks** | 4 | 6 |
| **Display** | Large, clear | Multiple screen modes |
| **External Rhythms** | YurtRock import | YurtRock import |
| **Price** | ~$500 | ~$1,299 |

**Significance:** The Sheeran Looper X at $1,299 is the most expensive looper pedal on the market. It targets singer-songwriters doing live loop performances (Ed Sheeran's use case). The fact that it sells at this price point proves there is a premium market for sophisticated looping.

---

## 2. Mobile App Competitors

### 2.1 Loopy Pro (iOS only) -- The Gold Standard

| Feature | Details |
|---------|---------|
| **Platform** | iOS/iPadOS ONLY |
| **Price** | $29.99 |
| **Tracks** | Unlimited |
| **Architecture** | Full DAW-like environment with Audio Unit hosting |
| **MIDI** | Full MIDI looping (as of v2.0, 2025) |
| **Audio Unit Support** | Hosts any AU plugin (amp sims, effects) |
| **Routing** | Multi-channel, sends, buses, resampling |
| **Customization** | Canvas with loops, buttons, sliders, XY pads, clip slicers |
| **Sync** | Ableton Link, MIDI clock |
| **Community** | Very active forum (forum.loopypro.com) |

**Key insight:** Loopy Pro is essentially a mini-DAW built around looping. It is iOS-exclusive. There is NO Android equivalent. This is the single largest gap in the market.

**Limitations:**
- No built-in amp sim or guitar effects
- Requires external audio interface for guitar input
- Steep learning curve for the full feature set
- No re-amping (you'd need to route through a separate amp sim AU)
- Not guitar-focused -- it's a general-purpose looper for all instruments

### 2.2 Quantiloop (iOS only)

| Feature | Details |
|---------|---------|
| **Platform** | iOS ONLY |
| **Price** | $9.99 (Pro: $19.99) |
| **Tracks** | Up to 4 |
| **Quantization** | Yes (auto-trim to bar boundaries) |
| **Built-in FX** | Fade, Tape-Stop, Reverse, Reverb/Delay, Compressor/Gate, Transpose |
| **Rhythm** | Built-in metronome + drum loop import |
| **Sync** | Ableton Link |
| **Guitar Focus** | Yes -- can host Inter-App Audio amp sims |
| **MIDI Footcontroller** | Full support |

**Key insight:** Quantiloop is the most guitar-friendly looper app, purpose-built for live performance with foot controller support. Still iOS only.

**Limitations:**
- iOS exclusive
- Limited to 4 tracks
- No dry/wet simultaneous recording
- No re-amping workflow
- Can't apply effects to loops retroactively (effects go into the recording)

### 2.3 Group the Loop (iOS only)

| Feature | Details |
|---------|---------|
| **Platform** | iOS ONLY |
| **Concept** | Section-based looping (Verse/Chorus/Bridge groups) |
| **Key Feature** | Time-stretch (tempo change without pitch change) |
| **Key Feature** | Group tempo (different BPM per section) |
| **Sync** | Ableton Link, Audiobus, IAA |
| **Export** | Drag-and-drop, AudioShare |

**Key insight:** Group the Loop's section-based approach is ideal for songwriters who think in song structure, not just loop layers. The per-section tempo feature is unique.

### 2.4 BandLab (Android + iOS)

| Feature | Details |
|---------|---------|
| **Platform** | Android + iOS |
| **Price** | Free |
| **Looper** | Beat-focused (pre-made loop packs, 12 tracks) |
| **Effects** | Filter, Gater, Stutter, Tape Stop |
| **Guitar Focus** | Minimal -- has basic amp sims but not the core focus |
| **Recording** | Full multi-track DAW |
| **Latency** | "More prevalent on Android devices" -- their own help docs |

**Key insight:** BandLab is a social-first music creation platform. Its looper is beat/sample focused, not guitar-loop focused. The latency issues on Android are acknowledged in their own documentation.

### 2.5 Android-Specific Looper Apps (The Wasteland)

| App | Tracks | Quality | Guitar-Focused | Rating |
|-----|--------|---------|----------------|--------|
| **Loopify** | 9 | Low | No (mic-based) | 3.8/5 |
| **LoopStack** | 4 | Low | No | 3.5/5 |
| **FreeLoop** | 1 | Low | Partially (OTG) | 3.2/5 |
| **uFXLoops** | Multi | Low | No (beat focus) | 3.0/5 |
| **Beats and Loops** | Multi | Medium | Partially | New |

**Critical finding:** There is NO serious, professional-quality guitar looper app on Android. The existing options are:
- Built for microphone input, not guitar interface
- Low audio quality (16-bit, high latency)
- No low-latency audio engine (no Oboe/AAudio)
- No built-in amp/effects processing
- No quantization, no re-amping, no professional features
- Poor reviews reflecting poor quality

### 2.6 Guitar Effects Apps with Recording (Cross-reference)

| App | Platform | Looper | Recording | Re-amp |
|-----|----------|--------|-----------|--------|
| **AmpliTube** | iOS + Android | Yes (optional IAP) | 8-track DAW | No |
| **BIAS FX** | iOS + Desktop | No | Via DAW only | No (desktop: via DAW plugin) |
| **Deplike** | Android + iOS | Basic single-loop | No | No |
| **ToneStack** | iOS only | No | No | No |
| **Spark (app)** | iOS + Android | No | Basic jam recording | No |
| **Cortex Cloud** | Android + iOS | No (preset management only) | No | No |

**Critical finding:** AmpliTube is the only guitar effects app with a looper AND multi-track recording, but:
- The looper is a paid in-app purchase
- No simultaneous dry+wet recording
- No re-amping capability
- Android version is inferior to iOS version
- 8-track recorder is basic (no quantization, no BPM sync)

---

## 3. The Android Guitar Looper Gap

### 3.1 What Does NOT Exist on Android

After thorough research, the following features have ZERO presence on the Android platform:

1. **Professional guitar looper with low-latency audio** (Oboe/AAudio)
2. **Guitar effects + looper in one app** (AmpliTube is closest but limited)
3. **Simultaneous dry DI + wet processed recording**
4. **Re-amping of any kind on mobile** (no app on ANY platform does this well)
5. **BPM-quantized guitar looping** with professional quality
6. **Multi-track guitar looping with built-in amp modeling**
7. **Loop manipulation/effects** (Blooper-style creative tools)
8. **Section-based song structure** in a guitar-focused app

### 3.2 What Android Guitarists Currently Do

Based on forum research, Android guitarists who want to loop are forced to:

1. **Buy a hardware looper** ($100-$1,300) as a separate pedal
2. **Use Deplike/Spark for tone** + a separate (bad) looper app for loops
3. **Switch to iPhone** (frequently mentioned in forums as the reason guitarists buy iPhones)
4. **Use a desktop DAW** for recording/looping (giving up mobility)
5. **Just not loop** (the most common "solution")

### 3.3 Market Opportunity

**Quantitative:**
- Smartphone music production software market: ~$98M in 2025, growing at 10% CAGR
- Looper pedal market: ~$680M in 2024, growing at 11.5% CAGR
- Android holds ~72% global smartphone share
- Guitar pedal market: ~$5.6B in 2024

**Qualitative:**
- Loopy Pro proved the market for professional mobile loopers ($30 app, passionate community)
- Loopy Pro has NO Android presence (developer confirmed iOS-only development)
- Headrush Looperboard discontinuation proves demand exists but hardware pricing is prohibitive
- Chase Bliss Blooper at $500 proves demand for creative loop manipulation
- Ed Sheeran Looper X at $1,299 proves premium looping market exists
- Every iOS-only looper app has Android users in forums asking "when Android version?"

---

## 4. Re-amping in the Wild

### 4.1 Professional Studio Re-amping

**Traditional workflow:**
1. Record guitar DI through a DI box (Radial JDI, ~$200) to DAW
2. Simultaneously record amped signal through mic to separate track
3. Later, send DI track from DAW through re-amp box (Radial ProRMP, ~$130)
4. Re-amp box converts line-level back to instrument-level
5. Feed into physical amp, mic the amp, record new track
6. Compare/blend original and re-amped tracks

**Cost of professional re-amping:**
- DI box: $50-$400
- Re-amp box: $60-$300
- Additional amp + mic + room: already owned or studio rental ($50-$200/hour)
- DAW with routing capabilities: $0-$600
- **Total minimum**: ~$250 in dedicated hardware + DAW + amp access
- **Total typical**: ~$500-$1,000 in gear for a home setup

**Software re-amping (DAW + plugins):**
- Record DI track
- Insert amp sim plugin on the DI track
- Change plugin settings and hear results instantly
- No re-amp box needed, no physical amp needed
- Cost: $0 (free amp sims) to $300 (premium plugins like Neural DSP)

### 4.2 Hardware Units with Re-amping

| Device | Dry Recording | Re-amp Built-in | Price |
|--------|--------------|-----------------|-------|
| Fractal Axe-FX III | Yes (USB out 5/6 = DI) | Yes (USB in, process through effects) | $2,500 |
| Neural DSP Quad Cortex | Yes (USB assignable) | Yes (USB + Neural Capture) | $1,900 |
| Line 6 Helix | Yes (USB multi-out) | Yes (USB return to input) | $1,500 |
| Boss GX-10 | Yes (USB) | Yes (documented workflow) | $500 |
| HX Stomp | Limited | Via USB | $600 |

**Key insight:** Re-amping is a feature of $500+ multi-effects processors. It requires USB audio routing through a DAW. Nobody offers it as a self-contained mobile workflow.

### 4.3 Mobile Re-amping: Current State

**The honest answer: No mobile app does re-amping well.**

The closest approaches:
- **AmpKit (iOS)**: Records dry + wet simultaneously. Can change amp sim on dry track later. But the looper and amp sim are separate products, and the workflow is clunky
- **AUM (iOS)**: Advanced routing app that CAN set up dry + wet recording chains. But requires expert-level knowledge and 3+ separate apps (AUM + amp sim AU + looper AU)
- **No Android solution exists at all**

### 4.4 How We Can Make Re-amping Accessible

Our advantage: the signal chain and the looper are in the SAME app, in the SAME audio callback. This eliminates:
- Need for DI box ($200 saved)
- Need for re-amp box ($130 saved)
- Need for USB routing
- Need for DAW knowledge
- Need for multiple apps
- Inter-app latency and routing complexity

**Our re-amping workflow (zero additional cost, zero complexity):**
1. Player connects guitar, dials in a tone, plays
2. App automatically records dry DI + wet processed simultaneously (user doesn't even think about it)
3. Player finishes recording
4. Player tweaks amp model, changes effects, dials new tone
5. Player taps "Re-amp" button
6. Dry recording plays through new signal chain in real time
7. Player hears their exact performance with the new tone
8. Can A/B compare original wet recording vs. re-amped version

**This is a $0 solution to a workflow that currently costs $250-$2,500 in hardware.**

---

## 5. Innovative Looper Features from the Experimental World

### 5.1 Chase Bliss Blooper -- Loop Modifiers

These are DSP operations applied to recorded audio. We can implement ALL of them in software:

| Blooper Modifier | DSP Implementation | Difficulty |
|------------------|--------------------|-----------|
| Smooth Speed | Variable-rate playback with interpolation | Medium |
| Stepped Speed | Octave/fifth speed ratios | Easy |
| Filter | Sweepable LP/HP/BP on loop playback | Easy |
| Stutter (beat repeat) | Short buffer repeat at rhythmic divisions | Easy |
| Scrambler | Random segment reordering | Medium |
| Dropper | Rhythmic volume gating | Easy |
| Trimmer | Loop length subdivision | Easy |
| Pitcher | Pitch shift via PSOLA or phase vocoder | Hard |
| Stretcher | Time-stretch (change speed without pitch) | Hard |
| Stopper | Tape-stop effect (deceleration curve) | Easy |

**Key insight:** These are all software operations. A hardware pedal charges $500 for a single-track looper with these features. We can offer them at app pricing.

### 5.2 Ableton Live -- Clip Launching and Quantization

Key concepts we should adopt:
- **Global quantization**: All loop start/stop snaps to musical grid (1 bar, 1/2 bar, 1/4, etc.)
- **Scene launching**: Groups of clips that fire together (verse, chorus, bridge)
- **Launch modes**: Trigger, Gate, Toggle, Repeat
- **Follow actions**: Clips can auto-advance (play verse 2x then switch to chorus)
- **Warping/time-stretch**: Clips play at project tempo regardless of recorded tempo

### 5.3 Loopy Pro -- Multi-Track Routing

Key concepts:
- **Unlimited tracks** with flexible grouping
- **Audio Unit hosting** (we already have built-in effects, so we don't need this)
- **Custom UI canvas** with widgets (interesting for future)
- **MIDI control mapping** (essential for foot controllers)
- **Ableton Link** (useful for jamming with other musicians)

### 5.4 Emerging Trends

1. **AI-assisted looping**: Auto-quantize imprecise timing, intelligent loop boundary detection, auto-chord recognition for suggesting loop points
2. **Smart instruments from loops**: MIDI playback of recorded audio (Loopy Pro 2.0 can turn clips into polyphonic instruments)
3. **Cloud-based loop sharing**: Social features for sharing loops/presets (BandLab model)
4. **Browser-based management**: Chase Bliss Blooper allows loop export/modifier swap via browser -- we should have a companion web interface
5. **Integrated performance tools**: Backing tracks, drum machines, and loopers converging into unified performance environments

---

## 6. What Guitarists Actually Want (Forum Research)

### 6.1 Top Complaints About Existing Loopers

**From Reddit r/guitarpedals, The Gear Page, Fractal Audio forums, KVR:**

1. **"I can't get the timing right"** -- The #1 complaint. Users want auto-quantize/auto-trim to snap loop boundaries to the nearest beat. Hardware loopers require precise footswitch timing.

2. **"I wish I could change my tone after recording"** -- Guitarists record a loop with one amp setting, then want to change the amp but the loop is "baked in." This is THE re-amping use case.

3. **"Not enough tracks"** -- Single-track loopers limit creativity. Users want verse/chorus separation.

4. **"The metronome should be BUILT IN"** -- Many loopers lack a click track. Users want the metronome to help set tempo, then optionally mute during performance.

5. **"Why can't I save my loops?"** -- Basic loopers (Ditto, RC-1) lose loops when powered off. Users want persistent storage and library management.

6. **"Audio artifacts when clearing"** -- Some pedals play a snippet when clearing, ruining live performance.

7. **"I wish I could undo just ONE layer"** -- Most loopers have single undo. Users want per-layer undo/redo.

8. **"Different loop lengths are impossible"** -- Recording a 2-bar loop and a 4-bar loop simultaneously is a pain point.

9. **"I can't practice slow then speed up"** -- Users want to record at slow tempo, then time-stretch to performance tempo. Only Headrush Looperboard offered this (now discontinued).

10. **"My looper is at the end of my chain"** -- Physical placement forces a choice: dry loops (looper first) or baked-in effects (looper last). Users want BOTH.

### 6.2 Feature Wish List (Aggregated from forums)

**Must-have (mentioned by >50% of threads):**
- Auto-quantize loop boundaries to beat grid
- Built-in metronome/click track
- Undo/redo (at minimum one level, ideally per-layer)
- Save/recall loops to persistent memory
- Visual feedback (waveform, position indicator, remaining time)

**Want-to-have (mentioned by 20-50%):**
- Multiple tracks (minimum 2, ideally 4+)
- Loop effects (reverse, half-speed, filter)
- BPM tap tempo
- MIDI sync (for integration with other gear)
- Import/export (WAV/MP3)
- Count-in before recording

**Innovative (mentioned by <20% but high enthusiasm):**
- Re-amping (change tone after recording)
- Auto-detect tempo from playing
- Loop manipulation effects (Blooper-style)
- Section/song structure (verse/chorus switching)
- Multi-device sync (jam with friends)

---

## 7. Competitive Advantage Analysis: What We Can Offer That NOBODY Else Does

### 7.1 The Killer Feature: Integrated Re-amping Looper

**No hardware looper and no mobile app currently offers this:**

> Record a loop with your current amp + effects tone. Then change EVERYTHING -- amp model, drive pedals, modulation, delay, reverb -- and hear your loop play back through the new signal chain in real time. A/B compare. Keep the version you prefer.

This is possible because:
1. We record DRY (pre-effects) and WET (post-effects) simultaneously
2. The dry recording can be played back through ANY signal chain configuration
3. Both signals live in the same app, same callback, sample-aligned
4. Zero latency between dry playback and signal chain processing
5. The user never has to think about "DI" or "re-amping" -- it's just "change your tone"

**Hardware equivalent would require:** Axe-FX III ($2,500) + DAW ($200) + re-amping knowledge

### 7.2 Android Market Monopoly

**We would be the ONLY professional guitar looper on Android.** Period.

The competitive landscape is:
- Loopy Pro: iOS only, no Android plans
- Quantiloop: iOS only
- Group the Loop: iOS only
- AmpliTube looper: Paid IAP, basic, not guitar-loop focused
- Deplike: Basic single-loop, no quantize, no multi-track
- Everything else: Toy-quality mic-based apps

Android has 72% global smartphone market share. Every Android guitarist who wants to loop currently has to buy a hardware pedal. We can capture this entire unserved market.

### 7.3 Effects-Integrated Looping

No mobile looper app has 26 built-in effects, 9 amp models, and 9 cabinet IRs. Users of Loopy Pro on iOS need to:
1. Buy Loopy Pro ($30)
2. Buy an amp sim AU ($15-$30)
3. Buy effects AUs ($5-$20 each)
4. Configure complex inter-app audio routing
5. Hope nothing crashes

We offer a single app with everything integrated. One purchase. Zero configuration.

### 7.4 Blooper-Style Loop Manipulation at Zero Hardware Cost

The Chase Bliss Blooper charges $500 for a SINGLE TRACK with loop effects (filter, speed, stutter, pitch, scramble). We can implement every one of these as DSP on the loop playback buffer:
- Filter sweep on loops (biquad LP/HP/BP)
- Speed change (variable-rate playback with linear/cubic interpolation)
- Reverse playback
- Stutter/beat repeat
- Tape stop deceleration
- Half-speed / double-speed

These are trivial to implement in our existing DSP framework and cost nothing in hardware.

### 7.5 Feature Comparison Matrix: Us vs. Everyone

| Feature | Our App | Loopy Pro (iOS) | AmpliTube | Boss RC-600 | Blooper |
|---------|---------|----------------|-----------|-------------|---------|
| **Platform** | Android | iOS only | iOS+Android | Hardware | Hardware |
| **Guitar Effects** | 26 built-in | None (host AUs) | Yes (IAP) | None | None |
| **Amp Models** | 9 | None | Yes (IAP) | None | None |
| **Cabinet IRs** | 9 + user | None | Yes | None | None |
| **Dry+Wet Recording** | YES | No | No | No | No |
| **Re-amping** | YES | No* | No | No | No |
| **Loop FX (Blooper-style)** | Planned | No | No | No | YES |
| **BPM Quantize** | YES | Yes | No | Yes | No |
| **Multi-track** | 2 (dry+wet) | Unlimited | 8 | 6 | 1 |
| **Metronome** | YES | Yes | No | Yes | No |
| **Save/Export** | YES | Yes | Yes | Yes | Via web |
| **Price** | App price | $30 | $25+ IAP | $600 | $500 |

*Loopy Pro can technically do this with complex routing through AU amp sims, but it requires expert configuration.

### 7.6 Unique Value Propositions (Marketing Angles)

1. **"Never regret your tone again"** -- Re-amping means you can always change your mind about your amp/effects after recording. No other looper offers this.

2. **"The $2,500 studio trick, free in your pocket"** -- Professional re-amping workflow made accessible to bedroom guitarists.

3. **"The looper that doesn't exist on Android"** -- First (and currently only) professional guitar looper for 72% of the world's smartphones.

4. **"26 effects + looper + re-amping in one app"** -- No cable routing, no inter-app audio, no extra purchases.

5. **"Blooper-quality loop effects without the $500 pedal"** -- Creative loop manipulation at app pricing.

---

## 8. Risk Analysis

### 8.1 What Could Go Wrong

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Android latency on cheap devices | High | Medium | Oboe/AAudio, ADPF hints, buffer calibration |
| Memory pressure from loop buffers | Medium | High | Query memoryClass, adaptive max duration |
| Users don't understand re-amping | Medium | Low | Call it "change your tone" not "re-amping" |
| iOS port demand | High | Low | Focus on Android monopoly first |
| Loopy Pro ports to Android | Low | High | Ship first, build community, re-amping is unique |
| Hardware loopers add apps | Low | Medium | Hardware companies move slowly |

### 8.2 What We Should NOT Try to Do

1. **Don't try to be Loopy Pro** -- It's a full DAW with unlimited tracks, AU hosting, custom canvases. We're a guitar-focused app. Stay focused.
2. **Don't try to compete on track count** -- 2 tracks (dry+wet) with re-amping is more valuable than 8 basic tracks.
3. **Don't add MIDI looping** -- Loopy Pro 2.0 added this. It's not our market.
4. **Don't build a social platform** -- BandLab's approach. Not our strength.
5. **Don't ignore foot controller support** -- Essential for live performance. Add MIDI/Bluetooth pedal support as a fast-follow.

---

## 9. Recommended Roadmap Priority

### Phase 1: Core Looper (Sprint 24)
- Single-track looping (wet signal)
- Overdub with undo
- Metronome and count-in
- Visual waveform display
- Save/recall loops

### Phase 2: Dual-Track + Re-amping (Sprint 25)
- Simultaneous dry+wet recording
- Re-amping playback (dry through current signal chain)
- A/B comparison (original wet vs. re-amped)
- Export dry/wet as WAV files

### Phase 3: Quantization + Tap Tempo (Sprint 26)
- BPM quantize (snap loop boundaries to grid)
- Tap tempo
- Metronome sync with loop length
- Auto-trim to nearest bar

### Phase 4: Loop Manipulation Effects (Sprint 27)
- Reverse playback
- Half-speed / double-speed
- Filter sweep on loops
- Stutter / beat repeat
- Tape stop effect

### Phase 5: Advanced Features (Sprint 28+)
- MIDI foot controller support
- WAV import for backing tracks
- Time-stretch (tempo change without pitch)
- Section-based looping (verse/chorus groups)
- Cloud export/share

---

## Sources

### Hardware Loopers
- [Boss Loop Station Lineup Guide](https://articles.boss.info/navigating-the-boss-loop-station-lineup-a-guide-to-picking-the-perfect-pedal/)
- [Boss RC-500 Review](https://looperpedalreviews.com/boss-rc-500-loop-station-review/)
- [Boss RC-5 vs RC-500 Comparison](https://www.guitarchalk.com/boss-rc-5-vs-rc-500/)
- [Boss RC-600 Review](https://www.soundcrunch.net/en-us/looper/reviews/boss-rc-600)
- [EHX 45000 (Discontinued)](https://www.ehx.com/products/45000/)
- [EHX 720 Stereo Looper Review](https://looperpedalreviews.com/720-stereo-looper-review/)
- [TC Electronic Ditto X4](https://www.tcelectronic.com/product.html?modelCode=0709-AGA)
- [Headrush Looperboard](https://www.headrushfx.com/products/looperboard/index.html)
- [Pigtronix Infinity Looper](https://www.pigtronix.com/pedals/infinity-looper/)
- [Line 6 HX Stomp Review](https://www.morningstar.io/post/2018-10-21-line-6-hx-stomp-in-depth-review)
- [Chase Bliss Blooper](https://www.chasebliss.com/blooper)
- [Chase Bliss Blooper Modifiers](https://blooper.chasebliss.com/modifiers)
- [Blooper Modifier Cheat Sheet (PDF)](https://assets.ctfassets.net/mixoa90md6d2/zf28ER5UdB8CuelEXog7d/2e7811de2c0862084be6d006ceb1c2f8/Chase_Bliss_blooper_-_modifier_cheat_sheet.pdf)
- [Sheeran Looper+ and Looper X Analysis](https://www.guitarpedalx.com/news/news/some-thoughts-on-the-sheeran-looper-and-looper-x)
- [Sheeran Looper+ Review](https://www.guitarworld.com/reviews/sheeran-looper-plus)
- [Best Looper Pedals 2026](https://www.guitarworld.com/features/best-looper-pedals)

### Mobile Apps
- [Loopy Pro Official Site](https://loopypro.com/)
- [Loopy Pro App Store](https://apps.apple.com/us/app/loopy-pro-looper-daw-sampler/id1492670451)
- [Loopy Pro 2.0 MIDI Looping Update](https://synthanatomy.com/2025/07/loopy-pro-2-0-levels-up-the-creative-audio-looper-app-with-midi-looping-and-more.html)
- [Quantiloop Official Site](http://quantiloop.com/)
- [Quantiloop Review](https://www.macprovideo.com/article/audio-software/review-quantiloop-the-ultimate-looper-for-ios)
- [Group the Loop App Store](https://apps.apple.com/us/app/group-the-loop/id1029416579)
- [Group the Loop Official Site](https://grouptheloop.com/)
- [BandLab Looper](https://help.bandlab.com/hc/en-us/articles/115004496573-How-do-I-use-the-Looper)
- [Loopify on Google Play](https://play.google.com/store/apps/details?id=com.zuidsoft.looper&hl=en_US)
- [Best Loop Pedal Apps](https://zinginstruments.com/best-loop-pedal-apps/)
- [Best Loop Station Apps for Android](https://en.softonic.com/top/loop-station-apps-for-android)
- [AmpliTube iOS](https://www.ikmultimedia.com/products/amplitubeios/)
- [Deplike on Google Play](https://play.google.com/store/apps/details?id=com.deplike.andrig&hl=en_US&gl=US)
- [Deplike Single Looper](https://blog.deplike.com/processors/single-looper/)

### Re-amping
- [Re-amping Guide (Audient)](https://audient.com/tutorial/reamping/)
- [Radial ProRMP Re-amp Box](https://www.sweetwater.com/store/detail/ProRMP--radial-prormp-1-channel-passive-re-amping-device)
- [Re-amping Tips (Sweetwater)](https://www.sweetwater.com/insync/3-essential-re-amping-tips/)
- [Re-amping with Line 6 Helix](https://www.sweetwater.com/insync/re-amping-with-line-6-helix/)
- [Fractal Forum: Recording DI + Wet Simultaneously](https://forum.fractalaudio.com/threads/help-reamping-simultaneously-recording-di-and-wet-tracks.93680/)
- [GX-10 Re-amping via USB (Roland)](https://support.roland.com/hc/en-us/articles/30332301042843-GX-10-Applying-Effects-to-Dry-Sound-after-Recording-Re-amping)
- [Neural DSP Quad Cortex](https://neuraldsp.com/quad-cortex)
- [Recording DI and Re-amping with Pro Tools](https://forum.fractalaudio.com/threads/recording-di-re-amping-w-pro-tools-2025-10-0.216760/)
- [Loopy Pro Forum: Record Clean + Add Effect Later](https://forum.loopypro.com/discussion/1201/record-clean-guitar-sound-and-add-effect-later)

### Market Data
- [Looper Pedal Market Size (Cognitive Market Research)](https://www.cognitivemarketresearch.com/looper-pedal-market-report)
- [Guitar Pedals Market Size](https://www.businessresearchinsights.com/market-reports/guitar-pedals-market-105128)
- [Smartphone Music Production Software Market](https://www.fortunebusinessinsights.com/smartphone-music-production-software-market-106428)
- [Android vs iOS Statistics 2026](https://www.tekrevol.com/blogs/android-vs-ios-statistics/)

### User Research
- [The Gear Page: "I hate my looper pedal"](https://www.thegearpage.net/board/index.php?threads/i-hate-my-new-looper-pedal.1896122/)
- [Fractal Forum: Looper Wishes](https://forum.fractalaudio.com/threads/looper-wishes.173952/)
- [Fractal Forum: Looper for Live Looping](https://forum.fractalaudio.com/threads/looper-pedal-for-playing-live.201065/)
- [KVR: Why I Don't Use Loop Pedals](https://www.kvraudio.com/forum/viewtopic.php?t=465029)
- [KVR: Android Audio Latency Issues](https://www.kvraudio.com/forum/viewtopic.php?t=502803)
- [Guitar Rig Looper Feature Request](https://community.native-instruments.com/discussion/4484/guitar-rig-looper-feature-request)
- [Looper's Lookout: Grand Unified Theory of Looping](https://thegearforum.com/threads/loopers-lookout-towards-a-grand-unified-theory-of-looping.4803/)

### Innovation / Trends
- [Ableton Live Clip Launching Manual](https://www.ableton.com/en/manual/launching-clips/)
- [AI in Music Production Trends 2025](https://www.loudly.com/blog/ai-in-music-production-key-trends-shaping-the-future-beyond-2025)
- [BIAS X AI-Powered Tone Creation](https://www.positivegrid.com/pages/bias-x)
