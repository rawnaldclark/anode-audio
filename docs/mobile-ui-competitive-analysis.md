# Mobile Guitar Effects App - Competitive UI/UX Analysis

## Research Date: March 2026

---

## 1. App-by-App Analysis

### 1.1 BIAS FX 2 Mobile (Positive Grid)

**Platform:** iOS only (no Android version as of research date)

**Signal Chain Layout:**
- Horizontal signal chain displayed on the main "Routing" screen
- Effects represented as small module icons arranged left-to-right in processing order
- Signal flows left-to-right with a visible signal path line connecting modules
- Dual signal path mode adds a splitter and mixer, creating a Y-shaped topology
- The chain is scrollable horizontally; one reviewer noted that during live performance, "I sometimes forget if I have certain effects turned on or off... because I can't bend down and scroll over" -- a critical limitation of horizontal layouts on mobile

**Component Browser / Adding Effects:**
- Tap an empty slot or press the "add" button to open a model finder drop-down menu
- Effects categorized by type (stomp, rack, amp, cab, mic)
- Drag-and-drop to reposition within the chain
- Double-tap an existing pedal to open its editor and replace it from there

**Parameter Editing:**
- Double-tap a module to open an "enlarged view" of that module
- Full-screen dedicated editor view with all controls for the selected component
- Controls are rendered as virtual knobs and switches matching the component's real-world appearance
- "Simply close the screen and you're back to your main rig overview" -- clear editor-to-overview transition

**Preset Browsing:**
- ToneCloud integration for community presets (huge library)
- LiveView mode: large, tap-friendly preset buttons for stage use -- switch entire rig with single tap, zero latency
- LiveView also allows toggling individual effects or effect categories on/off
- Long press for tap tempo assignment in LiveView

**Navigation:**
- Two primary modes: Routing view (signal chain editing) and LiveView (performance switching)
- Top toolbar provides access to tuner, metronome, looper, Guitar Match, output settings
- Settings accessible from top-level menu

**Strengths:**
- LiveView is the gold standard for live performance switching on mobile
- Dual signal path is unique among mobile apps
- ToneCloud community creates massive preset library
- Clean separation between editing mode and performance mode

**Weaknesses:**
- Horizontal chain scroll loses visibility at the extremes
- iOS only (no Android)
- Subscription pricing model frustrates some users
- Complex dual-path routing can be confusing on small screens

---

### 1.2 AmpliTube (IK Multimedia)

**Platform:** iOS and Android

**Signal Chain Layout:**
- Fixed slot architecture: 4 pre-amp stomp slots + 1 amp + 1 cab + 2 post-amp stomp slots
- Each section has its own dedicated view -- you navigate BETWEEN sections, not across a single chain view
- The signal chain is implicit in the section ordering, not visually rendered as a connected flow
- The fixed topology means no arbitrary reordering of entire chain; effects are constrained to their slot type

**Component Browser / Adding Effects:**
- "Carousel" gear browser -- swipe horizontally through available models within a slot type
- Skeuomorphic thumbnails of each piece of gear in the browser
- Drag-and-drop from browser into a slot
- Gear organized by brand partnership (MESA/Boogie, Orange, Fender, ENGL) as well as by type
- In-app purchases expand the gear collection (store model)

**Parameter Editing:**
- Fully skeuomorphic: each stomp box, amp, and cabinet is rendered as a photorealistic virtual version of the real hardware
- Knobs on the virtual amp/pedal are the actual controls -- twist/drag to adjust
- Amp section fills roughly the bottom half of the screen with a realistic amp face
- Cabinet "Cab Room" allows 3D mic placement with draggable microphone icons
- The skeuomorphic approach means each piece of gear has a unique visual identity and control layout

**Preset Browsing:**
- Preset management system with save/load/rename
- Organized by tonal character
- Community sharing via ToneNET

**Navigation:**
- Section-based navigation: tap to switch between Stomps, Amp, Cab, Mic views
- Navigation tabs or icons at the screen edges to move between sections
- Live Mode: optimized GUI showing the entire chain status at a glance with clear on/off indicators, "easy to read on-stage" layout
- Recorder and song player are separate navigation destinations

**Strengths:**
- The most mature mobile guitar app (on market since 2010+)
- Skeuomorphic design creates strong emotional connection -- feels like "real gear"
- Cab Room with 3D mic placement is unique and intuitive
- Brand partnerships provide officially modeled gear (MESA, Orange, etc.)
- Live Mode strips away editing complexity for stage use
- Available on Android (unlike BIAS FX 2)

