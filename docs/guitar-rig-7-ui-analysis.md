# Guitar Rig 7 UI Analysis & Mobile Adaptation Report

## Research Sources & Methodology

This report synthesizes information from the Native Instruments Guitar Rig 7 online manual, product pages, multiple professional reviews (Sound On Sound, MusicRadar, AudioNewsRoom, WaveInformer, PlugiNoise), community discussions, and comparative analysis of leading mobile guitar effect apps (BIAS FX 2 Mobile, AmpliTube iOS, ToneX, Deplike). Where exact pixel/color values are not publicly documented, specifications are inferred from visual analysis and industry conventions, cross-referenced with our existing `ui-overhaul-design.md`.

---

## 1. Global Layout Architecture

### 1.1 Guitar Rig 7 Desktop Layout

Guitar Rig 7 uses a **four-region layout**:

```
+-------------------------------------------------------------------+
|                          HEADER                                    |
| [Menu] [Browser] [Info] [Zoom] [Input Sel] [In Meter] [Gate]     |
| [Out Meter] [Limiter] [CPU] [Engine On/Off] [NI Logo]            |
+----------+---------------------------------+----------------------+
|          |                                 |                      |
| BROWSER  |            RACK                 |      SIDEBAR         |
| (left)   |          (center)               |      (right)         |
|          |                                 |                      |
| Presets  |  +---------------------------+  |  Signal Flow         |
| or       |  | Component 1 (expanded)    |  |  [Block] On/Off/Del |
| Comps    |  | Knobs, controls           |  |  [Block] On/Off/Del |
|          |  +---------------------------+  |  [Block] On/Off/Del |
| Search   |  +---------------------------+  |  ...                 |
| Filter   |  | Component 2 (collapsed)   |  |                      |
| Tiles    |  +---------------------------+  |  Info Pane           |
| Favs     |  | Component 3 (expanded)    |  |  (hover help)        |
|          |  | ...                        |  |                      |
|          |  +---------------------------+  |                      |
+----------+---------------------------------+----------------------+
```

**Header** (persistent top bar):
- Main Menu button (preferences, file operations)
- Browser toggle (show/hide left panel)
- Info toggle (show/hide right sidebar)
- View size controls (magnification 75%-200%, 9 steps)
- Input Selector (Left mono, Stereo, Right mono)
- Input Level meter (-20 to +20 dB range, peak indicator, red clipping dot)
- Noise Gate threshold controls (learn mode + manual)
- Output Level meter (-70 to +6 dB, limiter, clipping detection)
- CPU Load indicator (real-time processing usage)
- Audio Engine power toggle

**Browser** (left panel, hideable):
- Content selector toggle: "Presets" vs "Components"
- User Content filter toggle
- Search field (1-character minimum, matches titles + metadata + tags + author)
- Filter tags (collapsible groups, AND logic for multiple selections)
- Eight colored favorite tags (assigned via right-click)
- Results list with sorting (Curated, Abc, Zyx, Custom)
- Component Tiles (grid view with pin-to-top feature)
- Category Filter for components (single-level)
- Info pane (editable metadata: name, comments, author, vendor, tags)

**Rack** (center, main workspace):
- Toolbar: Preset name display (asterisk for unsaved), prev/next preset navigation, preset shuffle, save/save-as, show rack tools, undo/redo, clear rack, collapse/expand all
- Components stack vertically (top-to-bottom signal flow)
- Each component has: power on/off toggle, collapse/expand button, stereo L/R split toggle (some), expert panel access (some), content list (containers)
- Tools: Macros (8 or 16 mappable controls), Tapedeck Pre/Post, Tuner, Metronome, Preset Volume, Global FX

**Sidebar** (right panel, hideable):
- Signal Flow: visual block diagram of all components in processing order, with per-component on/off switches and delete buttons. Supports drag-and-drop reordering.
- Info Pane: context-sensitive help (hover over any control to see description + hints + manual links)
- Input Selector (duplicate of header for convenience)

### 1.2 Navigation Model

Guitar Rig 7 uses a **parallel panel** model, not tabs or drawers. All three regions (Browser, Rack, Sidebar) are simultaneously visible and independently toggleable. The workflow is:

1. **Browse** in the left panel (find a preset or component)
2. **Build** in the center rack (add, remove, reorder, configure)
3. **Navigate** via the right sidebar (quick signal flow overview, reorder, bypass)

Transitions between browsing and editing are seamless -- clicking a browser result loads it into the rack without leaving the view.

### 1.3 What This Means for Mobile

A phone screen cannot show three panels simultaneously. The adaptation requires:

- **Browser becomes a full-screen overlay** (triggered by a "+" button or long-press on empty rack space)
- **Rack is the primary view** (always visible, vertical scroll)
- **Sidebar signal flow is embedded in the rack** (the rack IS the signal flow on mobile)
- **Header compresses** to a single-line top bar with the most critical controls

---

## 2. Component Browser (CRITICAL)

### 2.1 Organization System

The browser operates in two modes, toggled by a segmented control at the top:

**Presets Mode:**
- Search field (searches titles, metadata, filter tags, author name)
- Filter tags organized in collapsible groups by type (Genre, Character, Use Case, Input Source)
- Eight colored favorite tags (Red, Orange, Yellow, Green, Cyan, Blue, Purple, Pink) -- assigned via right-click context menu, appear as colored dots next to preset names
- Results list (scrollable, sortable)
- Sorting: Curated (designer order), Alphabetical, Reverse Alpha, Custom (user drag-order, favorites only)

