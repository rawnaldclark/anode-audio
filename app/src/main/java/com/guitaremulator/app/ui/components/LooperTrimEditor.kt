package com.guitaremulator.app.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.LooperDelegate
import kotlin.math.abs
import kotlin.math.roundToInt

// -- Constants ----------------------------------------------------------------

/** Minimum gap in samples between trim start and trim end handles. */
private const val MIN_HANDLE_GAP_SAMPLES = 480

/** Handle hit-test radius in dp (48dp meets Material Design minimum touch target). */
private val HANDLE_HIT_RADIUS = 48.dp

/** Handle visual width in dp (wide enough for finger targeting). */
private val HANDLE_WIDTH = 16.dp

/** Maximum zoom level (32x provides sub-ms precision at typical configs). */
private const val MAX_ZOOM_LEVEL = 32f

/** Minimum zoom level (1x = entire loop visible). */
private const val MIN_ZOOM_LEVEL = 1f

/** Discrete zoom steps for +/- buttons. */
private val ZOOM_STEPS = floatArrayOf(1f, 2f, 4f, 8f, 16f, 32f)

/** Overview minimap height. */
private val OVERVIEW_HEIGHT = 40.dp

/** Edge zone in dp for auto-scroll during handle drag when zoomed. */
private val AUTO_SCROLL_EDGE_ZONE = 32.dp

/**
 * Full-screen loop trim / crop editor overlay.
 *
 * Renders on top of the pedalboard rack following the same pattern as
 * [LooperOverlay]. The overlay animates in from the top with a fade and
 * slide transition, provides a semi-transparent backdrop, and intercepts
 * the system back gesture.
 *
 * Layout:
 * 1. Header row: close [X], "TRIM LOOP" title, undo crop button.
 * 2. Overview minimap (visible when zoomed, shows full loop + viewport rect).
 * 3. Detail waveform canvas with draggable start/end trim handles.
 * 4. Zoom controls row (zoom out, zoom level text, zoom in).
 * 5. Time readout row (start, end, trimmed duration).
 * 6. Snap mode selector chips (Free / Zero-X / Beat).
 * 7. Full-width APPLY TRIM button.
 *
 * @param visible             Whether the overlay is shown.
 * @param waveformData        Packed min/max pairs [min0,max0,min1,max1,...] or null.
 * @param trimStart           Current trim start position in samples.
 * @param trimEnd             Current trim end position in samples.
 * @param loopLength          Total loop length in samples.
 * @param playbackPosition    Current playback position in samples.
 * @param sampleRate          Engine sample rate (for time display).
 * @param isCropUndoAvailable Whether a previous crop can be undone.
 * @param snapMode            Current snap mode for trim handles.
 * @param onTrimStartChange   Callback when the start handle is moved.
 * @param onTrimEndChange     Callback when the end handle is moved.
 * @param onSnapModeChange    Callback when snap mode chip is selected.
 * @param onCommit            Callback when APPLY TRIM is pressed.
 * @param onUndoCrop          Callback when undo crop is pressed.
 * @param onClose             Close overlay callback.
 * @param modifier            Layout modifier.
 */
@Composable
fun LooperTrimEditor(
    visible: Boolean,
    waveformData: FloatArray?,
    trimStart: Int,
    trimEnd: Int,
    loopLength: Int,
    playbackPosition: Int,
    sampleRate: Int,
    isCropUndoAvailable: Boolean,
    snapMode: LooperDelegate.SnapMode,
    onTrimStartChange: (Int) -> Unit,
    onTrimEndChange: (Int) -> Unit,
    onSnapModeChange: (LooperDelegate.SnapMode) -> Unit,
    onCommit: () -> Unit,
    onUndoCrop: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    // Intercept the system back gesture/button when the overlay is showing
    BackHandler(enabled = visible) { onClose() }

    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(250)) { -it / 3 },
        exit = fadeOut(tween(150)) + slideOutVertically(tween(200)) { -it / 3 },
        modifier = modifier
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            // 1. Semi-transparent backdrop
            TrimEditorBackdrop(onDismiss = onClose)

            // 2. Editor panel
            TrimEditorPanel(
                waveformData = waveformData,
                trimStart = trimStart,
                trimEnd = trimEnd,
                loopLength = loopLength,
                playbackPosition = playbackPosition,
                sampleRate = sampleRate,
                isCropUndoAvailable = isCropUndoAvailable,
                snapMode = snapMode,
                onTrimStartChange = onTrimStartChange,
                onTrimEndChange = onTrimEndChange,
                onSnapModeChange = onSnapModeChange,
                onCommit = onCommit,
                onUndoCrop = onUndoCrop,
                onClose = onClose,
                modifier = Modifier.align(Alignment.TopCenter)
            )
        }
    }
}

// -- Private sub-components ---------------------------------------------------

/**
 * Full-screen semi-transparent backdrop that dismisses the overlay on tap.
 *
 * Mirrors the LooperOverlay backdrop pattern: uses a [MutableInteractionSource]
 * with no indication to suppress ripple effects.
 *
 * @param onDismiss Callback when the backdrop is tapped.
 */
@Composable
private fun TrimEditorBackdrop(onDismiss: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(DesignSystem.Background.copy(alpha = 0.85f))
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onDismiss
            )
    )
}

/**
 * Main trim editor panel anchored to the top of the screen.
 *
 * A column containing the header, overview minimap, waveform canvas with
 * drag handles, zoom controls, time readout, snap mode chips, and the
 * apply button. Drawn with rounded bottom corners and a SafeGreen bottom
 * border line.
 */
