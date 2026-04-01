package com.guitaremulator.app.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/**
 * Color definitions for the GuitarEmulator dark theme.
 *
 * Dark-only palette optimized for musicians in dimly lit environments.
 * Colors align with the DesignSystem token system (warm dark grays
 * with slight blue undertone, matching Guitar Rig 7's aesthetic).
 */
private val DarkColorScheme = darkColorScheme(
    primary = DesignSystem.EffectCategory.GAIN.primary,
    secondary = DesignSystem.EffectCategory.TIME.primary,
    tertiary = DesignSystem.ClipRed,
    background = DesignSystem.Background,
    surface = DesignSystem.ModuleSurface,
    onPrimary = Color.White,
    onSecondary = Color.White,
    onTertiary = Color.White,
    onBackground = DesignSystem.TextPrimary,
    onSurface = DesignSystem.TextPrimary,
)

/**
 * Application-wide Material3 theme for GuitarEmulator.
 *
 * Always uses our custom dark palette. Dynamic color (Material You) is
 * disabled because it conflicts with the Guitar Rig-inspired color system
 * where each effect category has a specific palette.
 */
@Composable
fun GuitarEmulatorTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        content = content
    )
}
