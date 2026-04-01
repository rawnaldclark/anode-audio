# Progressive Disclosure for a 27-Effect Guitar Pedalboard
## Expert Audio UI Analysis & Recommendation

---

## 1. Professional Tool Analysis

### 1.1 IK Multimedia AmpliTube (iOS/Android + Desktop)

**How it handles active vs inactive:**
AmpliTube uses a fixed-slot signal chain with empty bays. The desktop version shows a linear chain of 5 regions: Stomp (pre-amp), Amp, Cabinet, Rack (post-amp effects), and a second Stomp slot. Each region has a fixed number of slots (e.g., 4 stomp slots before the amp). Empty slots show a dotted outline placeholder reading "TAP TO ADD." Active effects show their photorealistic pedal/rack unit graphic with a bypass footswitch.

**Key insight:** AmpliTube does NOT hide inactive slots. It shows them as explicitly empty placeholders. This is the "empty bay" pattern -- every position in the chain is always visible, whether occupied or not. On mobile, the chain scrolls horizontally with the amp centered, and you swipe left/right to reach pre and post effects.

**What works:** The empty bays make it obvious where new effects can be added and how many slots remain. The fixed-slot architecture caps complexity (you cannot have 27 effects simultaneously).

**What does NOT work for us:** AmpliTube's fixed-slot model caps at roughly 12-15 simultaneous effects. With 27 effects, empty bays would create massive scroll distance. The horizontal scroll on mobile also makes it impossible to see the full chain at once.

---

### 1.2 Positive Grid BIAS FX 2 (iOS + Desktop)

**How it handles the chain:**
BIAS FX 2 offers three views: Pedalboard View (top-down skeuomorphic pedals on a flat surface), Signal Flow View (block diagram showing the routing path), and Live View (large footswitch-style buttons for performance). The Signal Flow view is the most relevant -- it shows effects as labeled blocks connected by lines, with bypass state shown per-block. Inactive/empty slots appear as "+" icons inline in the signal chain.

**Key insight:** BIAS FX has a dual-path architecture (parallel chains that merge), so the signal flow view is essential for understanding routing. But critically, they use the "add" metaphor rather than "enable/disable" -- you ADD effects to the chain from a browser, rather than toggling pre-existing effects on/off.

**What works:** The Signal Flow view provides excellent spatial understanding of the chain without wasting screen space on photorealistic pedal images. The "+" insertion points make it clear where in the chain a new effect will land.

**What does NOT work for us:** Our architecture has all 27 effects permanently in the chain with bypass states, not an add/remove model. Converting to add/remove would require significant refactoring of the signal chain model.

---

### 1.3 Line 6 Helix / HX Edit (Hardware + Desktop Editor)

**How Helix handles 32 block slots:**
The Helix has a 4-path, 32-block grid. The hardware shows this as a 2x16 grid (2 rows of 16 blocks on the LCD). HX Edit on desktop shows all 32 slots as a grid where each path is a horizontal row. Empty slots are visible as dark empty rectangles. Occupied slots show the effect name, bypass state (grayed out = bypassed), and category color coding. The scribble strip (on hardware) or block label shows the effect name.

**Key insight:** Helix uses the "slot" model -- the chain has fixed positions, and you place effects INTO positions. This is the guitar-amp-modeler consensus model (Fractal, Kemper, and QC all use variations). It separates the concepts of "chain position" from "effect instance." A position can be empty, occupied-and-active, or occupied-and-bypassed.

**What works:** Category color coding is immediately useful -- blue for delays, green for dynamics, orange for distortion, purple for modulation. The grid makes spatial position obvious. Bypassed blocks use reduced opacity or a gray overlay, making the active chain instantly scannable.

**What does NOT work for us:** The grid model assumes effects can be placed in arbitrary positions. Our model has a fixed order for all 27 effects (reorderable, but all always present). A grid with 27 permanently occupied slots would look overwhelming.

---

### 1.4 Neural DSP Quad Cortex

**The grid/path UI:**
The Quad Cortex uses a 4-row grid (like a 4-lane highway) where each row is a signal path. Effects are placed as blocks within rows, and rows can be split/merged. The 7-inch touchscreen shows the grid directly. Empty positions are dark cells. Active blocks show their name and category icon. Bypassed blocks show a dimmed state with a bypass indicator.