@Composable
private fun TrimEditorPanel(
    waveformData: FloatArray?,
    trimStart: Int,
    trimEnd: Int,
    loopLength: Int,
    playbackPosition: Int,
    sampleRate: Int,
    isCropUndoAvailable: Boolean,
    snapMode: LooperDelegate.SnapMode,
    onTrimStartChange: (Int) -> Unit,
    onTrimEndChange: (Int) -> Unit,
    onSnapModeChange: (LooperDelegate.SnapMode) -> Unit,
    onCommit: () -> Unit,
    onUndoCrop: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    // -- Zoom state -----------------------------------------------------------
    var zoomLevel by remember { mutableFloatStateOf(1f) }
    var viewportCenter by remember { mutableFloatStateOf(0.5f) }

    // Derived viewport bounds (normalized 0.0 to 1.0)
    val viewportWidth = (1f / zoomLevel).coerceIn(0f, 1f)
    val viewportStart = (viewportCenter - viewportWidth / 2f)
        .coerceIn(0f, (1f - viewportWidth).coerceAtLeast(0f))
    val viewportEnd = (viewportStart + viewportWidth).coerceAtMost(1f)

    // Recalculate center from clamped start (keeps center consistent after clamping)
    val effectiveCenter = viewportStart + viewportWidth / 2f

    // Helper: clamp viewport center given current zoom
    fun clampCenter(center: Float): Float {
        val halfWidth = viewportWidth / 2f
        return center.coerceIn(halfWidth, (1f - halfWidth).coerceAtLeast(halfWidth))
    }

    // Helper: advance to next zoom step (for +/- buttons)
    fun zoomIn() {
        val nextStep = ZOOM_STEPS.firstOrNull { it > zoomLevel } ?: MAX_ZOOM_LEVEL
        zoomLevel = nextStep.coerceAtMost(MAX_ZOOM_LEVEL)
        viewportCenter = clampCenter(effectiveCenter)
    }

    fun zoomOut() {
        val prevStep = ZOOM_STEPS.lastOrNull { it < zoomLevel } ?: MIN_ZOOM_LEVEL
        zoomLevel = prevStep.coerceAtLeast(MIN_ZOOM_LEVEL)
        viewportCenter = clampCenter(effectiveCenter)
    }

    fun resetZoom() {
        zoomLevel = 1f
        viewportCenter = 0.5f
    }

    // When loopLength changes (e.g., undo crop), reset zoom
    val previousLoopLength = remember { mutableIntStateOf(loopLength) }
    if (loopLength != previousLoopLength.intValue) {
        previousLoopLength.intValue = loopLength
        resetZoom()
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(bottomStart = 16.dp, bottomEnd = 16.dp))
            .background(DesignSystem.ModuleSurface.copy(alpha = 0.97f))
            .drawBehind {
                val w = size.width
                val h = size.height

                // -- Amp-panel double-line border (machined panel edge) --
                val outerStroke = DesignSystem.PanelBorderWidth.toPx()
                drawRect(
                    color = DesignSystem.PanelShadow,
                    topLeft = Offset(0f, 0f),
                    size = size,
                    style = androidx.compose.ui.graphics.drawscope.Stroke(outerStroke)
                )
                val innerInset = outerStroke
                val innerStroke = 1.dp.toPx()
                drawRect(
                    color = DesignSystem.PanelHighlight,
                    topLeft = Offset(innerInset, innerInset),
                    size = Size(w - innerInset * 2f, h - innerInset * 2f),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(innerStroke)
                )

                // -- Bottom border: GoldPiping at 60% + PanelShadow shadow --
                val goldStroke = 2.dp.toPx()
                val shadowStroke = 1.dp.toPx()
                drawLine(
                    color = DesignSystem.PanelShadow,
                    start = Offset(0f, h - shadowStroke / 2f),
                    end = Offset(w, h - shadowStroke / 2f),
                    strokeWidth = shadowStroke
                )
                drawLine(
                    color = DesignSystem.GoldPiping.copy(alpha = 0.6f),
                    start = Offset(0f, h - shadowStroke - goldStroke / 2f),
                    end = Offset(w, h - shadowStroke - goldStroke / 2f),
                    strokeWidth = goldStroke
                )
            }
            .padding(bottom = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // -- 1. Header bar -----------------------------------------------
        TrimEditorHeader(
            isCropUndoAvailable = isCropUndoAvailable,
            onUndoCrop = onUndoCrop,
            onClose = onClose
        )

        Spacer(modifier = Modifier.height(8.dp))

        // -- 2. Overview minimap (visible when zoomed) --------------------
        if (zoomLevel > 1f) {
            OverviewMinimap(
                waveformData = waveformData,
                loopLength = loopLength,
                viewportStart = viewportStart,
                viewportEnd = viewportEnd,
                trimStart = trimStart,
                trimEnd = trimEnd,
                onViewportCenterChange = { newCenter ->
                    viewportCenter = clampCenter(newCenter)
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(OVERVIEW_HEIGHT)
                    .padding(horizontal = 24.dp)
            )

            Spacer(modifier = Modifier.height(4.dp))
        }

        // -- 3. Detail waveform canvas with drag handles ------------------
        TrimWaveformCanvas(
            waveformData = waveformData,
            trimStart = trimStart,
            trimEnd = trimEnd,
            loopLength = loopLength,
            playbackPosition = playbackPosition,
            viewportStart = viewportStart,
            viewportEnd = viewportEnd,
            zoomLevel = zoomLevel,
            onTrimStartChange = onTrimStartChange,
            onTrimEndChange = onTrimEndChange,
            onViewportCenterChange = { newCenter ->
                viewportCenter = clampCenter(newCenter)
            },
            modifier = Modifier
                .fillMaxWidth()
                .height(140.dp)
                .padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(8.dp))

        // -- 4. Zoom controls row -----------------------------------------
        ZoomControlsRow(
            zoomLevel = zoomLevel,
            onZoomIn = { zoomIn() },
            onZoomOut = { zoomOut() },
            onResetZoom = { resetZoom() },
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(8.dp))

        // -- 5. Time display row ------------------------------------------
        TrimTimeDisplay(
            trimStart = trimStart,
            trimEnd = trimEnd,
            loopLength = loopLength,
            sampleRate = sampleRate,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(12.dp))

        // -- 6. Snap mode selector ----------------------------------------
        SnapModeSelector(
            currentMode = snapMode,
            onModeChange = onSnapModeChange,
            modifier = Modifier.padding(horizontal = 24.dp)
        )

        Spacer(modifier = Modifier.height(16.dp))

        // -- 7. Apply trim button (primary metallic gradient) ──────────
        val applyEnabled = loopLength > 0 && trimEnd > trimStart
        Button(
            onClick = onCommit,
            enabled = applyEnabled,
            colors = ButtonDefaults.buttonColors(
                containerColor = Color.Transparent,
                contentColor = Color.White,
                disabledContainerColor = DesignSystem.ModuleBorder,
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
                .padding(horizontal = 24.dp)
                .then(
                    if (applyEnabled) {
                        Modifier
                            .background(
                                brush = Brush.verticalGradient(
                                    colors = listOf(
                                        DesignSystem.SafeGreen,
                                        DesignSystem.SafeGreen.copy(alpha = 0.8f)
                                    )
                                ),
                                shape = RoundedCornerShape(12.dp)
                            )
                            .drawBehind {
                                // Chrome highlight at top edge
                                val highlightStroke = 1.dp.toPx()
                                drawLine(
                                    color = DesignSystem.ChromeHighlight.copy(alpha = 0.4f),
                                    start = Offset(8.dp.toPx(), highlightStroke / 2f),
                                    end = Offset(size.width - 8.dp.toPx(), highlightStroke / 2f),
                                    strokeWidth = highlightStroke
                                )
                            }
                    } else {
                        Modifier
                    }
                )
        ) {
            Text(
                text = "APPLY TRIM",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 2.sp
            )
        }
    }
}

/**
 * Header row with close button, "TRIM LOOP" title, and undo crop button.
 *
 * The undo button is only visible when [isCropUndoAvailable] is true;
 * otherwise a same-sized spacer maintains layout symmetry.
 *
 * @param isCropUndoAvailable Whether the undo button should be shown.
 * @param onUndoCrop          Callback when the undo button is pressed.
 * @param onClose             Callback when the close button is pressed.
 */
@Composable
private fun TrimEditorHeader(
    isCropUndoAvailable: Boolean,
    onUndoCrop: () -> Unit,
    onClose: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        // Close button on the left
        IconButton(
            onClick = onClose,
            modifier = Modifier.size(36.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close trim editor",
                tint = DesignSystem.TextSecondary,
                modifier = Modifier.size(20.dp)
            )
        }

        // Title (silk-screen style)
        Text(
            text = "TRIM LOOP",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )

        // Undo crop button (or spacer for symmetry)
        if (isCropUndoAvailable) {
            IconButton(
                onClick = onUndoCrop,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Refresh,
                    contentDescription = "Undo crop",
                    tint = DesignSystem.WarningAmber,
                    modifier = Modifier.size(20.dp)
                )
            }
        } else {
            Spacer(modifier = Modifier.size(36.dp))
        }
    }
}

