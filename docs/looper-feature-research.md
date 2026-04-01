# Looper & Recording Feature Research
## Competitive Analysis, User Pain Points, and Feature Prioritization

---

## 1. What Guitarists Are Missing (And Don't Know They Need)

### 1.1 Findings from Hardware Loopers

**Boss RC-500/RC-5** ([Sweetwater Reviews](https://www.sweetwater.com/store/detail/RC500pedal--boss-rc-500-loop-station-compact-phrase-recorder-pedal/reviews), [Looper Pedal Reviews](https://looperpedalreviews.com/boss-rc-500-loop-station-review/)):
- 2 independent stereo tracks, 13 hours of recording, 57 rhythm patterns
- **Pain point**: Counter-intuitive interface. Users report spending more time fighting the UI than making music. The color LCD helps but the menu structure is still deep.
- **Pain point**: Drum tracks cannot be routed to separate outputs on the RC-5, so rhythm backing sounds thin through a guitar amp.
- **Pain point**: Rhythm track volume is extremely loud with no global volume setting -- users flood forums about this.
- **Lesson for us**: Volume controls for metronome/backing need to be prominent and persistent, not buried in settings.

**TC Electronic Ditto X4** ([TC Electronic](https://www.tcelectronic.com/product.html?modelCode=P0DE9), [Sweetwater Reviews](https://www.sweetwater.com/store/detail/DittoX4--tc-electronic-ditto-x4-looper-looper-pedal/reviews)):
- 2 loop channels (independent or synced), loop decay control, 7 onboard effects (tape stop, reverse, half-speed)
- **Key feature**: Loop decay -- determines how fast old layers fade during overdub, enabling evolving compositions
- **Key feature**: Dual-mode looping -- two loops can run independently or be ganged together
- **Lesson for us**: Loop decay / overdub feedback is a highly requested feature that separates creative loopers from basic ones.

**Headrush Looperboard** ([Sweetwater](https://www.sweetwater.com/store/detail/LooperBoard--headrush-looperboard-advanced-performance-looper-with-7-inch-touchscreen), [Amazon](https://www.amazon.com/Looperboard/dp/B07QY2Z734)):
- 4 stereo tracks, 7" touchscreen, 9 hours recording, quad-core DSP
- 5 track modes: fixed, serial, sync, serial/sync, free
- DAW-style waveform display with real-time position indicators
- 4 XLR inputs with phantom power, MIDI sync
- **Pain point**: Built-in guitar effects fall short of dedicated amp modelers. Serious tone chasers find them wanting.
- **Lesson for us**: Our existing 26-effect signal chain + 9 amp models is a massive advantage over standalone loopers.

**Chase Bliss Blooper** ([Chase Bliss](https://www.chasebliss.com/blooper)):
- "Bottomless looper" with 6 onboard modifiers (Pitcher, Smooth Speed, Trimmer, Scrambler, Filter, Stepped Speed)
- Stability effect: gradually introduces wow, flutter, noise, and filtering (tape degradation simulation)
- 8 levels of undo/redo as a KNOB (scrub through loop history)
- **Key feature**: Loop modifiers are the game-changer -- transform recorded loops with pitch, speed, glitch, and filter effects
- **Lesson for us**: Post-recording loop manipulation (reverse, half-speed, filter sweep) adds enormous creative value.

**Line 6 DL4 MkII** ([Line 6](https://line6.com/effects-pedals/dl4-mkii/), [Sweetwater](https://www.sweetwater.com/sweetcare/articles/line-6-dl4-mkii-looper-settings/)):
- Half-speed (doubles loop time at lower quality) and reverse playback
- Half-speed can be toggled DURING recording for unusual results
- 14 seconds + 800ms pre-delay
- **Lesson for us**: Half-speed and reverse are expected baseline features in any serious looper.

**Pigtronix Infinity 2, EHX Grand Canyon, Sheeran Looper+** ([various](https://www.pigtronix.com/pedals/infinity-2-double-looper/)):
- Common feature set: overdub decay, fade in/out, reverse, half-speed, solo/mute per track, undo/redo
- **Sheeran Looper+** adds: fade-in on record start (eliminates click from late button press)
- **Lesson for us**: Fade-in on record start is a tiny feature with outsized UX impact.

### 1.2 Findings from DAWs

**Ableton Live Session View** ([Ableton](https://help.ableton.com/hc/en-us/articles/360001431244-How-To-Looping-in-Session-View)):
- Fixed-length recording (1 beat to 32 bars, set in advance)
- "Drag and drop" loops from Looper into Session clips
- Scene-based arrangement: trigger groups of clips simultaneously
- Looper device: configurable overdub behavior, quantize start/stop to grid
- **Key insight**: The power of Ableton looping comes from QUANTIZATION OPTIONS -- record triggers can snap to beat, bar, or scene boundary. This eliminates timing mistakes.
- **Pain point**: Ableton's looper lacks visual feedback by default. Users install Max4Live patches to get waveform displays.

**GarageBand/Logic Pro**:
- Cycle recording: loop a section and record multiple takes, pick the best
- Comping: visual selection of best parts from multiple takes
- **Key insight**: "Comping" (choosing the best take from multiple passes) is a paradigm that doesn't exist in loopers but would be powerful for practice.

### 1.3 Findings from Mobile Apps

**Loopy Pro** ([Loopy Pro](https://loopypro.com/), [App Store](https://apps.apple.com/us/app/loopy-pro-looper-daw-sampler/id1492670451)):
- iOS only. Extremely powerful: up to 32 loops, AUv3 plugin hosting, MIDI, Ableton Link
- Version 2.0 adds MIDI looping and parameter automation
- Customizable canvas layout -- users design their own performance interface
- Metronome "count-in only" option (click only during countdown, not during recording)
- **Pain point**: iOS only. Android guitarists have NO equivalent.
- **Pain point**: Complexity -- users report spending weeks learning the interface before productive use.
- **Lesson for us**: Being the Loopy Pro of Android is a massive opportunity. Nobody else occupies this space.

**Quantiloop** ([Quantiloop](http://quantiloop.com/)):
- iOS only. 4 tracks with unlimited overdubs
- 5 looping styles: free, synced, parallel, serial, mixed
- Built-in rhythm guide and metronome
- Transpose feature (shift pitch of loops)
- Hosts Inter-App Audio / AUv3 effects
- **Key feature**: MIDI foot controller support is deeply integrated -- the app was designed for hands-free use
- **Lesson for us**: Hardware controller support (Bluetooth MIDI footswitch) transforms a phone looper from a toy to a tool.

**BandLab** ([BandLab Blog](https://blog.bandlab.com/introducing-looper-redefining-beat-creation/)):
- Android + iOS. Free. 12 sample packs with 24 loops each
- Performance effects (Filter, Gater, Stutter, Tape Stop)
- Up to 12 looper tracks
- **Pain point**: Oriented toward beat-making, not guitar looping. No audio input looping in the traditional sense.
- **Lesson for us**: BandLab proves the Android market wants looping features.

**Group the Loop / Loopify** ([Play Store](https://play.google.com/store/apps/details?id=com.zuidsoft.looper)):
- Android available. Multi-track looping.
- **Pain point**: Significant audio latency on many Android devices. Timing accuracy suffers.
- **Pain point**: Basic UIs with minimal visual feedback.
- **Lesson for us**: We already solved the latency problem with Oboe + exclusive mode. This is a technical moat.

### 1.4 What Power Users Complain About

Collected from forums ([Fractal Audio Forum](https://forum.fractalaudio.com/threads/looper-decay-fade-out-like-most-loopers-on-the-market.166048/), [The Gear Page](https://www.thegearpage.net/board/index.php?threads/looper-pedal-with-unlimited-undo-redo.1998836/), [Kemper Forum](https://forum.kemper-amps.com/forum/thread/48673-looper-can-we-change-profiles-for-different-passes/), [Loopy Pro Forum](https://forum.loopypro.com/discussion/55481/loopy-pro-long-count-in-for-recording-guitar)):

1. **"I want to change my tone while the loop plays"** -- With hardware loopers placed at the end of the chain, the recorded loop's tone is fixed. With digital processors, switching presets can alter the loop playback tone. Our dual-track architecture SOLVES THIS by re-amping the dry track.

2. **"I need more than 1 level of undo"** -- Most loopers offer only 1 undo. Chase Bliss Blooper offers 8, which users love. The Gear Page has multi-page threads requesting unlimited undo.

3. **"Why can't I export stems?"** -- Users want to take their loop performances into a DAW. Current hardware loopers make this difficult (USB transfer of mixed audio only, not stems).

4. **"The timing is always off on the first beat"** -- Quantized start/stop with count-in solves this. Surprisingly few mobile loopers implement it well.

5. **"I want to practice a riff at half speed"** -- Half-speed playback is common in hardware but rare in mobile looper apps.

6. **"I want to play along with a song"** -- Backing track import with speed/pitch control is a major gap in standalone loopers.

7. **"Loop decay should be standard"** -- Overdub feedback/decay (old layers gradually fade) enables evolving ambient loops. Fractal forum threads have multi-year feature requests for this.

---

## 2. Transport Controls for Mobile (No Footswitch)

### 2.1 Screen-Based Controls

**The fundamental problem**: A guitarist's hands are on the guitar. Touching a phone screen requires stopping playing, which breaks the performance flow.

**Solutions, ranked by practicality:**

**A. Large Tap Zones (Primary, v1)**
- Divide the looper screen into 2-3 large touch zones (not tiny buttons)
- Top 60%: waveform display (tap to toggle Record/Overdub/Play)
- Bottom 20%: dedicated Stop/Clear bar
- Bottom 20%: Undo button (large enough to hit without looking)
- Use the ENTIRE waveform area as a single giant button. This is what TC Ditto and Boss RC-1 do with their single footswitch -- one action (tap) cycles through states
- State cycle: IDLE --(tap)--> RECORDING --(tap)--> PLAYING --(tap)--> OVERDUBBING --(tap)--> PLAYING
- Double-tap in PLAYING: Stop
- Long-press in any state: Clear (with haptic confirmation)
- This one-tap cycle matches muscle memory from hardware looper pedals

**B. Bluetooth MIDI Footswitch (High priority, v1 or v2)**
- Support BLE MIDI (Bluetooth Low Energy MIDI) footswitches ([Amazon Controllers](https://www.amazon.com/Controller-Mate-Customizable-Footswitches-Programmable/dp/B0FB2XXF3Z))
- XSONIC AIRSTEP: 5 switches, BLE, <6ms latency ([XSONIC](https://xsonicaudio.com/pages/airstep))
- Looptimus+: 9 buttons, BLE MIDI ([Sunday Sounds](https://sundaysounds.com/pages/looptimus-plus-wireless-midi-foot-controller-for-worship))
- MIDI Maestro: 6 buttons with screens ([Singular Sound](https://www.singularsound.com/products/midi-maestro))
- Map MIDI CC or Note messages to: Record/Stop, Overdub, Undo, Clear, Preset+/Preset-
- **Critical**: BLE MIDI latency is 6-15ms. Combined with audio callback latency of 4ms, total is 10-19ms. Still under the 20ms perceptual threshold for rhythmic onset.

**C. Gesture Controls (v2)**
- Shake-to-undo (accelerometer): works but false positives while strumming
- Proximity sensor: wave hand over phone to trigger (unreliable)
- Volume button press: Android restricts override of hardware buttons in foreground service
- **Assessment**: Gesture controls are gimmicky. Footswitch or screen taps are more reliable.

### 2.2 Countdown/Pre-Roll

**Count-in before recording** is essential for quantized loops. Implementation:

- User sets BPM and bar count
- Presses Record
- 1 bar of metronome clicks plays (count-in) -- recording has NOT started yet
- After the count-in bar, recording begins automatically on beat 1
- Visual: beat number displayed prominently (1... 2... 3... 4... RECORDING)
- "Count-in only" metronome option: clicks play during count-in, then silence during recording (user preference, per [Loopy Pro Forum](https://forum.loopypro.com/discussion/66567/metronome-during-recording))

**For free-form (non-quantized) loops**:
- Optional: "3-2-1" countdown (3 seconds) with visual indicator
- More commonly: just start recording immediately (no pre-roll). The crossfade handles any start/end discontinuity.
- **Fade-in on record start**: Apply a 5ms fade-in to the first samples recorded. This eliminates the click from pressing the button mid-strum. The Sheeran Looper+ does this and users love it.

### 2.3 Visual Feedback

**Waveform Display (Essential, v1)**:
- Circular waveform display (like Loopy HD / Boss RC-1 LED ring)
  - Current playback position indicated by a rotating marker
  - Loop content shown as amplitude around the circle
  - Beat markers at quantized positions (if BPM set)
  - Color coding: green=playing, red=recording, yellow=overdubbing
- Alternative: linear scrolling waveform (like DAW, more familiar to some users)
  - Playback position as a vertical cursor
  - Beat grid lines visible
  - Headrush Looperboard uses this style on its 7" touchscreen
- **Recommendation**: Support BOTH. Circular for performance mode, linear for editing/export mode.

**Loop Position Indicator**:
- Beat number within the bar (1-2-3-4) displayed prominently
- Bar number within the loop (Bar 2 of 4) displayed
- Progress bar or circular progress showing position within the full loop
- On-screen "LED" that pulses on each beat (mimics Boss RC-1 LED circle)

**State Indicator**:
- Large, unambiguous state text: "IDLE" / "REC" / "PLAY" / "OVERDUB"
- Color-coded background: idle=grey, recording=red, playing=green, overdubbing=amber
- Use the phone's notification LED if available (deprecated on most modern phones)

**Level Meters**:
- Input level meter (already exists in the app as LevelMeter component)
- Loop playback level meter
- Combined output level meter

---

## 3. Export and Sharing

### 3.1 File Formats

| Format | Use Case | Quality | File Size (5 min) | Implementation Complexity |
|--------|----------|---------|-------------------|--------------------------|
| WAV 32-bit float | DAW import, archiving | Lossless, full precision | ~115 MB (dry+wet) | Already designed in architecture doc |
| WAV 16-bit PCM | General sharing | CD quality | ~57 MB (dry+wet) | Simple dithering from 32-bit |
| FLAC | Compressed archiving | Lossless, ~50% smaller | ~55 MB | Need FLAC encoder (background thread) |
| MP3 320kbps | Social media, casual | Lossy, transparent quality | ~12 MB | Need LAME or similar (licensing!) |
| AAC/M4A | Social media (Apple) | Lossy, good quality | ~10 MB | Android MediaCodec API |
| OGG Vorbis | Open source friendly | Lossy, good quality | ~10 MB | libvorbis (BSD) |

**Recommendation**:
- v1: WAV 32-bit float (primary), WAV 16-bit (for sharing)
- v2: FLAC, AAC/M4A (via Android MediaCodec -- no licensing issues)
- AVOID MP3: LAME licensing is complicated for commercial apps. AAC/M4A via MediaCodec is free.

### 3.2 Stem Export

This is the killer export feature enabled by dual-track recording:

- **Export dry + wet as separate WAV files** -- user gets both stems
- **Export re-amped version** alongside original wet -- user gets 3 stems
- **Naming convention**: `[SessionName]_dry.wav`, `[SessionName]_wet.wav`, `[SessionName]_reamp.wav`
- **Metadata**: Embed BPM, time signature, loop length, preset name in WAV metadata (BWF/iXML format)

### 3.3 Sharing Workflow

**Direct Share (v1)**:
- Android Share Intent with exported WAV/M4A file
- Works with: Files, Google Drive, Dropbox, email, messaging apps
- Show share dialog after export completes

**Social Media Optimized (v2)**:
- Generate a short video with waveform visualization + audio
- Share as MP4 to Instagram/TikTok/YouTube Shorts
- Include the preset name and effect chain as a text overlay
- This turns recordings into content and markets the app virally

**Cloud Integration (v3)**:
- Direct upload to Google Drive / Dropbox
- Generate shareable link
- Collaborative features (share dry stem, someone else re-amps with their chain)

### 3.4 DAW Import Workflow

The dry stem export is specifically designed for professional DAW import:

1. User records a guitar performance in the app
2. Exports `song_dry.wav` (32-bit float, native sample rate)
3. Imports into Logic/Ableton/Reaper
4. Applies their own amp sims, EQ, compression, mixing
5. The dry signal preserves all dynamics and nuance of the original performance

**Metadata to include**: Sample rate, bit depth, BPM (if set), recording date/time, app version, signal chain preset name.

---

## 4. Practice and Performance Features

### 4.1 Half-Speed Playback

**Implementation**: Play back at 0.5x speed, which drops pitch by one octave.

- Simple approach: read every sample twice (zero-order hold). Pitch drops 1 octave. Quality is mediocre.
- Better approach: linear interpolation between samples at 0.5x read rate. Smoother but still drops pitch.
- Best approach: time-stretch without pitch change (WSOLA, phase vocoder). Complex but maintains original pitch.

**For v1**: Simple half-speed (pitch drops one octave). This matches what hardware loopers do (TC Ditto X2, Line 6 DL4, Boss RC-5). Guitarists expect and accept the pitch drop -- it's considered a feature, not a bug. The octave-down sound is useful for learning bass lines from guitar recordings.

**For v2**: Add pitch-preserved slow-down using WSOLA (Waveform Similarity Overlap-Add). Variable speed: 25%-200%. This matches practice apps like [Practice Session](https://www.practicesession.app/) and [Anytune](https://www.anytune.app/).

### 4.2 Pitch Shift Without Time-Stretch

**Transpose loop up/down by semitones** without changing speed or duration.

- Use cases: Practice a song in a different key, create harmonies
- Quantiloop offers this as a core feature
- Implementation: Resampling with interpolation, then time-stretch to compensate for duration change. Or phase vocoder with frequency shifting.
- **For v2 or v3**: This is computationally expensive on mobile and requires a quality time-stretch algorithm.

### 4.3 Loop Library / Session Management

**The problem**: Users accumulate recordings and loops with no way to organize them.

**Session Model**:
```
Session
  |-- name: "Blues Jam 2026-03-18"
  |-- created: timestamp
  |-- bpm: 85.0
  |-- timeSignature: 4/4
  |-- preset: "Gilmour Comfortably Numb"
  |-- loop_dry.wav
  |-- loop_wet.wav
  |-- recordings/
  |     |-- take1_dry.wav
  |     |-- take1_wet.wav
  |     |-- take2_reamp.wav
  |-- notes: "Working on the second solo, bars 5-8"
```

**Session List UI**:
- Sorted by date (most recent first)
- Search by name, preset name, BPM range
- Favorites / tags
- Bulk delete, bulk export
- Storage usage indicator ("12 sessions, 340 MB used")

**Practice journal integration** (v3): Track practice time per session, note progress, set goals.

### 4.4 A/B Comparison

**This is uniquely enabled by the dry+wet dual recording architecture.**

Workflow:
1. Record a performance (dry + wet saved)
2. Change the signal chain (different amp, different pedals, different preset)
3. Press "Re-Amp" -- the dry recording plays through the NEW signal chain
4. Toggle A/B button to switch between hearing the ORIGINAL wet recording and the NEWLY re-amped version
5. Both play simultaneously (zero latency switching)

**Implementation**:
- Buffer A: original wet recording (from initial capture)
- Buffer B: real-time re-amp output (from dry -> current signal chain)
- A/B button swaps which buffer feeds the output
- Existing ABComparisonBar UI component can be reused/adapted

**This is a feature NO other mobile looper offers.** It turns tone-tweaking from guesswork into a scientific process. The same performance, different tone settings, instant comparison.

### 4.5 Backing Track Import

**Import any audio file and play along**:
- Supported formats: WAV, MP3, M4A, OGG, FLAC (via Android MediaCodec / MediaExtractor)
- Features:
  - Speed control (50%-150% without pitch change -- needs WSOLA, v2)
  - Simple speed control (50%-200% with pitch change -- v1, simple resampling)
  - Loop markers (A-B repeat -- mark start and end points to loop a section)
  - Backing track plays ALONGSIDE the looper (summed into output after signal chain)
  - Backing track does NOT go through the signal chain (it's already processed)

**AI stem separation (v3)**: Import a song, use an on-device ML model to separate guitar from the mix, play along with the guitar-removed backing. This is what [Moises](https://moises.ai/) does. However, on-device stem separation requires significant ML inference resources. Consider using a cloud API instead, or offering it as a premium feature.

---

## 5. Multi-Layer Considerations

### 5.1 Architecture Comparison: In-Place vs. Separate Layers

| Approach | Memory per layer (120s@48kHz) | CPU per layer | Mixability | Undo depth |
|----------|-------------------------------|---------------|------------|------------|
| In-place sum (current design) | 0 MB (merged) | 0 (merged) | None (all or nothing) | 1 level |
| Separate buffers | 22 MB wet + 22 MB dry | 1 read/sample | Full (vol/mute/solo) | Unlimited |
| Hybrid: in-place + 1 undo | 22 MB undo buffer | 0 + 1 conditional write | None | 1 level |

### 5.2 Per-Layer Visual Management (v2)

If implementing separate layers:

- Each layer shown as a horizontal track strip (like a mini DAW)
- Per-layer controls: Volume slider, Mute (M), Solo (S)
- Color-coded by order of recording (Layer 1=blue, Layer 2=green, Layer 3=orange, Layer 4=red)
- Drag to reorder layers (affects visual display only, not mixing)
- Waveform thumbnail for each layer

### 5.3 Different Signal Chains Per Layer

**This is the holy grail of looper design** -- record a clean rhythm loop, then overdub a distorted lead part, without the distortion affecting the rhythm loop.

Current hardware solutions:
- Fractal / Kemper: Looper block placed post-effects, so recorded tone is fixed. Changing presets changes live input tone but not loop playback. Users complain about this ([Fractal Forum](https://forum.fractalaudio.com/threads/looper-changing-tone-when-changing-scenes.191379/), [Kemper Forum](https://forum.kemper-amps.com/forum/thread/48673-looper-can-we-change-profiles-for-different-passes/)).
- XSONIC ULooper: Connects via USB to keep loop tone unchanged during preset switches ([XSONIC](https://xsonicaudio.com/pages/ulooper)).

**Our solution (enabled by dual-track architecture)**:
- Each layer stores the WET signal (tone-baked) AND the DRY signal
- During playback, always play back the WET version (original tone preserved)
- For re-amping, each layer's DRY signal can be independently re-amped with different presets
- In v1 (in-place overdub): When the user changes presets between overdubs, the new overdub layer has the new tone baked in, while previous layers keep their tone. This happens naturally because we record the wet output.
- In v2 (separate layers): Each layer's dry recording can be re-amped independently with any preset.

### 5.4 Practical Layer Count on Mobile

| Layers | Memory (wet only, 120s) | Memory (dry+wet, 120s) | Feasibility |
|--------|-------------------------|------------------------|-------------|
| 1 (current) | 22 MB | 44 MB | All devices |
| 2 | 44 MB | 88 MB | 6+ GB RAM |
| 4 | 88 MB | 176 MB | 8+ GB RAM |
| 8 | 176 MB | 352 MB | Exceeds per-app limit |

**Recommendation**:
- v1: In-place overdub (unlimited layers, merged, 1 undo)
- v2: 4 separate layers (wet only, 88 MB) on devices with 6+ GB RAM
- v3: 4 separate layers with dry+wet (176 MB) on devices with 8+ GB RAM, enabling per-layer re-amping

---

## 6. Differentiators

### 6.1 Unique Advantage: Dry+Wet Dual Capture

**No other mobile looper does this.** Not Loopy Pro. Not Quantiloop. Not BandLab. Not any hardware looper under $1000.

The closest hardware equivalent is the professional studio re-amping workflow, which requires:
- A DI box ($50-200)
- Two channels on an audio interface ($200-500)
- A DAW ($100-600)
- A re-amping box ($50-150)
- Total: $400-1450 in gear

We deliver this workflow in a free mobile app. This is the #1 differentiator and should be the central marketing message.

### 6.2 Re-Amping Workflow: Making It Intuitive

The re-amping concept is powerful but unfamiliar to most guitarists. The UX must make it feel natural:

**Naming**: Don't call it "Re-Amping". Call it **"Try Different Tones"** or **"Remix Your Tone"**. The word "re-amp" is jargon that excludes beginners.

**Workflow simplification**:
1. After recording, show a prominent button: "Try Different Tones on This Recording"
2. Tapping it activates re-amp mode. The recording loops automatically.
3. The user tweaks effects as if playing live -- but they hear their recorded performance with the new tone.
4. A split-screen A/B toggle: "Original Tone" vs "New Tone"
5. "Save This Tone" captures the re-amped version as a new file.
6. The original recording is NEVER modified.

**Progressive disclosure**: Advanced users can access "Re-Amp Settings" for:
- Input gain adjustment for the dry signal
- Monitor mix (dry playback + live input)
- Offline bounce with progress bar

### 6.3 Integration with Existing Preset System

**Preset-linked loop sessions**: When saving a loop, save the current preset alongside it. When loading the loop, offer to restore the preset. This way the user can recall both their performance AND their tone.

**"Tone Audition" mode**: Browse through all 22 factory presets while a loop plays. Each preset switch re-amps the dry recording in real-time. This is the fastest way to find a tone -- play once, hear it through 22 different amp+effect combinations.

**Preset comparison history**: After auditioning multiple presets on the same recording, show a list: "You tried these 5 presets on this recording. Tap to hear each one." This would require caching re-amp outputs, which is a v3 feature.

### 6.4 AI/ML Features (Genuinely Useful, Not Gimmicky)

**Tier 1 -- Practical, low-risk, high-value:**

1. **Auto-BPM Detection from First Loop** ([Gearspace](https://gearspace.com/board/so-many-guitars-so-little-time/1246853-looper-integrated-bpm-detection.html)):
   - After the first loop is recorded, analyze it for tempo
   - Onset detection (look for transient peaks) + autocorrelation on the onset times
   - Set the detected BPM for the metronome and subsequent quantized overdubs
   - This is NOT deep learning -- it's classic DSP (onset detection + autocorrelation). Very low CPU cost.
   - Risk of half/double tempo detection (55 BPM detected as 110). Mitigate by constraining to 60-180 BPM range and offering manual correction.

2. **Chord Detection from Loop** ([AI Chord Identifier Tools](https://www.topmediai.com/ai-chord-identifier/)):
   - Analyze the dry recording to identify chord changes
   - Display chord names above the waveform at the detected positions
   - Useful for: practice (see what you played), sharing (others can play along)
   - Can use lightweight chromagram + template matching (not neural network)
   - Accuracy: decent for clean guitar, poor for heavily distorted signal. Use the DRY recording for better accuracy.
   - **This is why dual-track recording matters for AI features** -- the dry signal is far easier to analyze than the wet signal.

**Tier 2 -- Valuable but complex:**

3. **Smart Loop Point Detection**:
   - If the user records slightly more than a perfect loop (common mistake), automatically suggest the best loop point based on waveform similarity
   - Cross-correlate the end of the recording with the beginning to find the optimal trim point
   - Present as "Your loop seems to be [X] bars at [Y] BPM. Trim to fit?" dialog

4. **Practice Progress Tracking**:
   - Compare consecutive takes of the same passage
   - Measure timing accuracy (onset alignment with metronome grid)
   - Display improvement over time: "Your timing accuracy improved from 78% to 91% this session"

**Tier 3 -- Future/experimental:**

5. **AI Accompaniment** ([Flow Machines](https://www.flow-machines.com/history/projects/ai-musical/)):
   - Detect chords from the loop and generate a complementary bass line or drum pattern
   - Requires on-device ML inference or cloud API
   - Very high complexity, moderate reliability, significant scope creep risk
   - **Assessment**: Cool demo, but unreliable in practice. Not for v1 or v2.

6. **AI Stem Separation for Backing Tracks** ([Moises](https://moises.ai/)):
   - Import any song, remove guitar track, play along with the rest
   - Requires a model like Demucs (700+ MB) or cloud API
   - On-device: feasible on flagship phones with NPU, not on mid-range
   - **Assessment**: Better as a cloud-based premium feature. Not for v1.

---

## 7. Prioritized Feature Tiers

### TIER 1: Must-Have for v1 (Core Looper)

These features define the minimum viable looper. Without ALL of these, the feature is not worth shipping.

| # | Feature | Justification |
|---|---------|---------------|
| 1.1 | **Free-form loop record/play/stop** | Core looper function. State machine: IDLE->RECORDING->PLAYING->IDLE |
| 1.2 | **Overdub with in-place sum** | Expected baseline feature. Every looper has this. |
| 1.3 | **Undo last overdub** | Critical safety net. Single level sufficient for v1. |
| 1.4 | **Dual-track dry+wet capture** | Our primary differentiator. Must be present from day one. |
| 1.5 | **Loop playback volume control** | Basic mixing capability. Atomic float, trivial to implement. |
| 1.6 | **Crossfade at loop boundary** | 5ms Hann crossfade eliminates clicks. Non-negotiable for quality. |
| 1.7 | **Large single-tap transport control** | One giant button cycling through states. Matches hardware looper UX. |
| 1.8 | **Visual state indicator** | Color-coded state display (red=rec, green=play, amber=overdub). |
| 1.9 | **Loop position indicator** | Circular or linear progress showing current position in loop. |
| 1.10 | **Metronome (bypass signal chain)** | Click track for practice. MUST NOT go through distortion/delay. |
| 1.11 | **Tap tempo** | Trimmed mean algorithm. Display BPM. 2-8 taps. |
| 1.12 | **BPM-quantized loop length** | User sets BPM + bar count. Loop auto-stops at boundary. |
| 1.13 | **Count-in (1 bar before recording)** | Essential for quantized loops. Click for 1 bar, then record starts. |
| 1.14 | **Fade-in on record start** | 5ms fade-in eliminates click from button press timing. Tiny effort, big UX win. |
| 1.15 | **Clear loop (with confirmation)** | Long-press to clear. Haptic feedback. |
| 1.16 | **Basic re-amping (real-time)** | Load dry recording, play through current signal chain. The killer feature. |
| 1.17 | **WAV export (32-bit float)** | Export dry and wet as separate WAV files. Universal DAW compatibility. |
| 1.18 | **Session save/load** | Save loop + metadata (BPM, preset, timestamp). Load and resume. |
| 1.19 | **Linear recording to disk** | SPSC ring buffer -> writer thread -> WAV. For longer-than-loop recordings. |
| 1.20 | **Loop waveform display** | Rendered waveform of loop content. Updates after overdub. |

**Estimated scope**: 5 phases, ~3-4 sprints. This aligns with the existing architecture document's phasing.

### TIER 2: High-Value Additions for v2

These features elevate the looper from "functional" to "compelling". They are what make users choose this app over alternatives.

| # | Feature | Justification |
|---|---------|---------------|
| 2.1 | **Overdub decay / feedback control** | Knob (0-100%) controlling how much previous content fades per overdub pass. Enables evolving ambient loops. Heavily requested on forums. |
| 2.2 | **Reverse playback** | Play loop backwards. Simple: read buffer in reverse. Expected feature in creative loopers. |
| 2.3 | **Half-speed playback** | Read at 0.5x rate (pitch drops 1 octave). Standard practice/creative feature. |
| 2.4 | **A/B tone comparison** | Toggle between original wet and re-amped output. Unique to our architecture. |
| 2.5 | **Bluetooth MIDI footswitch support** | BLE MIDI for hands-free control. Transforms app from toy to tool. |
| 2.6 | **Auto-BPM detection from first loop** | Onset detection + autocorrelation. Set metronome automatically. |
| 2.7 | **Backing track import** | Load MP3/WAV, play alongside loop. Simple speed control (with pitch change). |
| 2.8 | **A-B loop markers on backing track** | Mark section start/end, loop that section for practice. |
| 2.9 | **AAC/M4A export** | Compressed export via Android MediaCodec. Smaller files for sharing. |
| 2.10 | **Share via Android Share Intent** | Standard share dialog for exported files. |
| 2.11 | **Session list with search/sort** | Browse saved sessions by date, name, BPM, preset. |
| 2.12 | **Multiple time signatures** | 3/4, 6/8, 5/4, 7/8 in addition to 4/4. |
| 2.13 | **Double-speed playback** | Read at 2x rate (pitch up 1 octave). Creative effect. |
| 2.14 | **Offline re-amp bounce** | Process dry recording through chain on background thread, save result. Progress bar UI. |
| 2.15 | **Chord detection from dry recording** | Chromagram + template matching. Display chords above waveform. |
| 2.16 | **"Tone Audition" mode** | Cycle through presets while loop plays, hearing each one on the fly. |

### TIER 3: Future Differentiators

These features are ambitious, technically complex, and represent long-term competitive advantages.

| # | Feature | Justification |
|---|---------|---------------|
| 3.1 | **4 separate mixable layers** | Per-layer volume/mute/solo. 88 MB memory. 6+ GB devices only. |
| 3.2 | **Per-layer re-amping with different presets** | Each layer's dry recording re-amped independently. Requires separate layer buffers. |
| 3.3 | **Time-stretch without pitch change (WSOLA)** | Variable speed 25%-200% preserving pitch. Essential for serious practice. |
| 3.4 | **Pitch shift without time-stretch** | Transpose loop +/- 12 semitones. Phase vocoder. |
| 3.5 | **Video recording with waveform overlay** | Record screen as MP4 for social media sharing. Viral marketing potential. |
| 3.6 | **Smart loop point suggestion** | Cross-correlate recording end with beginning to auto-trim. |
| 3.7 | **Practice timing analysis** | Compare onset alignment with metronome grid. Track improvement over sessions. |
| 3.8 | **FLAC export** | Lossless compressed export. Background encoding. |
| 3.9 | **Loop effects (filter sweep, stutter, tape stop)** | Post-recording loop manipulation a la Chase Bliss Blooper. |
| 3.10 | **AI stem separation for backing tracks** | Import song, remove guitar, play along. Cloud API or on-device ML. |
| 3.11 | **Ableton Link sync** | Sync looper tempo with other apps/devices on the same network. |
| 3.12 | **Multi-level undo (8 levels)** | Requires 8x undo buffers (176 MB). Chase Bliss Blooper has this. |
| 3.13 | **AI auto-accompaniment** | Generate bass/drums from detected chords. Experimental. |

---

## 8. Critical Assessment: What NOT to Build

### Scope Creep Traps

| Feature | Why It Seems Good | Why It's a Trap |
|---------|------------------|-----------------|
| Full DAW timeline editor | "Power users want it" | Completely different product. Compete with BandLab, GarageBand, Cubasis -- all with enormous teams. |
| MIDI sequencer | "Loopy Pro 2.0 has it" | Our app is a GUITAR effects processor. MIDI sequencing is a different user base. |
| Built-in drum machine patterns | "Boss RC-500 has 57 patterns" | Significant content creation effort. Drum sounds through guitar speakers sound terrible (RC-5 users complain about this). Offer metronome click only for v1, backing track import for everything else. |
| Social network / community features | "BandLab has a social feed" | Requires server infrastructure, content moderation, ongoing maintenance. Completely different business. |
| Real-time collaboration | "Jam with friends remotely" | Network latency makes real-time music impossible over the internet (~50-200ms round trip). |
| Auto-tune / vocal processing | "Expand the market" | Different product category entirely. Guitar focus is the brand. |

### Features That Sound Unique But Aren't Worth It

| Feature | Assessment |
|---------|-----------|
| **Accelerometer-based transport** (shake to undo) | Too many false positives while strumming. Unreliable. |
| **Voice commands** ("Hey app, start recording") | Latency of speech recognition (300-1000ms) makes it useless for musical timing. |
| **Auto-harmonize** (detect pitch, add harmony) | Extremely difficult for polyphonic guitar. Works for single notes only. Niche use case. |
| **Infinite canvas loop grid** (Loopy Pro style) | Massive UI complexity. Our app's strength is simplicity. |

---

## 9. Competitive Positioning Summary

### The Android Looper Gap

| Feature | Loopy Pro (iOS) | Quantiloop (iOS) | BandLab (Android) | Our App (Android) |
|---------|----------------|-----------------|-------------------|-------------------|
| Platform | iOS only | iOS only | Android + iOS | Android |
| Audio looping | Yes | Yes | Beat-based only | Yes |
| Multi-track | 32 tracks | 4 tracks | 12 (samples) | 1 (v1), 4 (v2) |
| AUv3/Plugin hosting | Yes | Yes | No | N/A (built-in effects) |
| Built-in effects | No (hosts external) | No (hosts external) | Basic | 26 effects + 9 amps |
| Dual-track dry/wet | No | No | No | **YES** |
| Re-amping | No | No | No | **YES** |
| A/B tone comparison | No | No | No | **YES** |
| Stem export | No | No | No | **YES** |
| MIDI footswitch | Yes | Yes | No | v1-v2 |
| Waveform display | Yes | Limited | No | v1 |
| Overdub decay | No | Yes (via track decay) | No | v2 |
| Backing track import | Via IAA/AUv3 | Via IAA | Loop packs | v2 |
| Price | $15 + subscription | $10 / $20 Pro | Free | Free (our app) |

**Key insight**: There is NO serious guitar looper app on Android. Every professional option is iOS-only. We have the entire Android market to ourselves, AND we have features (dual-track, re-amping, A/B comparison) that even iOS apps lack.

---

## Sources

- [Boss RC-500 Sweetwater Reviews](https://www.sweetwater.com/store/detail/RC500pedal--boss-rc-500-loop-station-compact-phrase-recorder-pedal/reviews)
- [Boss RC-500 Looper Pedal Reviews](https://looperpedalreviews.com/boss-rc-500-loop-station-review/)
- [Boss RC-5 Sweetwater Reviews](https://www.sweetwater.com/store/detail/RC5pedal--boss-rc-5-loop-station-compact-phrase-recorder-pedal/reviews)
- [TC Electronic Ditto X4](https://www.tcelectronic.com/product.html?modelCode=P0DE9)
- [Ditto X4 Sweetwater Reviews](https://www.sweetwater.com/store/detail/DittoX4--tc-electronic-ditto-x4-looper-looper-pedal/reviews)
- [Headrush Looperboard Sweetwater](https://www.sweetwater.com/store/detail/LooperBoard--headrush-looperboard-advanced-performance-looper-with-7-inch-touchscreen)
- [Headrush Looperboard Amazon](https://www.amazon.com/Looperboard/dp/B07QY2Z734)
- [Chase Bliss Blooper](https://www.chasebliss.com/blooper)
- [Line 6 DL4 MkII](https://line6.com/effects-pedals/dl4-mkii/)
- [DL4 MkII Looper Settings](https://www.sweetwater.com/sweetcare/articles/line-6-dl4-mkii-looper-settings/)
- [Pigtronix Infinity 2](https://www.pigtronix.com/pedals/infinity-2-double-looper/)
- [Ableton Session View Looping](https://help.ableton.com/hc/en-us/articles/360001431244-How-To-Looping-in-Session-View)
- [Loopy Pro](https://loopypro.com/)
- [Loopy Pro 2.0 - SYNTH ANATOMY](https://synthanatomy.com/2025/07/loopy-pro-2-0-levels-up-the-creative-audio-looper-app-with-midi-looping-and-more.html)
- [Loopy Pro Forum - Count-in](https://forum.loopypro.com/discussion/55481/loopy-pro-long-count-in-for-recording-guitar)
- [Loopy Pro Forum - Metronome](https://forum.loopypro.com/discussion/66567/metronome-during-recording)
- [Quantiloop](http://quantiloop.com/)
- [BandLab Looper](https://blog.bandlab.com/introducing-looper-redefining-beat-creation/)
- [Loopify Android](https://play.google.com/store/apps/details?id=com.zuidsoft.looper)
- [Android Looper Forum](https://forums.androidcentral.com/threads/guitar-looper-app.1026222/)
- [Best Loop Pedal Apps](https://zinginstruments.com/best-loop-pedal-apps/)
- [XSONIC AIRSTEP](https://xsonicaudio.com/pages/airstep)
- [MIDI Maestro](https://www.singularsound.com/products/midi-maestro)
- [Wireless MIDI Controller Amazon](https://www.amazon.com/Controller-Mate-Customizable-Footswitches-Programmable/dp/B0FB2XXF3Z)
- [Fractal Looper Decay Request](https://forum.fractalaudio.com/threads/looper-decay-fade-out-like-most-loopers-on-the-market.166048/)
- [Fractal Looper Preset Switching](https://forum.fractalaudio.com/threads/looper-changing-tone-when-changing-scenes.191379/)
- [Kemper Looper Profiles Per Pass](https://forum.kemper-amps.com/forum/thread/48673-looper-can-we-change-profiles-for-different-passes/)
- [The Gear Page Unlimited Undo](https://www.thegearpage.net/board/index.php?threads/looper-pedal-with-unlimited-undo-redo.1998836/)
- [Practice Session App](https://www.practicesession.app/)
- [Anytune App](https://www.anytune.app/)
- [Backtrackit App](https://www.backtrackitapp.com/)
- [Moises AI](https://moises.ai/)
- [Re-amping Workflow - Radial](https://www.radialeng.com/blog/the-reamping-workflow)
- [Re-amping Guide - LANDR](https://blog.landr.com/reamping/)
- [Guitar World Best Loopers 2026](https://www.guitarworld.com/features/best-looper-pedals)
- [Best Loop Pedal Apps - JeffRadio](https://jeffradio.com/the-best-loop-pedal-apps-in-2023/)
- [Flow Machines AI Looper](https://www.flow-machines.com/history/projects/ai-musical/)
- [AI Chord Identifier Tools](https://www.topmediai.com/ai-chord-identifier/)
- [Gearspace BPM Detection](https://gearspace.com/board/so-many-guitars-so-little-time/1246853-looper-integrated-bpm-detection.html)
- [Overdub Decay - MOD WIGGLER](https://modwiggler.com/forum/viewtopic.php?t=183362)
- [Looper Undo - Loopy Pro Wiki](https://wiki.loopypro.com/Overdub)
- [Free The Tone Motion Loop](https://www.guitarworld.com/news/free-the-tone-introduces-intelligent-pitch-shiftable-short-looper-the-motion-loop-ml-1l)
- [XSONIC ULooper](https://xsonicaudio.com/pages/ulooper)
- [Singular Sound Undo/Redo Behavior](https://forum.singularsound.com/t/undo-redo-behavior-options/34135)
