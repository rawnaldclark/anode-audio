plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.guitaremulator.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.guitaremulator.app"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        // NDK ABI filters: arm64 for modern devices, armv7 for older, x86_64 for emulator
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        // CMake configuration for the native audio engine
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-O2", "-ffast-math")
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_TOOLCHAIN=clang"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isMinifyEnabled = false
            // Use lower optimization in debug for faster builds
            externalNativeBuild {
                cmake {
                    cppFlags += "-O0"
                }
            }
        }
    }

    testOptions {
        unitTests {
            isReturnDefaultValues = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    // Point to the CMakeLists.txt for the native audio engine build
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    // AndroidX Core
    implementation(libs.androidx.core.ktx)

    // Lifecycle
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.service)

    // Compose BOM ensures consistent versions across all Compose artifacts
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)

    // Activity Compose integration
    implementation(libs.androidx.activity.compose)

    // DataStore Preferences for persisting user settings
    implementation(libs.androidx.datastore.preferences)

    // Reorderable (drag-and-drop for LazyRow/LazyColumn)
    implementation(libs.reorderable)

    // Debug-only Compose tooling (layout inspector, preview)
    debugImplementation(libs.androidx.compose.ui.tooling)

    // Unit testing
    testImplementation(libs.junit)
    testImplementation(libs.org.json)
    testImplementation(libs.mockito.core)
    testImplementation(libs.kotlinx.coroutines.test)
}