// -- Overview Minimap ---------------------------------------------------------

/**
 * Compact overview minimap showing the full loop waveform with a viewport
 * rectangle indicating the currently visible region in the detail view.
 *
 * The minimap allows the user to tap to center the viewport or drag the
 * viewport rectangle to pan. Trim handle positions are shown as thin
 * colored vertical lines.
 *
 * @param waveformData          Full loop waveform (packed min/max pairs).
 * @param loopLength            Total loop length in samples.
 * @param viewportStart         Normalized viewport start (0.0 to 1.0).
 * @param viewportEnd           Normalized viewport end (0.0 to 1.0).
 * @param trimStart             Trim start in samples.
 * @param trimEnd               Trim end in samples.
 * @param onViewportCenterChange Callback with new normalized center position.
 * @param modifier              Layout modifier.
 */
@Composable
private fun OverviewMinimap(
    waveformData: FloatArray?,
    loopLength: Int,
    viewportStart: Float,
    viewportEnd: Float,
    trimStart: Int,
    trimEnd: Int,
    onViewportCenterChange: (Float) -> Unit,
    modifier: Modifier = Modifier
) {
    // Use rememberUpdatedState to avoid restarting pointerInput coroutine
    // when viewport changes during detail view interactions.
    val currentVpStart by rememberUpdatedState(viewportStart)
    val currentVpEnd by rememberUpdatedState(viewportEnd)
    val currentLoopLength by rememberUpdatedState(loopLength)

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(4.dp))
            .background(DesignSystem.Background)
            .pointerInput(Unit) {
                if (currentLoopLength <= 0) return@pointerInput
                val canvasWidth = size.width.toFloat()

                awaitEachGesture {
                    val down = awaitFirstDown(requireUnconsumed = false)
                    down.consume()

                    val tapNormalized = down.position.x / canvasWidth

                    // Check if touch is inside viewport rectangle
                    val isInsideViewport =
                        tapNormalized in currentVpStart..currentVpEnd

                    if (isInsideViewport) {
                        // Drag the viewport: compute grab offset from viewport center
                        val vpCenterNorm = (currentVpStart + currentVpEnd) / 2f
                        val grabOffset = tapNormalized - vpCenterNorm

                        do {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull() ?: break
                            if (change.pressed) {
                                change.consume()
                                val dragNorm = change.position.x / canvasWidth
                                val newCenter = dragNorm - grabOffset
                                onViewportCenterChange(newCenter)
                            } else {
                                break
                            }
                        } while (true)
                    } else {
                        // Tap outside viewport -> center viewport on tap position
                        onViewportCenterChange(tapNormalized)
                    }
                }
            }
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            if (waveformData == null || waveformData.isEmpty() || loopLength <= 0) {
                return@Canvas
            }

            val canvasWidth = size.width
            val canvasHeight = size.height
            val centerY = canvasHeight / 2f
            val pairCount = waveformData.size / 2
            val barWidth = canvasWidth / pairCount.toFloat()

            // Pre-compute trim boundaries in pixels
            val trimStartPx =
                trimStart.toFloat() / loopLength.toFloat() * canvasWidth
            val trimEndPx =
                trimEnd.toFloat() / loopLength.toFloat() * canvasWidth

            // -- Draw waveform bars (compact, muted) --
            for (i in 0 until pairCount) {
                val minVal = waveformData[i * 2]
                val maxVal = waveformData[i * 2 + 1]

                val barX = i.toFloat() / pairCount.toFloat() * canvasWidth
                val barCenterX = barX + barWidth / 2f

                val isInTrimRange =
                    barCenterX >= trimStartPx && barCenterX <= trimEndPx

                val barColor = if (isInTrimRange) {
                    DesignSystem.SafeGreen.copy(alpha = 0.4f)
                } else {
                    DesignSystem.TextMuted.copy(alpha = 0.4f)
                }

                val topY =
                    centerY - (maxVal.coerceIn(-1f, 1f) * centerY * 0.75f)
                val bottomY =
                    centerY - (minVal.coerceIn(-1f, 1f) * centerY * 0.75f)
                val minBarHeight = 1f
                val adjTopY: Float
                val adjBottomY: Float
                if (bottomY - topY < minBarHeight) {
                    adjTopY = centerY - minBarHeight / 2f
                    adjBottomY = centerY + minBarHeight / 2f
                } else {
                    adjTopY = topY
                    adjBottomY = bottomY
                }

                drawRect(
                    color = barColor,
                    topLeft = Offset(barX, adjTopY),
                    size = Size(
                        width = (barWidth - 0.5f).coerceAtLeast(1f),
                        height = adjBottomY - adjTopY
                    )
                )
            }

            // -- Dark overlay outside viewport --
            val vpLeftPx = viewportStart * canvasWidth
            val vpRightPx = viewportEnd * canvasWidth
            val overlayColor = DesignSystem.Background.copy(alpha = 0.6f)

            if (vpLeftPx > 0f) {
                drawRect(
                    color = overlayColor,
                    topLeft = Offset(0f, 0f),
                    size = Size(vpLeftPx, canvasHeight)
                )
            }
            if (vpRightPx < canvasWidth) {
                drawRect(
                    color = overlayColor,
                    topLeft = Offset(vpRightPx, 0f),
                    size = Size(canvasWidth - vpRightPx, canvasHeight)
                )
            }

            // -- Viewport rectangle outline --
            val vpWidth = (vpRightPx - vpLeftPx).coerceAtLeast(2f)
            drawRoundRect(
                color = DesignSystem.SafeGreen.copy(alpha = 0.05f),
                topLeft = Offset(vpLeftPx, 0f),
                size = Size(vpWidth, canvasHeight),
                cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx())
            )
            drawRoundRect(
                color = DesignSystem.SafeGreen.copy(alpha = 0.6f),
                topLeft = Offset(vpLeftPx, 0f),
                size = Size(vpWidth, canvasHeight),
                cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx()),
                style = Stroke(width = 1.5f.dp.toPx())
            )

            // -- Trim handle markers (thin lines) --
            drawLine(
                color = DesignSystem.SafeGreen.copy(alpha = 0.5f),
                start = Offset(trimStartPx, 0f),
                end = Offset(trimStartPx, canvasHeight),
                strokeWidth = 1.dp.toPx()
            )
            drawLine(
                color = DesignSystem.WarningAmber.copy(alpha = 0.5f),
                start = Offset(trimEndPx, 0f),
                end = Offset(trimEndPx, canvasHeight),
                strokeWidth = 1.dp.toPx()
            )
        }
    }
}