**Weaknesses:**
- Fixed slot architecture limits creative signal routing
- Skeuomorphic controls can be small and hard to manipulate on phones (better on iPad)
- In-app purchase model means the free version feels limited
- Navigation between sections can feel disconnected from the signal flow
- Photorealistic renderings do not scale well to small phone screens -- details get lost below ~200dp width

---

### 1.3 ToneX (IK Multimedia)

**Platform:** iOS (standalone + AUv3); TONEX Control on iOS and Android

**Signal Chain Layout:**
- Simplified architecture: tone model is the central element, with optional pre/post processing
- Built-in compressor, pre-amp EQ, noise gate, post-amp EQ, stereo reverb
- Signal chain is more linear and less customizable than full multi-effects apps
- The interface focuses on the tone model itself, not the chain routing

**Component Browser / Adding Effects:**
- ToneNET integration: browse, search, filter, demo, and download community tone models
- Filter by Type, Instrument, Character, Favorite, and Skin
- Search is first-class: instant, with multiple filter dimensions
- Each tone model has metadata (author, genre, character tags)

**Parameter Editing:**
- "Full real-time editing" with intuitive controls over amps, cabs, effects, IRs, and EQ
- VIR (Virtual IR) technology for mic placement
- Knob-based controls for gain, EQ, and effect parameters
- Interface is simpler than AmpliTube since fewer parameters are exposed at once

**Preset Browsing:**
- Unlimited user presets on device
- "Easily organize, rename, back up, and sync across devices"
- 30 onboard presets accessible quickly
- ToneNET cloud library provides thousands of community captures

**Navigation:**
- Focused, minimal navigation since the app has fewer modes
- Main view: tone model + basic controls
- Library/browser as a separate view
- Works as AUv3 plugin inside host apps

**Strengths:**
- Clarity through simplicity: fewer controls means less cognitive load
- AI-captured tone models are the core value, not signal chain complexity
- ToneNET community creates massive library with strong metadata/search
- Minimal interface is well-suited for phone screens
- Fast workflow: browse, tap, play -- get a usable tone in seconds

**Weaknesses:**
- Limited signal chain customization (by design)
- No arbitrary effect ordering
- Less suited for users who want to build complex rigs from scratch
- The simplicity that is its strength also limits power users

---

### 1.4 Deplike and Budget Android Apps

**Platform:** Android and iOS

**Deplike Signal Chain:**
- Simple, clean interface with pedalboard + amp/cab view
- Effects displayed as a row of small pedal icons
- 21 effects pedals available (matching our count exactly)
- 15 amp simulations with 12 electric, 2 bass, 1 acoustic guitar amps and cabinets
- Setting up is described as "straightforward" with clear interface for assembling rigs

**Deplike Parameter Editing:**
- "Knobs and faders are straightforward"
- Touch controls described as responsive with quick tweaking capability
- "Simple yet elegant UI with no eye candy for useless items"
- Practical over flashy design philosophy

**Deplike Preset Browsing:**
- 10,000+ community presets
- Search by artist name (Metallica, Led Zeppelin, Hendrix, etc.)
- "Reach their tones with a single tap"

**Common Budget Android App Patterns (from market analysis):**
- Many free/ad-supported apps use very basic Material Design components
- Stock Android sliders instead of knobs
- Flat, generic backgrounds with no visual identity
- Garish color choices (bright neon green, oversaturated gradients)
- Tiny touch targets that require precision tapping
- Excessive advertising banners that steal screen real estate
- Poor latency handling creates perception of "broken" product

**What Makes Budget Apps Feel "Cheap":**
1. Generic Material Design with no audio-domain visual language
2. Stock sliders for all parameters (no knobs, no switches)
3. No visual distinction between effect types
4. Bright, saturated, primary colors on white or pure black backgrounds
5. Small touch targets with no haptic feedback
6. Visible loading/stuttering during parameter changes
7. Cluttered layouts trying to show everything at once
8. Banner ads eating 10-15% of screen height
9. No live-performance mode or consideration for on-stage use
10. No metering or signal feedback (user cannot see if signal is present/clipping)

**What Makes an App Feel Professional:**
1. Dark theme with warm grays (not pure black)
2. Custom-drawn controls (knobs, LED indicators, meters)
3. Effect category color coding
4. Responsive parameter changes with no visible latency
5. Signal metering (input/output levels, clip indicator)
6. Thoughtful typography (monospace for values, sans-serif for labels)
7. Adequate touch targets (44dp minimum, 48dp preferred)
8. Clear bypass state per effect (LED indicator, not just opacity)
9. Preset system with search and categories
10. Loading states and error handling that feel polished

---

### 1.5 Line 6 Helix / HX Stomp Edit / HX Effects

**Platform:** Hardware-first with desktop editor (HX Edit); Cortex Mobile for Neural DSP ecosystem

