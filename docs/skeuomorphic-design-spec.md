# Skeuomorphic UI Design Specification: "Forge" Design Language

## Table of Contents
1. [Design Philosophy & Visual Direction](#1-design-philosophy--visual-direction)
2. [Critical Analysis of Three Directions](#2-critical-analysis-of-three-directions)
3. [The Chosen Direction: Forge](#3-the-chosen-direction-forge)
4. [Complete Color Palette](#4-complete-color-palette)
5. [Typography System](#5-typography-system)
6. [Component Specifications](#6-component-specifications)
7. [Layout Templates](#7-layout-templates)
8. [Compose Implementation Strategy](#8-compose-implementation-strategy)
9. [Migration from Current Design System](#9-migration-from-current-design-system)
10. [Implementation Priority](#10-implementation-priority)

---

## 1. Design Philosophy & Visual Direction

### 1.1 The Problem with the Current Aesthetic

The app currently uses what I call "Guitar Rig 7 Lite" -- a clean modern dark UI with category-tinted surfaces, radial gradient knobs, and glowing LEDs. This was the right starting point. The DesignSystem.kt, RotaryKnob, LedIndicator, and StompSwitch components are well-built and use proper Canvas rendering. However, the overall impression is of a *well-designed utility* rather than a *guitar instrument*. The rack modules feel like IDE panels with colored borders. The surfaces are flat dark rectangles with slight tint variation. Nothing about the visual language says "this is an instrument."

The request is to move toward skeuomorphism -- but specifically a blend of **terminal simplicity** (monospace precision, dark backgrounds, minimal chrome) with **physical hardware elements** (real knobs, metal textures, LED pilot lights, stomp switches). This is not about photorealistic renders of Boss pedals. It is about creating a visual language that *suggests* physical materials -- brushed aluminum, painted steel, Bakelite knobs, glass lens LEDs -- through carefully composed gradients, noise textures, and lighting effects, all rendered at 60fps in Compose Canvas calls.

### 1.2 What Musicians Actually Need from Skeuomorphism

After analyzing the competitive landscape (AmpliTube's photorealism, Guitar Rig 7's clean modernism, BIAS FX 2's routing view, Deplike's utilitarian flat design), the research distills to three evidence-backed reasons why skeuomorphism persists in audio software:

**1. Cognitive mapping.** A guitarist who has turned a real DS-1 Distortion knob clockwise for 20 years has an embodied memory of what that gesture means. When they see a round control with a pointer line that rotates from 7 o'clock to 5 o'clock, they immediately know how to interact with it and what the range means. Sliders do not trigger this mapping. The RotaryKnob we already have accomplishes this -- but the *surface around it* needs to reinforce the metaphor.

**2. Material trust.** Musicians associate certain materials with quality and reliability. Metal enclosures suggest durability (Boss, MXR). Tolex suggests warmth (Fender, Marshall). Chrome hardware suggests precision (studio rack gear). When a digital interface echoes these materials -- even abstractly -- it triggers an unconscious "this is a real tool" response. Our current flat dark rectangles do not trigger this.

**3. Spatial memory under pressure.** On stage, a guitarist does not read labels. They reach for "the orange pedal, third from the left, the big knob on the right." Skeuomorphic differentiation -- where each effect module has a distinctive visual identity rooted in its real-world counterpart -- enables this spatial memory. Our current category color coding is a good start, but all modules look structurally identical.

### 1.3 What Skeuomorphism Should NOT Do on Mobile

The FutureSonic analysis and the AmpliTube experience highlight critical failures:

- **Dial controls obscure visibility when touched on small screens.** Photorealistic knobs with realistic diameter (15mm on screen) become useless when a 10mm fingertip covers the entire control.
- **Bitmap texture rendering is expensive and scales poorly** across Android's device fragmentation (mdpi to xxxhdpi).
- **Photorealistic gear views create a false expectation** that the DSP matches the specific modeled hardware (legal risk, since we cannot use brand names).
- **Excessive detail fights readability** in varying lighting conditions (stage, bedroom, studio).

The solution is **suggestive skeuomorphism** -- using the *minimum visual cues* needed to trigger material recognition, combined with the information density and legibility of a terminal-style interface.

---

## 2. Critical Analysis of Three Directions

Before recommending a direction, three distinct approaches were evaluated. Each is analyzed for strengths, weaknesses, and mobile-specific concerns.

### Direction A: "Pedalboard Photorealism" (AmpliTube / BIAS FX approach)

**Description:** Each effect is rendered as a top-down view of its real-world hardware counterpart. The DS-1 module looks like a photograph of an orange Boss compact pedal. The amp module looks like a Marshall front panel. The cabinet module shows a speaker grille.

**Strengths:**
- Maximum visual identity per effect -- instant recognition.
- Strongest cognitive mapping to physical gear.
- Highest "wow factor" for first-time users.
- Proven by AmpliTube (the most commercially successful mobile guitar app).

**Weaknesses:**
- Bitmap textures required for each of 26 effects = significant APK size increase and memory pressure.
- Each pedal needs a unique layout, dramatically increasing design and implementation effort.
- Legal risk: pedal appearances may be trade dress protected (Boss orange, MXR script font).
- Poor scalability: adding effects in future sprints requires creating new photorealistic assets.
- Touch target problems: realistic knob proportions on a phone screen (15-20mm) are too small.
- Readability degrades on low-brightness screens and in bright environments.
- Performance: compositing multiple bitmap layers per module is expensive in LazyColumn.

**Verdict:** REJECTED. The engineering cost is prohibitive, the legal risk is real, and the mobile usability problems are well-documented.

### Direction B: "Pure Terminal" (Monochrome industrial / hacker aesthetic)

**Description:** All chrome stripped away. Pure monospace text. Effects as labeled rows in a dark terminal-like grid. Knobs replaced with precise numeric readouts that respond to vertical drag. LED indicators as simple colored dots. No gradients, no shadows, no textures. Color only in LED indicators and meter bars. Think: the UI of a command-line audio processor with a minimal GUI wrapper.

**Strengths:**
- Maximum information density -- every pixel carries data.
- Excellent readability in all lighting conditions.
- Extremely lightweight: zero bitmap textures, minimal Canvas draw calls.
- Perfect performance: nothing to animate except value changes and meters.
- Strong aesthetic identity that no competitor has attempted.
- Zero legal risk -- no visual reference to any hardware product.

**Weaknesses:**
- Completely abandons the cognitive mapping advantage of skeuomorphism. Musicians expect knobs.
- No spatial differentiation between effects -- every module looks identical.
- Feels cold, technical, unwelcoming. The opposite of what attracts a guitarist who just plugged in and wants to dial in a tone quickly.
- The value proposition of "this sounds like real hardware" is undercut by an interface that looks nothing like hardware.
- Numeric-only controls remove the "glance and know" benefit of a knob at 3 o'clock (roughly 50%).
- No fun. Musicians pick tools partly on emotion. A terminal does not spark joy.

**Verdict:** REJECTED as a primary direction. However, specific elements are extremely valuable and should be incorporated: monospace value readouts, terminal-style labeling, information density, minimal decorative chrome.

### Direction C: "Forge" (Suggestive industrial skeuomorphism + terminal precision)

**Description:** Each effect module is rendered as a dark metal enclosure with subtle material cues -- brushed aluminum texture (drawn as fine horizontal gradient lines in Canvas), painted steel surfaces (solid color with micro-noise overlay), and machined bezels around controls. Knobs are rendered with radial gradients suggesting machined aluminum or Bakelite, with clear pointer lines. LED indicators have realistic glow. Stomp switches look metallic and three-dimensional. BUT: all text uses a monospace or condensed sans-serif system, all values are displayed as precise numeric readouts, layout follows a rigid grid, and decorative elements are restrained to functional minimum. The result looks like a custom-machined effects processor designed by a precision engineer -- not a toy, not a photograph, and not a spreadsheet.

**Strengths:**
- Preserves the cognitive mapping of rotary knobs and stomp switches (proven to be essential).
- Material cues (brushed metal, painted enclosure color) trigger "real tool" trust.
- Per-effect color differentiation (painted enclosure colors referencing real pedals) enables spatial memory.
- Terminal-style text and layout provide information density and legibility.
- Everything is Canvas-drawn -- zero bitmaps, scales perfectly, runs at 60fps.
- Distinctive aesthetic that no competitor has: "precision instrument" rather than "toy guitar pedal" or "DAW panel."
- Legal safety: suggests material qualities without copying specific products.
- Scalable: new effects use the same component vocabulary with new enclosure colors.
- The "machined" aesthetic maps well to the high-quality DSP underneath (WDF circuit modeling, neural amp modeling).

**Weaknesses:**
- More Canvas drawing complexity than the current flat design.
- Texture overlays (noise, brushed lines) need careful performance testing.
- Risk of the "uncanny valley" -- if the material suggestion is slightly wrong, it looks worse than either flat or photorealistic.
- Requires a consistent lighting model across all components (top-left light source must be maintained everywhere).

**Verdict:** SELECTED. This is the right balance for a professional Android guitar app. The next sections specify it in full detail.

---

## 3. The Chosen Direction: Forge

### 3.1 Visual Mood Description

Imagine a custom guitar effects processor built by a precision instrument maker. The chassis is machined from dark anodized aluminum. Each effect module is a panel set into the chassis, painted in a color that suggests its character -- warm burnt orange for distortion, deep sea blue for modulation, teal-green for delay and reverb. The knobs are machined aluminum with knurled edges and crisp white pointer lines. LED indicators are real glass-lens LEDs that glow with category-appropriate colors. Stomp switches are heavy chrome buttons with satisfying mechanical resistance. All labels are stamped into the panel in a precise, industrial typeface -- like the text on a Neve console or a Tektronix oscilloscope. Numeric readouts use the monospace font of a laboratory instrument display.

The overall impression: this is not a toy. This is not a software mockup. This is a precision instrument that happens to live on your phone.

### 3.2 Core Visual Principles

1. **Single consistent light source.** Top-left at approximately 45 degrees. All gradients, highlights, and shadows are computed relative to this. This is the single most important factor in making 2D Canvas drawing look three-dimensional.

2. **Material hierarchy.** Three material types, in order of depth:
   - **Chassis** (deepest): Dark anodized aluminum. The background of the screen and the space between modules.
   - **Panel** (middle): Painted/powder-coated steel. The surface of each effect module. Color varies by effect.
   - **Hardware** (topmost): Machined aluminum or chrome. Knobs, switches, screws, and metal details. Always reflective gradient.

3. **Minimum viable texture.** Every texture effect must be achievable with fewer than 5 Canvas draw calls per texture region. No bitmaps. Brushed metal = 4-6 thin horizontal lines with 2% white alpha. Noise = nothing at runtime (too expensive); instead, use color variation in the base gradient.

4. **Terminal-grade information display.** All parameter values in monospace. All labels in condensed caps. Numeric precision always visible. No ambiguity about what a control is set to.

5. **Color as identity.** Each effect module's painted panel color is the primary differentiator. These colors reference real-world pedal enclosure colors without copying them exactly. The DS-1 module is orange-painted steel, not "Boss DS-1 orange." The Big Muff module is cream/green, not "EHX Ram's Head green."

---

## 4. Complete Color Palette

### 4.1 Chassis & Structure Colors

These are the foundational colors used for the app background, spacing between modules, and structural elements.

```
Token                  Hex         Usage
-------                --------    ----------------------------------------
Chassis.Dark           #0E0E10     Deepest background (behind everything)
Chassis.Mid            #161618     Main screen background
Chassis.Light          #1C1C20     Recessed areas, inset panels
Chassis.Highlight      #242428     Top edge of chassis (light catch)
Chassis.Screw          #2A2A30     Rack screw fills, fastener accents
Chassis.DividerLine    #1E1E22     Fine separator lines between modules
```

### 4.2 Module Panel Colors (Per-Effect Enclosure)

Each effect has a "painted enclosure" color. These are organized by category but individual effects may override to reference their real-world pedal color. The panel color is used as the primary fill for the module's surface.

**GAIN / DISTORTION category:**
```
Token                      Hex         Reference
-------                    --------    --------------------------------
Panel.NoiseGate            #282830     Gunmetal gray (utility feel)
Panel.Compressor           #28282E     Dark silver (studio rack unit)
Panel.Boost                #2E2420     Warm dark brown (EP booster)
Panel.Overdrive            #2A3020     Dark olive green (Tube Screamer)
Panel.DS1                  #3A2410     Burnt orange-brown (Boss orange)
Panel.RAT                  #1E1E1E     Near-black (RAT enclosure)
Panel.DS2                  #3A2812     Deep orange-brown (DS-2 turbo)
Panel.HM2                  #342018     Oxidized copper (HM-2 character)
Panel.FuzzFace             #2A2030     Dark purple-gray (germanium mystique)
Panel.Rangemaster          #302820     Dark brass-brown (treble booster)
Panel.BigMuff              #2C2828     Dark cream-charcoal (Big Muff)
Panel.Octavia              #2E2028     Dark magenta-gray (octave fuzz)
```

**AMP / CABINET category:**
```
Panel.AmpModel             #2A2418     Dark tolex brown-amber
Panel.CabinetSim           #242420     Dark olive-brown (speaker cab)
```

**TONE / EQ category:**
```
Panel.ParametricEQ         #222228     Cool dark silver (studio EQ)
```

**MODULATION category:**
```
Panel.Wah                  #202028     Dark indigo (CryBaby mystique)
Panel.Chorus               #1E2430     Dark navy blue (Boss CE-1)
Panel.Vibrato              #202830     Dark teal-blue (watery)
Panel.Phaser               #28201E     Dark warm brown-orange (Phase 90)
Panel.Flanger              #1E2028     Dark blue-gray (jet sweep)
Panel.Tremolo              #20282A     Dark sea green (pulsing)
Panel.Univibe              #262024     Dark mauve (vintage psychedelia)
Panel.RotarySpeaker        #22241E     Dark sage green (Leslie cab)
```

**TIME category:**
```
Panel.Delay                #182428     Dark teal (echo depth)
Panel.Reverb               #1C2028     Dark slate blue (cavernous)
```

**UTILITY category:**
```
Panel.Tuner                #1E2024     Dark neutral (instrument display)
```

### 4.3 Category Accent Colors (LEDs, Knob Arcs, Active Indicators)

These are the high-saturation accent colors used sparingly for LED glow, value arcs, and active state indicators. They replace the current DesignSystem EffectCategory color sets.

```
Category       LED Active   LED Bypass   Knob Arc     Header Accent
---------      ----------   ----------   ----------   -------------
GAIN           #FF6030      #3D2218      #E8724A      #C04030
AMP            #F0C040      #382E18      #D4A84A      #B88830
TONE           #C0C0CC      #2A2A30      #A8A8B0      #707078
MODULATION     #6888F0      #1E2038      #7B90E0      #5868A8
TIME           #40D8C8      #183028      #50C8BE      #308880
UTILITY        #6898C0      #1E2430      #7890A8      #506878
```

### 4.4 Per-Pedal Header Accent Overrides

```
DS-1:          #D06020   (Boss orange)
RAT:           #484848   (charcoal)
DS-2:          #D07020   (deep orange)
HM-2:         #C04830   (burnt orange-red)
BigMuff:       #8B7D6B   (cream/olive -- Ram's Head)
FuzzFace:      #6B5B8B   (purple -- germanium)
Rangemaster:   #A08848   (brass)
Octavia:       #8B4870   (magenta)
RotarySpeaker: #607050   (sage green -- Leslie)
```

### 4.5 Hardware Surface Colors (Knobs, Switches, Screws)

These colors create the "machined metal" look for interactive controls.

```
Token                  Hex         Usage
-------                --------    ----------------------------------------
Hardware.KnobDark      #303038     Knob base / shadow side
Hardware.KnobMid       #3C3C44     Knob body primary fill
Hardware.KnobLight     #505058     Knob highlight (top-left catch)
Hardware.KnobEdge      #282830     Knurled edge ring (outer rim)
Hardware.KnobPointer   #E8E8EC     White pointer line
Hardware.SwitchRing    #38383E     Stomp switch outer ring
Hardware.SwitchFace    #444448     Stomp switch inner face
Hardware.SwitchHi      #58585E     Stomp switch specular highlight
Hardware.Chrome        #68686E     Chrome trim / screw heads
Hardware.ChromeHi      #88888E     Chrome highlight
Hardware.Bezel         #222228     Recessed bezel around controls
```

### 4.6 Text Colors

```
Token                  Hex         Usage
-------                --------    ----------------------------------------
Text.Stamped           #C8C8CC     Primary labels -- effect names, section headers
                                   (slightly warm off-white, like silkscreened text
                                   on a painted metal panel)
Text.Readout           #D8D8DC     Numeric value readouts (monospace, bright)
Text.Secondary         #808088     Parameter names, descriptions, dim labels
Text.Muted             #484850     Disabled text, placeholder, bypassed labels
Text.Terminal          #78B878     Terminal green -- used ONLY for special readouts
                                   like tuner frequency, BPM display, level values
                                   (use very sparingly -- evokes LCD/VFD displays)
```

### 4.7 Meter & Indicator Colors

Retained from current DesignSystem with minor refinements:

```
Token                  Hex         Usage
-------                --------    ----------------------------------------
Meter.Background       #101018     Meter track background (deeper than chassis)
Meter.Green            #40A850     Normal signal level
Meter.Yellow           #D8C030     Hot signal level
Meter.Red              #D83838     Clipping level
Meter.PeakHold         #D0D0D4     Peak hold indicator line
Indicator.Clip         #F04040     Clip LED (persistent, bright)
Indicator.Warning      #E8A030     Approaching limit
Indicator.Safe         #40B858     Safe / active
Indicator.Danger       #FF8A80     Destructive action text
Indicator.DangerBg     #2A1A1A     Destructive action background
```

---

## 5. Typography System

### 5.1 Font Selection

The design uses two font roles:

**Display / Labels: System sans-serif (Roboto on Android)**
Used for: effect names, section headers, category labels, button text.
Weight hierarchy:
- SemiBold (W600) for effect names and primary headers
- Medium (W500) for parameter names and secondary labels
- Regular (W400) for descriptions and body text

**Values / Readouts: Monospace (JetBrains Mono preferred, system Monospace fallback)**
Used for: all numeric values, frequency readouts, dB displays, BPM, milliseconds.
This is the "terminal" element of the design -- precision numeric displays that feel like instrument readouts.

JetBrains Mono is recommended because it has clear numeral differentiation (especially 0/O and 1/l), is open-source (OFL), and has a slightly more modern character than system monospace. However, to avoid adding a font dependency in the first phase, system FontFamily.Monospace is acceptable.

### 5.2 Size Hierarchy

```
Role                    Size    Weight        Font          Tracking
-----                   -----   ------        ----          --------
Screen Title            18sp    SemiBold      Sans-serif    0.5sp
Module Effect Name      14sp    SemiBold      Sans-serif    0.25sp
Section Label           11sp    Bold          Sans-serif    1.5sp (uppercase)
Parameter Label         11sp    Medium        Sans-serif    0sp
Category Badge          10sp    Bold          Sans-serif    1.0sp (uppercase)
Value Readout (large)   14sp    Regular       Monospace     0sp
Value Readout (std)     12sp    Regular       Monospace     0sp
Value Readout (small)   11sp    Regular       Monospace     0sp
Button Text             13sp    SemiBold      Sans-serif    0.5sp
Knob Value              11sp    Regular       Monospace     0sp
Knob Label              10sp    Medium        Sans-serif    0.5sp (uppercase)
Muted/Disabled          10sp    Regular       Sans-serif    0sp
```

### 5.3 The "Stamped" Label Treatment

All parameter labels on module panels use an ALL-CAPS style with slightly widened letter spacing (0.5-1.5sp). This mimics the silk-screened or stamped text on real hardware panels. The color is Text.Stamped (#C8C8CC) -- not pure white, which would feel like a screen, but a warm off-white that feels like paint on metal.

### 5.4 The "Terminal Readout" Treatment

Numeric values that represent precise measurements (dB, Hz, ms, BPM, percentage) use monospace font in Text.Readout (#D8D8DC). For special scientific/engineering readouts (tuner frequency, spectrum analyzer axis labels, engine status), Text.Terminal (#78B878) -- a muted phosphor green -- can be used sparingly. This green is NOT used for general UI elements; it is reserved for displays that evoke LCD/VFD instrument readouts.

---

## 6. Component Specifications

### 6.1 RotaryKnob (Enhanced)

The existing RotaryKnob.kt is well-implemented. The Forge redesign enhances it with material depth.

**Visual changes from current:**

Current (4 Canvas draws):
1. Value arc (stroke)
2. Outer shadow ring (circle, KnobOuterRing)
3. Knob body (radial gradient circle)
4. Pointer line

Forge (8 Canvas draws):
1. **Recessed bezel** -- drawCircle, radius = knobRadius + 3dp, color = Hardware.Bezel. Creates a machined recess in the panel surface that the knob sits in.
2. **Value arc** -- drawArc, same as current but using category Knob Arc color at 50% alpha.
3. **Knob edge ring** (knurled rim) -- drawCircle, radius = knobRadius, color = Hardware.KnobEdge. Outer rim of the knob body.
4. **Knob body** -- drawCircle with RadialGradient:
   - Center offset: (-20%, -20%) of knob radius (top-left light source)
   - Colors: [Hardware.KnobLight, Hardware.KnobMid, Hardware.KnobDark]
   - Three-stop gradient creates more convincing metal depth than two-stop.
   - Radius = knobRadius - 2dp (inset from edge ring).
5. **Knob surface shine** -- drawCircle, radius = knobRadius * 0.4, center offset (-15%, -15%), color = Hardware.KnobLight at 15% alpha. Subtle specular highlight on the "dome" of the knob.
6. **Pointer line** -- drawLine, same geometry as current, color = Hardware.KnobPointer, strokeWidth = 2.5dp, StrokeCap.Round. Slightly thicker for visibility.
7. **Pointer dot** (at tip) -- drawCircle, radius = 2dp, at the outer end of the pointer line, color = Hardware.KnobPointer. Makes the pointer end more visible.
8. **Scale tick marks** -- 5 ticks at 0%, 25%, 50%, 75%, 100% positions. drawLine from (outerEdge - 2dp) to (outerEdge + 2dp), strokeWidth 1dp, color = Text.Muted at 60% alpha. These sit on the bezel, outside the knob body.

**Sizes:**
- SMALL: 44dp (secondary parameters)
- STANDARD: 56dp (most parameters)
- LARGE: 68dp (featured controls -- amp gain, master volume)

**Gesture:** No change from current implementation. Vertical drag with awaitEachGesture is correct.

**Labels:**
- Below knob: parameter name in Text.Secondary, 10sp, uppercase, Medium weight
- Below name: value readout in Text.Readout, 11sp, Monospace

### 6.2 LedIndicator (Enhanced)

Current implementation is good. Forge adds a **lens bezel**.

**Visual changes:**

Current (3 Canvas draws when active):
1. Glow circle (activeColor at 25% alpha)
2. LED body (radial gradient)
3. Hot spot (white dot)

Forge (5 Canvas draws when active):
1. **Lens bezel** -- drawCircle, radius = ledRadius + 2dp, color = Hardware.Bezel. A dark recessed ring that the LED sits in, like a real panel-mount LED holder.
2. **Glow** -- drawCircle, radius = ledRadius * 2.5, color = activeColor at 20% alpha. Slightly larger glow radius.
3. **LED body** -- drawCircle with RadialGradient, same as current.
4. **Hot spot** -- drawCircle, same as current.
5. **Lens reflection** -- drawCircle, radius = ledRadius * 0.15, center offset (ledRadius * 0.2, -ledRadius * 0.2), color = White at 30% alpha. A tiny specular highlight that makes the LED look like it has a glass lens.

**Sizes:**
- Standard: 10dp (module header)
- Large: 14dp (tuner overlay, engine status)

### 6.3 StompSwitch (Enhanced)

Current implementation is good. Forge adds **hex socket head** detail.

**Visual changes:**

Current (3-4 Canvas draws):
1. Optional active glow
2. Outer metallic ring
3. Inner surface (radial gradient)
4. Chrome highlight crescent

Forge (6 Canvas draws):
1. **Mounting plate** -- drawRoundRect, size = switchSize + 8dp, cornerRadius = 4dp, color = Hardware.Bezel. The recessed plate the switch mounts to.
2. **Active glow** -- same as current but on the mounting plate surface.
3. **Outer ring** -- drawCircle, color = Hardware.SwitchRing. Unchanged.
4. **Inner surface** -- drawCircle with RadialGradient:
   - Colors: [Hardware.SwitchHi, Hardware.SwitchFace, Hardware.SwitchRing]
   - Three-stop gradient for better depth.
5. **Chrome highlight** -- same as current but using Hardware.ChromeHi color.
6. **Socket head detail** -- drawCircle, radius = switchRadius * 0.15, center, color = Hardware.KnobDark at 60% alpha. A small dark circle in the center of the switch suggesting the hex socket of a cap screw. This is the "machined" detail that sells the industrial look.

**Sizes:**
- Compact: 36dp (module header, used in current RackModule)
- Standard: 44dp (pedal card)
- Stage: 56dp (metering strip bypass, live performance mode)

### 6.4 RackModule (Enhanced)

The existing RackModule.kt has the correct architecture (collapsed header + animated expanded content). Forge enhances the visual treatment.

**Collapsed Header (52dp):**

Current drawing:
- Surface color (category tint composited over ModuleSurface)
- Vertical accent bar (3dp left edge, category header color)

Forge drawing (via drawBehind):
1. **Panel base** -- drawRect, full module bounds, color = per-effect Panel color (from Section 4.2).
2. **Top edge highlight** -- drawLine, y=0, full width, strokeWidth = 1dp, color = White at 5% alpha. Top-lit edge catch.
3. **Bottom edge shadow** -- drawLine, y=height, full width, strokeWidth = 1dp, color = Black at 15% alpha. Bottom shadow.
4. **Left accent bar** -- drawRect, x=0, width=3dp, full height, color = per-effect header accent (from Section 4.4). This is the "painted stripe" on the panel that identifies the effect category.
5. **Brushed texture** (optional, Phase 3) -- 3-4 horizontal drawLine calls across the panel surface, 1px wide, 1-2% white alpha, spaced 4-6px apart. Creates a very subtle horizontal grain suggesting brushed metal.

**Collapsed header content (unchanged layout, updated colors):**
```
[3dp accent bar | 12dp pad | LED(10dp) | 12dp | Icon(16sp) | 8dp | Name(14sp) | flex | StompSwitch(36dp) | 8dp]
```

**Expanded content panel:**

When expanded, the area below the header reveals the parameter controls. The expanded area uses:
- Same Panel base color as the header (continuous surface -- it is one physical panel).
- A recessed "control well" for the knobs: drawRoundRect inset 8dp from edges, cornerRadius 6dp, color = Chassis.Light (darker than the panel, creating an inset feel where controls live).
- Knobs sit on this recessed surface.
- Horizontal divider between header and controls: drawLine using Chassis.DividerLine color.

### 6.5 MeteringStrip (Enhanced)

The existing MeteringStrip.kt is well-structured. Forge treatment:

**Collapsed (52dp):**
- Background: Chassis.Mid (matches main screen background, not ModuleSurface).
- Top edge: 1dp line, Chassis.Highlight color.
- Meter bars: Unchanged Canvas rendering, use Meter.Background for track.
- Mini spectrum: Unchanged.
- Labels: "IN" / "OUT" in Text.Secondary uppercase. dBFS values in Monospace, Text.Readout.

**Expanded:**
- Full spectrum analyzer: Unchanged Canvas rendering.
- Master controls row: IN/OUT RotaryKnobs with Forge treatment, bypass StompSwitch (Stage size, 56dp).
- Gain value readouts in Monospace, Text.Terminal (#78B878) -- this is the ONE place where terminal green is used, because these are instrument-level readouts.

### 6.6 TopBar (Enhanced)

The 56dp fixed top bar gets a "rack panel" treatment.

**Visual:**
- Background: Chassis.Light (#1C1C20) -- slightly lighter than main background to distinguish as a control surface.
- Bottom edge: 1dp line, Chassis.DividerLine.
- Left accent: Engine status LED (LedIndicator, Large size, 14dp) with Forge lens bezel.
- Preset name: Text.Stamped color, 16sp SemiBold sans-serif. Tap opens preset browser.
- Right icons: Unicode icons in Text.Secondary. Active states (tuner, looper, A/B) use their respective accent colors.

**No skeuomorphic treatment on the TopBar** -- it should feel like the "control panel frame" of the rack, clean and functional, contrasting with the textured effect modules below it.

### 6.7 Module-Specific Enhancements (Phase 3)

These are per-effect visual customizations that go beyond the base Forge treatment:

**AmpModel:**
- Panel color: Panel.AmpModel (#2A2418) -- deep tolex brown.
- Knobs: LARGE size (68dp). Three knobs in a row.
- Special: Vertical grain texture (drawLine calls, vertical, 1-2% white alpha) suggesting tolex texture.
- "CHANNEL" label plate: a 36dp x 16dp rounded rect in Hardware.Chrome color with amp model name stamped in Text.Stamped at 10sp.

**CabinetSim:**
- Panel color: Panel.CabinetSim (#242420).
- Special: Circular speaker grille hint -- drawCircle at center of expanded panel, 60dp radius, Hardware.Bezel color at 30% alpha. Inside, 8-12 radial lines emanating from center (2% white alpha). Suggests looking at a speaker cone.

**ParametricEQ:**
- Expanded panel includes a mini frequency response curve (Canvas-drawn path), 80dp height, showing the approximate EQ shape. This exists in the design doc but not yet implemented.

**Delay:**
- Tap tempo button styled as a StompSwitch (Stage size) in the expanded panel.
- BPM readout in Text.Terminal (#78B878) monospace, 14sp -- the "LCD display" of the delay.

**Tuner (overlay):**
- Retains existing TunerDisplay but wraps it in a "instrument panel" frame: rounded rect with Hardware.Bezel border, Chassis.Dark background, and subtle corner screws (4 small circles at corners, Hardware.Chrome color).

---

## 7. Layout Templates

### 7.1 Main Screen (PedalboardScreen)

```
+================================================================+
| TOPBAR (56dp)                                                   |
| [LED 14dp] [Preset Name >>>>>>>>>>>] [Tuner] [Loop] [AB] [Gear]|
+================================================================+
| ENGINE ERROR BANNER (conditional)                               |
+----------------------------------------------------------------+
| A/B COMPARISON BAR (conditional)                                |
+================================================================+
|                                                                  |
|  RACK (LazyColumn, weight=1f, scrollable, drag-reorderable)    |
|                                                                  |
|  +------------------------------------------------------------+ |
|  |===| [LED] [icon] Noise Gate                    [STOMP]     | |  <- Collapsed
|  +------------------------------------------------------------+ |
|  |===| [LED] [icon] Compressor                    [STOMP]     | |  <- Collapsed
|  +------------------------------------------------------------+ |
|  |===| [LED] [icon] Overdrive                     [STOMP]     | |  <- Collapsed
|  |   |  ---------------------------------------------------- | |
|  |   |  +--recessed control well--+                           | |  <- Expanded
|  |   |  |  (Drive)  (Tone)  (Level)  |                        | |
|  |   |  |   0.50     0.65    0.80    |                        | |
|  |   |  +----------------------------+                        | |
|  |   |  Mix  [========O==============]  85%                   | |
|  |   |                                          [Advanced]    | |
|  +------------------------------------------------------------+ |
|  |===| [LED] [icon] Amp Model                     [STOMP]     | |  <- Collapsed
|  +------------------------------------------------------------+ |
|  ...                                                             |
|                                                                  |
+================================================================+
| METERING STRIP (52dp collapsed)                                  |
| IN -12  [====]  [||||spectrum||||]  [====]  -18 OUT             |
+================================================================+
```

### 7.2 Expanded Module Detail (Per-Effect)

**2-knob effect (Chorus, Vibrato):**
```
+------------------------------------------------------------+
|===| [LED] [icon] Chorus                         [STOMP]    |
|   | --------------------------------------------------------|
|   |  +-- recessed well (Chassis.Light bg, 6dp corner) ----+ |
|   |  |                                                     | |
|   |  |     (Rate)          (Depth)                         | |
|   |  |     5.2 Hz           0.65                           | |
|   |  |                                                     | |
|   |  +-----------------------------------------------------+ |
|   |  MODE  [=====Chorus=======|Vibrato=====]  Chorus       | |
|   |  MIX   [================O==============]  78%          | |
+------------------------------------------------------------+
```

**3-knob effect (Overdrive, DS-1, Flanger):**
```
+------------------------------------------------------------+
|===| [LED] [icon] Overdrive                      [STOMP]    |
|   | --------------------------------------------------------|
|   |  +-- recessed well ----+                                |
|   |  |  (Drive) (Tone) (Level)  |                           |
|   |  |   0.50    0.65   0.80    |                           |
|   |  +-------------------------+                            |
|   |  MIX  [========O==============]  85%                    |
+------------------------------------------------------------+
```

**4-knob effect (Noise Gate, Phaser, Reverb):**
```
+------------------------------------------------------------+
|===| [LED] [icon] Reverb                         [STOMP]    |
|   | --------------------------------------------------------|
|   |  +-- recessed well (2x2 knob grid) --+                 |
|   |  |  (Decay)    (Damping)             |                  |
|   |  |   3.20 s     0.45                 |                  |
|   |  |  (Pre-Dly)  (Size)               |                  |
|   |  |   25 ms      0.70                 |                  |
|   |  +-----------------------------------+                  |
|   |  MIX  [========O==============]  85%                    |
+------------------------------------------------------------+
```

**6+ knob effect (Compressor, Parametric EQ):**
```
+------------------------------------------------------------+
|===| [LED] [icon] Compressor                     [STOMP]    |
|   | --------------------------------------------------------|
|   |  +-- recessed well (2x3 knob grid, SMALL 44dp) --+     |
|   |  |  (Thresh) (Ratio)  (Attack)                   |     |
|   |  |  -18 dB    4.0:1   5.0 ms                    |     |
|   |  |  (Release) (Makeup) (Knee)                    |     |
|   |  |  150 ms    +3.0 dB  0.50                     |     |
|   |  +-----------------------------------------------+     |
|   |  MODE  [===VCA===|====OTA====]  VCA                     |
|   |  MIX  [========O==============]  85%                    |
+------------------------------------------------------------+
```

### 7.3 Metering Strip Expanded

```
+================================================================+
| METERING STRIP (expanded, ~200dp)                               |
| ================================================================|
| IN -12  [==============]  [||||spectrum||||]  [==============]  |
|                           [||||||||||||||||]                     |
|                           [||||||||||||||||]  80dp               |
|                           [||||||||||||||||]                     |
|  --------- dB reference lines: -12, -24, -48 ---------         |
|                                                                  |
|       (IN GAIN)        [BYPASS STOMP]        (OUT GAIN)         |
|       -2.4 dB           56dp switch          +1.2 dB            |
|                         ACTIVE / BYPASS                          |
+================================================================+
```

---

## 8. Compose Implementation Strategy

### 8.1 Canvas Drawing Performance

**Key principle: every Forge enhancement is additive Canvas draw calls, not structural changes.**

The current RotaryKnob uses 4 draw calls. Forge uses 8. At 3 visible expanded knobs, that is 24 draw calls for knobs alone. With module surfaces (5 draws per module, ~5-6 visible modules = 30 draws) and LEDs (5 draws per LED, ~6 visible = 30 draws), the total is approximately:

```
Scenario: 6 collapsed modules + 1 expanded (3 knobs)
Module surfaces: 6 collapsed * 4 draws = 24
                 1 expanded * 6 draws = 6
LEDs: 7 * 5 draws = 35
StompSwitches: 7 * 6 draws = 42
Knobs: 3 * 8 draws = 24
Meters: ~10 draws
Total: ~141 Canvas draw calls
```

For comparison, the spectrum analyzer alone draws 64 bars + 3 reference lines = 67 draws, and it runs smoothly at 30fps. 141 total draw calls per frame is well within the Canvas budget on any modern Android device.

**Brushed texture optimization:** If horizontal line brushed metal texture is added (Phase 3), limit to 4 lines per module, drawn only for visible modules (LazyColumn handles this automatically). The lines are drawLine calls at 1-2% alpha -- nearly invisible in isolation but perceptible as texture in aggregate.

### 8.2 Gradient and Texture Techniques

**Three-stop RadialGradient for knobs:**
```kotlin
Brush.radialGradient(
    colorStops = arrayOf(
        0.0f to Hardware.KnobLight,   // Center highlight
        0.6f to Hardware.KnobMid,     // Body
        1.0f to Hardware.KnobDark     // Edge shadow
    ),
    center = Offset(center.x - radius * 0.2f, center.y - radius * 0.2f),
    radius = bodyRadius
)
```

**Top-edge highlight on module surfaces:**
```kotlin
Modifier.drawBehind {
    // Panel fill
    drawRect(color = panelColor)
    // Top edge catch (1dp, white at 5%)
    drawLine(
        color = Color.White.copy(alpha = 0.05f),
        start = Offset(0f, 0.5f),
        end = Offset(size.width, 0.5f),
        strokeWidth = 1.dp.toPx()
    )
    // Bottom edge shadow (1dp, black at 15%)
    drawLine(
        color = Color.Black.copy(alpha = 0.15f),
        start = Offset(0f, size.height - 0.5f),
        end = Offset(size.width, size.height - 0.5f),
        strokeWidth = 1.dp.toPx()
    )
}
```

**Recessed control well:**
```kotlin
// Inside expanded content, before knobs
Box(
    modifier = Modifier
        .fillMaxWidth()
        .padding(horizontal = 8.dp)
        .clip(RoundedCornerShape(6.dp))
        .background(Chassis.Light)
        .drawBehind {
            // Inner shadow at top edge (inset effect)
            drawLine(
                color = Color.Black.copy(alpha = 0.2f),
                start = Offset(4.dp.toPx(), 0.5f),
                end = Offset(size.width - 4.dp.toPx(), 0.5f),
                strokeWidth = 1.dp.toPx()
            )
            // Inner highlight at bottom edge
            drawLine(
                color = Color.White.copy(alpha = 0.03f),
                start = Offset(4.dp.toPx(), size.height - 0.5f),
                end = Offset(size.width - 4.dp.toPx(), size.height - 0.5f),
                strokeWidth = 1.dp.toPx()
            )
        }
        .padding(12.dp)
) {
    // Knobs go here
}
```

### 8.3 DesignSystem.kt Migration

The existing DesignSystem.kt object structure is retained and extended. The new tokens are added alongside the existing ones, and the existing `EffectCategory` enum is preserved (its color values remain the accent colors). What changes:

1. **New `object Chassis`** nested inside DesignSystem for chassis colors.
2. **New `object Hardware`** nested inside DesignSystem for control surface colors.
3. **New `object Text` rename** -- current TextPrimary, TextSecondary etc. are aliased to the new Text.Stamped etc. names.
4. **New `fun getPanelColor(effectIndex: Int): Color`** method that returns the per-effect panel fill color.
5. **New per-pedal header overrides** expanded to include BigMuff, FuzzFace, Rangemaster, Octavia, RotarySpeaker.

The existing `getCategory()`, `getHeaderColor()`, `getEffectIcon()`, and `EffectCategory` enum remain unchanged -- they provide the accent system. The Forge layer adds the panel (enclosure) colors on top.

### 8.4 What Stays the Same

These components need NO changes in the Forge redesign:

- **PedalboardScreen.kt** -- Layout structure (TopBar, LazyColumn of RackModules, MeteringStrip, overlays) is correct.
- **EffectEditorSheet.kt** -- Bottom sheet architecture stays. Visual treatment of the sheet content may use RotaryKnob Forge style, but the layout is unchanged.
- **Gesture handling** in RotaryKnob.kt -- awaitEachGesture, vertical drag, slop detection, pointer consumption all stay.
- **TunerDisplay.kt** -- Wrap in a Forge "instrument panel" frame, but the Canvas LED meter stays.
- **LooperOverlay, LooperTrimEditor, LooperWaveform** -- These already have their own visual language; Forge treatment is optional polish.
- **PresetBrowserOverlay** -- Already a full-screen overlay; updated colors but layout unchanged.
- **ABComparisonBar** -- Color updates only.

---

## 9. Migration from Current Design System

### 9.1 Color Token Mapping (Current to Forge)

```
Current Token           -> Forge Token              Notes
------                     ---------                -----
Background (#141416)    -> Chassis.Mid (#161618)     Slightly warmer
ModuleSurface (#222226) -> Varies per module         getPanelColor(effectIndex)
ModuleBorder (#333338)  -> Chassis.Screw (#2A2A30)   Slightly darker
Divider (#2E2E34)       -> Chassis.DividerLine       Nearly same
TextPrimary (#E0E0E4)   -> Text.Stamped (#C8C8CC)    Warmer, less bright
TextSecondary (#888890) -> Text.Secondary (#808088)   Nearly same
TextValue (#D0D0D8)     -> Text.Readout (#D8D8DC)    Slightly brighter
TextMuted (#555560)     -> Text.Muted (#484850)       Slightly darker
KnobBody (#3C3C42)      -> Hardware.KnobMid (#3C3C44) Nearly same
KnobOuterRing (#2A2A30) -> Hardware.KnobEdge (#282830) Nearly same
KnobHighlight (#4A4A52) -> Hardware.KnobLight (#505058) Slightly brighter
KnobPointer (#F0F0F0)   -> Hardware.KnobPointer (#E8E8EC) Slightly warmer
```

Most changes are subtle refinements (2-4 points in each RGB channel). The visual difference per-token is barely perceptible; the cumulative effect is a warmer, more cohesive palette.

### 9.2 Backward Compatibility

The existing `DesignSystem.Background`, `DesignSystem.ModuleSurface`, etc. can be kept as deprecated aliases pointing to the new Forge tokens during migration. This allows incremental component updates without breaking the build.

---

## 10. Implementation Priority

### Phase 1: Forge Foundation (1 sprint)

**Goal:** Maximum visual transformation with minimum code changes. Focus on the components that are drawn every frame and visible on every screen.

1. **Update DesignSystem.kt** -- Add Chassis, Hardware, and Text nested objects. Add getPanelColor(). Add per-pedal header overrides for new pedals (BigMuff, FuzzFace, Rangemaster, Octavia, RotarySpeaker). Keep old tokens as aliases.

2. **Update RotaryKnob.kt** -- Add recessed bezel, three-stop radial gradient, surface shine, pointer dot, and tick marks. This is the single highest-impact visual change because knobs are the primary interactive element.

3. **Update LedIndicator.kt** -- Add lens bezel and lens reflection draws.

4. **Update StompSwitch.kt** -- Add mounting plate and socket head detail.

5. **Update Theme.kt** -- Update DarkColorScheme colors to match Forge Chassis and Text tokens.

**Effort estimate:** ~200 lines of code changes across 5 files. Zero architectural changes. Zero risk to functionality.

### Phase 2: Module Surfaces (1 sprint)

**Goal:** Transform the rack module visual from "flat colored rectangle" to "painted metal panel."

1. **Update RackModule.kt** -- Replace flat surface color with per-effect Panel color. Add top-edge highlight and bottom-edge shadow. Add recessed control well for expanded content.

2. **Update MeteringStrip.kt** -- Apply Chassis colors. Use Text.Terminal for dB readouts in expanded mode.

3. **Update TopBar.kt** -- Apply Chassis.Light background. Larger LED (14dp) with Forge treatment.

4. **Update MasterControlsRow** -- Stage-size bypass StompSwitch. Forge-style gain knobs.

**Effort estimate:** ~150 lines of code changes. No architectural changes.

### Phase 3: Material Polish (1 sprint)

**Goal:** The "last 20%" of visual refinement that separates "good" from "professional."

1. **Brushed texture** on module surfaces (4 horizontal lines per module, drawLine calls).
2. **Per-effect special treatments** (amp tolex texture, cab speaker grille, delay LCD readout).
3. **Tuner instrument frame** (bezel border, corner screws).
4. **Subtle noise overlay** on Panel surfaces (optional -- test performance first).
5. **Preset browser** visual refresh (per-preset category color dot, active indicator).

**Effort estimate:** ~300 lines of new code, mostly in drawBehind modifiers.

### Priority Matrix

```
                    HIGH VISUAL IMPACT
                          |
   Phase 1                |          Phase 3
   RotaryKnob Forge       |          Brushed texture
   LED lens bezel         |          Per-effect specials
   StompSwitch detail     |          Tuner frame
                          |
LOW EFFORT ---------------+--------------- HIGH EFFORT
                          |
   Phase 1b               |          Phase 2
   Color token update     |          Module panel colors
   Theme.kt refresh       |          Recessed control well
                          |          TopBar/MeteringStrip
                    LOW VISUAL IMPACT
```

Phase 1 is the critical path. It delivers 70% of the visual transformation for 30% of the total effort. A user comparing the app before and after Phase 1 will see the difference in the first 2 seconds -- the knobs, LEDs, and stomp switches are the elements they interact with directly.

---

## Sources & References

### Design Research
- [Skeuomorphism: How Plugin Design Affects What You Hear - LANDR](https://blog.landr.com/skeuomorphism-plugins/)
- [Is Skeuomorphism the Best Approach for iOS Audio Apps? - FutureSonic](https://futuresonic.io/discussion/is-skeuomorphism-the-best-approach-for-ios-audio-apps/)
- [Why Is Skeuomorphism Not Dead Yet in Music Production UI - UX Collective](https://uxdesign.cc/why-is-skeuomorphism-hard-to-die-in-music-production-interface-design-a634efa3e089)
- [The Terminal Aesthetic and the Return of Texture to the Web - Medium](https://medium.com/@phazeline/the-terminal-aesthetic-and-the-return-of-texture-to-the-web-ed37ee8183bd)
- [What is Skeuomorphism? A Complete 2026 Guide - Big Human](https://www.bighuman.com/blog/guide-to-skeuomorphic-design-style)
- [What Is Neumorphism in UI Design? A Complete 2026 Guide - Big Human](https://www.bighuman.com/blog/neumorphism)
- [Plugin Designs - Why We Love Some And Hate Others - Production Expert](https://www.production-expert.com/production-expert-1/plugin-designs-why-we-love-some-and-hate-others)
- [Beyond Skeuomorphism: The Evolution of Music Production Software User Interface Metaphors - ARPJ](https://www.arpjournal.com/asarpwp/beyond-skeuomorphism-the-evolution-of-music-production-software-user-interface-metaphors-2/)
- [Skeuomorphism: Balancing Realism and Figurative Design - JustInMind](https://www.justinmind.com/ui-design/skeuomorphic)
- [Skeuomorphism Tips and Tricks - JUCE Forum](https://forum.juce.com/t/skeuomorphism-tips-and-tricks/44994)

### Competitive Analysis
- [Opinion on Skeuomorphic Design - Neural DSP Community](https://unity.neuraldsp.com/t/opinion-on-skeuomorphic-design/19525)
- [Guitar Rig 7 Pro - Native Instruments](https://www.native-instruments.com/en/products/komplete/guitar/guitar-rig-7-pro/)
- [TH-U - Overloud](https://www.overloud.com/products/thu)
- [GarageBand iOS Amp Controls - Apple Support](https://support.apple.com/guide/garageband-iphone/play-the-amp-chs39281180/ios)

### Compose Implementation
- [Neumorphism in Jetpack Compose - Sina Samaki](https://www.sinasamaki.com/neumorphism-in-jetpack-compose/)
- [compose-neumorphism Library - GitHub](https://github.com/sridhar-sp/compose-neumorphism)
- [Implementing Custom Shadows with Jetpack Compose - Medium](https://medium.com/@hanihashemi/implementing-custom-shadows-with-jetpack-compose-for-neumorphism-design-cd666887a642)

### UI Trends
- [16 Key Mobile App UI/UX Design Trends 2026 - SPDLoad](https://spdload.com/blog/mobile-app-ui-ux-design-trends/)
- [I Love Terminal Aesthetics - DEV Community](https://dev.to/micronink/i-love-terminal-aesthetics-not-everyone-does-heres-how-i-solved-that-56ef)