**Components Mode:**
- Search field (searches component titles, category names, associated preset names)
- Category Filter (single-level, multi-select via Ctrl/Cmd+click, OR logic between categories):
  - Amplifiers (27 components)
  - Cabinets (5)
  - Delay/Echo (7)
  - Distortion (22)
  - Dynamics (16)
  - EQ (5)
  - Filters (11)
  - Legacy (1)
  - Modulation (14)
  - Pitch (5)
  - Reverb (12)
  - Special FX (8)
  - Tools (Container, Crossover, Loop Machine Pro, Split Mix, Split M/S)
  - Modifiers (Analog Sequencer, Envelope, Input Level, LFO, Step Sequencer)
- Component Tiles: grid of selectable items, each with a pin icon for favorites
- "Show Component presets" option for selected component
- Total: ~139 components across 12+ categories

### 2.2 Component Tile Appearance

Based on the manual documentation and reviews, Component Tiles appear as:
- **Rectangular cards in a grid layout** (not a simple list)
- Each tile shows the component name
- A **pin icon** appears on hover (desktop) for marking favorites
- Pinned components float to the top of the grid
- No photorealistic thumbnails -- the tiles are clean, functional rectangles
- The visual is minimal: name text + category identification
- No individual component icons/emblems are described in the documentation (Guitar Rig 7 moved away from the skeuomorphic pedal icons of earlier versions)

### 2.3 Adding Components to the Rack

The documentation describes that components are added from the Component Tiles to the rack. Based on the desktop interaction model:
- **Double-click** a tile to add it at the end of the chain
- **Drag** a tile from the browser into the rack at a specific position
- Components can also be **replaced** by dragging a new tile onto an existing component

### 2.4 Mobile Adaptation: Component Browser

For our Android app, the component browser needs to be reimagined as a full-screen overlay or bottom sheet. Recommended design:

```
+------------------------------------------+
| [< Back]    ADD EFFECT          [Search] |
+------------------------------------------+
| [All] [Gain] [Amp] [Mod] [Time] [Util]  |  Scrollable category chips
+------------------------------------------+
|                                          |
| GAIN / DISTORTION                        |
| +--------+  +--------+  +--------+      |
| |  [LED]  |  |  [LED]  |  |  [LED]  |   |
| | Noise   |  | Comp-   |  | Boost  |    |  2-3 column grid
| | Gate    |  | ressor  |  |        |    |
| +--------+  +--------+  +--------+      |
| +--------+  +--------+  +--------+      |
| | Over-  |  | DS-1   |  | RAT    |      |
| | drive  |  |        |  |        |      |
| +--------+  +--------+  +--------+      |
|                                          |
| MODULATION                               |
| +--------+  +--------+  +--------+      |
| | Chorus |  | Phaser |  | Flanger|      |
| +--------+  +--------+  +--------+      |
| ...                                      |
+------------------------------------------+
```

Each tile:
- 100dp x 80dp minimum touch target
- Category-colored top accent bar (3dp)
- LED dot in category color (10dp, indicating if effect is already in chain)
- Effect name (13sp, semibold, center-aligned)
- Tap to add at end of chain (with undo snackbar)
- Long-press for insert-at-position behavior
- Background: category surface tint over ModuleSurface

Category chip bar:
- Horizontal scroll, 36dp height chips
- Category color on selected chip (filled), dark outline on unselected
- "All" chip uses TextPrimary color

Search:
- Expanding search field (icon collapses to full-width input on tap)
- Case-insensitive fuzzy match against effect names and category names
- Results highlighted with match emphasis

---

## 3. Signal Chain Visualization

### 3.1 Guitar Rig 7 Rack (Center Area)

The rack displays components in a **single vertical column, top-to-bottom signal flow**:

- Signal enters at the top, exits at the bottom
- Each component occupies the **full width** of the rack area
- Components have **two display states**:
  - **Expanded**: Full parameter controls visible (knobs, sliders, switches, meters)
  - **Collapsed**: Thin bar showing only component name + power toggle + expand button
- **Collapse/Expand All** button in toolbar
- The rack is scrollable when components exceed the visible area

### 3.2 Signal Flow Sidebar

The Signal Flow sidebar (right panel) provides a simplified view:

- Each component appears as a **labeled block** in vertical sequence
- Blocks are connected by implied flow (stacked vertically, no explicit connection lines)
- Each block has:
  - Component name
  - Power on/off switch
  - Delete (X) button
- **Drag-and-drop reordering**: Drag blocks up/down to change signal chain order
- Click a block to scroll the rack to that component and select it
- The sidebar shows the complete chain at a glance even when the rack is scrolled to a specific component

### 3.3 Parallel/Split Routing

Guitar Rig 7 supports advanced routing through Tool components:
- **Split Mix**: Divides signal into two parallel paths, each with its own effects, then recombines
- **Split M/S**: Mid/Side split for stereo processing
- **Crossover**: Frequency-based split (low/high bands processed separately)
- **Container**: Groups multiple effects into a single collapsible unit

These are shown in the sidebar as branching structures, but the rack itself remains a single column (splits are contained within their respective tool modules).

### 3.4 How Effects Connect

Guitar Rig does NOT show explicit wires, cables, or connection lines between components. The connection is implied by vertical stacking order. The signal flow is understood through:
- Top-to-bottom ordering
- The "Signal Path" section in the sidebar labels it explicitly
- Input and output labels at top and bottom of the rack

### 3.5 Reordering

- **In the Rack**: Drag a component's header to move it up or down
- **In the Sidebar**: Drag blocks to reorder
- Both methods update the actual processing order

### 3.6 Removing Effects

- **In the Rack**: Right-click component header > Delete, or use a header delete button
- **In the Sidebar**: Click the delete (X) button on the component block

### 3.7 Mobile Adaptation: Signal Chain