**Key insight:** The QC's most innovative UI decision is that the grid IS the primary interface -- there is no separate "list of all effects" or "browser." You tap an empty cell, a category picker appears, you select the effect, and it populates the cell. This is extremely fast for building patches.

**What works:** The cell-based layout gives maximum spatial clarity. The QC also uses a "stomp mode" for live performance where the grid is replaced by large footswitch labels (8 large buttons), each mapped to a block's bypass.

**What does NOT work for us:** The QC's model is add/remove, not enable/disable of pre-existing effects. And a 4-row grid on a phone screen (even 6+ inches) gives very small touch targets.

---

### 1.5 Apple GarageBand (iOS)

**Amp/effects chain view:**
GarageBand uses a horizontal scrolling strip at the bottom of the amp view. The amp is the hero element (large, skeuomorphic). Effects appear as small stomp pedals in a horizontal row below the amp. You tap a "+" button to add effects from a category browser. Active effects show their pedal graphic; tapping one opens its parameter editor as a modal.

**Key insight:** GarageBand aggressively limits complexity. You get approximately 4-5 effect slots. The "+" button opens a clean categorized list (Distortion, Modulation, Delay, etc.) with effect descriptions. This is the most approachable model for casual users.

**What works:** The category browser with descriptions helps discovery. The limited slot count prevents overwhelm.

**What does NOT work for us:** Our power users need access to 27 effects. A 4-5 slot limit would be unacceptable. GarageBand's model is "select which effects to use," not "manage a fixed chain of many effects."

---

### 1.6 Logic Pro (Desktop DAW)

**Channel strip plugin management:**
Logic uses a vertical channel strip with insert slots. Each slot can hold one plugin. Empty slots show "---" or nothing. Occupied slots show the plugin name with a power button for bypass. The slot list is scrollable if many plugins are inserted. Bypassed plugins show the name in gray/dimmed text. There is no separate "available plugins" sidebar -- you click an empty slot or the "+" at the bottom of the insert list, which opens a hierarchical menu (by manufacturer, by category, by recent).

**Key insight:** Logic separates "what's in the chain" from "what's available." The channel strip ONLY shows what is inserted. The plugin browser is accessed through an explicit add action. This is the DAW consensus -- Pro Tools, Ableton, Cubase, and Studio One all work this way.

**What works:** The channel strip is extremely scannable because it only contains active content. The hierarchical browser makes 1000+ plugins navigable.

**What does NOT work for us:** The DAW model assumes an add/remove paradigm. Our 27-effect fixed chain is fundamentally different.

---

### 1.7 Ableton Live

**Device chain view:**
Ableton shows the device chain as a horizontal strip at the bottom of the screen. Each device is a rectangular panel with its controls visible inline (no expand/collapse -- all parameters are always shown when the device is in the chain). Devices can be bypassed (title bar turns from colored to gray), deleted, or reordered via drag. The "Add" action opens the browser sidebar.

**Key insight:** Ableton's radical transparency -- showing ALL parameters of ALL chain devices simultaneously -- works because typical chains have 3-8 devices. With 27 effects, this would create an unscrollable wall of controls.

**What works:** The inline parameter editing (no modal/sheet required) is extremely efficient for quick tweaks. The color-coded title bars (blue for audio effects, green for MIDI, yellow for instruments) aid scanning.

**What does NOT work for us:** Ableton relies on horizontal screen width (1920px+) that a phone does not have. And the "all parameters visible" model does not scale to 27 effects.

---

### 1.8 Universal Audio UAFX App (iOS)

**Pedal management:**
The UAFX app is designed to control individual hardware pedals. Each pedal gets its own screen with a photorealistic view showing all controls. The app does NOT manage chains -- it controls one pedal at a time. Preset management is per-pedal.

**Key insight:** The UAFX model is irrelevant to chain management, but its per-pedal preset system is excellent. Each pedal has its own preset library, and you can store "scenes" (combinations of settings across multiple pedals). This is worth noting for our preset system.

**What does NOT work for us:** Single-pedal model; no chain management at all.

---

### 1.9 Boss Tone Studio (GT-1000 Editor)