// -- Zoom Controls Row --------------------------------------------------------

/**
 * Row of zoom controls: zoom out [-], zoom level display, zoom in [+].
 *
 * The zoom level text is tappable to reset to 1x when zoomed.
 *
 * @param zoomLevel   Current zoom multiplier.
 * @param onZoomIn    Callback to advance to next zoom step.
 * @param onZoomOut   Callback to go to previous zoom step.
 * @param onResetZoom Callback to reset zoom to 1x.
 * @param modifier    Layout modifier.
 */
@Composable
private fun ZoomControlsRow(
    zoomLevel: Float,
    onZoomIn: () -> Unit,
    onZoomOut: () -> Unit,
    onResetZoom: () -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Zoom out button (secondary metallic style)
        Button(
            onClick = onZoomOut,
            enabled = zoomLevel > MIN_ZOOM_LEVEL,
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ControlPanelSurface,
                contentColor = DesignSystem.CreamWhite,
                disabledContainerColor = DesignSystem.ControlPanelSurface.copy(alpha = 0.4f),
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(8.dp),
            modifier = Modifier
                .size(width = 44.dp, height = 32.dp)
                .drawBehind {
                    drawRect(
                        color = DesignSystem.PanelHighlight,
                        size = size,
                        style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx())
                    )
                }
        ) {
            Text(
                text = "-",
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center
            )
        }

        Spacer(modifier = Modifier.width(12.dp))

        // Zoom level display (tappable to reset)
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = if (zoomLevel > 1f) {
                Modifier.clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = onResetZoom
                )
            } else {
                Modifier
            }
        ) {
            Text(
                text = "%.0fx".format(zoomLevel),
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                color = if (zoomLevel > 1f) {
                    DesignSystem.SafeGreen
                } else {
                    DesignSystem.TextMuted
                },
                textAlign = TextAlign.Center
            )
            if (zoomLevel > 1f) {
                Text(
                    text = "TAP TO RESET",
                    fontSize = 8.sp,
                    letterSpacing = 1.sp,
                    color = DesignSystem.TextMuted,
                    textAlign = TextAlign.Center
                )
            }
        }

        Spacer(modifier = Modifier.width(12.dp))

        // Zoom in button (secondary metallic style)
        Button(
            onClick = onZoomIn,
            enabled = zoomLevel < MAX_ZOOM_LEVEL,
            colors = ButtonDefaults.buttonColors(
                containerColor = DesignSystem.ControlPanelSurface,
                contentColor = DesignSystem.CreamWhite,
                disabledContainerColor = DesignSystem.ControlPanelSurface.copy(alpha = 0.4f),
                disabledContentColor = DesignSystem.TextMuted
            ),
            shape = RoundedCornerShape(8.dp),
            modifier = Modifier
                .size(width = 44.dp, height = 32.dp)
                .drawBehind {
                    drawRect(
                        color = DesignSystem.PanelHighlight,
                        size = size,
                        style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx())
                    )
                }
        ) {
            Text(
                text = "+",
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center
            )
        }
    }
}

// -- Detail Waveform Canvas ---------------------------------------------------