**Signal Chain Layout:**
- Block-based grid: each processing block occupies a position in the signal flow
- Blocks arranged left-to-right with clear signal path connections
- Parallel paths supported (split + merge)
- HX Stomp: maximum 6 blocks, displayed on a small color LCD
- Full Helix: up to 32 blocks across dual signal paths

**Effect Categories and Color Coding (Industry Standard):**
This is the most important reference point. Line 6 established the color-coding convention that most hardware and software now follows:

| Category | Helix Color | LED Ring Color |
|----------|-------------|----------------|
| Distortion | Light Orange | Orange |
| Dynamics | Gray/White | White |
| EQ | Gray/White | White |
| Modulation | Blue | Blue |
| Delay | Green | Green |
| Reverb | Dark Orange/Amber | Amber |
| Pitch/Synth | Purple | Purple |
| Wah/Filter | Purple | Purple |
| Volume/Pan | Gray | Gray |
| Looper | Cyan | Cyan |
| Amp | Yellow | Yellow |
| Cab/IR | Brown/Orange | Orange-Brown |

**HX Edit (Desktop Editor) Signal Chain:**
- Top section: large signal flow view with draggable blocks
- Bottom section: parameter editing panel for selected block
- Copy/paste/clear actions for blocks
- Customizable footswitch labels and LED colors
- Effects browser organized by category with subcategories

**Neural DSP Quad Cortex (Comparison):**
- 7-inch touchscreen with grid-based signal chain ("The Grid")
- Tap empty slot to open Virtual Device List
- Swipe vertically to browse devices
- Categories collapsible/expandable
- Long-press to pin favorite devices to top of category
- Touch knobs on screen to adjust parameters

**Strengths:**
- Category color coding is the clearest visual language in the industry
- Block-based paradigm is infinitely flexible for routing
- The desktop editor provides full visibility of entire chain
- Hardware integration means the editor mirrors the hardware exactly

**Weaknesses:**
- HX Edit is desktop-only, not optimized for mobile
- Block-based routing can be overwhelming for beginners
- The grid paradigm is better suited for larger screens

---

### 1.6 Positive Grid Spark App (Bonus Reference)

**Platform:** iOS and Android (companion app for Spark hardware)

**Signal Chain:**
- Fixed order: Noise Gate / Comp / Drive slots before amp, Mod / Delay / Reverb after amp
- Effect slot order is unchangeable
- Swipe the FX chain left-right to see full list of effects
- Swipe up/down on a pedal icon to enable/disable the effect
- Double-tap pedal to open effect type selection list
- Knob adjustment via vertical swipe (up to increase, down to decrease)

**Key UX Patterns:**
- Swipe up/down enable/disable is a novel gesture that saves screen space (no separate bypass button)
- Vertical swipe for knob adjustment is the standard mobile audio interaction
- "Fresh and well laid out, like something Apple would release" -- indicates high visual polish
- Fixed chain order simplifies UI but limits flexibility

---

## 2. Comparative Matrix

### Signal Chain Layout

| App | Orientation | Scrollable | Reorderable | Parallel Paths | Fixed/Free |
|-----|------------|------------|-------------|----------------|------------|
| BIAS FX 2 | Horizontal | Yes | Yes (drag) | Yes (dual) | Free ordering |
| AmpliTube | Section-based | N/A (sections) | No | No | Fixed slots |
| ToneX | Linear (minimal) | No | No | No | Fixed |
| Deplike | Horizontal row | Likely | Unknown | No | Unknown |
| Helix/HX | Horizontal blocks | Yes | Yes | Yes | Free ordering |
| Spark | Horizontal | Yes (swipe) | No | No | Fixed slots |
| **Our App** | **Horizontal LazyRow** | **Yes** | **Yes (drag)** | **No** | **Free ordering** |

### Parameter Editing Pattern

| App | Editing Location | Control Type | Precision Gesture | Value Display |
|-----|-----------------|--------------|-------------------|---------------|
| BIAS FX 2 | Full-screen editor | Virtual knobs | Vertical drag | Numeric readout |
| AmpliTube | In-place skeuomorphic | Photorealistic knobs | Drag (rotation feel) | On-pedal display |
| ToneX | In-place + detail view | Knobs + sliders | Vertical drag | Numeric |
| Deplike | In-place | Knobs + faders | Swipe | Values shown |
| Helix/HX | Bottom panel (desktop) | Sliders + numeric | Click/drag | Numeric with units |
| Spark | In-place | Virtual knobs | Vertical swipe | Numeric |
| **Our App** | **Bottom sheet** | **Material3 Sliders** | **Horizontal drag** | **Numeric** |

