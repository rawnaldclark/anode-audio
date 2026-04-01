package com.guitaremulator.app.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.LooperSession
import com.guitaremulator.app.ui.theme.DesignSystem

/**
 * Full-screen session library overlay.
 *
 * Displays a list of saved looper sessions with options to re-amp
 * ("Try Different Tones"), delete, or share. Each session shows its
 * name, duration, BPM, preset, and creation date.
 *
 * @param visible         Whether the overlay is shown.
 * @param sessions        List of saved sessions.
 * @param isReamping      Whether re-amp playback is active.
 * @param onReamp         Callback to start re-amping a session.
 * @param onStopReamp     Callback to stop re-amp playback.
 * @param onDelete        Callback to delete a session by ID.
 * @param onClose         Callback to close the overlay.
 * @param modifier        Layout modifier.
 */
@Composable
fun SessionLibraryOverlay(
    visible: Boolean,
    sessions: List<LooperSession>,
    isReamping: Boolean,
    onReamp: (LooperSession) -> Unit,
    onStopReamp: () -> Unit,
    onDelete: (String) -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(250)) { it / 3 },
        exit = fadeOut(tween(150)) + slideOutVertically(tween(200)) { it / 3 },
        modifier = modifier
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            // Semi-transparent backdrop
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(DesignSystem.Background.copy(alpha = 0.9f))
                    .clickable(onClick = onClose)
            )

            // Session library panel
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(top = 24.dp)
                    .clip(RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp))
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

                        // -- Top border: GoldPiping at 60% + PanelShadow shadow --
                        val goldStroke = 2.dp.toPx()
                        val shadowStroke = 1.dp.toPx()
                        drawLine(
                            color = DesignSystem.GoldPiping.copy(alpha = 0.6f),
                            start = Offset(0f, goldStroke / 2f),
                            end = Offset(w, goldStroke / 2f),
                            strokeWidth = goldStroke
                        )
                        drawLine(
                            color = DesignSystem.PanelShadow,
                            start = Offset(0f, goldStroke + shadowStroke / 2f),
                            end = Offset(w, goldStroke + shadowStroke / 2f),
                            strokeWidth = shadowStroke
                        )
                    }
            ) {
                // Header
                SessionLibraryHeader(
                    isReamping = isReamping,
                    onStopReamp = onStopReamp,
                    onClose = onClose
                )

                if (sessions.isEmpty()) {
                    // Empty state
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f),
                        contentAlignment = Alignment.Center
                    ) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text(
                                text = "No saved sessions",
                                fontSize = 16.sp,
                                color = DesignSystem.TextMuted
                            )
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "Record a loop and save it to see it here",
                                fontSize = 12.sp,
                                color = DesignSystem.TextMuted.copy(alpha = 0.6f)
                            )
                        }
                    }
                } else {
                    // Session list
                    LazyColumn(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f)
                            .padding(horizontal = 16.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        item { Spacer(modifier = Modifier.height(8.dp)) }
                        items(sessions, key = { it.id }) { session ->
                            SessionCard(
                                session = session,
                                isReamping = isReamping,
                                onReamp = { onReamp(session) },
                                onDelete = { onDelete(session.id) }
                            )
                        }
                        item { Spacer(modifier = Modifier.height(16.dp)) }
                    }
                }
            }
        }
    }
}

/**
 * Header for the session library with close button and title.
 */
@Composable
private fun SessionLibraryHeader(
    isReamping: Boolean,
    onStopReamp: () -> Unit,
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
        IconButton(
            onClick = onClose,
            modifier = Modifier.size(36.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close session library",
                tint = DesignSystem.TextSecondary,
                modifier = Modifier.size(20.dp)
            )
        }

        Text(
            text = "SESSIONS",
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 2.sp,
            color = DesignSystem.CreamWhite
        )

        if (isReamping) {
            TextButton(onClick = onStopReamp) {
                Text(
                    text = "STOP",
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFFE53935)
                )
            }
        } else {
            Spacer(modifier = Modifier.width(36.dp))
        }
    }
}

/**
 * Individual session card showing metadata and action buttons.
 *
 * @param session    The session to display.
 * @param isReamping Whether any re-amp is currently active.
 * @param onReamp    Callback to start re-amping this session.
 * @param onDelete   Callback to delete this session.
 */