/**
 * Canvas-based waveform display with draggable trim handles and zoom support.
 *
 * Draws vertical bars from [waveformData] (packed as [min0,max0,min1,max1,...]).
 * When zoomed (viewportStart/End != 0/1), only the portion of the waveform
 * within the viewport is rendered, stretched to fill the canvas width.
 *
 * Bars within the selected trim range are drawn in [DesignSystem.SafeGreen];
 * bars outside the range are dimmed. Semi-transparent overlay rectangles
 * darken the excluded regions. A thin playback cursor line is drawn at the
 * current playback position.
 *
 * When zoomed and a single finger touches outside any handle's hit radius,
 * horizontal drag pans the viewport. When a handle is grabbed, it drags
 * as before with auto-scroll near edges.
 *
 * If a trim handle is off-screen, an arrow indicator is drawn at the
 * corresponding edge showing the direction to the off-screen handle.
 *
 * @param waveformData          Packed min/max pairs or null.
 * @param trimStart             Current trim start in samples.
 * @param trimEnd               Current trim end in samples.
 * @param loopLength            Total loop length in samples.
 * @param playbackPosition      Current playback position in samples.
 * @param viewportStart         Normalized viewport start (0.0 to 1.0).
 * @param viewportEnd           Normalized viewport end (0.0 to 1.0).
 * @param zoomLevel             Current zoom level (1.0 = no zoom).
 * @param onTrimStartChange     Callback when the start handle moves.
 * @param onTrimEndChange       Callback when the end handle moves.
 * @param onViewportCenterChange Callback with new normalized center for panning.
 * @param modifier              Layout modifier.
 */