### Navigation Pattern

| App | Primary Nav | Preset Access | Settings | Tuner | Performance Mode |
|-----|-----------|---------------|----------|-------|-----------------|
| BIAS FX 2 | Mode tabs (Routing/Live) | Top bar + LiveView | Menu | Top bar icon | LiveView |
| AmpliTube | Section tabs (Stomp/Amp/Cab) | Preset panel | Menu | Dedicated | Live Mode |
| ToneX | Minimal (main + library) | Library tab | Menu | Built-in | Minimal by nature |
| Deplike | Tab-like sections | Preset browser | Menu | Built-in | None |
| Spark | Single screen + overlays | Preset list | Settings icon | Built-in | None |
| **Our App** | **Single screen** | **Dropdown** | **Separate screen** | **Inline (scroll)** | **None** |

### Visual Design Approach

| App | Skeuomorphism Level | Dark Theme | Category Colors | LED Indicators | Custom Knobs |
|-----|-------------------|------------|-----------------|----------------|-------------|
| BIAS FX 2 | Medium (stylized gear) | Yes | Yes | Yes (glow) | Yes |
| AmpliTube | High (photorealistic) | Yes (dark tones) | Implicit (gear type) | Yes | Yes (photo) |
| ToneX | Low-Medium (clean) | Yes | Minimal | Minimal | Yes |
| Deplike | Low-Medium | Yes | Unknown | Basic | Yes (simple) |
| Helix/HX | Low (functional blocks) | Yes (dark LCD) | Yes (strong) | Yes (LED rings) | No (sliders) |
| Spark | Medium (clean modern) | Yes | Implicit | Yes | Yes |
| **Our App** | **None (flat Material)** | **Yes** | **No** | **Flat circles** | **No (sliders)** |

---

## 3. Key Questions Answered

### 3.1 Signal Chain Layout: Vertical vs. Horizontal?

**Finding:** All mobile apps use horizontal layouts for the signal chain overview, matching the real-world left-to-right signal flow mental model. However, this universality does not mean horizontal is optimal for mobile.

**The horizontal problem:** On a phone screen (~360dp wide), a horizontal chain of 21 effects at 100dp each requires scrolling through 2100dp of content. The user can see only 3-4 effects at a time. BIAS FX 2 users explicitly complain about losing track of bypass states mid-chain during performance.

**The vertical alternative (Guitar Rig model):** A vertical LazyColumn with full-width collapsed modules (~48dp height each) can show 12-14 effects on screen simultaneously (assuming a 700dp tall content area). This provides much better overview of the entire chain.

**Recommendation for our app:** The vertical rack layout described in our existing `ui-overhaul-design.md` is the correct architectural choice. It is unconventional in mobile guitar apps, but it solves the visibility problem that plagues horizontal layouts. The key advantage: collapsed modules at 48dp each means 21 effects x 48dp = 1008dp total, requiring minimal scrolling on most phones. With a fixed top bar (preset) and fixed bottom bar (meters), a ~600dp scrollable area can show 12 effects without scrolling.

**Critical detail for thumb reachability:** Vertical scrolling is the natural phone gesture. Horizontal scrolling requires deliberate horizontal swipes that conflict with back-gesture navigation on gesture-navigation Android. Vertical wins for ergonomics.

### 3.2 Component Adding: How to Add Effects to the Chain?

**Finding:** Most mobile apps use one of these patterns:
- **Tap empty slot + category list** (Quad Cortex, Helix)
- **Drag from browser sidebar** (AmpliTube desktop, Guitar Rig desktop)
- **Tap "add" button + overlay picker** (BIAS FX 2)
- **Double-tap to replace** (Spark, BIAS FX 2)

**Our situation:** We have a fixed set of 21 effects always present in the chain. Users do not add or remove effects -- they enable/disable and reorder. This simplifies our UI significantly.

**Recommendation:** No "add effect" UI needed. Focus on clear enable/disable per module (LED + stomp switch) and drag-to-reorder. The vertical rack with collapsed/expanded modules naturally exposes bypass toggling.

### 3.3 Parameter Editing: Full-Screen vs. Inline vs. Bottom Sheet?

**Finding:** The industry has converged on two patterns:

1. **In-place editing with dedicated focus view** (AmpliTube, Spark): Edit directly on the module, with an option to open a larger focused view.
2. **Dedicated editor screen/sheet** (BIAS FX 2, our current approach): Tap to open a separate editing context.

The Spark app's approach is instructive: vertical swipe on a knob adjusts it in-place. This minimizes context switching.

