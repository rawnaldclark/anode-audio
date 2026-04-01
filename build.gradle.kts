// Root build file for GuitarEmulator
// Plugin declarations are managed via the version catalog (gradle/libs.versions.toml)
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
}