@Composable
private fun TrimWaveformCanvas(
    waveformData: FloatArray?,
    trimStart: Int,
    trimEnd: Int,
    loopLength: Int,
    playbackPosition: Int,
    viewportStart: Float,
    viewportEnd: Float,
    zoomLevel: Float,
    onTrimStartChange: (Int) -> Unit,
    onTrimEndChange: (Int) -> Unit,
    onViewportCenterChange: (Float) -> Unit,
    modifier: Modifier = Modifier
) {
    // Track which handle is being dragged: 0 = start, 1 = end, -1 = none
    var activeHandle by remember { mutableIntStateOf(-1) }

    // Use rememberUpdatedState so pointerInput reads current values without
    // restarting the coroutine on every handle move (avoids gesture drops).
    val currentTrimStart by rememberUpdatedState(trimStart)
    val currentTrimEnd by rememberUpdatedState(trimEnd)
    val currentViewportStart by rememberUpdatedState(viewportStart)
    val currentViewportEnd by rememberUpdatedState(viewportEnd)
    val currentZoomLevel by rememberUpdatedState(zoomLevel)
    val haptic = LocalHapticFeedback.current

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(DesignSystem.Background)
            .pointerInput(loopLength) {
                if (loopLength <= 0) return@pointerInput
                val canvasWidth = size.width.toFloat()
                val autoScrollEdgePx = AUTO_SCROLL_EDGE_ZONE.toPx()

                // Helper: convert sample to pixel within zoomed viewport
                fun sampleToPixel(sample: Int): Float {
                    val vpStart = currentViewportStart
                    val vpEnd = currentViewportEnd
                    val vpSpan = (vpEnd - vpStart).coerceAtLeast(0.0001f)
                    val normalized = sample.toFloat() / loopLength.toFloat()
                    return ((normalized - vpStart) / vpSpan) * canvasWidth
                }

                // Helper: convert pixel to sample within zoomed viewport
                fun pixelToSample(pixel: Float): Int {
                    val vpStart = currentViewportStart
                    val vpEnd = currentViewportEnd
                    val vpSpan = (vpEnd - vpStart).coerceAtLeast(0.0001f)
                    val normalized = (pixel / canvasWidth) * vpSpan + vpStart
                    return (normalized * loopLength).roundToInt().coerceIn(0, loopLength)
                }

                awaitEachGesture {
                    val down = awaitFirstDown(requireUnconsumed = false)
                    down.consume()

                    // Determine which handle is closest to the touch point
                    val startPixel = sampleToPixel(currentTrimStart)
                    val endPixel = sampleToPixel(currentTrimEnd)
                    val hitRadius = HANDLE_HIT_RADIUS.toPx()

                    val distToStart = abs(down.position.x - startPixel)
                    val distToEnd = abs(down.position.x - endPixel)

                    val handle = when {
                        distToStart <= hitRadius && distToStart <= distToEnd -> 0
                        distToEnd <= hitRadius -> 1
                        else -> -1  // No handle within hit radius
                    }

                    if (handle >= 0) {
                        // -- HANDLE DRAG MODE --
                        activeHandle = handle

                        // Compute grab offset so handle moves relative to finger,
                        // not jumping to absolute finger position (precision fix).
                        val handlePixel = if (handle == 0) startPixel else endPixel
                        val grabOffsetPx = down.position.x - handlePixel

                        // Haptic feedback on grab
                        haptic.performHapticFeedback(HapticFeedbackType.LongPress)

                        // Track drag with zero slop for immediate response
                        do {
                            val event = awaitPointerEvent()
                            val currentPos = event.changes.firstOrNull() ?: break
                            if (currentPos.pressed) {
                                currentPos.consume()
                                val adjustedX = currentPos.position.x - grabOffsetPx
                                val samplePos = pixelToSample(adjustedX)

                                when (handle) {
                                    0 -> {
                                        val clamped = samplePos.coerceAtMost(
                                            currentTrimEnd - MIN_HANDLE_GAP_SAMPLES
                                        ).coerceAtLeast(0)
                                        onTrimStartChange(clamped)
                                    }
                                    1 -> {
                                        val clamped = samplePos.coerceAtLeast(
                                            currentTrimStart + MIN_HANDLE_GAP_SAMPLES
                                        ).coerceAtMost(loopLength)
                                        onTrimEndChange(clamped)
                                    }
                                }

                                // Auto-scroll viewport when handle is near edge
                                if (currentZoomLevel > 1f) {
                                    val handlePx = currentPos.position.x
                                    val vpSpanNorm =
                                        currentViewportEnd - currentViewportStart
                                    val maxScrollNorm = vpSpanNorm * 0.05f

                                    val scrollFraction = when {
                                        handlePx < autoScrollEdgePx -> {
                                            -((autoScrollEdgePx - handlePx) / autoScrollEdgePx)
                                        }
                                        handlePx > canvasWidth - autoScrollEdgePx -> {
                                            ((handlePx - (canvasWidth - autoScrollEdgePx)) / autoScrollEdgePx)
                                        }
                                        else -> 0f
                                    }

                                    if (scrollFraction != 0f) {
                                        val scrollDelta =
                                            scrollFraction * maxScrollNorm
                                        val currentCenter =
                                            (currentViewportStart + currentViewportEnd) / 2f
                                        val newCenter = currentCenter + scrollDelta
                                        onViewportCenterChange(newCenter)
                                    }
                                }
                            } else {
                                break
                            }
                        } while (true)

                        activeHandle = -1

                    } else if (currentZoomLevel > 1f) {
                        // -- PAN MODE (single finger, not on a handle, zoomed) --
                        var prevX = down.position.x

                        do {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull() ?: break
                            if (change.pressed) {
                                change.consume()
                                val deltaX = change.position.x - prevX
                                prevX = change.position.x

                                // Convert pixel delta to normalized viewport delta
                                val vpSpanNorm =
                                    currentViewportEnd - currentViewportStart
                                val deltaNorm =
                                    -(deltaX / canvasWidth) * vpSpanNorm
                                val currentCenter =
                                    (currentViewportStart + currentViewportEnd) / 2f
                                val newCenter = currentCenter + deltaNorm
                                onViewportCenterChange(newCenter)
                            } else {
                                break
                            }
                        } while (true)
                    }
                    // else: single finger outside handle at 1x zoom = no-op
                }
            }
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            if (waveformData == null || waveformData.isEmpty() || loopLength <= 0) {
                drawPlaceholderText()
                return@Canvas
            }

            val canvasWidth = size.width
            val canvasHeight = size.height
            val centerY = canvasHeight / 2f

            // Number of waveform bar pairs
            val pairCount = waveformData.size / 2

            // Viewport bounds (normalized)
            val vpStart = viewportStart
            val vpEnd = viewportEnd
            val vpSpan = (vpEnd - vpStart).coerceAtLeast(0.0001f)

            // Helper: map a normalized position (0..1) to canvas pixel x
            fun normToPixel(norm: Float): Float {
                return ((norm - vpStart) / vpSpan) * canvasWidth
            }

            // Helper: map a sample position to canvas pixel x
            fun sampleToPixel(sample: Int): Float {
                val norm = sample.toFloat() / loopLength.toFloat()
                return normToPixel(norm)
            }

            // Pre-compute pixel positions for trim boundaries
            val trimStartPx = sampleToPixel(trimStart)
            val trimEndPx = sampleToPixel(trimEnd)

            // Determine which bars fall within the viewport
            val samplesPerBar = loopLength.toFloat() / pairCount.toFloat()
            val firstBar = ((vpStart * loopLength / samplesPerBar).toInt())
                .coerceIn(0, pairCount - 1)
            val lastBar = ((vpEnd * loopLength / samplesPerBar).toInt())
                .coerceIn(0, pairCount - 1)

            // -- Draw waveform bars (only visible portion) --
            for (i in firstBar..lastBar) {
                val minVal = waveformData[i * 2]
                val maxVal = waveformData[i * 2 + 1]

                // Bar sample range in normalized coords
                val barStartNorm = i.toFloat() / pairCount.toFloat()
                val barEndNorm = (i + 1).toFloat() / pairCount.toFloat()

                val barLeftPx = normToPixel(barStartNorm)
                val barRightPx = normToPixel(barEndNorm)
                val barW = (barRightPx - barLeftPx).coerceAtLeast(1f)
                val barCenterPx = barLeftPx + barW / 2f

                // Determine if this bar is inside the selected trim range
                val isSelected =
                    barCenterPx >= trimStartPx && barCenterPx <= trimEndPx

                val barColor = if (isSelected) {
                    DesignSystem.SafeGreen
                } else {
                    DesignSystem.TextMuted.copy(alpha = 0.3f)
                }

                // Scale amplitude to canvas height (values are -1..1)
                val topY =
                    centerY - (maxVal.coerceIn(-1f, 1f) * centerY * 0.85f)
                val bottomY =
                    centerY - (minVal.coerceIn(-1f, 1f) * centerY * 0.85f)

                // Ensure at least a 1px line for silence
                val minBarHeight = 1f
                val adjustedTopY: Float
                val adjustedBottomY: Float
                if (bottomY - topY < minBarHeight) {
                    adjustedTopY = centerY - minBarHeight / 2f
                    adjustedBottomY = centerY + minBarHeight / 2f
                } else {
                    adjustedTopY = topY
                    adjustedBottomY = bottomY
                }

                // Clip bar drawing to canvas bounds
                val clippedLeft = barLeftPx.coerceAtLeast(0f)
                val clippedRight = (barLeftPx + barW).coerceAtMost(canvasWidth)
                if (clippedRight > clippedLeft) {
                    drawRect(
                        color = barColor,
                        topLeft = Offset(clippedLeft, adjustedTopY),
                        size = Size(
                            width = (clippedRight - clippedLeft - 0.5f)
                                .coerceAtLeast(1f),
                            height = adjustedBottomY - adjustedTopY
                        )
                    )
                }
            }

            // -- Semi-transparent overlay on excluded regions --
            val overlayColor = DesignSystem.Background.copy(alpha = 0.55f)

            // Left excluded region (everything before trimStart)
            val clampedTrimStartPx = trimStartPx.coerceIn(0f, canvasWidth)
            if (clampedTrimStartPx > 0f) {
                drawRect(
                    color = overlayColor,
                    topLeft = Offset(0f, 0f),
                    size = Size(clampedTrimStartPx, canvasHeight)
                )
            }

            // Right excluded region (everything after trimEnd)
            val clampedTrimEndPx = trimEndPx.coerceIn(0f, canvasWidth)
            if (clampedTrimEndPx < canvasWidth) {
                drawRect(
                    color = overlayColor,
                    topLeft = Offset(clampedTrimEndPx, 0f),
                    size = Size(canvasWidth - clampedTrimEndPx, canvasHeight)
                )
            }

            // -- Trim handle lines (only if within or near viewport) --
            val handleVisible = 100f // extra pixels tolerance for pill
            val startHandleVisible =
                trimStartPx > -handleVisible && trimStartPx < canvasWidth + handleVisible
            val endHandleVisible =
                trimEndPx > -handleVisible && trimEndPx < canvasWidth + handleVisible

            if (startHandleVisible) {
                drawTrimHandle(
                    trimStartPx,
                    canvasHeight,
                    isStart = true,
                    isActive = activeHandle == 0
                )
            }
            if (endHandleVisible) {
                drawTrimHandle(
                    trimEndPx,
                    canvasHeight,
                    isStart = false,
                    isActive = activeHandle == 1
                )
            }

            // -- Off-screen handle indicators (arrows at edges) --
            if (zoomLevel > 1f) {
                if (trimStartPx < 0f) {
                    drawOffScreenIndicator(
                        canvasHeight = canvasHeight,
                        isLeft = true,
                        color = DesignSystem.SafeGreen
                    )
                }
                if (trimStartPx > canvasWidth) {
                    drawOffScreenIndicator(
                        canvasHeight = canvasHeight,
                        isLeft = false,
                        color = DesignSystem.SafeGreen
                    )
                }
                if (trimEndPx < 0f) {
                    drawOffScreenIndicator(
                        canvasHeight = canvasHeight,
                        isLeft = true,
                        color = DesignSystem.WarningAmber
                    )
                }
                if (trimEndPx > canvasWidth) {
                    drawOffScreenIndicator(
                        canvasHeight = canvasHeight,
                        isLeft = false,
                        color = DesignSystem.WarningAmber
                    )
                }
            }

            // -- Playback cursor line --
            if (playbackPosition in 0 until loopLength) {
                val cursorX = sampleToPixel(playbackPosition)
                if (cursorX in 0f..canvasWidth) {
                    drawLine(
                        color = Color.White.copy(alpha = 0.7f),
                        start = Offset(cursorX, 0f),
                        end = Offset(cursorX, canvasHeight),
                        strokeWidth = 1.5f
                    )
                }
            }
        }
    }
}