**Recommendation:** Hybrid approach matching our existing design document:
- **Phase 1:** Retain bottom sheet for full parameter editing (current architecture). Replace Material3 Sliders with RotaryKnob controls in the sheet.
- **Phase 2:** Add inline editing in expanded rack modules. When a module is expanded, its 2-3 primary knobs are interactive directly in the rack view. The bottom sheet becomes a "full editor" for secondary parameters and special controls (IR loader, tap tempo, etc.).

This gives the best of both worlds: quick in-place tweaking AND full editing capability.

### 3.4 Navigation Pattern: Tab Bar? Hamburger? Swipe?

**Finding:**
- **AmpliTube:** Section-based tabs (Stomp/Amp/Cab/Mic) -- appropriate because their fixed slot architecture maps to distinct views.
- **BIAS FX 2:** Mode-based tabs (Routing/LiveView) -- two distinct modes of use.
- **Spark, ToneX, Deplike:** Single-screen with overlays/modals for secondary functions.
- **No mobile guitar app uses a bottom navigation bar** (Material Design pattern). The single-screen-with-overlays pattern dominates.

**Recommendation:** Single-screen architecture with overlays. Our app does not have enough distinct "destinations" to justify tab navigation. The signal chain IS the app. Secondary functions (presets, settings, tuner) are overlays/sheets/dialogs.

**Specific navigation map:**
```
[Pedalboard Screen] -- always visible
  tap preset bar     --> Full-screen preset browser overlay
  tap tuner button   --> Semi-transparent tuner overlay
  tap settings gear  --> Settings screen (full-screen navigate)
  tap/expand module  --> Inline editing OR bottom sheet
  A/B button         --> A/B comparison bar (inline toggle)
```

### 3.5 Screen Real Estate Allocation

**Finding from professional apps (approximate percentages on phone):**

| Area | BIAS FX 2 | AmpliTube | ToneX | Spark |
|------|----------|-----------|-------|-------|
| Top bar (preset/nav) | 8% | 8% | 10% | 8% |
| Signal chain | 55% | 0% (sections) | 30% | 45% |
| Parameter controls | 25% (in editor) | 80% (in section) | 40% | 35% |
| Meters/monitoring | 5% | 5% | 10% | 5% |
| Bottom nav/tools | 7% | 7% | 10% | 7% |

**Recommendation for our app (default view):**
```
Top bar (preset, tuner, A/B, gear): 8%   (~56dp)
Signal chain rack (scrollable):     72%  (~500dp)
Bottom metering strip:               8%   (~56dp)
Master controls (collapsible):      12%  (~80dp)
```

When editing a module (bottom sheet open), the sheet covers the bottom 50-60% of the screen, the rack remains visible behind it (scrolled to show the edited module).

### 3.6 Skeuomorphism vs. Flat on Mobile

**Finding:** There is a clear spectrum across apps:

| Level | Example | Works on Phone? |
|-------|---------|----------------|
| Full photorealistic | AmpliTube stomps | Marginal -- details lost below ~200dp, but emotional impact is high |
| Styled 3D | BIAS FX 2, Spark | Good -- enough realism for recognition without detail dependency |
| Clean 3D touches | Guitar Rig 7 | Best for mobile -- clear controls, 3D depth cues, no lost detail |
| Flat functional | Helix HX Edit | Works well for information density, feels clinical |
| Generic Material | Budget Android apps | Feels "cheap" -- no audio-domain identity |

**Fractal Audio forum poll insight:** The community splits roughly 50/50 on skeuomorphic vs. functional. However, the "functional with tasteful 3D" camp (Guitar Rig 7 style) is the one that satisfies both sides.

**Recommendation:** The "Clean 3D" approach from our existing design document is correct:
- Canvas-drawn knobs with radial gradient (3D illusion) but no photorealistic textures
- LED indicators with glow effects
- Category color tinting on module surfaces
- No attempt to reproduce specific hardware appearances (except subtle nods like Boss orange headers)
- This scales perfectly across phone sizes because it is vector-rendered, not texture-dependent

### 3.7 Color and Branding for Effect Categories

**Finding:** The Helix color-coding system is the industry reference. All professional hardware (Helix, Kemper, Quad Cortex, Axe-FX) and most software uses some form of category coloring.

**Comparison of category color conventions:**

| Category | Helix | Our Design Doc | Notes |
|----------|-------|----------------|-------|
| Distortion/Gain | Orange | Red-Orange #D4533B | Compatible -- warm tones for gain |
| Amp | Yellow | Amber/Gold #C8963C | Compatible |
| EQ | White/Gray | Silver #909098 | Compatible -- neutral |
| Modulation | Blue | Blue-Violet #6B7EC8 | Compatible |
| Delay | Green | Teal #3CA8A0 | Slightly different (Helix=green, ours=teal) |
| Reverb | Amber | Teal #3CA8A0 (same as delay) | We merge delay+reverb as "Time-Based" |
| Filter/Wah | Purple | Blue-Violet (with modulation) | We group wah with modulation |
| Utility | Gray | Slate #607088 | Compatible |

