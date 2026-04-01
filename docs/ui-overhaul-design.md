# Guitar Emulator App - UI Overhaul Design Document

## Table of Contents
1. [Guitar Rig Design Philosophy Analysis](#1-guitar-rig-design-philosophy-analysis)
2. [Visual Language for Our 21 Effects](#2-visual-language-for-our-21-effects)
3. [Screen Architecture for Mobile](#3-screen-architecture-for-mobile)
4. [Compose Techniques Required](#4-compose-techniques-required)
5. [Phased Implementation Plan](#5-phased-implementation-plan)
6. [Appendix: Color System Reference](#appendix-color-system-reference)

---

## 1. Guitar Rig Design Philosophy Analysis

### 1.1 Visual Language Evolution

Guitar Rig has evolved through three distinct visual phases:

**Guitar Rig 1-5 (2004-2012): Full Skeuomorphism**
Hyper-realistic 3D renderings of hardware. Scratched metal faceplates, photorealistic
chicken-head knobs with specular highlights, realistic tolex textures on amp panels,
rack screws and ventilation grilles. Every component looked like a high-resolution
photograph of its hardware counterpart.

**Guitar Rig 6 (2020): Transitional Modern**
Stripped away photorealistic textures while retaining 3D depth. Introduced cleaner
flat-shaded surfaces with subtle gradients suggesting material properties. Knobs became
simpler 3D shapes with shadow/highlight rather than photographed textures. The rack
metaphor was retained but visually simplified.

**Guitar Rig 7 (2024): Clean Modern with 3D Touches**
Completed the transition to a predominantly flat aesthetic with tasteful 3D accents.
Every component (even stomp pedals) is presented as a standard rack device module.
No attempt to reproduce "scratched, beer-stained pedals." Instead, clean surfaces with
consistent module heights, clear labeling, and functional visual hierarchy.

### 1.2 The Rack-Based Signal Chain Layout

Guitar Rig's defining UI concept is the vertical rack. Effects stack in a single
column from top to bottom, with each module occupying a standardized rack height.
Key properties:

- **Vertical flow**: Signal flows top-to-bottom, matching the mental model of
  rack-mounted studio gear. Each module is the full width of the rack.
- **Standardized module heights**: Most modules occupy 1U, 2U, or 3U of rack space.
  Simple effects (gate, boost) get 1U. Complex effects (amp, EQ) get 2-3U.
- **Collapsed/expanded states**: Modules can collapse to a thin strip showing only
  name + bypass, expanding on click to reveal full parameter controls.
- **Drag-and-drop reordering**: Modules can be grabbed and dragged vertically to
  change signal chain order.
- **Signal flow sidebar**: Guitar Rig 7 added a simplified signal flow diagram on
  the left side showing the chain as connected blocks, enabling quick reordering.

### 1.3 Color Palette and Dark Theme

Guitar Rig uses a dark, muted industrial palette:

- **Background**: Not pure black. A warm dark gray around #1C1C1C to #242424.
- **Module surfaces**: Slightly lighter than background, #2A2A2A to #343434,
  with subtle gradient to suggest depth (lighter at top edge, darker at bottom).
- **Accent colors**: Muted and desaturated compared to typical Material Design.
  Teal/cyan for active states rather than bright green. Orange for gain/distortion
  indicators. Blue for modulation. No saturated primary colors on surfaces.
- **Text**: Off-white (#D8D8D8) for primary labels, medium gray (#808080) for
  secondary, bright white (#FFFFFF) only for active values and selected items.
- **LED indicators**: Realistic LED rendering with glow effect (not flat circles).
  Green for active, red for clipping, amber for caution.

### 1.4 Knob and Control Rendering

Guitar Rig 6/7 uses a "clean 3D" knob style:

- **Rotary knobs**: Circular with a visible pointer line. Subtle radial gradient
  suggesting a metallic surface. Drop shadow below. Position indicator dot or line
  in contrasting color. No photorealistic texture, but clearly 3D (not flat).
- **Sliders**: Thin groove with a prominent thumb. Used primarily for EQ bands
  and linear parameters.
- **Switches**: Toggle switches for on/off, segmented controls for mode selection.
  Subtle 3D emboss/inset effect.
- **Interaction**: Desktop uses mouse drag (vertical movement changes value).
  No rotation gesture required.

### 1.5 How Different Effect Categories Are Presented

In Guitar Rig 7, all effects use the same rack module format, but differentiation
comes through:

- **Header color accent**: A thin colored strip or tinted header area identifies
  the category (warm for distortion, cool for modulation, etc.).
- **Module layout**: Amp modules get larger panels with more knobs in a row.
  Stomp effects get compact layouts. Utility effects are minimal.
- **Knob count and arrangement**: The number and layout of knobs visually
  communicates complexity.

### 1.6 The Browser Sidebar

Guitar Rig 7's browser enables effect selection through:

- **Keyword-based filtering**: Character, Genre, Artist tags narrow results.
- **Category hierarchy**: Amps > Stompboxes > Rack Effects > Tools.
- **Instant preview**: Click to load, hear immediately.
- **Visual thumbnails**: Small module previews in the browser list.

### 1.7 What Translates to Mobile and What Does Not

**Translates well:**
- Dark theme with warm grays
- Rack-style vertical stacking (natural for mobile vertical scroll)
- Collapsed/expanded module states
- LED indicators with glow effects
- Category color coding
- Clean 3D knob aesthetics via Canvas drawing

**Does NOT translate:**
- Side-by-side browser + rack (no room on phone; needs separate view or overlay)
- Hover tooltips (no hover on touch)
- Small, precise knob adjustments (need larger touch targets)
- Dual signal path visualization (too complex for phone width)
- Multi-column knob layouts (5+ knobs in a row won't fit)

**Mobile adaptations needed:**
- Signal chain as a scrollable vertical list (not side-by-side with browser)
- Bottom sheet for effect editing (thumb-reachable)
- Knobs must be minimum 56dp diameter with vertical drag gesture
- Two knobs per row maximum on phone; three on tablet
- Collapse/expand to manage vertical space

---

## 2. Visual Language for Our 21 Effects

### 2.1 Effect Category Taxonomy

Our 21 effects group into 6 visual categories, each with a distinct color identity:

```
GAIN/DISTORTION (Warm palette)
  - Noise Gate (index 0)
  - Compressor (index 1)
  - Boost (index 3)
  - Overdrive (index 4)
  - Boss DS-1 (index 16)
  - ProCo RAT (index 17)
  - Boss DS-2 (index 18)
  - Boss HM-2 (index 19)

AMP/CABINET (Amber/brown palette)
  - Amp Model (index 5)
  - Cabinet Sim (index 6)

TONE/EQ (Neutral/silver palette)
  - Parametric EQ (index 7)

MODULATION (Cool blue-purple palette)
  - Wah (index 2)
  - Chorus (index 8)
  - Vibrato (index 9)
  - Phaser (index 10)
  - Flanger (index 11)
  - Tremolo (index 15)
  - Uni-Vibe (index 20)

TIME-BASED (Teal/cyan palette)
  - Delay (index 12)
  - Reverb (index 13)

UTILITY (Slate/blue-gray palette)
  - Tuner (index 14)
```

### 2.2 Category Color System

Each category has a primary color, a surface tint, a knob accent, and an LED color:

```
GAIN/DISTORTION
  Primary:      #D4533B  (warm red-orange)
  Surface Tint: #2D1F1A  (dark warm brown, module background)
  Knob Accent:  #E8724A  (orange indicator line on knobs)
  LED Active:   #FF6B3D  (orange LED glow)
  LED Bypass:   #3D2218  (dim orange)
  Header Bar:   #C04030  (thin accent strip at top of module)

AMP/CABINET
  Primary:      #C8963C  (amber/gold)
  Surface Tint: #2A2418  (dark amber-brown)
  Knob Accent:  #D4A84A  (gold indicator)
  LED Active:   #F0C040  (warm amber LED)
  LED Bypass:   #382E18  (dim amber)
  Header Bar:   #B88830  (amber strip)

TONE/EQ
  Primary:      #909098  (neutral silver)
  Surface Tint: #242428  (cool dark gray)
  Knob Accent:  #A8A8B0  (silver indicator)
  LED Active:   #C0C0CC  (white-blue LED)
  LED Bypass:   #2A2A30  (dim silver)
  Header Bar:   #707078  (gray strip)

MODULATION
  Primary:      #6B7EC8  (blue-violet)
  Surface Tint: #1C1E2D  (dark blue-tinted)
  Knob Accent:  #7B90E0  (blue indicator)
  LED Active:   #6888F0  (blue LED)
  LED Bypass:   #1E2038  (dim blue)
  Header Bar:   #5868A8  (blue strip)

TIME-BASED
  Primary:      #3CA8A0  (teal)
  Surface Tint: #182A28  (dark teal-tinted)
  Knob Accent:  #50C8BE  (teal indicator)
  LED Active:   #40D8C8  (teal LED)
  LED Bypass:   #183028  (dim teal)
  Header Bar:   #308880  (teal strip)

UTILITY
  Primary:      #607088  (slate blue-gray)
  Surface Tint: #1C2028  (dark slate)
  Knob Accent:  #7890A8  (gray-blue indicator)
  LED Active:   #6898C0  (cool blue LED)
  LED Bypass:   #1E2430  (dim blue-gray)
  Header Bar:   #506878  (slate strip)
```

### 2.3 Global Color Constants

```
BACKGROUND:         #141416  (near-black with slight blue undertone)
MODULE_SURFACE:     #222226  (base module surface before category tint)
MODULE_BORDER:      #333338  (subtle border around modules)
DIVIDER:            #2E2E34  (section dividers within modules)

TEXT_PRIMARY:        #E0E0E4  (main labels, effect names)
TEXT_SECONDARY:      #888890  (parameter labels, descriptions)
TEXT_VALUE:          #D0D0D8  (parameter value readouts)
TEXT_MUTED:          #555560  (disabled, placeholder text)

KNOB_BODY:           #3C3C42  (knob face color)
KNOB_OUTER_RING:     #2A2A30  (shadow ring around knob)
KNOB_HIGHLIGHT:      #4A4A52  (top highlight on knob surface)
KNOB_POINTER:        #F0F0F0  (white pointer line on knob)

CLIP_RED:            #F04040  (clipping indicator)
WARNING_AMBER:       #E8A030  (approaching limit)
SAFE_GREEN:          #40B858  (safe level)

METER_BG:            #181820  (meter track background)
METER_GREEN:         #40A850  (normal level)
METER_YELLOW:        #D8C030  (hot level)
METER_RED:           #D83838  (clipping level)
METER_PEAK_HOLD:     #E0E0E0  (peak hold line)
```

### 2.4 Per-Effect Visual Specifications

#### Gain/Distortion Effects

**Noise Gate** (index 0)
- Visual: Compact utility-style module. 1U height collapsed, 2U expanded.
- Layout: 4 knobs in 2x2 grid (Threshold, Hysteresis, Attack, Release).
- Special: Gate meter showing open/closed state as a horizontal bar.
- Color: Gain category palette (warm red-orange accents).

**Compressor** (index 1)
- Visual: 2U module with gain reduction meter.
- Layout: 6 knobs in 3x2 grid (Threshold, Ratio, Attack, Release, Makeup, Knee).
- Special: Horizontal gain reduction meter (0 to -20 dB) below knobs.
- Color: Gain category palette.

**Boost** (index 3)
- Visual: Minimal 1U module. Single large centered knob.
- Layout: 1 large knob (Level) centered.
- Special: Level boost indicator arc around the knob.
- Color: Gain category palette.

**Overdrive** (index 4)
- Visual: Classic 2U stomp-box inspired module. Three knobs in a row.
- Layout: 3 knobs left-to-right (Drive, Tone, Level).
- Special: Drive knob has a "heat" glow effect that intensifies with value.
- Color: Gain category palette, slightly warmer (#2D1D18 surface).

**Boss DS-1** (index 16)
- Visual: 2U module with orange accent strip. Three knobs in a row.
- Layout: 3 knobs (Dist, Tone, Level). Reference: real DS-1 is orange.
- Special: Header bar uses #D06020 (Boss orange). Surface tint warmer.
- Unique header color: #D06020 (distinct orange, referencing the real pedal).

**ProCo RAT** (index 17)
- Visual: 2U module. Three knobs.
- Layout: 3 knobs (Distortion, Filter, Volume).
- Special: Header bar uses #404040 (dark gray, referencing the black RAT enclosure).
  Knob style slightly different: larger pointer dot instead of line.
- Unique header color: #484848 (charcoal gray).

**Boss DS-2** (index 18)
- Visual: 2U module with turbo orange accents. Four controls.
- Layout: 3 knobs (Dist, Tone, Level) + 1 toggle (Mode I/II).
- Special: Mode toggle switch rendered as a physical-looking 2-position toggle.
- Unique header color: #D07020 (deep orange, like the real DS-2).

**Boss HM-2** (index 19)
- Visual: 2U module. Four knobs.
- Layout: 4 knobs in a row (Dist, Low, High, Level).
- Special: Header uses #C84020 (red-orange, HM-2's distinctive color).
  Knobs should feel heavier/more industrial.
- Unique header color: #C04830 (burnt orange-red).

#### Amp/Cabinet

**Amp Model** (index 5)
- Visual: Largest module, 3U. Amp front-panel aesthetic.
- Layout: 3 knobs in a row (Input Gain, Output Level, Model Type).
  Model Type uses a segmented selector, not a knob.
- Special: Textured background suggesting tolex. Larger knobs (68dp vs standard 56dp).
  Chicken-head style knob pointers. Warm back-glow behind active state.
  Neural model loader section below knobs.
- Color: Amp palette (amber/gold). Richer surface tint.

**Cabinet Sim** (index 6)
- Visual: 2U module. Speaker grille texture hint in background.
- Layout: 2 controls (Cabinet Type selector, Mix knob).
  Cabinet Type uses segmented control (4 options).
- Special: Small speaker cone icon. IR loader section.
- Color: Amp palette.

#### Tone/EQ

**Parametric EQ** (index 7)
- Visual: 3U module (7 parameters need space). Silver/neutral aesthetic.
- Layout: 7 knobs in structured layout.
  Row 1: Low Freq, Low Gain
  Row 2: Mid Freq, Mid Gain, Mid Q
  Row 3: High Freq, High Gain
- Special: Mini frequency response curve visualization above knobs
  (Canvas-drawn, showing approximate EQ shape based on current settings).
- Color: Tone/EQ palette (neutral silver).

#### Modulation Effects

**Wah** (index 2)
- Visual: 2U module. Four controls.
- Layout: 3 knobs (Sensitivity, Frequency, Resonance) + 1 toggle (Auto/Manual).
- Special: Wah sweep indicator showing current frequency position.
- Color: Modulation palette (blue-violet).

**Chorus** (index 8)
- Visual: Compact 1U-2U module. Two knobs.
- Layout: 2 knobs side by side (Rate, Depth).
- Special: Subtle animated shimmer on module surface when active.
- Color: Modulation palette.

**Vibrato** (index 9)
- Visual: Compact 1U-2U module. Two knobs.
- Layout: 2 knobs (Rate, Depth).
- Color: Modulation palette.

**Phaser** (index 10)
- Visual: 2U module. Four knobs.
- Layout: 4 knobs in 2x2 grid (Rate, Depth, Feedback, Stages).
  Stages could use a segmented control (2/4/6/8).
- Color: Modulation palette.

**Flanger** (index 11)
- Visual: 2U module. Three knobs.
- Layout: 3 knobs in a row (Rate, Depth, Feedback).
- Color: Modulation palette.

**Tremolo** (index 15)
- Visual: 2U module. Three controls.
- Layout: 2 knobs (Rate, Depth) + 1 toggle (Sine/Square).
- Special: Shape toggle rendered as a waveform icon switch.
- Color: Modulation palette.

**Uni-Vibe** (index 20)
- Visual: 2U module. Three controls.
- Layout: 2 knobs (Speed, Intensity) + 1 toggle (Chorus/Vibrato).
- Special: Vintage-inspired slightly warmer surface tint within modulation palette.
  Reference: real Uni-Vibe has a distinctive round shape.
- Color: Modulation palette with slight warm shift (#22202D surface).

#### Time-Based Effects

**Delay** (index 12)
- Visual: 2U module. Two knobs + tap tempo.
- Layout: 2 knobs (Delay Time, Feedback) + tap tempo button.
- Special: Tap tempo button styled as a physical footswitch.
  Delay time displayed prominently in ms AND BPM.
  Rhythmic delay time indicator (visual pulse at tempo).
- Color: Time-based palette (teal/cyan).

**Reverb** (index 13)
- Visual: 2U-3U module. Four knobs.
- Layout: 4 knobs in 2x2 grid (Decay, Damping, Pre-Delay, Size).
- Special: Decay tail visualization (small animated waveform showing reverb envelope).
- Color: Time-based palette.

#### Utility

**Tuner** (index 14)
- Visual: Special module. Full-width tuner display (existing TunerDisplay).
- Layout: No knobs. LED meter + note display + frequency readout.
- Special: Already well-designed (Boss TU-3 style). Retain and enhance with
  category-consistent coloring.
- Color: Utility palette (slate blue-gray).

### 2.5 Knob Specifications

**Standard Knob** (56dp diameter)
Used for most parameters. Canvas-drawn with:
- Outer shadow ring: 2dp, color KNOB_OUTER_RING
- Knob body: 48dp circle, radial gradient from KNOB_HIGHLIGHT (top-left) to KNOB_BODY (bottom-right)
- Pointer line: 2dp wide, 16dp long from center toward edge, color KNOB_POINTER
- Rotation range: 30 degrees to 330 degrees (270-degree sweep, 7 o'clock to 5 o'clock)
- Value arc: Thin 2dp arc behind knob from minimum to current value, in category accent color
- Value readout: Centered below knob, 11sp monospace, TEXT_VALUE color

**Large Knob** (68dp diameter)
Used for amp model and key featured controls. Same style, proportionally larger.

**Small Knob** (44dp diameter)
Used for secondary parameters (EQ Q factor, knee width) in tight layouts.

**Knob Interaction**
- Vertical drag: dragging up increases value, down decreases
- Sensitivity: Full value range maps to 200dp of vertical drag
- Fine mode: Hold with second finger to reduce sensitivity to 25% (800dp for full range)
- Haptic feedback: Light tick at min, max, center (where applicable), and detents
- Value display: Shows numeric value while dragging, fades after 1.5s release

### 2.6 LED Indicator Specifications

**Active LED** (10dp diameter)
- Base circle: 10dp, filled with LED Active color (per category)
- Glow effect: 20dp circle behind, same color at 20% alpha, Gaussian blur 4dp
- When bypassed: Filled with LED Bypass color (dim), no glow

**Stomp Switch** (48dp diameter)
- Outer ring: 48dp circle, #3A3A40 (metallic dark gray)
- Inner surface: 36dp circle, subtle radial gradient (#444448 center, #38383C edge)
- Chrome highlight: 2dp crescent at top (#5A5A60)
- Press animation: Scale to 0.93 over 50ms, return over 100ms
- Active state: Subtle glow ring matching category LED color at 15% alpha

---

## 3. Screen Architecture for Mobile

### 3.1 Navigation Structure

```
[Main Screen: Pedalboard]
    |
    |-- [Bottom Sheet: Effect Editor]      (tap any module)
    |-- [Full Screen: Preset Browser]      (tap preset bar)
    |-- [Full Screen: Settings]            (gear icon)
    |-- [Overlay: Tuner]                   (tuner button, semi-transparent overlay)
```

### 3.2 Main Screen: Pedalboard (Signal Chain View)

The main screen uses a vertical scrolling rack layout. This is the most significant
departure from the current horizontal LazyRow of cards.

**Layout (top to bottom):**

```
+------------------------------------------+
| [PRESET BAR]                    [Gear]   |  Fixed top bar
|  "Clean Sparkle"          [Tuner] [A/B]  |
+------------------------------------------+
| [INPUT] ============================ [LED]|  Input indicator strip
+------------------------------------------+
|                                          |
| +--------------------------------------+ |
| | [LED] Noise Gate              [BYP]  | |  Collapsed module (1U)
| +--------------------------------------+ |
|                                          |
| +--------------------------------------+ |
| | [LED] Compressor              [BYP]  | |  Collapsed module
| +--------------------------------------+ |
|                                          |
| +--------------------------------------+ |
| | [LED] Overdrive               [BYP]  | |
| |                                      | |
| |  (Drive)    (Tone)     (Level)       | |  Expanded module (2U)
| |   0.30       0.50       0.70         | |
| |                                      | |
| |  Wet/Dry: ====O==================== | |
| +--------------------------------------+ |
|                                          |
| +--------------------------------------+ |
| | [LED] Amp Model               [BYP]  | |  3U module
| | ...                                  | |
| +--------------------------------------+ |
|                                          |
| ... (scrollable, more modules) ...       |
|                                          |
+------------------------------------------+
| [IN Meter] [Spectrum] [OUT Meter]        |  Fixed bottom bar
| -12 dBFS   ||||||||   -18 dBFS          |
+------------------------------------------+
```

**Key Design Decisions:**

1. **Vertical rack replaces horizontal scroll**: The horizontal LazyRow of small cards
   is the biggest usability problem. Effects are too small (100dp wide) to show
   meaningful information. A vertical list with full-width modules provides space for
   knobs, labels, and visual identity.

2. **Collapsed/expanded states**: By default, all modules are collapsed (showing only
   name + LED + bypass button, ~48dp height). Tapping a module expands it to show
   parameters inline. This keeps the chain overview compact while allowing editing
   without leaving the screen.

3. **Fixed top bar**: Preset name, tuner shortcut, A/B toggle, and settings gear
   are always accessible.

4. **Fixed bottom bar**: Level meters and spectrum analyzer are always visible,
   docked at the bottom. They collapse to a compact 48dp strip and expand on tap.

5. **Drag-and-drop reordering**: Long-press a module to initiate drag. Other modules
   slide out of the way with smooth animation. Drop indicator line shows insertion point.

**Collapsed Module Layout (48dp height):**
```
+--[LED 10dp]--[Effect Name 14sp bold]--------[Category dot]--[BYP switch 36dp]--+
```

**Expanded Module Layout (varies by effect):**
```
+--[Category Color Bar 3dp]------------------------------------------------------+
|                                                                                  |
|  [LED] Effect Name                                        [BYP]                  |
|         "Active" / "Bypassed"                                                   |
|                                                                                  |
|  +----------+  +----------+  +----------+                                       |
|  |  (Knob)  |  |  (Knob)  |  |  (Knob)  |                                       |
|  |  Drive   |  |  Tone    |  |  Level   |                                       |
|  |  0.30    |  |  0.50    |  |  0.70    |                                       |
|  +----------+  +----------+  +----------+                                       |
|                                                                                  |
|  Wet/Dry  [=======O=========================]  0.85                             |
|                                                                                  |
+---------------------------------------------------------------------------------+
```

### 3.3 Effect Editor (Bottom Sheet)

The bottom sheet is for FOCUSED editing of a single effect. It opens when the user
taps the expanded module's edit area (or double-taps a collapsed module).

The bottom sheet provides:
- Larger knobs (68dp) for precision
- Full parameter list with labels and units
- Special sections (IR loader, neural model, tap tempo)
- The same visual style as the inline module, but scaled up

This is retained from the current architecture but visually enhanced with
knob controls replacing Material3 Sliders.

**Bottom Sheet Layout:**
```
+-- Drag Handle --+
|                                                    |
|  [LED]  OVERDRIVE                    [ON/OFF]     |
|         "Active"                                   |
|                                                    |
|  +---+  +---------+  +---------+  +---------+    |
|  |CAT|  |         |  |         |  |         |    |
|  |bar|  | (DRIVE) |  | (TONE)  |  | (LEVEL) |    |
|  | 3 |  |  0.30   |  |  0.50   |  |  0.70   |    |
|  |dp |  +---------+  +---------+  +---------+    |
|  +---+                                            |
|                                                    |
|  Wet/Dry Mix                             0.85     |
|  [============================O===========]       |
|                                                    |
+----------------------------------------------------+
```

### 3.4 Preset Browser (Full-Screen Overlay)

Replace the current DropdownMenu with a full-screen overlay. The dropdown approach
cannot accommodate the number of presets a serious user accumulates, and it lacks
visual richness.

**Preset Browser Layout:**
```
+------------------------------------------+
| [X Close]    PRESETS        [+ New]      |
+------------------------------------------+
| [Search: ___________________________]    |
+------------------------------------------+
| [All] [Clean] [Crunch] [Heavy] [...]     |  Category filter chips
+------------------------------------------+
|                                          |
| FACTORY                                  |
| +--------------------------------------+ |
| | Clean Sparkle                 Clean  | |
| |   Factory  |  Jan 2024              | |
| +--------------------------------------+ |
| | Crunch Machine               Crunch  | |
| |   Factory  |  Jan 2024              | |
| +--------------------------------------+ |
| ...                                      |
|                                          |
| USER                                     |
| +--------------------------------------+ |
| | My Lead Tone                 Heavy   | |
| |   User  |  Mar 2026    [*] [...]    | |
| +--------------------------------------+ |
|                                          |
+------------------------------------------+
| [Import]                    [Export]      |
+------------------------------------------+
```

Each preset card shows:
- Name (16sp, bold, white)
- Category badge (colored dot + text)
- Author and date (12sp, gray)
- Favorite star icon
- Context menu (rename, delete, overwrite)
- Active preset highlighted with category color left border

### 3.5 Tuner (Overlay Mode)

The tuner should be accessible as a quick overlay on top of the pedalboard.
Tap the tuner button in the top bar to toggle it. It appears as a semi-transparent
overlay covering the top 60% of the screen, with the rack visible beneath.

This is better than scrolling to find the tuner module or opening a separate screen.
Musicians need instant tuner access.

The existing TunerDisplay component (Boss TU-3 style) is well-designed and would
be used directly in this overlay, with the utility category color palette applied.

### 3.6 Master Controls

The master controls (input gain, output gain, bypass) move from a separate panel
to integrated positions:

- **Global bypass**: Large toggle in the top bar (always visible)
- **Input/Output gain**: Accessible via the fixed bottom metering strip
  (tap to expand into gain controls)

This ensures critical controls are never more than one tap away.

---

## 4. Compose Techniques Required

### 4.1 Custom Canvas-Drawn Rotary Knob

This is the single most important custom component. Every effect parameter
except toggles and segmented controls will use this.

**Implementation approach:**

```kotlin
@Composable
fun RotaryKnob(
    value: Float,              // Current value (normalized 0..1)
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    label: String = "",
    displayValue: String = "",  // Formatted display (e.g., "0.50", "300 ms")
    accentColor: Color = Color(0xFFE8724A),  // Category accent
    size: KnobSize = KnobSize.STANDARD,      // SMALL(44dp), STANDARD(56dp), LARGE(68dp)
    enabled: Boolean = true
)
```

**Canvas drawing steps (inside drawScope):**

1. **Value arc**: drawArc from 150 degrees, sweep proportional to value * 270 degrees,
   stroke 2dp, accentColor at 40% alpha behind the knob.

2. **Outer shadow ring**: drawCircle, radius = knobRadius + 4dp, color = KNOB_OUTER_RING.

3. **Knob body**: drawCircle with RadialGradient brush:
   - Center offset top-left by 20%
   - Colors: [KNOB_HIGHLIGHT, KNOB_BODY]
   - This creates the 3D metallic illusion.

4. **Pointer line**: Calculate angle from value (30 to 330 degrees).
   drawLine from (center + 8dp at angle) to (center + (radius-4dp) at angle),
   color = KNOB_POINTER, strokeWidth = 2dp, StrokeCap.Round.

5. **Scale markings** (optional): Small tick marks at 0%, 25%, 50%, 75%, 100%
   positions around the outer ring. 1dp lines, 4dp long, TEXT_MUTED color.

**Gesture handling:**

```kotlin
Modifier.pointerInput(Unit) {
    detectVerticalDragGestures(
        onDragStart = { /* show value label, haptic tick */ },
        onVerticalDrag = { change, dragAmount ->
            change.consume()
            // Invert: drag UP = increase (negative dragAmount = increase)
            val sensitivity = size.dpSize.toPx() * 3.5f  // Full range = 3.5x knob height drag
            val delta = -dragAmount / sensitivity
            val newValue = (value + delta).coerceIn(0f, 1f)
            onValueChange(newValue)
        },
        onDragEnd = { /* hide value label after delay */ }
    )
}
```

**Performance notes:**
- Use `remember` for Paint objects and Path objects
- Use `graphicsLayer` for the value display fade animation (avoids recomposition)
- The knob itself recomposes only when `value` changes (which is expected)
- Do NOT use Modifier.rotate() for the pointer -- compute the angle in drawScope

### 4.2 LED Indicator with Glow Effect

```kotlin
@Composable
fun LedIndicator(
    active: Boolean,
    activeColor: Color,
    inactiveColor: Color,
    modifier: Modifier = Modifier,
    size: Dp = 10.dp
) {
    Canvas(modifier = modifier.size(size * 2)) {
        val center = Offset(this.size.width / 2, this.size.height / 2)
        val ledRadius = size.toPx() / 2

        if (active) {
            // Glow: larger circle with low alpha
            drawCircle(
                color = activeColor.copy(alpha = 0.25f),
                radius = ledRadius * 2f,
                center = center
            )
            // LED body
            drawCircle(
                brush = RadialGradient(
                    colors = listOf(
                        activeColor.copy(alpha = 1f),
                        activeColor.copy(alpha = 0.8f)
                    ),
                    center = center,
                    radius = ledRadius
                ),
                radius = ledRadius,
                center = center
            )
            // Hot spot (bright center)
            drawCircle(
                color = Color.White.copy(alpha = 0.4f),
                radius = ledRadius * 0.3f,
                center = center.copy(y = center.y - ledRadius * 0.15f)
            )
        } else {
            // Dim LED
            drawCircle(
                color = inactiveColor,
                radius = ledRadius,
                center = center
            )
        }
    }
}
```

### 4.3 Module Surface with Category Tint

Each rack module needs a surface with:
- Category color tint on the background
- Thin accent bar at top
- Subtle top-edge highlight
- Drop shadow below

```kotlin
@Composable
fun RackModule(
    effectInfo: EffectInfo,
    category: EffectCategory,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit  // knobs, controls when expanded
) {
    val surfaceTint = category.surfaceTint
    val headerBar = category.headerBar

    Column(
        modifier = modifier
            .fillMaxWidth()
            .shadow(elevation = 2.dp, shape = RoundedCornerShape(6.dp))
            .clip(RoundedCornerShape(6.dp))
            .drawBehind {
                // Top accent bar (3dp)
                drawRect(
                    color = headerBar,
                    topLeft = Offset.Zero,
                    size = Size(size.width, 3.dp.toPx())
                )
                // Surface with gradient (lighter at top edge for depth)
                drawRect(
                    brush = Brush.verticalGradient(
                        colors = listOf(
                            surfaceTint.copy(alpha = 1f).compositeOver(Color(0xFF2A2A30)),
                            surfaceTint.copy(alpha = 0.7f).compositeOver(Color(0xFF222228))
                        )
                    ),
                    topLeft = Offset(0f, 3.dp.toPx()),
                    size = Size(size.width, size.height - 3.dp.toPx())
                )
            }
            .clickable { onToggleExpand() }
    ) {
        // Module header (always visible)
        ModuleHeader(...)

        // Expandable content
        AnimatedVisibility(visible = expanded) {
            content()
        }
    }
}
```

### 4.4 Texture and Gradient Techniques

**Metallic surface gradients:**
Use `Brush.radialGradient` with off-center origin for metallic knob surfaces.
Use `Brush.verticalGradient` for module surfaces (lighter at top = top-lit).

**Subtle noise texture (optional, Phase 3):**
To add realism to module surfaces, overlay a semi-transparent noise texture.
Generate a small (64x64) noise bitmap at startup, tile it as a shader:

```kotlin
val noiseBitmap = remember {
    ImageBitmap(64, 64).also { bmp ->
        val pixels = IntArray(64 * 64) {
            val noise = Random.nextInt(0, 12)
            android.graphics.Color.argb(noise, 128, 128, 128)
        }
        bmp.readPixels(pixels)
    }
}
```

This is a Phase 3 enhancement. The gradient-only approach is sufficient for Phase 1-2.

**Tolex texture for Amp Model:**
Use a very subtle vertical stripe pattern rendered in Canvas:
```kotlin
for (x in 0..canvasWidth.toInt() step 3) {
    drawLine(
        color = Color.White.copy(alpha = 0.02f),
        start = Offset(x.toFloat(), 0f),
        end = Offset(x.toFloat(), canvasHeight),
        strokeWidth = 1f
    )
}
```

### 4.5 Animations

**Knob rotation**: Not animated (follows finger immediately). Value arc updates
are animated with `animateFloatAsState(tween(50ms))` for smooth visual tracking.

**LED glow**: `animateFloatAsState` on the glow alpha. Transition from 0 to 0.25
over 100ms on enable, 200ms fade on disable.

**Module expand/collapse**: `AnimatedVisibility` with `expandVertically` +
`fadeIn`/`fadeOut`. Duration 200ms with `FastOutSlowInEasing`.

**Bypass switch**: `animateColorAsState` on the switch track color. 150ms transition.

**Stomp press**: `animateFloatAsState(spring(stiffness=800f))` on scale
(1.0 to 0.93 on press). Already implemented in current PedalCard.

**Drag reorder**: `animateItemPlacement()` on LazyColumn items.

### 4.6 Touch Gesture Patterns

**Knob control**: Vertical drag (described in 4.1). No rotation gesture --
rotation gestures on small mobile targets are imprecise and frustrating.

**Module tap**: Single tap toggles expanded/collapsed state.

**Module long press**: Initiates drag-and-drop reorder.

**Stomp switch tap**: Toggles effect bypass.

**Swipe to bypass**: Optional future enhancement -- swipe left on a collapsed
module to toggle bypass (like iOS swipe-to-delete pattern).

**Double-tap knob**: Reset to default value (with confirmation haptic).

### 4.7 Performance Considerations

**Recomposition scope isolation:**
- Knob values change frequently during drag. Each knob must be its own composable
  with `value` parameter so only the changed knob recomposes.
- Use `graphicsLayer` for scale/alpha animations (bypasses recomposition).
- Level meters and spectrum analyzer already use Canvas (good).

**LazyColumn for the rack:**
- Use `LazyColumn` with `key = { effectState.index }` for the module list.
- `animateItemPlacement()` for smooth drag-and-drop.
- Only expanded modules draw knobs (collapsed modules are cheap).

**Canvas drawing cost:**
- A single knob Canvas draw is ~15 draw calls (arc, circles, line, ticks).
  At 56dp this is trivial.
- With 3 knobs visible in an expanded module, that's ~45 draw calls. Fine.
- The spectrum analyzer (64 bars + 3 lines = 67 draw calls) is the heaviest
  but already works at acceptable performance.

**Memory:**
- No bitmap textures needed for Phase 1-2. Everything is vector/Canvas drawn.
- The noise texture (Phase 3) is a single 64x64 bitmap (16KB). Negligible.

---

## 5. Phased Implementation Plan

### Phase 1: Foundation (Maximum Visual Impact, Minimum Risk)
**Estimated effort: 2 sprints**

**Goals:** Establish the new visual language. Replace the most visible, lowest-risk
components first. Everything in this phase is additive -- no architectural changes.

**Deliverables:**

1. **DesignSystem.kt** - Central color/dimension constants
   - All hex colors from Section 2.3 as `object DesignSystem`
   - `EffectCategory` enum with per-category color sets
   - `getCategory(effectIndex: Int): EffectCategory` mapping function
   - Dimension constants for knob sizes, module heights, spacing
   - File: `ui/theme/DesignSystem.kt`

2. **RotaryKnob.kt** - Custom Canvas knob component
   - Implements the knob drawing from Section 4.1
   - Vertical drag gesture with sensitivity control
   - Value display (numeric readout below knob)
   - Three sizes: SMALL, STANDARD, LARGE
   - File: `ui/components/RotaryKnob.kt`

3. **LedIndicator.kt** - Glowing LED component
   - Canvas-drawn LED with glow effect (Section 4.2)
   - Active/inactive states with category-appropriate colors
   - File: `ui/components/LedIndicator.kt`

4. **Enhanced PedalCard.kt** - Category-colored cards
   - Apply category surface tint to existing PedalCard backgrounds
   - Replace flat LED circle with LedIndicator
   - Add thin category-colored accent bar at top of card
   - Keep existing layout structure (still horizontal LazyRow for now)
   - This gives immediate visual impact with minimal risk

5. **Enhanced EffectEditorSheet** - Knobs replace sliders
   - Replace Material3 Slider with RotaryKnob for continuous parameters
   - Keep Slider for enum parameters (segmented feel better as slider with steps)
   - Apply category colors to the sheet header
   - Keep ModalBottomSheet architecture

6. **Theme.kt update** - Apply new background colors
   - Update background from #121212 to #141416
   - Disable dynamic color (Material You) -- it fights our custom palette
   - Set surface colors to match new design system

**Why this order:** The RotaryKnob and category colors are the two changes that
will most dramatically transform the app's visual character. A user opening the
effect editor and seeing real-looking knobs with warm/cool category coloring will
immediately feel the "Guitar Rig quality" difference. And all of this can be done
without changing the screen architecture.

### Phase 2: Rack Layout (Architecture Change)
**Estimated effort: 2-3 sprints**

**Goals:** Replace the horizontal card scroll with the vertical rack layout.
This is the biggest architectural change and touches PedalboardScreen heavily.

**Deliverables:**

1. **RackModule.kt** - Full-width rack module component
   - Collapsed state: 48dp height, name + LED + bypass
   - Expanded state: variable height, inline knobs + wet/dry
   - Category-tinted surface with accent bar
   - Expand/collapse animation
   - File: `ui/components/RackModule.kt`

2. **PedalboardScreen.kt rewrite** - Vertical rack layout
   - Replace `LazyRow` of `PedalCard` with `LazyColumn` of `RackModule`
   - Fixed top bar with preset name, tuner button, A/B, settings
   - Fixed bottom bar with compact meters
   - Drag-and-drop reordering via long-press (reuse reorderable library)
   - Per-module expand/collapse state management

3. **Compact metering strip** - Bottom-docked meters
   - Horizontal layout: input meter + mini spectrum + output meter
   - Collapsed: 48dp showing bar meters + dBFS values
   - Expanded: Full meters + spectrum (existing components)

4. **Inline module editing** - Edit without bottom sheet
   - When a module is expanded, knobs are interactive inline
   - Bottom sheet becomes optional "full editor" mode for complex effects
   - This reduces the number of screen transitions

5. **Preset browser upgrade** - Full-screen overlay
   - Replace DropdownMenu with full-screen overlay/dialog
   - Better scrolling, larger touch targets, visual richness
   - Category filter chips, search, favorite stars (retain existing logic)

### Phase 3: Polish and Enhancement
**Estimated effort: 1-2 sprints**

**Goals:** Visual refinements that add depth and professionalism.

**Deliverables:**

1. **Module-specific layouts**
   - Amp Model: larger knobs, tolex texture, neural model section
   - Parametric EQ: frequency response curve visualization
   - Delay: tap tempo footswitch styling, BPM display
   - Tuner: overlay mode (quick access from top bar)
   - Compressor: gain reduction meter
   - Noise Gate: gate open/closed indicator

2. **Surface textures**
   - Subtle noise overlay on module surfaces
   - Tolex pattern for amp module
   - Speaker grille hint for cabinet module

3. **Micro-animations**
   - LED glow pulse when signal passes through an active effect
   - Knob value arc smooth animation
   - Module expand/collapse spring physics
   - Stomp switch 3D press depth effect

4. **Tuner overlay mode**
   - Semi-transparent overlay triggered from top bar
   - Large, readable from distance
   - Auto-mutes output while active (optional)

5. **Accessibility and polish**
   - Content descriptions for all custom-drawn components
   - TalkBack support for knob values
   - High-contrast mode variant
   - Landscape orientation support for rack view

### Implementation Priority Matrix

```
                    HIGH IMPACT
                        |
     Phase 1            |           Phase 3b
     RotaryKnob         |           EQ curve viz
     Category colors    |           Texture overlays
     LED glow           |           Tuner overlay
                        |
  LOW EFFORT -----------+----------- HIGH EFFORT
                        |
     Phase 1b           |           Phase 2
     Theme update       |           Rack layout
     PedalCard colors   |           Preset browser
                        |           Inline editing
                    LOW IMPACT
```

Phase 1 occupies the ideal quadrant: high impact, lower effort.
Phase 2 is high effort but necessary for the full vision.
Phase 3 adds polish that makes the difference between "good" and "professional."

---

## Appendix: Color System Reference

### Quick Reference Table

| Element | Current Color | New Color | Notes |
|---------|--------------|-----------|-------|
| Background | #121212 | #141416 | Slight blue undertone |
| Module surface | #2A2A2A | #222226 + category tint | Category-specific |
| LED active | #00E676 (all) | Per-category | 6 distinct LED colors |
| LED bypass | #5D2020 (all) | Per-category dim | Matches active color |
| Knob body | N/A (no knobs) | #3C3C42 | New component |
| Knob pointer | N/A | #F0F0F0 | White pointer line |
| Knob accent | N/A | Per-category | Value arc color |
| Slider thumb | #00E676 | Category accent | Only for wet/dry |
| Slider track | #00C853 / #333333 | Category accent / #2A2A30 | |
| Text primary | #FFFFFF | #E0E0E4 | Slightly softer |
| Text secondary | #CCCCCC / #888888 | #888890 | Standardized |
| Text values | #00E676 | #D0D0D8 | Neutral, not green |
| Clip indicator | #F44336 | #F04040 | Nearly identical |
| Meter green | #4CAF50 | #40A850 | Slightly shifted |
| Meter yellow | #FFC107 | #D8C030 | Less saturated |
| Accent bar | N/A | Per-category | New: top of each module |

### Effect-to-Category Mapping

```kotlin
fun getCategory(index: Int): EffectCategory = when (index) {
    0, 1, 3, 4, 16, 17, 18, 19 -> EffectCategory.GAIN
    5, 6                         -> EffectCategory.AMP
    7                            -> EffectCategory.TONE
    2, 8, 9, 10, 11, 15, 20     -> EffectCategory.MODULATION
    12, 13                       -> EffectCategory.TIME
    14                           -> EffectCategory.UTILITY
    else                         -> EffectCategory.UTILITY
}
```

### Boss Pedal Unique Colors (Override Category Header)

These specific pedals reference their real-world hardware colors:

| Pedal | Real Color | Header Override |
|-------|-----------|----------------|
| Boss DS-1 | Orange | #D06020 |
| Boss DS-2 | Deep Orange | #D07020 |
| Boss HM-2 | Burnt Orange-Red | #C04830 |
| ProCo RAT | Black | #484848 |

These pedals use the GAIN category palette for everything EXCEPT the header
accent bar, which uses their signature color.

---

## Sources and References

- [Guitar Rig 7 Pro - Native Instruments](https://www.native-instruments.com/en/products/komplete/guitar/guitar-rig-7-pro/)
- [Guitar Rig 7 Pro Review - AudioNewsRoom](https://audionewsroom.net/2024/09/guitar-rig-7-pro-review-new-amps-new-technology-better-ui.html)
- [Native Instruments Guitar Rig Pro 7 - Sound On Sound](https://www.soundonsound.com/reviews/native-instruments-guitar-rig-pro-7)
- [Native Instruments Guitar Rig 7 Pro - Guitar World](https://www.guitarworld.com/reviews/native-instruments-guitar-rig-7-pro)
- [compose-audio-controls - Jetpack Compose audio widgets](https://github.com/atsushieno/compose-audio-controls)
- [Skeuomorphism in Plugin Design - LANDR](https://blog.landr.com/skeuomorphism-plugins/)