/**
 * Draw an off-screen handle indicator arrow at the left or right edge of
 * the detail canvas.
 *
 * A small filled triangle pointing toward the off-screen handle, drawn at
 * the vertical center of the canvas.
 *
 * @param canvasHeight Canvas height in pixels.
 * @param isLeft       True for left edge, false for right edge.
 * @param color        Handle color (SafeGreen or WarningAmber).
 */
private fun DrawScope.drawOffScreenIndicator(
    canvasHeight: Float,
    isLeft: Boolean,
    color: Color
) {
    val arrowSize = 8.dp.toPx()
    val centerY = canvasHeight / 2f
    val margin = 4.dp.toPx()

    val path = Path().apply {
        if (isLeft) {
            // Left-pointing triangle at left edge
            moveTo(margin + arrowSize, centerY - arrowSize)
            lineTo(margin, centerY)
            lineTo(margin + arrowSize, centerY + arrowSize)
            close()
        } else {
            // Right-pointing triangle at right edge
            val rightEdge = size.width - margin
            moveTo(rightEdge - arrowSize, centerY - arrowSize)
            lineTo(rightEdge, centerY)
            lineTo(rightEdge - arrowSize, centerY + arrowSize)
            close()
        }
    }

    drawPath(
        path = path,
        color = color.copy(alpha = 0.8f)
    )
}

/**
 * Draw a trim handle as a full-height vertical bar with a pill grip indicator.
 *
 * The handle consists of:
 * 1. A vertical line spanning the full canvas height (2dp normally, 3dp active).
 * 2. A rounded-rect "pill" at vertical center for grab affordance (16dp x 40dp).
 *
 * Start handle uses [DesignSystem.SafeGreen], end handle uses
 * [DesignSystem.WarningAmber]. When active (being dragged), the handle is
 * drawn at full opacity and wider; when idle, it is slightly transparent.
 *
 * @param x         Horizontal pixel position.
 * @param height    Canvas height in pixels.
 * @param isStart   True for the start handle, false for the end handle.
 * @param isActive  True when this handle is currently being dragged.
 */
private fun DrawScope.drawTrimHandle(
    x: Float,
    height: Float,
    isStart: Boolean,
    isActive: Boolean
) {
    val baseColor = if (isStart) DesignSystem.SafeGreen else DesignSystem.WarningAmber
    val lineAlpha = if (isActive) 1f else 0.7f
    val lineWidth = if (isActive) 3.dp.toPx() else 2.dp.toPx()

    // Clamp line x to canvas bounds for drawing
    val clampedLineX = x.coerceIn(0f, size.width)

    // 1. Full-height vertical line
    drawLine(
        color = baseColor.copy(alpha = lineAlpha),
        start = Offset(clampedLineX, 0f),
        end = Offset(clampedLineX, height),
        strokeWidth = lineWidth
    )

    // 2. Pill grip indicator at vertical center
    val pillWidth = 14.dp.toPx()
    val pillHeight = 36.dp.toPx()
    val centerY = height / 2f
    val pillAlpha = if (isActive) 0.9f else 0.6f

    // Clamp pill x so it stays fully within canvas bounds at extremes
    val clampedX = x.coerceIn(pillWidth / 2f, size.width - pillWidth / 2f)
    drawRoundRect(
        color = baseColor.copy(alpha = pillAlpha),
        topLeft = Offset(clampedX - pillWidth / 2f, centerY - pillHeight / 2f),
        size = Size(pillWidth, pillHeight),
        cornerRadius = CornerRadius(pillWidth / 2f, pillWidth / 2f)
    )

    // 3. Inner grip lines (3 horizontal lines inside the pill for drag affordance)
    val gripLineColor = if (isActive) {
        Color.White.copy(alpha = 0.7f)
    } else {
        Color.White.copy(alpha = 0.35f)
    }
    val gripLineWidth = 1.5f
    val gripSpacing = 6.dp.toPx()
    val gripHalfWidth = 4.dp.toPx()

    for (i in -1..1) {
        val lineY = centerY + i * gripSpacing
        drawLine(
            color = gripLineColor,
            start = Offset(clampedX - gripHalfWidth, lineY),
            end = Offset(clampedX + gripHalfWidth, lineY),
            strokeWidth = gripLineWidth
        )
    }
}

