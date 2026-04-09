package com.guitaremulator.app.audio

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import com.guitaremulator.app.MainActivity
import com.guitaremulator.app.R

/**
 * Foreground service that keeps the audio engine alive when the app is
 * in the background.
 *
 * Android kills background audio processing aggressively. A foreground service
 * with an ongoing notification tells the system this is a user-initiated,
 * long-running operation that should not be interrupted.
 *
 * The service uses foregroundServiceType="mediaPlayback" declared in the
 * manifest, which is the correct type for sustained audio I/O on Android 14+.
 *
 * Doze mode mitigation:
 *   When the screen turns off and the device enters Doze mode, the CPU may
 *   be suspended, causing audio dropouts. We hold a PARTIAL_WAKE_LOCK to
 *   keep the CPU running while audio is being processed. The wake lock is
 *   acquired when the service starts and released when it stops.
 *
 *   Note: A PARTIAL_WAKE_LOCK keeps the CPU running but lets the screen
 *   turn off, which is the correct behavior for background audio processing.
 *
 * Android 15 (API 35) time limit:
 *   Android 15 introduces a 6-hour time limit on media foreground services.
 *   After 6 hours, the system stops the service. For extended jam sessions,
 *   the ViewModel should detect the service stop and prompt the user to
 *   restart. This is handled by the onTaskRemoved/onDestroy callbacks.
 *
 * Lifecycle:
 *   1. Activity/ViewModel starts the service via startForegroundService()
 *   2. Service creates a notification channel and shows an ongoing notification
 *   3. WakeLock is acquired to prevent CPU sleep during processing
 *   4. Audio engine runs as long as the service is alive
 *   5. Activity/ViewModel stops the service when the user taps "Stop"
 *   6. WakeLock is released and the audio engine shuts down
 */
class AudioProcessingService : Service() {

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "guitar_emulator_audio"
        private const val NOTIFICATION_ID = 1
        private const val WAKE_LOCK_TAG = "GuitarEmulator::AudioProcessing"
    }

    private var wakeLock: PowerManager.WakeLock? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val notification = buildNotification()

        // On Android 14+ (API 34), foreground service type must be specified at start time
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK or
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }

        // Acquire a partial wake lock to keep the CPU running when the screen
        // is off. Without this, Doze mode can suspend the CPU and cause audio
        // dropouts. PARTIAL_WAKE_LOCK is the least intrusive option: it keeps
        // the CPU awake but allows the screen to turn off normally.
        acquireWakeLock()

        // START_NOT_STICKY: do not auto-restart this service if the system kills it.
        // The ViewModel will explicitly restart the engine when the app reopens.
        // START_STICKY caused stale audio services to persist across APK updates,
        // holding kernel-level Oboe stream resources that blocked the new instance.
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? {
        // This is a started service, not a bound service.
        // The ViewModel communicates with the engine directly, not through the binder.
        return null
    }

    override fun onDestroy() {
        releaseWakeLock()
        super.onDestroy()
    }

    /**
     * Called when the user swipes away the app or the system kills the task
     * (e.g., during an APK update). Without this, START_STICKY would keep
     * the service alive with stale Oboe stream references, preventing the
     * new app instance from acquiring audio resources after the update.
     */
    override fun onTaskRemoved(rootIntent: Intent?) {
        releaseWakeLock()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
        super.onTaskRemoved(rootIntent)
    }

    /**
     * Acquire a partial wake lock to prevent CPU sleep during audio processing.
     *
     * The wake lock has a 7-hour timeout as a safety net to prevent battery
     * drain if the service is never explicitly stopped. This exceeds Android 15's
     * 6-hour media service limit, so the system will stop the service before
     * the wake lock times out.
     */
    private fun acquireWakeLock() {
        if (wakeLock == null) {
            val powerManager = getSystemService(POWER_SERVICE) as PowerManager
            wakeLock = powerManager.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK,
                WAKE_LOCK_TAG
            ).apply {
                // 7-hour timeout as safety net (Android 15 enforces 6-hour limit)
                acquire(7 * 60 * 60 * 1000L)
            }
        }
    }

    /**
     * Release the wake lock when audio processing stops.
     */
    private fun releaseWakeLock() {
        wakeLock?.let {
            if (it.isHeld) {
                it.release()
            }
        }
        wakeLock = null
    }

    /**
     * Create the notification channel required on Android 8+ (API 26).
     * The channel is created once and persists across service restarts.
     * Users can customize notification behavior per-channel in system settings.
     */
    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW // Low importance = no sound, shows in shade
        ).apply {
            description = getString(R.string.notification_channel_description)
            setShowBadge(false) // No badge on the app icon
        }

        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.createNotificationChannel(channel)
    }

    /**
     * Build the ongoing notification shown while the audio engine is active.
     * Tapping the notification returns the user to the main activity.
     */
    private fun buildNotification(): Notification {
        // PendingIntent to reopen the main activity when the notification is tapped
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
            },
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(getString(R.string.notification_text))
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(pendingIntent)
            .setOngoing(true)       // Cannot be swiped away
            .setSilent(true)        // No sound when posted
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
    }
}