**Effect management:**
Boss Tone Studio shows the GT-1000's signal chain as a horizontal flow diagram. The GT-1000 has a semi-fixed architecture: specific positions for Compressor, Overdrive 1, Overdrive 2, Preamp (amp model), EQ, Delay 1, Delay 2, Chorus, Reverb, and FX (generic slot). Each block is always visible in the chain view. Bypassed blocks are dimmed but remain in place. Tapping a block opens its parameter editor.

**Key insight:** The GT-1000 model is closest to our architecture -- a fixed set of effect types that are always present in the chain, individually bypassable. Boss solves the "too many blocks" problem by keeping the block SMALL when bypassed (just a label + on/off dot) and expanding it when selected for editing.

**What works:** The fixed-position model means the user always knows where to find the Compressor (it is always in the same slot). No searching. Bypassed blocks being visually reduced but present maintains spatial memory.

**What does NOT work for us with the FULL Boss model:** The GT-1000 has approximately 10-12 distinct positions. We have 27. Even with small bypassed blocks, 27 in a row creates excessive scrolling.

---

### 1.10 Kemper Rig Manager

**Stomp/effect slot management:**
The Kemper has a fixed signal flow: Input -> Stomp A -> Stomp B -> Stomp C -> Stomp D -> Amp Stack -> Stomp X -> Stomp Y -> Stomp DLY -> Stomp REV -> Output. That is 8 generic stomp slots plus the fixed amp stack. Rig Manager shows this as a horizontal strip of labeled slots. Empty slots show "Empty." Occupied slots show the effect type and name. The Kemper constrains: only one effect per slot, and you choose what goes in each slot from a categorized list.

**Key insight:** Kemper solves the "too many effects" problem by not having it. 8 generic slots + fixed amp block = 9 total positions. The constraint IS the UX solution. However, for our app, the constraint would mean reducing from 27 to roughly 8-12 user-choosable slots, which conflicts with the existing architecture.

---

## 2. Hardware Metaphor Analysis & Recommendation

### The Five Metaphors Ranked for Mobile + 27 Effects

**1. Boss GT-1000 / "Fixed Chain with Bypass" (BEST FIT)**
Every effect has a permanent home in the chain. Bypassed effects are visually reduced. Active effects are prominent. The user's mental model is: "All 27 effects are always there; I turn them on and off." This matches our existing architecture exactly. The challenge is making 27 items scannable -- which is a progressive disclosure problem, not an architecture problem.

**2. Helix / "Grid with Slots" (POOR FIT)**
Would require rethinking the 27-effect chain as a sparse grid. Too many empty cells or too many occupied cells. Does not match our fixed-chain model.

**3. Kemper / "Generic Slots + Browser" (ARCHITECTURE MISMATCH)**
Would require reducing 27 effects to 8-10 generic slots where the user assigns effects. Massive refactor.

**4. Fractal Axe-FX / "Flexible Routing Grid" (OVERKILL)**
Too complex for mobile. The Axe-FX's 4x12 routing grid with split/merge is powerful but overwhelming on a phone screen.

**5. Eventide H9 / "Single Effect Focus" (TOO LIMITED)**
One effect at a time. Irrelevant for a multi-effect chain.

### Recommendation: "Smart Rack" -- Fixed Chain with Adaptive Visibility

The GT-1000 model, enhanced with progressive disclosure. All 27 effects remain in the signal chain at their fixed positions. The UI adapts what it SHOWS based on bypass state, but the underlying model does not change. This preserves:
- The existing signal chain architecture (no refactoring needed)
- Drag-and-drop reordering (already implemented)
- Per-effect bypass via stomp switch (already implemented)
- Preset system that stores all 27 bypass states (already implemented)

---

## 3. Top 3 UI Patterns That Work for This Use Case

### Pattern 1: "Compact Bypass Strip" (RECOMMENDED PRIMARY)

**What it is:** Bypassed effects collapse to a MINIMAL strip -- much smaller than the current 52dp collapsed RackModule. Instead of showing the full header row with LED, icon, name, and stomp switch, a bypassed effect shows only a thin 28-32dp strip with: the category accent bar, the effect name in muted text, and a small bypass dot. Active effects show the full 52dp header (or expanded state).