**Recommendation:** Our 6-category system from the design doc is well-chosen. The one potential improvement: consider separating Delay and Reverb colors if users would benefit from distinguishing them visually in the chain. However, 6 categories is already the practical maximum for color differentiation without confusion (human working memory limit ~7 items). Keep the current system.

### 3.8 Real-Time Performance

**Finding:** All professional apps (BIAS FX 2, AmpliTube, ToneX) achieve smooth parameter updates because they:
1. Run audio processing on a dedicated native thread (Oboe/Core Audio)
2. Use lock-free parameter passing (atomics, ring buffers)
3. Keep UI and audio threads decoupled
4. Update visual feedback at display refresh rate, not audio rate

**Our architecture already handles this correctly:** C++ audio callback with atomic parameters, JNI bridge, Kotlin StateFlow for UI updates. No architectural changes needed for performance.

**UI-specific performance recommendations:**
- Knob value changes during drag must NOT trigger recomposition of sibling composables -- isolate each knob with its own `value` state parameter
- Use `graphicsLayer` for alpha/scale animations (avoids recomposition)
- Spectrum analyzer and meters on a 30fps coroutine (already implemented)
- Avoid allocations during drag gestures (pre-allocate Paint, Path objects in `remember` blocks)

---

## 4. Best-of-Breed Recommendations

### 4.1 Signal Chain: Adopt Vertical Rack (Guitar Rig 7 Pattern)

**Best practice from:** Guitar Rig 7 (desktop), adapted for mobile vertical scrolling.

No mobile guitar app currently uses a vertical rack layout. This is an opportunity to differentiate while solving the horizontal-scroll visibility problem that affects BIAS FX 2 and other apps.

**Implementation in Jetpack Compose:**
```
LazyColumn(
    state = reorderableState,
    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
    verticalArrangement = Arrangement.spacedBy(4.dp)
) {
    items(effectStates, key = { it.index }) { effect ->
        ReorderableItem(reorderableState, key = effect.index) {
            RackModule(
                effect = effect,
                expanded = expandedIndex == effect.index,
                onToggleExpand = { expandedIndex = if (expandedIndex == effect.index) -1 else effect.index },
                onToggleBypass = { onToggleEffect(effect.index) },
                onLongPressDrag = { /* drag handle */ }
            )
        }
    }
}
```

### 4.2 Effect Bypass: Adopt LED + Stomp Switch (Universal Pattern)

**Best practice from:** All professional apps. Every single app uses a clear visual indicator for bypass state. The LED + stomp switch metaphor is universal.

**Why it works:** Musicians have decades of muscle memory around LED indicators on hardware. Green/colored = active, dim/dark = bypassed. This is the one area where skeuomorphism is non-negotiable.

**Our implementation:** Already designed in the overhaul document. LED with glow + stomp switch circle. Category-colored LEDs provide additional information (tells you the effect type at a glance from the LED color alone).

### 4.3 Parameter Controls: Adopt Vertical-Drag Rotary Knobs (Spark/BIAS Pattern)

**Best practice from:** Spark app (vertical swipe on knobs), BIAS FX 2 (virtual knobs), and the desktop audio plugin convention.

**Why vertical drag wins over rotation gesture on mobile:**
1. Rotation gesture requires two-finger precision or tracking angle from center -- imprecise on small targets
2. Vertical drag is a natural, single-finger gesture that maps intuitively (up = more, down = less)
3. Vertical drag precision can be modulated by horizontal distance from control (move finger horizontally away from knob to enter "fine" mode)
4. Industry consensus (from KVR Audio forums): "Dragging up/down is a sensitive operation... developers should make the control respond to one axis OR the other, not both simultaneously"

**Sizing for touch targets:**
- Standard knob: 56dp diameter (canvas area: 72dp x 72dp including glow/value arc padding)
- Large knob: 68dp diameter (for amp model, featured parameters)
- Small knob: 44dp diameter (absolute minimum, only for tight secondary parameters)
- Label below knob: 11sp monospace
- Spacing between knobs: minimum 12dp

### 4.4 Preset Browsing: Adopt Full-Screen Overlay with Category Chips (Enhanced from Multiple Apps)

**Best practice from:** ToneX (strong search/filter), BIAS FX 2 (ToneCloud model), our existing category chips.

The current dropdown menu is the weakest point of our UI. Every professional app uses a substantial preset browsing interface, not a Material Design dropdown.