@Composable
private fun SessionCard(
    session: LooperSession,
    isReamping: Boolean,
    onReamp: () -> Unit,
    onDelete: () -> Unit
) {
    var showDeleteConfirm by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .drawBehind {
                // Panel border treatment on session cards
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
                    size = Size(
                        size.width - innerInset * 2f,
                        size.height - innerInset * 2f
                    ),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(innerStroke)
                )
            },
        colors = CardDefaults.cardColors(
            containerColor = DesignSystem.ControlPanelSurface
        ),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp)
        ) {
            // Top row: name and delete
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = session.name,
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Bold,
                    color = DesignSystem.TextPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )

                IconButton(
                    onClick = { showDeleteConfirm = true },
                    modifier = Modifier.size(28.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Delete,
                        contentDescription = "Delete session",
                        tint = DesignSystem.TextMuted,
                        modifier = Modifier.size(16.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(4.dp))

            // Metadata row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                // Duration
                val durationText = formatDuration(session.durationMs)
                MetadataChip(label = durationText)

                // BPM (if quantized)
                session.bpm?.let { bpm ->
                    MetadataChip(label = "${bpm.toInt()} BPM")
                }

                // Time signature
                if (session.timeSignature.isNotEmpty()) {
                    MetadataChip(label = session.timeSignature)
                }

                // Bar count (if quantized)
                session.barCount?.let { bars ->
                    MetadataChip(label = "${bars} bars")
                }
            }

            // Preset name (if any)
            session.presetName?.let { preset ->
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Preset: $preset",
                    fontSize = 11.sp,
                    color = DesignSystem.TextMuted,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }

            // Date
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = formatDate(session.createdAt),
                fontSize = 10.sp,
                color = DesignSystem.TextMuted.copy(alpha = 0.6f)
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Action button: Try Different Tones (secondary metallic style)
            Button(
                onClick = onReamp,
                enabled = !isReamping,
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFF5C6BC0),
                    contentColor = Color.White,
                    disabledContainerColor = DesignSystem.ModuleBorder,
                    disabledContentColor = DesignSystem.TextMuted
                ),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(36.dp)
                    .drawBehind {
                        // Chrome highlight at top edge
                        val highlightStroke = 1.dp.toPx()
                        drawLine(
                            color = DesignSystem.ChromeHighlight.copy(alpha = 0.3f),
                            start = Offset(6.dp.toPx(), highlightStroke / 2f),
                            end = Offset(size.width - 6.dp.toPx(), highlightStroke / 2f),
                            strokeWidth = highlightStroke
                        )
                    }
            ) {
                Text(
                    text = "TRY DIFFERENT TONES",
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    letterSpacing = 1.sp
                )
            }
        }
    }

    // Delete confirmation dialog
    if (showDeleteConfirm) {
        AlertDialog(
            onDismissRequest = { showDeleteConfirm = false },
            containerColor = DesignSystem.ModuleSurface,
            title = {
                Text(
                    "DELETE SESSION?",
                    color = DesignSystem.CreamWhite,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    letterSpacing = 2.sp
                )
            },
            text = {
                Text(
                    "\"${session.name}\" will be permanently deleted. This cannot be undone.",
                    color = DesignSystem.TextSecondary,
                    fontSize = 14.sp
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        showDeleteConfirm = false
                        onDelete()
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = Color(0xFFE53935),
                        contentColor = Color.White
                    )
                ) {
                    Text("Delete", fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteConfirm = false }) {
                    Text("Cancel", color = DesignSystem.TextSecondary)
                }
            }
        )
    }
}

/**
 * Small metadata chip for displaying session info.
 */
@Composable
private fun MetadataChip(label: String) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(4.dp))
            .background(DesignSystem.ModuleBorder.copy(alpha = 0.5f))
            .padding(horizontal = 6.dp, vertical = 2.dp)
    ) {
        Text(
            text = label,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            color = DesignSystem.TextValue
        )
    }
}

/**
 * Format milliseconds to a readable duration string.
 */
private fun formatDuration(ms: Long): String {
    val totalSec = ms / 1000
    val mins = totalSec / 60
    val secs = totalSec % 60
    return if (mins > 0) "${mins}m ${secs}s" else "${secs}s"
}

/**
 * Format a timestamp to a readable date string.
 */
private fun formatDate(timestamp: Long): String {
    val sdf = java.text.SimpleDateFormat("MMM d, yyyy h:mm a", java.util.Locale.getDefault())
    return sdf.format(java.util.Date(timestamp))
}