**Why it works:** This is a variant of what Boss Tone Studio does but optimized for vertical mobile scroll. With 27 effects where typically 5-10 are active, the bypassed ones consume minimal vertical space (28dp x 17 = 476dp for 17 bypassed effects, versus 52dp x 17 = 884dp currently). The active effects get the full visual treatment. Total scroll height drops dramatically.

**Visual hierarchy:**
- Active + Expanded: Full header (52dp) + parameter panel = 200-400dp. One at a time (accordion).
- Active + Collapsed: Full header (52dp) with lit LED, bright name, stomp switch.
- Bypassed: Compact strip (28dp) with dimmed name, no LED, no stomp switch. Tap to toggle bypass. Long-press or swipe to expand and edit parameters while bypassed.

**Pros:**
- Zero architectural change needed
- Signal chain order always visible
- User can see ALL 27 effects without excessive scrolling
- Active effects are instantly distinguishable
- Maintains spatial memory (Overdrive is always in the same relative position)

**Cons:**
- Still shows 27 items (some users may want even fewer)
- The bypass strip must be carefully designed to not look like a broken/loading UI element

---

### Pattern 2: "Active Chain + Drawer" (SECONDARY OPTION)

**What it is:** The main rack view shows ONLY active (enabled) effects. A collapsible drawer at the bottom of the rack (above the MeteringStrip) shows "INACTIVE EFFECTS" grouped by category. The drawer header shows a count badge: "17 BYPASSED." Tapping the drawer header expands it to reveal the inactive effects as compact category-grouped lists.

**Why it works:** The main view becomes extremely clean -- if only 5 effects are active, you see 5 rack modules with no scrolling needed. The drawer is always accessible but does not clutter the primary view.

**Visual design:**
- Active chain: Full RackModules as they exist now, in signal chain order, with vertical flow lines connecting them.
- Signal flow connectors: Thin lines between active modules showing the signal path.
- Drawer header (always visible): A 40dp bar reading "17 INACTIVE -- TAP TO SHOW" with a chevron.
- Drawer content (expanded): LazyColumn of compact effect rows grouped under category headers (GAIN, AMP, MODULATION, TIME, UTILITY). Each row shows: effect name, category color dot, and a "+" or toggle to activate.

**Pros:**
- Dramatically cleaner primary view
- Inactive effects are organized by category (easier to find than scanning a long list)
- Users only see what they are using

**Cons:**
- Signal chain order is NOT preserved in the drawer (it is grouped by category instead). This breaks the mental model of "where does this effect sit in the chain?"
- Enabling an effect from the drawer must animate it into the correct position in the active chain, which can be disorienting if the user does not understand the fixed signal chain order.
- "Out of sight, out of mind" -- users may forget effects exist.

---

### Pattern 3: "Hybrid -- Compact Bypass + Category Filter Tabs"

**What it is:** Combines Pattern 1's compact bypass strips with a horizontal tab row above the rack that lets the user filter the view by category. Tabs: ALL | GAIN | AMP | MOD | TIME | UTIL. When "ALL" is selected, all 27 effects are shown with compact bypass strips for inactive ones. When a category tab is selected, only effects in that category are shown (both active and inactive in that category).

**Why it works:** Gives the user TWO ways to reduce visual noise: bypassed effects auto-compact, AND the user can filter to a specific category when they know what they want to adjust.

**Pros:**
- Maximum flexibility
- Category filtering is excellent for "I want to tweak my modulation effects" workflow
- ALL tab maintains the full chain view for spatial orientation
- Compact bypass strips reduce scroll height even in ALL view

**Cons:**
- Adds a tab row that consumes 40-48dp of vertical space
- Category filtering hides effects from view, which could confuse users about signal chain order
- More complex implementation (filter state management, transition animations)

---

## 4. Detailed Specification of the Recommended Approach

### Recommendation: Pattern 1 (Compact Bypass Strip) with selective elements from Pattern 3

The core idea: **Every effect in the chain is always visible, but bypassed effects collapse to a minimal 28dp strip that gets out of the way.** This preserves spatial memory, maintains signal chain visualization, requires minimal architecture changes, and dramatically reduces scroll distance.

### 4.1 The Three Visual States of a RackModule