Our vertical rack design (from `ui-overhaul-design.md`) maps almost perfectly to Guitar Rig 7's rack concept. Key adaptations:

**Full-Width Vertical Modules** (replace current horizontal LazyRow):
- Each effect occupies the full screen width
- Collapsed: 52dp height (name + LED + bypass toggle + category accent bar)
- Expanded: Variable height (knobs + controls inline)
- Signal flows top-to-bottom (matches Guitar Rig exactly)
- No explicit connection lines needed (vertical stacking implies flow)

**Reordering**:
- Long-press to initiate drag (already implemented with reorderable library)
- Adapt from horizontal to vertical drag (LazyColumn instead of LazyRow)
- Drop indicator: horizontal line between modules, category-colored, 2dp height

**The sidebar is unnecessary on mobile** because the rack IS the signal flow. The mobile rack serves the same function as both the desktop rack and sidebar combined.

---

## 4. Effect/Component Editing

### 4.1 Guitar Rig 7 Component Panels

When a component is expanded in the rack, it reveals its full parameter interface:

**Visual Style (GR7 "Clean Modern with 3D Touches"):**
- Clean, modern controls with subtle 3D depth
- NOT photorealistic (no scratched metal, no beer stains, no tolex textures)
- Every component (even stomp pedals) presented as a standard rack module
- Consistent module dimensions and layout conventions
- "Eye-pleasing touch of 3D" -- not flat, not skeuomorphic, a refined middle ground

**Control Types:**
- **Rotary knobs**: Circular, with visible pointer line, subtle radial gradient for metallic look, drop shadow. Position indicated by pointer angle. Macro controls use knobs extensively.
- **Sliders**: Used for EQ bands and linear parameters
- **Switches/toggles**: For on/off states and two-position mode selection, subtle 3D emboss
- **Segmented controls**: For multi-position selectors (channel selection, etc.)

**Component Header Controls:**
- Power toggle (on/off bypass)
- Collapse/Expand button
- Stereo L/R link/split (some components)
- Expert panel toggle (reveals advanced parameters)
- Component preset selector

**Interaction**: Desktop uses mouse drag (vertical movement changes value for knobs). No rotation gesture required.

### 4.2 Category-Based Visual Differentiation

Guitar Rig 7 does NOT use dramatically different visual designs per effect. Instead, differentiation comes through:
- **Header accent color**: A thin colored strip or tinted header identifies the category
- **Module layout variations**: Amp modules are larger with more knobs. Simple effects are compact. Utility modules are minimal.
- **Knob count and arrangement**: Visual complexity communicates effect complexity
- All effects share the same module chrome, control style, and interaction patterns

### 4.3 Editing is Inline (Not a Separate Panel)

In Guitar Rig 7, editing happens directly in the rack. There is no separate "editor panel" -- you expand the component and tweak its controls right there. This is significant for our mobile design because:

- Guitar Rig validates our "expand module inline" approach
- The bottom sheet should be a secondary, optional "detailed editor" for complex effects
- Most parameter adjustment should happen inline in the vertical rack

### 4.4 Mobile Adaptation: Effect Editing

**Dual-mode editing:**

1. **Inline editing (primary)**: Tap a collapsed module to expand it. Knobs appear directly in the rack module, full width. Most effects have 2-4 knobs that fit comfortably at 56dp each in a 2-column layout.

2. **Bottom sheet editor (secondary)**: Double-tap or tap an "edit" icon on expanded module for the full editor sheet. This provides:
   - Larger knobs (68dp)
   - Complete parameter list with units
   - Effect-specific features (IR loader, neural model selector, tap tempo)
   - More breathing room for complex effects (7-parameter EQ)

This mirrors Guitar Rig's approach (expand inline) while adapting to mobile constraints (bottom sheet for precision editing).

**Knob specifications** (from `ui-overhaul-design.md`, validated against GR7 aesthetic):
- Canvas-drawn with radial gradient (metallic look)
- Pointer line (not dot) for position indication
- Value arc behind knob showing current setting
- Vertical drag gesture (up = increase, down = decrease)
- 56dp standard size, 44dp minimum, 68dp for featured controls
- Numeric value displayed below knob (monospace, 11sp)

---

## 5. Preset System

### 5.1 Guitar Rig 7 Preset Browser

**What a Preset Stores:**
A Guitar Rig preset recalls the complete rack: all components, their parameter settings, and toolbar options (macro mappings, etc.).

**Browser Layout:**
- Search field at top
- Filter tags below search, organized by type (collapsible groups)
- Eight colored favorite tags (dots next to preset names)
- Results list (scrollable)
- Info pane (metadata: name, comments, author, vendor, custom tags)

**Filter Tag Types:**
- Genre (Pop, Rock, Metal, Blues, Jazz, Country, Electronic, etc.)
- Character (Clean, Colored, Spacious, Evolving, Creative)
- Input Source (Guitar, Bass, Drums, Keys, Vocals, Mixbus)
- Use Case (various)

**Preset Switching:**
- Double-click in results list, or select and press Enter
- Previous/Next preset buttons in toolbar (sequential through filtered results)
- Preset Shuffle button (random from filtered list)
- Asterisk (*) next to name when modified

**Saving:**
- "Save" updates existing user preset (grayed out if factory)
- "Save New" creates a new user preset with USER label
- Metadata editable in Info pane

**Component-Level Presets:**
Individual components also have their own preset systems, accessible through component headers. This allows saving/recalling settings for a single effect independently of the full rack preset.

### 5.2 Mobile Adaptation: Preset System

Our current PresetSelector (dropdown) should evolve to a **full-screen preset browser** in Phase 2:

```
+------------------------------------------+
| [X]          PRESETS             [+ New]  |
+------------------------------------------+
| [Search: ________________________________]|
+------------------------------------------+
| [All] [Clean] [Crunch] [Heavy] [Ambient] |
| [Acoustic] [User] [Favorites]            |
+------------------------------------------+
|                                          |
| * Clean Sparkle                   Clean  |   * = currently loaded
|   Factory | Modified                     |
|                                          |
|   Crunch Machine                 Crunch  |
|   Factory                                |
|                                          |
|   My Lead Tone                   Heavy   |
|   User | Mar 2026         [star] [...]   |
|                                          |
+------------------------------------------+
| [Import]                      [Export]   |
+------------------------------------------+
```

Additions over Guitar Rig's approach for mobile:
- **Larger touch targets** (56dp minimum row height for preset items)
- **Swipe actions** (swipe left to delete user presets, with confirmation)
- **Active preset indicator**: Left border in category color + bold name
- **Fewer filter groups**: Simplify to one level of category chips (not multiple collapsible filter groups -- too complex for mobile)
- **Retain colored favorites** from GR7 concept but simplify to star/unstar (not 8 colors -- too many for mobile context menus)

---

## 6. Metering & Monitoring

### 6.1 Guitar Rig 7 Metering

**Input Level Meter** (in header):
- Range: -20 dB to +20 dB
- Shows current peak input level
- Clipping indicated by a red dot that persists
- Adjustable input gain

**Output Level Meter** (in header):
- Range: -70 dB to +6 dB
- Shows peak output level
- Clipping indicated by a red dot
- Limiter function available
- Adjustable output level

**CPU Load Indicator** (in header):
- Real-time processing usage percentage

**Tuner** (toolbar tool):
- Accessible from the rack toolbar
- Standard chromatic tuner
- Always available (not buried in menus)

**Metronome** (toolbar tool):
- BPM display and control
- Available alongside tuner

Guitar Rig does NOT have a built-in spectrum analyzer as a prominent UI feature. The metering is functional and utilitarian, integrated into the header rather than being a showcase feature.

### 6.2 Mobile Adaptation: Metering

Our app already has more metering than Guitar Rig 7 (spectrum analyzer, dBFS meters, CLIP indicator, tuner). The key is positioning:

**Fixed Bottom Metering Strip** (always visible, 52dp collapsed):
```
+--[IN: -12dB]--[||||spectrum||||]--[OUT: -18dB]--+
```

- Input meter: Vertical bar (12dp wide) + dBFS value
- Mini spectrum: 48dp tall, same Canvas bars as current SpectrumAnalyzer but compressed
- Output meter: Vertical bar (12dp wide) + dBFS value
- CLIP indicator: Red dot that appears above the output meter
- Tap to expand: Full 120dp meters + full spectrum + gain controls

**Tuner**: Overlay mode (not in the signal chain). Tap tuner icon in top bar to overlay the existing TunerDisplay component on the upper portion of the screen.

---

## 7. Color System & Visual Identity

### 7.1 Guitar Rig 7 Color Palette

Guitar Rig 7 uses a **dark, muted industrial palette**:

**Background tones:**
- Not pure black -- warm dark gray range (#1C1C1C to #242424)
- Module surfaces slightly lighter (#2A2A2A to #343434)
- Subtle gradient on module surfaces (lighter top edge, darker bottom) suggesting top-lit depth

**Accent colors:**
- Muted and desaturated compared to Material Design
- Teal/cyan for active states (not bright green)
- Orange for gain/distortion indicators
- Blue for modulation
- No saturated primary colors on surfaces

**Text:**
- Off-white (#D8D8D8) for primary labels
- Medium gray (#808080) for secondary text
- Bright white (#FFFFFF) only for active values and selected items

**LED indicators:**
- Realistic rendering with glow effect (not flat circles)
- Green for active, red for clipping, amber for caution

**Control surfaces:**
- Knobs: Radial gradient (metallic sheen), visible pointer line, drop shadow
- No photorealistic textures
- "Clean 3D" -- subtle dimension without skeuomorphism

### 7.2 What Makes It Look "Professional"

Based on analysis of Guitar Rig 7 and comparison with amateur alternatives:

1. **Restraint in saturation**: Professional audio UIs use muted, desaturated accent colors. Amateur UIs blast bright green (#00FF00) and electric blue (#0088FF). GR7's accents are toned down: think #D4533B not #FF0000, #6B7EC8 not #0066FF.

2. **Consistent temperature**: All colors share a warm-neutral undertone. No jarring cold elements in an otherwise warm palette.

3. **Depth without excess**: The subtle 3D gradients on knobs and module surfaces add perceived quality without looking like a 2005 Photoshop tutorial. The depth is functional (distinguishes interactive elements from backgrounds).

4. **Whitespace management**: Even in a dark theme, the spacing between modules, padding within modules, and clearance around knobs is generous. Nothing is cramped.

5. **Typography hierarchy**: Clean sans-serif throughout. Effect names are medium weight, not overly bold. Parameter labels are lighter and smaller. Values are in a different shade than labels. Three distinct levels of text hierarchy max.

6. **Consistent control sizing**: Every knob in GR7 has the same visual weight for the same importance level. There are not random size variations within a single module.

### 7.3 Our Design System Validation

Our existing `DesignSystem.kt` already captures the GR7 aesthetic accurately:

| GR7 Characteristic | Our Implementation | Status |
|---|---|---|
| Dark warm gray background | #141416 (slight blue undertone) | GOOD -- close to GR7's #1C1C1C-#242424, slightly cooler |
| Module surfaces | #222226 + category tint | GOOD |
| Muted accent colors | 6-category palette with desaturated colors | GOOD |
| LED glow effect | LedIndicator with radial gradient + alpha glow | GOOD |
| Clean 3D knobs | RotaryKnob spec with radial gradient | DESIGNED (not yet implemented) |
| Category color coding | 6 categories with full color sets | GOOD |
| Off-white text | #E0E0E4 primary, #888890 secondary | GOOD |
| Per-pedal header overrides | DS-1 orange, RAT charcoal, etc. | GOOD |

### 7.4 Recommended Color Refinements

Based on deeper GR7 analysis, minor adjustments:

| Token | Current | Recommended | Rationale |
|---|---|---|---|
| Background | #141416 | #181818 | Slightly warmer, closer to GR7 |
| ModuleSurface | #222226 | #242426 | Marginal warmth increase |
| TextPrimary | #E0E0E4 | #D8D8DC | Slightly softer, matches GR7's off-white |
| SafeGreen | #40B858 | #3BA855 | Slightly less saturated |

These are optional refinements. The current palette is already well-aligned.

---

## 8. Mobile Competitor Analysis

### 8.1 BIAS FX 2 Mobile (Positive Grid) -- iOS only

**Layout:**
- **Routing View** (default): Shows complete signal path with icons representing each effect and amp. Effects appear as labeled nodes connected by signal flow lines.
- **Pedalboard View**: Top-down view of effects chain showing enlarged amp heads and pedal graphics. Used for quick parameter tweaks during performance (cannot add/remove effects in this view).
- **LiveView Mode**: Preset switching with single-tap, no latency. Large preset names, minimal controls visible.

**Signal Chain:**
- Horizontal signal flow (left-to-right) with icons for each component
- Supports **dual signal path** (splitter before amp, mixer after cab) for parallel processing
- Effects categorized: EQ, Modulation, Delay, Reverb, Distortion, Dynamics
- Add/remove by tapping "+" slots or "X" on existing effects
- Drag to reposition in chain

**Effect Editing:**
- Double-tap a pedal or amp to enter enlarged editing view
- Dedicated controls per effect type
- Model finder drop-down for switching between different pedals/amps within a slot

**Strengths:**
- Comprehensive signal chain (on par with desktop)
- Dual signal path support
- Huge effects library from desktop version
- ToneCloud preset sharing community

**Weaknesses:**
- "Hard to use and master" -- steep learning curve
- Vertical-ish arrangement "slightly confusing" per some reviews
- iOS only (no Android version at time of writing)
- UI scaling issues on larger iPads (elements sized for regular iPad, feel tiny on iPad Pro)

### 8.2 AmpliTube iOS (IK Multimedia) -- iOS/Android

**Layout:**
- Three main sections: **Rack** (signal chain overview), **Gear View** (photorealistic equipment rendering), **Gear View Selector** (component browser)
- Bottom-left signal chain shows complete rig with actual gear icons
- Clicking any component shows detailed gear view with realistic hardware representation

**Signal Chain:**
- Supports 6 stompboxes (4 pre-amp, 2 post) + amp + cabinet with 2 positionable mics
- Advanced: up to 57 simultaneous models, series/parallel routing, wet-dry-wet
- Drag-and-drop reorganization
- Color-coded by signal path in multi-amp setups

**Effect Editing:**
- Gear view shows photorealistic (skeuomorphic) representation of actual hardware
- Amps look like their real counterparts (Mesa, Marshall, Fender, etc.)
- Direct knob manipulation on realistic-looking panels

**Live Mode:**
- Full-screen optimized view for performance
- Shows entire chain with on/off indicators
- Activates with single tap
- Large, readable from stage distance

**VIR Technology** (cabinet innovation):
- 3D microphone positioning grid
- Simulates speaker/cabinet/floor interactions

**Strengths:**
- Most comprehensive gear library on mobile (100+ amps, cabinets, pedals)
- Photorealistic gear views provide strong visual identity
- Live Mode is well-designed for performance
- Android version available (AmpliTube UA)
- Drag-and-drop is intuitive

**Weaknesses:**
- Expensive: each gear piece sold individually (in-app purchases)
- No social preset sharing
- Can feel cluttered with so many gear models
- Skeuomorphic rendering requires significant resources

### 8.3 ToneX (IK Multimedia) -- iOS

**Layout:**
- Mirrors desktop ToneX layout
- Tone Model as central element (AI-captured amp/cab/pedal)
- Surrounding effects: compressor, pre/post-amp EQ, noise gate, reverb

**Signal Chain:**
- Simpler than AmpliTube: one Tone Model + pre/post effects
- 2024 update added 8 new pre/post effects (chorus, flanger, tremolo, phaser, rotary, spring reverb, 2 delays)
- Two effects each switchable before and after amp/cab

**Effect Editing:**
- Same controls as desktop version
- Built-in VIR cabinet technology
- Simple, focused parameter set

**Strengths:**
- Extremely high-quality tone modeling (AI-captured real amps)
- Simple, focused interface (not overwhelming)
- Community tone sharing (ToneNET)
- Consistent with desktop version

**Weaknesses:**
- iOS only (no Android as of research date)
- Limited effects compared to AmpliTube or BIAS FX
- Missing Librarian section for pedal management on mobile
- Users want more editing capabilities on mobile vs requiring desktop

### 8.4 Deplike (Deplike) -- Android/iOS

**Layout:**
- Simple, functional interface
- Pedalboard metaphor: virtual pedal arrangement
- "No eye candy for useless items" -- utilitarian approach

**Signal Chain:**
- 21 effects pedals + 15 amp/cabinet models
- Create personalized pedalboard with drag arrangement
- Physical pedalboard simulation

**Key Differentiator:**
- **Only Android guitar app with USB audio interface support** (custom USB driver)
- This solves the Android latency problem that affects competitors

**Strengths:**
- Available on Android (rare for quality guitar apps)
- Low latency via USB interface support
- 10,000+ community presets searchable by artist name
- Simple, not overwhelming
- Easy one-handed use

**Weaknesses:**
- Less sophisticated UI compared to BIAS FX or AmpliTube
- Fewer effects and amps
- Basic visual design (functional but not premium-feeling)
- No desktop companion software

### 8.5 Competitive Landscape Summary

| Feature | Guitar Rig 7 | BIAS FX 2 | AmpliTube | ToneX | Deplike | Our App |
|---|---|---|---|---|---|---|
| Platform | Desktop | iOS | iOS/Android | iOS | Android/iOS | Android |
| Effects Count | 139 | 100+ | 100+ | ~20 | 21 | 21 |
| Signal Flow | Vertical | Horizontal | Both | Linear | Flat | Horizontal (current) |
| Visual Style | Clean Modern 3D | Modern Flat | Skeuomorphic | Modern Flat | Basic Flat | Flat (current) |
| Dual Path | Yes | Yes | Yes | No | No | No |
| Preset Sharing | No (User Libraries) | ToneCloud | No | ToneNET | Community | No |
| USB Audio | N/A (desktop) | No | No | No | YES | No |
| Live Mode | N/A | Yes | Yes | No | No | No |
| Inline Editing | Yes (rack expand) | No (separate view) | Yes (gear view) | N/A | Yes | No (bottom sheet only) |
| Category Colors | Header accents | Minimal | Gear-realistic | Minimal | No | Yes (implemented) |

---

## 9. Comprehensive Mobile Adaptation Recommendations

### 9.1 Primary Screen: Vertical Rack (Priority 1)

This is the most impactful change. Replace the current horizontal LazyRow with a vertical LazyColumn rack.

**Specifications:**

```kotlin
// Collapsed module: 52dp height
Row(height = 52.dp, width = fillMaxWidth) {
    // 3dp category accent bar (left edge, vertical)
    Box(width = 3.dp, height = fillMaxHeight, color = categoryHeaderBar)
    // 12dp padding
    // LED indicator (10dp with 20dp glow canvas)
    LedIndicator(size = 10.dp, active, categoryLedActive, categoryLedBypass)
    // 8dp spacing
    // Effect name (14sp, semibold, TextPrimary)
    Text(effectName, fontSize = 14.sp, fontWeight = SemiBold)
    // Spacer (weight 1f)
    // Category dot (6dp, filled circle, category primary color)
    // 8dp spacing
    // Bypass toggle (36dp height switch)
    Switch(active, onToggle, categoryLedActive track color)
    // 8dp padding
}

// Expanded module: variable height (effect-specific)
Column(width = fillMaxWidth) {
    // 3dp category accent bar (top, horizontal)
    Box(height = 3.dp, width = fillMaxWidth, color = categoryHeaderBar)
    // Module header (same as collapsed, minus bottom edge)
    // 16dp vertical padding
    // Knob row(s): 2 per row on phone, 3 on tablet
    Row(horizontalArrangement = SpaceEvenly) {
        RotaryKnob(size = STANDARD(56.dp), value, onValueChange, accentColor = categoryKnobAccent)
        RotaryKnob(...)
    }
    // 12dp vertical padding
    // Wet/Dry slider (if applicable)
    Slider(wetDry, onWetDryChange, colors = categoryAccentSliderColors)
    // 8dp bottom padding
}
```

**Module background rendering:**
```kotlin
Modifier.drawBehind {
    // Vertical gradient: category tint composited over ModuleSurface
    drawRect(
        brush = Brush.verticalGradient(
            listOf(
                categoryTint.copy(alpha = 1f).compositeOver(Color(0xFF2A2A30)),
                categoryTint.copy(alpha = 0.7f).compositeOver(Color(0xFF222228))
            )
        )
    )
}
```

**Spacing between modules:** 6dp (Spacer in LazyColumn)

**Expand/collapse animation:**
```kotlin
AnimatedVisibility(
    visible = expanded,
    enter = expandVertically(tween(200, easing = FastOutSlowInEasing)) + fadeIn(tween(150)),
    exit = shrinkVertically(tween(200)) + fadeOut(tween(100))
)
```

### 9.2 Top Bar (Priority 1)

Fixed top bar, 56dp height:

```
+--[Preset Name (tap to browse)]--[Tuner icon]--[A/B]--[Gear]--+
```

Specifications:
- Background: ModuleSurface (#242426)
- Preset name: 16sp, SemiBold, TextPrimary, left-aligned, tappable (opens preset browser)
- Category dot before preset name (6dp, colored by current preset category)
- Tuner icon: 24dp, TextSecondary, tap to toggle tuner overlay
- A/B toggle: Custom composable, 36dp height, clear A/B state indication
- Gear icon: 24dp, TextSecondary, opens Settings
- Bottom edge: 1dp Divider color separator

### 9.3 Bottom Metering Strip (Priority 1)

Fixed bottom bar, 52dp collapsed / 160dp expanded:

**Collapsed:**
```
+--[IN bar 12dp + dBFS]--[Mini Spectrum 48dp tall]--[OUT bar 12dp + dBFS]--+
```

**Expanded (tap to toggle):**
```
+----------------------------------------------------------------------+
| INPUT                                           OUTPUT               |
| [======] -12 dBFS                              [======] -18 dBFS    |
|                                                                      |
| [|||||||||||||||||| Spectrum Analyzer ||||||||||||||||||||]           |
|                                                                      |
| Input Gain  [===========O=============] +2.4 dB                    |
| Output Gain [=======O=================] -1.2 dB                    |
| [Global Bypass toggle]                                               |
+----------------------------------------------------------------------+
```

- Collapsed: compact bar meters + mini spectrum (same Canvas, reduced bin count to 32)
- CLIP indicator: 6dp red dot above output meter, persistent 2.5s
- Tap collapsed strip to expand to full metering + gain controls
- Swipe down or tap again to collapse
- Background: ModuleSurface with 50% alpha overlay (semi-transparent to show rack behind)

### 9.4 Preset Browser (Priority 2)

Full-screen overlay (not a dropdown):

**Header:** Close button (X), title "PRESETS", New Preset button (+)
**Search:** Full-width text field, instant filter, 48dp height
**Category Chips:** Scrollable row, 36dp height, category-colored fill on selected
**Preset List:** LazyColumn, each item 64dp height minimum
**Item layout:**
```
Row(height = 64.dp) {
    // Active indicator: 3dp left bar in category color (or transparent)
    // Preset name: 16sp, SemiBold (Bold if active)
    // Category badge: 10sp, category color text
    // Favorite star: 20dp, amber when favorited
    // Context menu: 3-dot icon, 24dp
}
// Subtitle row:
Row {
    // Author: 11sp, TextSecondary
    // Date: 11sp, TextMuted
}
```

**Import/Export:** Bottom row, 48dp height, full-width buttons

### 9.5 Tuner Overlay (Priority 2)

Semi-transparent overlay triggered from top bar:
- Covers top 60% of screen
- Existing TunerDisplay component centered
- Dark scrim (#141416 at 80% alpha) behind tuner
- Tap outside tuner or tap tuner icon again to dismiss
- Optional: auto-mute output while tuner overlay is active

### 9.6 Custom Rotary Knob (Priority 1)

Implementation specifications (from `ui-overhaul-design.md`, validated against GR7):

```kotlin
@Composable
fun RotaryKnob(
    value: Float,                    // Normalized 0..1
    onValueChange: (Float) -> Unit,
    label: String = "",
    displayValue: String = "",       // e.g., "0.50", "300 ms", "6.5 dB"
    accentColor: Color,              // Category knob accent
    size: Dp = DesignSystem.KnobSizeStandard,  // 44/56/68dp
    enabled: Boolean = true,
    modifier: Modifier = Modifier
)
```

Canvas drawing order:
1. Value arc (2dp stroke, accentColor at 40% alpha, 270-degree sweep from 7-o'clock)
2. Outer shadow ring (knobRadius + 4dp, KnobOuterRing color)
3. Knob body (radial gradient: KnobHighlight center-offset-top-left to KnobBody)
4. Pointer line (2dp wide, KnobPointer color, 16dp long from center toward edge at angle)
5. Optional tick marks at 0/25/50/75/100% (1dp, 4dp long, TextMuted color)

Gesture: Vertical drag, sensitivity = 3.5x knob height for full range
Label: Below knob, 10sp, TextSecondary
Value: Below label, 11sp monospace, TextValue (visible during drag + 1.5s after release)

### 9.7 Phased Implementation (Updated)

**Phase 1 -- Visual Foundation (2 sprints)**
Priority: Maximum visual impact with minimum architecture change

1. `RotaryKnob.kt` -- Custom Canvas knob (replaces Material3 Slider in effect editor)
2. `LedIndicator.kt` -- Canvas LED with glow (already partially implemented)
3. `StompSwitch.kt` -- Enhanced metallic 3D stomp button (already partially implemented)
4. Update `EffectEditorSheet` to use RotaryKnob for continuous params
5. Update Theme.kt: disable dynamic colors, apply refined palette
6. Apply category colors to existing PedalCard headers (DONE per current code)

**Phase 2 -- Rack Layout (2-3 sprints)**
Priority: Architecture change to vertical rack

1. `RackModule.kt` -- Full-width collapsed/expanded module component
2. Rewrite PedalboardScreen: LazyColumn of RackModule (replace LazyRow of PedalCard)
3. Fixed top bar with preset name + tuner + A/B + settings
4. Fixed bottom metering strip (compact/expanded)
5. Inline parameter editing in expanded modules
6. Vertical drag-and-drop reordering (adapt reorderable library)
7. Full-screen preset browser (replace dropdown)

**Phase 3 -- Polish (1-2 sprints)**
Priority: Professional-grade refinements

1. Effect-specific module layouts (Amp: larger knobs + tolex; EQ: frequency curve; Delay: tap tempo)
2. Tuner overlay mode
3. Surface textures (subtle noise overlay, speaker grille hint for cabinet)
4. Micro-animations (LED glow pulse on signal, knob value arc animation, spring physics)
5. Accessibility (TalkBack for knobs, content descriptions, high-contrast mode)
6. Landscape support for rack view

---

## 10. Specific Recommendations Summary

### 10.1 Highest-Priority Changes (from Guitar Rig 7 analysis)

1. **Vertical rack layout**: This is the single most impactful change. Guitar Rig 7, AmpliTube 5, and every professional rack processor uses a vertical signal chain. Our horizontal scroll of small cards is the primary UX bottleneck.

2. **Inline parameter editing**: Guitar Rig 7 validates expanding modules to edit inline rather than always opening a separate editor. This reduces navigation friction by eliminating the constant open/close of bottom sheets.

3. **Canvas-drawn rotary knobs**: Sliders are not how musicians think about guitar parameters. Every competitor uses rotary knobs. This is the single most impactful visual change.

4. **Collapsed/expanded module states**: Guitar Rig 7's collapse/expand is essential for managing a 21-effect chain in a vertical scroll. Without it, the rack would be impossibly long.

5. **Fixed metering strip**: Level meters and spectrum should be always-visible, not scrolled offscreen. Guitar Rig's header meters are always present.

### 10.2 Features Guitar Rig Has That We Should NOT Copy

1. **Dual signal path / split routing**: Too complex for our 21-effect linear chain on a phone screen. Defer to a future "advanced mode."

2. **Multiple filter tag types for presets**: Too complex for mobile. Our single category chip row + search is sufficient.

3. **8-color favorite system**: Simplify to star/unstar. Color favorites are a desktop power-user feature.

4. **Side-by-side browser + rack**: No room on phone. Full-screen browser overlay is the correct mobile pattern.

5. **CPU load indicator**: On mobile, the audio engine manages CPU internally. No need to display this to the user.

6. **Undo/Redo stack**: Good feature but complex to implement. A/B comparison serves a similar purpose for tone exploration. Defer.

### 10.3 Features Competitors Do Well That Guitar Rig Does Not

1. **Live Mode** (AmpliTube, BIAS FX): A full-screen, large-touch-target mode for stage use. We should add this as a Phase 3 feature -- showing just the preset name, big prev/next buttons, bypass stomp, and tuner.

2. **Community preset sharing** (BIAS FX ToneCloud, ToneX ToneNET): Guitar Rig relies on NI User Libraries. Social preset sharing is a differentiator. Defer to future sprint.

3. **USB audio interface support** (Deplike): Critical for Android latency. This should be investigated independently of the UI redesign.

4. **Photorealistic gear views** (AmpliTube): Visually impressive but resource-intensive. Our "clean modern with 3D touches" approach (matching GR7) is the right balance for mobile.

---

## Sources

- [Guitar Rig 7 Pro -- Native Instruments Product Page](https://www.native-instruments.com/en/products/komplete/guitar/guitar-rig-7-pro/)
- [Guitar Rig 7 Manual -- Overview of Guitar Rig](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/overview-of-guitar-rig.html)
- [Guitar Rig 7 Manual -- Overview of the Browser](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/overview-of-the-browser)
- [Guitar Rig 7 Manual -- Using the Browser](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/using-the-browser)
- [Guitar Rig 7 Manual -- Components Reference](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/components-reference)
- [Guitar Rig 7 Manual -- Signal Path](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/signal-path)
- [Guitar Rig 7 Manual -- Signal Flow](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/signal-flow)
- [Guitar Rig 7 Manual -- Using Presets](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/using-presets)
- [Guitar Rig 7 Manual -- Overview of the Rack](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/overview-of-the-rack)
- [Guitar Rig 7 Manual -- Browser](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/browser.html)
- [Guitar Rig 7 Manual -- Getting Started](https://native-instruments.com/ni-tech-manuals/guitar-rig-manual/en/getting-started)
- [Guitar Rig 7 Pro -- Effects and Tools](https://www.native-instruments.com/en/products/komplete/guitar/guitar-rig-7-pro/effects-and-tools/)
- [Guitar Rig 7 Pro -- Amps and Cabinets](https://www.native-instruments.com/en/products/komplete/guitar/guitar-rig-7-pro/amps-and-cabinets/)
- [Native Instruments Guitar Rig Pro 7 Review -- Sound On Sound](https://www.soundonsound.com/reviews/native-instruments-guitar-rig-pro-7)
- [Guitar Rig 7 Pro Review -- AudioNewsRoom](https://audionewsroom.net/2024/09/guitar-rig-7-pro-review-new-amps-new-technology-better-ui.html)
- [Guitar Rig 7 Pro Review -- MusicRadar](https://www.musicradar.com/reviews/native-instruments-guitar-rig-pro-7-review)
- [Guitar Rig 7 Pro Benefits Review -- WaveInformer](https://waveinformer.com/2025/06/04/guitar-rig-7-pro/)
- [Guitar Rig 7 Pro In-Depth Review -- PlugiNoise](https://pluginoise.com/ni-guitar-rig-7-pro-review/)
- [Guitar Rig 7 Pro Walkthrough -- ToolFarm](https://www.toolfarm.com/tutorial/guitar-rig-7-pro-walkthrough-from-native-instruments/)
- [NI Community -- Browser Help Discussion](https://community.native-instruments.com/discussion/comment/211634/)
- [Positive Grid BIAS FX 2 Mobile Review -- MusicRadar](https://www.musicradar.com/reviews/positive-grid-bias-fx-2-mobile)
- [BIAS FX 2 Review -- HomeStudioToday](https://www.homestudiotoday.com/positive-grid-bias-fx-2-review/)
- [BIAS FX 2 Signal Chain -- Positive Grid Blog](https://www.positivegrid.com/blogs/positive-grid/how-to-create-your-own-signal-chain-in-bias-x)
- [AmpliTube for iPhone/iPad -- IK Multimedia](https://www.ikmultimedia.com/products/amplitubeios/)
- [AmpliTube 5 Review -- Guitar Gear Finder](https://guitargearfinder.com/reviews/amplitube-5/)
- [ToneX iOS -- IK Multimedia](https://www.ikmultimedia.com/products/tonexios/)
- [ToneX Update 2024 -- Guitar World](https://www.guitarworld.com/news/ik-multimedia-tonex-update-2024)
- [Deplike Guitar FX -- Google Play](https://play.google.com/store/apps/details?id=com.deplike.andrig)
- [Top Mobile Guitar Apps -- Deplike Blog](https://blog.deplike.com/top-5-guitar-amp-apps-on-mobile/)
- [Guitar Rig Manual PDF](https://www.native-instruments.com/fileadmin/ni_media/downloads/manuals/gr7/Guitar_Rig_7_Manual_English_07_09_23.pdf)