/**
 * Draw centered placeholder text when no waveform data is available.
 */
private fun DrawScope.drawPlaceholderText() {
    // Simple fallback: draw a horizontal center line to indicate an empty state.
    // Text rendering requires a TextMeasurer (Canvas scope only), so we draw
    // a dashed-style center line and rely on the composable layer for text.
    val centerY = size.height / 2f
    drawLine(
        color = DesignSystem.TextMuted.copy(alpha = 0.3f),
        start = Offset(16f, centerY),
        end = Offset(size.width - 16f, centerY),
        strokeWidth = 1f
    )
}

/**
 * Time readout row showing trim start, trim end, and trimmed duration.
 *
 * Uses monospace font for numeric alignment and displays values in
 * MM:SS.d format.
 *
 * @param trimStart  Trim start position in samples.
 * @param trimEnd    Trim end position in samples.
 * @param loopLength Total loop length in samples.
 * @param sampleRate Engine sample rate.
 * @param modifier   Layout modifier.
 */
@Composable
private fun TrimTimeDisplay(
    trimStart: Int,
    trimEnd: Int,
    loopLength: Int,
    sampleRate: Int,
    modifier: Modifier = Modifier
) {
    val safeSampleRate = sampleRate.coerceAtLeast(1)
    val startSeconds = trimStart.toFloat() / safeSampleRate
    val endSeconds = trimEnd.toFloat() / safeSampleRate
    val durationSeconds = (trimEnd - trimStart).coerceAtLeast(0).toFloat() / safeSampleRate

    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Start time
        Column(horizontalAlignment = Alignment.Start) {
            Text(
                text = "START",
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 1.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.7f)
            )
            Text(
                text = formatTime(startSeconds),
                fontSize = 14.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }

        // Trimmed duration (center)
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                text = "DURATION",
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 1.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.7f)
            )
            Text(
                text = formatTime(durationSeconds),
                fontSize = 14.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold,
                color = DesignSystem.SafeGreen
            )
        }

        // End time
        Column(horizontalAlignment = Alignment.End) {
            Text(
                text = "END",
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 1.sp,
                color = DesignSystem.CreamWhite.copy(alpha = 0.7f)
            )
            Text(
                text = formatTime(endSeconds),
                fontSize = 14.sp,
                fontFamily = FontFamily.Monospace,
                color = DesignSystem.TextValue
            )
        }
    }
}

/**
 * Row of snap mode selector chips.
 *
 * Three selectable chip buttons: "Free", "Zero-X", and "Beat". The
 * currently active mode chip gets a [DesignSystem.SafeGreen] background;
 * inactive chips use [DesignSystem.ModuleBorder].
 *
 * @param currentMode Current snap mode.
 * @param onModeChange Callback when a chip is selected.
 * @param modifier Layout modifier.
 */
@Composable
private fun SnapModeSelector(
    currentMode: LooperDelegate.SnapMode,
    onModeChange: (LooperDelegate.SnapMode) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            text = "SNAP MODE",
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite.copy(alpha = 0.7f)
        )

        Spacer(modifier = Modifier.height(8.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            SnapModeChip(
                label = "Free",
                isSelected = currentMode == LooperDelegate.SnapMode.FREE,
                onClick = { onModeChange(LooperDelegate.SnapMode.FREE) },
                modifier = Modifier.weight(1f)
            )
            SnapModeChip(
                label = "Zero-X",
                isSelected = currentMode == LooperDelegate.SnapMode.ZERO_CROSSING,
                onClick = { onModeChange(LooperDelegate.SnapMode.ZERO_CROSSING) },
                modifier = Modifier.weight(1f)
            )
            SnapModeChip(
                label = "Beat",
                isSelected = currentMode == LooperDelegate.SnapMode.BEAT,
                onClick = { onModeChange(LooperDelegate.SnapMode.BEAT) },
                modifier = Modifier.weight(1f)
            )
        }
    }
}

/**
 * Individual snap mode chip button.
 *
 * @param label      Display label for the chip.
 * @param isSelected Whether this chip is the active selection.
 * @param onClick    Callback when the chip is tapped.
 * @param modifier   Layout modifier.
 */
@Composable
private fun SnapModeChip(
    label: String,
    isSelected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val backgroundColor = if (isSelected) {
        DesignSystem.SafeGreen
    } else {
        DesignSystem.ControlPanelSurface
    }

    val textColor = if (isSelected) {
        Color.White
    } else {
        DesignSystem.CreamWhite
    }

    Button(
        onClick = onClick,
        colors = ButtonDefaults.buttonColors(
            containerColor = backgroundColor,
            contentColor = textColor
        ),
        shape = RoundedCornerShape(8.dp),
        modifier = modifier
            .height(36.dp)
            .then(
                if (!isSelected) {
                    Modifier.drawBehind {
                        drawRect(
                            color = DesignSystem.PanelHighlight,
                            size = size,
                            style = androidx.compose.ui.graphics.drawscope.Stroke(1.dp.toPx())
                        )
                    }
                } else {
                    Modifier
                }
            )
    ) {
        Text(
            text = label,
            fontSize = 12.sp,
            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
            letterSpacing = 1.sp,
            textAlign = TextAlign.Center
        )
    }
}

// -- Utility ------------------------------------------------------------------

/**
 * Format a duration in seconds to MM:SS.d format.
 *
 * @param seconds Duration in seconds.
 * @return Formatted string (e.g., "01:23.4").
 */
private fun formatTime(seconds: Float): String {
    val totalMs = (seconds * 1000f).toLong().coerceAtLeast(0)
    val mins = totalMs / 60000
    val secs = (totalMs % 60000) / 1000
    val tenths = (totalMs % 1000) / 100
    return String.format("%02d:%02d.%d", mins, secs, tenths)
}