**Recommended pattern:**
- Full-screen overlay (not a separate Activity/screen -- an overlay so the user mentally stays in the "pedalboard context")
- Top section: search bar + category filter chips
- Main section: scrollable preset list with rich cards (name, category badge, author, date)
- Factory and User presets in separate sections (with headers)
- Active preset highlighted with category color indicator
- Long-press for context menu (rename, delete, export)
- Import/Export buttons at bottom

### 4.5 Live Performance Mode: Adopt LiveView Concept (BIAS FX 2 Pattern)

**Best practice from:** BIAS FX 2 LiveView, AmpliTube Live Mode.

**Both apps agree on the core principle:** A live performance mode must show:
1. Current preset name (LARGE, readable from distance)
2. Quick-switch buttons for adjacent presets (large tap targets)
3. Per-effect on/off toggles (large, clearly labeled)
4. No editing controls (remove anything that could be accidentally changed)

**Our app currently lacks a performance mode.** This should be a Phase 3 addition:
- Toggle via a "Live" button in the top bar
- Show current preset name in large text (24sp+)
- Show a grid of large bypass toggles (one per enabled effect in the chain)
- Prev/Next preset navigation with large arrow buttons
- No knobs, no sliders, no editor access

### 4.6 Metering: Keep Current Architecture, Enhance Visual (Universal Standard)

**Best practice from:** Universal across professional apps. All show input/output levels with clip indication. Our dBFS meters with CLIP indicator and spectrum analyzer are already at or above the industry standard for mobile apps.

**Enhancement:** Dock meters in a fixed bottom strip (not scrolled away). Current implementation scrolls with the page content, which means meters are invisible when scrolling through the chain.

### 4.7 Tuner: Adopt Quick-Access Overlay (AmpliTube/BIAS Pattern)

**Best practice from:** Both BIAS FX 2 and AmpliTube make the tuner accessible from the top bar with a single tap. Our tuner is currently embedded inline in the scrollable content.

**Recommendation:** Tuner overlay triggered by a top-bar icon. Semi-transparent overlay covering the upper portion of the screen. Existing TunerDisplay component (Boss TU-3 style) is well-designed and can be used directly.

---

## 5. Interaction Pattern Recommendations for Jetpack Compose

### 5.1 Gestures

| Gesture | Action | Compose Implementation |
|---------|--------|----------------------|
| Single tap (module header) | Toggle expand/collapse | `Modifier.clickable { }` |
| Single tap (stomp switch) | Toggle bypass | `Modifier.clickable { }` with haptic |
| Long press (module) | Initiate drag reorder | `longPressDraggableHandle()` from reorderable library |
| Vertical drag (knob) | Adjust parameter value | `detectVerticalDragGestures()` in `pointerInput` |
| Double-tap (knob) | Reset to default | `detectTapGestures(onDoubleTap = { })` |
| Swipe down (from top) | Dismiss overlay | Built into `ModalBottomSheet` / custom overlay |

### 5.2 Transitions

| Transition | Pattern | Duration | Easing |
|-----------|---------|----------|--------|
| Module expand | `AnimatedVisibility` + `expandVertically` | 200ms | FastOutSlowIn |
| Module collapse | `AnimatedVisibility` + `shrinkVertically` | 150ms | FastOutLinearIn |
| Bottom sheet open | `ModalBottomSheet` spring animation | 300ms | Spring (stiffness=800) |
| Preset browser open | `AnimatedVisibility` + `fadeIn` + `slideInVertically` | 250ms | FastOutSlowIn |
| LED toggle | `animateColorAsState` | 100ms on, 200ms off | Linear |
| Drag reorder | `animateItemPlacement()` | 200ms | Spring |

### 5.3 Layout Breakpoints

| Screen Width | Knobs per Row | Module Layout | Signal Chain |
|-------------|--------------|---------------|-------------|
| < 360dp (small phone) | 2 | Compact, stacked labels | Full-width rack |
| 360-400dp (standard phone) | 2-3 | Standard | Full-width rack |
| 400-600dp (large phone/foldable) | 3 | Standard with wider spacing | Full-width rack |
| > 600dp (tablet) | 3-4 | Expanded, side labels | Rack with 80dp side margins |

---

## 6. Summary: Priority Actions Ranked by Impact