#### State A: Active + Expanded (one at a time, accordion)
Exactly as currently implemented. Full 52dp header with lit LED, bright name, stomp switch, plus the expanded parameter panel below. Only one module can be in this state at a time.

**Dimensions:** 52dp header + variable content (typically 200-400dp)

#### State B: Active + Collapsed
Current collapsed state. Full 52dp header with:
- Category accent bar (left edge, full height)
- Lit LED indicator (green glow)
- Effect icon (Unicode symbol)
- Effect name (TextPrimary, FontWeight.Medium, 14sp, ALL CAPS, monospace letter spacing)
- Stomp switch (right side)
- Rack screws (decorative, left/right)

**Dimensions:** 52dp total height

#### State C: Bypassed (NEW -- compact strip)
A dramatically reduced visual treatment:

**Dimensions:** 28dp total height

**Layout (left to right):**
1. Category accent bar: 4dp wide, full height. Same color as active but at 40% alpha.
2. Bypass dot: 6dp circle, category's `ledBypass` color (dim). Positioned 16dp from left edge, vertically centered.
3. Effect name: `DesignSystem.TextMuted` color (#555560), 11sp, FontWeight.Normal, monospace letterSpacing 1.5sp. ALL CAPS. Positioned 28dp from left edge.
4. No stomp switch, no rack screws, no LED glow, no icon.

**Surface:** Same `surfaceTint` as active but at 25% alpha over `ModuleSurface`, creating a noticeably darker/more muted appearance than active modules.

**Border:** Single-line border using `PanelShadow` at 50% alpha (not the full double-line machined border).

**Bottom padding:** 1dp (not 4dp like active modules), so bypassed strips visually compress together.

**Interactions:**
- Single tap anywhere on the strip: Toggle bypass ON (effect becomes active, transitions to State B). This is the most common action a user wants.
- Long press: Expand to show parameter controls WITHOUT changing bypass state. This lets users pre-configure an effect before enabling it. The module temporarily becomes State A visually but with the LED showing bypass color. Release or tap the collapse area to return to State C.

**Animation (State C -> State B):**
- Height: `animateDpAsState` from 28dp to 52dp, spring damping 0.72, stiffness MediumLow (matches existing expand animation).
- LED: Fade in glow over 180ms with 40ms delay.
- Name: Color transition from TextMuted to TextPrimary over 150ms.
- Stomp switch: Fade in from 0 to 1 alpha over 200ms.
- The accent bar transitions from 40% to 100% alpha simultaneously.

**Animation (State B -> State C):**
- Reverse of above, but faster. Height spring damping 0.88, stiffness Medium.
- LED glow fades out over 120ms.
- Stomp switch fades out over 100ms.

### 4.2 Signal Chain Connector Lines

Between each module (regardless of bypass state), draw a thin vertical signal flow line connecting them. This reinforces the "these are in a chain" mental model even when most effects are bypassed compact strips.

**Specification:**
- Width: 1.5dp
- Color: `DesignSystem.ChromeHighlight` at 10% alpha for bypassed-to-bypassed connections, 25% alpha for connections involving at least one active effect.
- Height: Spans the gap between modules (the 1-4dp padding area).
- Position: Horizontally centered in the accent bar column (x = 2dp from left edge).
- Optional enhancement: Animated pulse dot traveling down the line at 1.5s cycle, only on connections between active effects. Color: category color of the upstream effect at 60% alpha.

### 4.3 Scroll Height Comparison

**Current state (all 27 effects as 52dp collapsed headers + 4dp bottom padding):**
27 x 56dp = 1512dp total. On a 5.8" phone (approximately 720dp viewport minus 56dp TopBar minus 52dp MeteringStrip = 612dp viewable), this requires significant scrolling.

**With compact bypass strips (assume 10 active, 17 bypassed):**
- Active: 10 x 56dp = 560dp
- Bypassed: 17 x 29dp = 493dp
- Total: 1053dp (30% reduction)
- If only 5 active: 5 x 56dp + 22 x 29dp = 280 + 638 = 918dp (39% reduction)

**With one expanded module (the common case):**
- 1 expanded: 52dp header + ~250dp content = 302dp
- 4 active collapsed: 4 x 56dp = 224dp
- 22 bypassed: 22 x 29dp = 638dp
- Total: 1164dp

The user can see roughly 60% of the chain without scrolling in the common case. The expanded module and its immediate neighbors are almost always visible without scrolling.

### 4.4 "Quick Enable" Zone

To address the live performance need for rapid effect toggling, add a horizontal strip above the rack (below the A/B bar or error banner, consuming only 36dp):

**Quick Enable Strip:**
- A horizontally scrollable (LazyRow) strip of small category-colored circles (24dp diameter), one per effect.
- Active effects: Filled circle with category color, bright.
- Bypassed effects: Ring outline only, dimmed.
- Tap to toggle bypass.
- Long-press to scroll the rack to that effect's position.

**Layout:**
```
[ NG ] [ CP ] [ WA ] [ BO ] [ OD ] [ AM ] [ CA ] [ EQ ] [ CH ] [ VI ] ...
```

Each circle shows a 2-letter abbreviation (NG=NoiseGate, CP=Compressor, etc.) in 8sp monospace.

**Why this matters:** It gives a "pedalboard overview" at the top of the screen -- the user can see all 27 effects' bypass states at a glance, and toggle any effect with a single tap, WITHOUT scrolling the rack. This is the mobile equivalent of looking down at a physical pedalboard.

**Dimensions:** 36dp height (24dp circles + 6dp top padding + 6dp bottom padding). Only 2.5% of viewport height.

**Visibility:** Always visible. Does not scroll with the rack content.

### 4.5 Component Hierarchy (Compose)

```
PedalboardScreen
  Box (root)
    TolexNoiseOverlay
    Column
      TopBar (56dp)
      EngineErrorBanner (conditional)
      ABComparisonBar (conditional)
      QuickEnableStrip (36dp, NEW)                    // <-- NEW
      Box (weight 1f)
        LazyColumn (with rack rails)
          items(orderedStates) { state ->
            ReorderableItem {
              if (state.isEnabled) {                   // <-- NEW branching
                RackModule(                            //     existing component
                  compact = false,
                  ...
                )
              } else {
                BypassedStrip(                         // <-- NEW component
                  effectState = state,
                  onToggleEnabled = ...,
                  onLongPressExpand = ...,
                )
              }
            }
          }
        ScrollShadows
      MeteringStrip (52dp)
    Overlays (Preset, Tuner, Looper, etc.)
```

Alternative (simpler, recommended): Modify `RackModule` to accept a `compact: Boolean` parameter rather than creating a separate `BypassedStrip` composable. When `compact = true AND isEnabled = false`, render the 28dp bypass strip. This keeps the component unified and avoids duplicating the accent bar / category color logic.

---

## 5. The "Add Effect" (Enable Effect) Interaction Flow

Since our model is "all 27 effects are permanently in the chain, toggle bypass," the "add effect" workflow is actually "enable a bypassed effect." Three paths to this action:

### Path A: Direct Tap on Bypass Strip (Primary)
1. User sees the compact bypass strip for "CHORUS" in the rack.
2. User taps the strip.
3. Strip animates to full collapsed header (State C -> State B) over ~300ms.
4. LED lights up with category glow.
5. Stomp switch appears.
6. Audio engine receives `setEffectEnabled(index, true)`.
7. Effect is now active in the signal chain at its fixed position.

**Total interaction time:** Single tap. Under 500ms including animation. No confirmation dialog.

### Path B: Quick Enable Strip Tap (Fast, No Scroll)
1. User sees the Quick Enable Strip at the top showing all 27 effect dots.
2. User taps the dimmed "CH" (Chorus) circle.
3. Circle fills with the Modulation category color.
4. Simultaneously, in the rack below, the Chorus bypass strip transitions to active collapsed.
5. Rack auto-scrolls to show the newly activated effect briefly (300ms pause), then returns to current position. Or: no auto-scroll, just update in place.

**Total interaction time:** Single tap. The user does not need to find the effect in the rack first.

### Path C: Long-Press to Preview (Power User)
1. User long-presses the "CHORUS" bypass strip.
2. Haptic tick feedback.
3. The strip expands to show the full parameter panel (State A layout) but with the LED showing bypass color and a banner reading "PREVIEW -- TAP STOMP TO ENABLE."
4. User adjusts Chorus parameters (rate, depth, mix) while the effect remains bypassed.
5. When satisfied, user taps the stomp switch -- effect enables with the pre-configured settings.
6. Or: user taps outside the module / scrolls away -- module collapses back to bypass strip, settings are retained for next time.

**Why this matters:** Musicians often want to dial in an effect BEFORE introducing it to the live sound. "Let me set the chorus rate to match the song tempo, THEN kick it in." This is how real pedalboards work -- you adjust the knobs and then step on the switch.

### The Effect Appears in Signal-Chain Order (Not at the End)

This is critical. Because all 27 effects have fixed positions in the chain (reorderable via drag, but persistent), enabling an effect does not "add" it at the end. It activates it AT ITS EXISTING POSITION. The Chorus was always between Vibrato and Phaser; enabling it fills in that gap. The user sees it expand in place within the rack.

If the user has reordered effects via drag-and-drop, the reordered positions are preserved. The Chorus appears wherever the user last placed it in the chain.

### How Users Know What Effects Are Available

Because ALL 27 effects are always visible in the rack (just compact when bypassed), there is no "hidden" effect to discover. The user scrolls through the rack and sees every effect, active or bypassed. The Quick Enable Strip provides a complete overview without scrolling.

For users unfamiliar with the effects, the bypass strip shows the effect name. Long-pressing expands it to reveal the parameter names, which communicates what the effect does.

---

## 6. Anti-Patterns to Explicitly Avoid

### 6.1 The "Junk Drawer" (CRITICAL)
**Anti-pattern:** Hiding inactive effects in a separate screen, tab, or deeply nested menu. Once effects are "out of sight, out of mind," users forget they exist. Worse, re-enabling requires navigating away from the main view, breaking flow.

**Our mitigation:** All effects are always in the main rack view. Bypassed effects are compact but visible.

### 6.2 The "Modal Maze" (CRITICAL)
**Anti-pattern:** Requiring a modal dialog to enable/disable effects. "Are you sure you want to enable Chorus? [Yes] [No]." This is common in enterprise software but deadly in audio tools. A guitarist stepping on a pedal does not get a confirmation dialog.

**Our mitigation:** Single-tap toggle with immediate audio response. Zero confirmation dialogs for bypass toggle.

### 6.3 The "Identical Gray Blocks" (HIGH SEVERITY)
**Anti-pattern:** All bypassed effects looking identical -- same color, same shape, same size. The user cannot distinguish the Noise Gate bypass strip from the Reverb bypass strip without reading the text.

**Our mitigation:** Category accent bars maintain color coding even in bypass state (at 40% alpha). The strip position in the chain provides spatial context. The Quick Enable Strip uses category colors for each dot.

### 6.4 The "Orphaned Parameters" (HIGH SEVERITY)
**Anti-pattern:** Enabling an effect resets its parameters to defaults, losing any previous configuration.

**Our mitigation:** Parameters are always preserved regardless of bypass state (this is already the case in our signal chain architecture). Enabling an effect restores it with exactly the settings it had when bypassed.

### 6.5 The "Invisible Signal Flow" (MEDIUM)
**Anti-pattern:** Showing active effects without any indication of their position in the chain relative to each other. "Reverb and Delay are both active, but which comes first?" Without connectors or ordering cues, the user must remember.

**Our mitigation:** Signal chain connector lines between modules. Fixed ordering (or user-reordered, but persistent).

### 6.6 The "Scroll Desert" (MEDIUM)
**Anti-pattern:** Showing all effects at full size regardless of state, creating an enormous scrollable area where the user loses their place. This is the CURRENT state of the app.

**Our mitigation:** Compact bypass strips reduce scroll distance by 30-40%.

### 6.7 The "Surprise Reorder" (MEDIUM)
**Anti-pattern:** Enabling an effect causes it to animate to an unexpected position, or the active effects section shifts/reorders when toggling bypass.

**Our mitigation:** Effects never change position when toggled. They expand/contract in place. The rack order is always the signal chain order.

### 6.8 The "Touch Target Trap" (MOBILE-SPECIFIC)
**Anti-pattern:** Making the bypassed effect strip so compact that it becomes untappable. A 20dp strip with 1dp spacing between strips is a touch nightmare.

**Our mitigation:** The bypass strip is 28dp (above the 24dp absolute minimum for touch on Android). But for actual touch target purposes, the tappable area extends to include the padding, giving an effective 29dp hit zone. If user testing reveals this is too small, increase to 32dp with minimal scroll impact.

### 6.9 The "Latent Feature Bloat" (ARCHITECTURAL)
**Anti-pattern:** Treating the bypass strip as a micro-version of the full module, adding so many features (mini-meters, tiny parameter previews, status badges) that it becomes almost as complex as the full module.

**Our mitigation:** The bypass strip has EXACTLY three visual elements: accent bar, bypass dot, effect name. Nothing else. It is deliberately spartan.

### 6.10 The "Mode Confusion" (SUBTLE BUT DANGEROUS)
**Anti-pattern:** Having too many visual states that the user cannot quickly determine what "state" an effect is in. Is it bypassed? Is it disabled? Is it in preview mode? Is it loading?

**Our mitigation:** Exactly THREE states with clear visual differentiation:
- State A (expanded): Full panel, lit LED, visible parameters = "I am editing this right now"
- State B (active collapsed): Full header, lit LED, stomp switch = "I am in the chain and processing audio"
- State C (bypassed): Compact strip, no LED, muted text = "I am off"

---

## 7. Live Performance Mode Considerations

### 7.1 The Quick Enable Strip IS the Live Performance Solution

Rather than building a separate "Live Mode" screen (as BIAS FX and AmpliTube do), the Quick Enable Strip provides the essential live functionality within the main view:

- All 27 effect bypass states visible at a glance (one horizontal row)
- Single-tap toggle on any effect
- No scrolling, no navigation, no mode switching
- Category colors provide instant visual identification
- Tap targets are 24dp circles -- large enough for stage use when combined with the spacing

### 7.2 When a Full Live Mode IS Needed (Future Sprint)

For professional live use, a dedicated Live Performance Mode overlay would show:
- 6-8 large stomp switch buttons (matching configurable effect assignments)
- Current preset name (LARGE, readable from 3+ feet)
- Tuner display (always visible, not an overlay)
- Tempo/BPM display
- Input/output meters

This is a separate feature (Sprint N+2 or later) and does NOT need to be solved by the progressive disclosure system. The Quick Enable Strip bridges the gap.

### 7.3 Preset-Driven Live Performance

The most important live performance feature is PRESET SWITCHING, not individual effect toggling. A guitarist who needs to go from "Clean Verse" to "Heavy Chorus" changes presets, not individual effects. The existing preset system (with the TopBar preset selector and full-screen PresetBrowserOverlay) handles this. The Quick Enable Strip handles the secondary case: "I am in my Heavy preset and need to kick the Delay on for the bridge."

### 7.4 Setlist Mode (Already Exists)

The codebase already has `SetlistManager.kt` and `Setlist.kt`, indicating setlist-based preset ordering is implemented or in progress. This is the correct solution for structured live performance (ordered list of songs/presets, advance with a single tap or MIDI footswitch).

---

## Summary Matrix

| Pattern | Scroll Reduction | Spatial Memory | Implementation Effort | Live Perf | Architect Fit |
|---------|-----------------|----------------|----------------------|-----------|---------------|
| Compact Bypass Strip (P1) | 30-40% | Excellent | Low (modify RackModule) | Good with QuickStrip | Perfect |
| Active Chain + Drawer (P2) | 60-80% | Poor (drawer breaks flow) | Medium | Poor | Requires category grouping |
| Hybrid + Category Tabs (P3) | 30-40% + filtering | Moderate | High | Moderate | Good but complex |

### Final Recommendation

**Implement Pattern 1 (Compact Bypass Strip) plus the Quick Enable Strip.** This is the most architecturally compatible, the lowest risk, and the best balance of progressive disclosure and spatial memory preservation. It requires modifying one existing component (RackModule) and adding one new component (QuickEnableStrip), with no changes to the signal chain architecture, preset system, or effect reordering logic.

Implementation priority:
1. RackModule bypass strip rendering (State C) -- highest impact, lowest effort
2. Signal chain connector lines between modules
3. QuickEnableStrip component
4. Long-press to preview workflow
5. Transition animations and polish
