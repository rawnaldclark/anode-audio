package com.guitaremulator.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.guitaremulator.app.ui.theme.DesignSystem
import com.guitaremulator.app.viewmodel.EffectUiState

/**
 * Individual pedal card composable representing a single effect in the chain.
 *
 * Styled to resemble a physical guitar effects pedal with:
 *   - Category-colored accent bar at top (or pedal-specific color for DS-1, RAT, etc.)
 *   - Category-tinted dark enclosure
 *   - LED indicator with glow effect (category colors)
 *   - Effect name label
 *   - Stomp switch with metallic 3D appearance and press animation
 *
 * @param effectState Current UI state of the effect.
 * @param onStompClick Callback when the stomp switch is pressed (toggles effect).
 * @param onEditClick Callback when the card body is tapped (opens editor).
 * @param modifier Modifier for layout customization.
 */
@Composable
fun PedalCard(
    effectState: EffectUiState,
    onStompClick: () -> Unit,
    onEditClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val category = DesignSystem.getCategory(effectState.index)
    val headerColor = DesignSystem.getHeaderColor(effectState.index)

    // Enclosure color: blend category surface tint with module surface
    val enclosureColor = category.surfaceTint
        .copy(alpha = if (effectState.enabled) 1f else 0.7f)
        .compositeOver(DesignSystem.ModuleSurface)

    Column(
        modifier = modifier
            .width(100.dp)
            .shadow(
                elevation = 4.dp,
                shape = RoundedCornerShape(DesignSystem.ModuleCornerRadius)
            )
            .clip(RoundedCornerShape(DesignSystem.ModuleCornerRadius))
            .drawBehind {
                // Category accent bar at top (3dp)
                drawRect(
                    color = headerColor,
                    topLeft = Offset.Zero,
                    size = Size(size.width, DesignSystem.AccentBarHeight.toPx())
                )
                // Enclosure fill below the accent bar
                drawRect(
                    color = enclosureColor,
                    topLeft = Offset(0f, DesignSystem.AccentBarHeight.toPx()),
                    size = Size(size.width, size.height - DesignSystem.AccentBarHeight.toPx())
                )
            }
            .clickable(onClick = onEditClick)
            .padding(top = DesignSystem.AccentBarHeight + 6.dp, start = 8.dp, end = 8.dp, bottom = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        // LED indicator with category-specific colors and glow
        LedIndicator(
            active = effectState.enabled,
            activeColor = category.ledActive,
            inactiveColor = category.ledBypass
        )

        Spacer(modifier = Modifier.height(6.dp))

        // Effect name
        Text(
            text = effectState.name,
            fontSize = 11.sp,
            fontWeight = FontWeight.SemiBold,
            color = DesignSystem.TextPrimary,
            textAlign = TextAlign.Center,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            lineHeight = 14.sp,
            modifier = Modifier.fillMaxWidth()
        )

        Spacer(modifier = Modifier.height(6.dp))

        // Stomp switch (not shown for read-only effects like Tuner)
        if (!effectState.isReadOnly) {
            StompSwitch(
                active = effectState.enabled,
                categoryColor = category.ledActive,
                onClick = onStompClick,
                size = 44.dp
            )
        } else {
            // Tuner: show category-colored label instead of stomp switch
            Text(
                text = "TUNER",
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                color = category.primary,
                modifier = Modifier.padding(vertical = 12.dp)
            )
        }

        Spacer(modifier = Modifier.height(2.dp))
    }
}
