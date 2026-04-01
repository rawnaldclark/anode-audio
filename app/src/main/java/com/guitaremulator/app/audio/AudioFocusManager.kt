package com.guitaremulator.app.audio

import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager

/**
 * Manages Android audio focus for the guitar processing engine.
 *
 * Audio focus is Android's cooperative mechanism for multiple apps sharing
 * audio output. When we request focus, other apps (music players, podcasts)
 * duck or pause. When we lose focus (incoming call, navigation prompt),
 * we respond appropriately.
 *
 * We use AUDIOFOCUS_GAIN (permanent focus) with USAGE_GAME because:
 *   - USAGE_GAME routes through the low-latency audio path on most devices
 *   - CONTENT_TYPE_MUSIC accurately describes processed guitar audio
 *   - Permanent focus is correct for an active effects processor
 *
 * The [Listener] interface allows the service/ViewModel to react to focus changes
 * without this class having direct knowledge of the audio engine.
 */
class AudioFocusManager(private val audioManager: AudioManager) {

    /**
     * Callback interface for audio focus state changes.
     * Implementors should start/stop/duck the audio engine as appropriate.
     */
    interface Listener {
        /** Focus gained (or regained after transient loss). Resume full-volume processing. */
        fun onAudioFocusGained()

        /** Permanent focus loss (another app took focus). Stop the engine. */
        fun onAudioFocusLost()

        /** Temporary focus loss (e.g., notification sound). Pause or mute output. */
        fun onAudioFocusLostTransient()

        /** Temporary loss, but we may continue at reduced volume (e.g., navigation prompt). */
        fun onAudioFocusLostTransientCanDuck()
    }

    private var listener: Listener? = null
    private var focusRequest: AudioFocusRequest? = null
    private var hasFocus = false

    /**
     * Audio focus change callback. Dispatches to the [Listener] interface.
     * Called on the main thread by the AudioManager.
     */
    private val focusChangeListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        when (focusChange) {
            AudioManager.AUDIOFOCUS_GAIN -> {
                hasFocus = true
                listener?.onAudioFocusGained()
            }
            AudioManager.AUDIOFOCUS_LOSS -> {
                hasFocus = false
                listener?.onAudioFocusLost()
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                hasFocus = false
                listener?.onAudioFocusLostTransient()
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                // We still have partial focus but should reduce volume
                listener?.onAudioFocusLostTransientCanDuck()
            }
        }
    }

    /**
     * Set the listener that will receive audio focus change callbacks.
     *
     * @param listener The listener to notify, or null to remove.
     */
    fun setListener(listener: Listener?) {
        this.listener = listener
    }

    /**
     * Request permanent audio focus for guitar processing.
     *
     * @return true if focus was granted immediately, false if denied.
     */
    fun requestFocus(): Boolean {
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build()

        val request = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attributes)
            .setOnAudioFocusChangeListener(focusChangeListener)
            .setAcceptsDelayedFocusGain(false) // We need focus now or not at all
            .setWillPauseWhenDucked(false)     // We handle ducking ourselves
            .build()

        focusRequest = request

        val result = audioManager.requestAudioFocus(request)
        hasFocus = (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
        return hasFocus
    }

    /**
     * Abandon audio focus, allowing other apps to resume playback.
     * Should be called when the audio engine is stopped.
     */
    fun abandonFocus() {
        focusRequest?.let { request ->
            audioManager.abandonAudioFocusRequest(request)
        }
        focusRequest = null
        hasFocus = false
    }

    /** Check if we currently hold audio focus. */
    fun hasFocus(): Boolean = hasFocus
}
