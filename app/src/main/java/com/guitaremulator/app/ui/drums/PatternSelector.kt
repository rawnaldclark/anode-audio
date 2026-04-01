package com.guitaremulator.app.ui.drums

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.data.DrumPatternData
import com.guitaremulator.app.ui.theme.DesignSystem

// ── Genre Grouping ───────────────────────────────────────────────────────────

/**
 * Infer genre from a pattern name for grouping in the selector.
 *
 * Groups factory patterns by keyword matching. User-created patterns and
 * unrecognized names are grouped under "OTHER".
 *
 * @param name Pattern display name.
 * @return Genre label string.
 */
private fun inferGenre(name: String): String {
    val lower = name.lowercase()
    return when {
        lower.contains("rock") || lower.contains("metal")   -> "ROCK"
        lower.contains("funk") || lower.contains("disco")   -> "FUNK"
        lower.contains("hip") || lower.contains("lo-fi")
            || lower.contains("trap") || lower.contains("808") -> "HIP-HOP"
        lower.contains("blues") || lower.contains("shuffle") -> "BLUES"
        lower.contains("jazz") || lower.contains("brush")
            || lower.contains("bossa")                       -> "JAZZ"
        lower.contains("empty") || lower.contains("blank")  -> "UTILITY"
        else                                                 -> "OTHER"
    }
}

/**
 * Genre display order for the preset selector.
 */
private val genreOrder = listOf("ROCK", "FUNK", "HIP-HOP", "BLUES", "JAZZ", "UTILITY", "OTHER")

// ── PatternSelector Composable ───────────────────────────────────────────────

/**
 * Horizontally scrollable pattern/preset selector with genre headers.
 *
 * Patterns are grouped by genre and displayed as large tappable chips.
 * The currently active pattern is highlighted with a gold border, tinted
 * background, and a checkmark indicator. Factory patterns are listed first,
 * followed by user-created patterns.
 *
 * The selector automatically scrolls to the active pattern on load.
 *
 * Styled to match the Blackface Terminal design language: monospace text,
 * muted surfaces, and gold accent for the active selection.
 *
 * @param patterns        List of all available drum patterns.
 * @param currentId       ID of the currently active pattern.
 * @param onSelectPattern Callback invoked with the pattern ID when tapped.
 * @param modifier        Layout modifier.
 */
@Composable
fun PatternSelector(
    patterns: List<DrumPatternData>,
    currentId: String,
    onSelectPattern: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    // Pre-compute color tokens once
    val activeChipBg = remember { DesignSystem.GoldPiping.copy(alpha = 0.15f) }
    val activeBorderColor = remember { DesignSystem.GoldPiping.copy(alpha = 0.9f) }
    val inactiveBorderColor = remember { DesignSystem.TextMuted.copy(alpha = 0.35f) }
    val genreLabelColor = remember { DesignSystem.TextMuted.copy(alpha = 0.6f) }

    // Group patterns by genre, maintaining genre order
    val grouped = remember(patterns) {
        val byGenre = patterns.groupBy { inferGenre(it.name) }
        genreOrder.mapNotNull { genre ->
            val group = byGenre[genre]
            if (group != null) genre to group else null
        }
    }

    val scrollState = rememberScrollState()

    // Auto-scroll to the selected pattern on initial composition
    LaunchedEffect(currentId) {
        // Estimate chip position: each chip is ~120dp + spacing
        // Find the flat index of the current pattern
        var flatIndex = 0
        for ((_, group) in grouped) {
            for (p in group) {
                if (p.id == currentId) break
                flatIndex++
            }
            if (group.any { it.id == currentId }) break
        }
        // Scroll to approximate position (120dp per chip + genre headers)
        val targetPx = (flatIndex * 130).coerceAtLeast(0)
        scrollState.animateScrollTo(targetPx)
    }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(DesignSystem.AmpChassisGray)
            .horizontalScroll(scrollState)
            .padding(horizontal = 8.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        for ((genre, group) in grouped) {
            // Genre header label
            Text(
                text = genre,
                fontSize = 9.sp,
                fontWeight = FontWeight.Black,
                fontFamily = FontFamily.Monospace,
                color = genreLabelColor,
                letterSpacing = 1.sp,
                modifier = Modifier.padding(end = 2.dp)
            )

            for (pattern in group) {
                val isActive = pattern.id == currentId
                val chipShape = RoundedCornerShape(8.dp)

                Box(
                    modifier = Modifier
                        .clickable { onSelectPattern(pattern.id) }
                        .background(
                            color = if (isActive) activeChipBg else DesignSystem.ModuleSurface,
                            shape = chipShape
                        )
                        .border(
                            width = if (isActive) 2.dp else 1.dp,
                            color = if (isActive) activeBorderColor else inactiveBorderColor,
                            shape = chipShape
                        )
                        .padding(horizontal = 14.dp, vertical = 8.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        // Checkmark for active pattern
                        if (isActive) {
                            Text(
                                text = "\u2713", // checkmark
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Bold,
                                color = DesignSystem.GoldPiping
                            )
                        }

                        Text(
                            text = pattern.name,
                            fontSize = 13.sp,
                            fontWeight = if (isActive) FontWeight.Bold else FontWeight.Medium,
                            fontFamily = FontFamily.Monospace,
                            color = if (isActive) DesignSystem.GoldPiping else DesignSystem.CreamWhite,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }

            // Separator between genres (except last)
            if (genre != grouped.last().first) {
                Spacer(modifier = Modifier.width(4.dp))
                Box(
                    modifier = Modifier
                        .width(1.dp)
                        .padding(vertical = 2.dp)
                        .background(DesignSystem.Divider)
                )
                Spacer(modifier = Modifier.width(4.dp))
            }
        }
    }
}
