package com.guitaremulator.app

import android.app.Application

/**
 * Application class for GuitarEmulator.
 *
 * Provides application-wide initialization and serves as the global context
 * for components that need it (services, managers). Currently minimal -- it
 * will grow as we add dependency injection, crash reporting, and analytics.
 *
 * Declared in AndroidManifest.xml via android:name=".GuitarEmulatorApp".
 */
class GuitarEmulatorApp : Application() {

    override fun onCreate() {
        super.onCreate()
        // Application-level initialization goes here.
        // Future additions: Timber logging, Hilt DI, crash reporting.
    }
}