| Priority | Action | Effort | Impact | Reference App |
|----------|--------|--------|--------|---------------|
| 1 | Replace Material3 Sliders with RotaryKnob in EffectEditorSheet | Medium | Very High | All pro apps |
| 2 | Add category color coding (surface tint + LED color + header bar) | Medium | Very High | Helix, Guitar Rig |
| 3 | Implement LED indicator with glow effect | Low | High | All pro apps |
| 4 | Switch to vertical rack layout (LazyColumn) | High | Very High | Guitar Rig 7 |
| 5 | Add collapsed/expanded module states | Medium | High | Guitar Rig 7 |
| 6 | Replace dropdown with full-screen preset browser | Medium | High | ToneX, BIAS FX 2 |
| 7 | Add tuner overlay (quick access from top bar) | Low | Medium | BIAS FX 2, AmpliTube |
| 8 | Fixed bottom metering strip | Low | Medium | Universal |
| 9 | Inline editing in expanded modules | High | Medium | Spark, AmpliTube |
| 10 | Live performance mode | Medium | Medium | BIAS FX 2, AmpliTube |

These priorities align exactly with the phased plan in our existing `ui-overhaul-design.md`. The competitive analysis validates that document's approach.

---

## Sources

- [Positive Grid BIAS FX 2 Mobile Review - MusicRadar](https://www.musicradar.com/reviews/positive-grid-bias-fx-2-mobile)
- [BIAS FX 2 - App Store](https://apps.apple.com/us/app/bias-fx-2-1-guitar-tone-app/id1475438828)
- [BIAS FX 2 Full Product Tour - Help Center](https://help.positivegrid.com/hc/en-us/articles/360025136732-Full-Product-Tour-for-BIAS-FX-2)
- [12 Things About BIAS FX 2 - Charles Pennefather](https://www.charlespennefather.com/post/12-things-you-should-know-about-bias-fx-2-before-you-purchase-it)
- [AmpliTube for iPhone/iPad - IK Multimedia](https://www.ikmultimedia.com/products/amplitubeios/)
- [AmpliTube 5 Review - Guitar Site](https://www.guitarsite.com/amplitube/)
- [AmpliTube 5 In-Depth Review - Guitar Gear Finder](https://guitargearfinder.com/reviews/amplitube-5/)
- [TONEX iOS - IK Multimedia](https://www.ikmultimedia.com/products/tonexios/)
- [AmpliTube TONEX App - App Store](https://apps.apple.com/us/app/amplitube-tonex/id1613359930)
- [Deplike Guitar FX - Google Play](https://play.google.com/store/apps/details?id=com.deplike.andrig&hl=en_US)
- [Deplike Guitar FX and Amps - Blog](https://blog.deplike.com/processors/deplike-guitar-fx-and-amps-mobile-app-for-android-ios/)
- [Top Mobile Apps for Guitar Effects - Deplike Blog](https://blog.deplike.com/top-5-guitar-amp-apps-on-mobile/)
- [Line 6 HX Stomp - Official](https://line6.com/hx-stomp/)
- [Line 6 Helix Models - Helix Help](https://helixhelp.com/models?categoryId=1)
- [Line 6 Helix Effect Models - DShowMusic](https://dshowmusic.com/line-6-helix-effect-models/)
- [Neural DSP Quad Cortex Manual](https://neuraldsp.com/manual/quad-cortex)
- [Positive Grid Spark App](https://www.positivegrid.com/pages/spark-app)
- [Spark Effect Signal Chain Setup - Help Center](https://help.positivegrid.com/hc/en-us/articles/22873986405389-Set-Up-the-Effect-Signal-Chain)
- [Spark Effects List - Spark Amp Lovers](https://sparkamplovers.com/kb/spark-available-effects/)
- [BIAS X - Positive Grid](https://www.positivegrid.com/pages/bias-x)
- [Knobs and Dials in Mobile App Interfaces - DesignModo](https://designmodo.com/knobs-dials-mobile-app/)
- [From Physical Knobs to Software Knobs - Jorge Echeverry](https://jorgeecheverry.com/blog/knobs/from-physical-knobs-to%20-software-knobs.html)
- [KVR Audio - Knob Behaviour Discussion](https://www.kvraudio.com/forum/viewtopic.php?t=478099)
- [Skeuomorphism in Audio - Fractal Audio Forum](https://forum.fractalaudio.com/threads/skeuomorphic-or-not.137526/)
- [Skeuomorphic vs Flat Design - Storyly](https://www.storyly.io/post/skeuomorphism-vs-flat-design)
- [Positive Grid BIAS FX 2 - Sound on Sound](https://www.soundonsound.com/reviews/positive-grid-bias-fx-2-elite)
- [BIAS FX 2 Mobile - Engadget](https://www.engadget.com/bias-fx-2-ios-guitar-app-161712090.html)
- [Tonebridge Guitar Effects - Google Play](https://play.google.com/store/apps/details?id=com.ultimateguitar.tonebridge&hl=en_US)
