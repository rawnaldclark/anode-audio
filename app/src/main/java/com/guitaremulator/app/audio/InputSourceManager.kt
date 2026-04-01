package com.guitaremulator.app.audio

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import androidx.core.content.ContextCompat

/**
 * Detects and manages audio input sources using a three-tier priority system:
 *   1. USB Audio (highest priority) -- professional audio interfaces
 *   2. Analog Adapter -- wired headset/adapter with microphone input
 *   3. Phone Microphone (fallback) -- built-in device microphone
 *
 * USB audio devices are the preferred input for guitar processing because they
 * typically offer lower latency and higher quality ADCs than the phone's built-in mic.
 *
 * This class monitors for USB attach/detach events and audio routing changes
 * to automatically select the best available input source. The [Listener]
 * interface notifies consumers when the active source changes.
 */
class InputSourceManager(private val context: Context) {

    /** Supported input source types, ordered by priority (highest first). */
    enum class InputSource(val nativeType: Int) {
        /** USB audio interface (iRig HD, Focusrite, etc.) */
        USB_AUDIO(2),

        /** Wired analog adapter (1/4" to TRRS, iRig 2, etc.) */
        ANALOG_ADAPTER(1),

        /** Built-in phone microphone (fallback) */
        PHONE_MIC(0)
    }

    /** Callback for input source changes. */
    interface Listener {
        /**
         * Called when the active input source changes.
         * @param source The newly detected best input source.
         */
        fun onInputSourceChanged(source: InputSource)
    }

    private val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private var listener: Listener? = null
    private var currentSource = InputSource.PHONE_MIC
    private var isRegistered = false

    /**
     * BroadcastReceiver for USB device attach/detach events.
     * When a USB audio device is connected or disconnected, we re-evaluate
     * the best input source and notify the listener.
     */
    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                UsbManager.ACTION_USB_DEVICE_ATTACHED,
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val newSource = detectBestInputSource()
                    if (newSource != currentSource) {
                        currentSource = newSource
                        listener?.onInputSourceChanged(currentSource)
                    }
                }
            }
        }
    }

    /**
     * Set the listener for input source change notifications.
     * @param listener The listener to notify, or null to remove.
     */
    fun setListener(listener: Listener?) {
        this.listener = listener
    }

    /**
     * Start monitoring for input source changes.
     * Registers broadcast receivers for USB events and performs
     * an initial source detection.
     */
    fun startMonitoring() {
        if (isRegistered) return

        val filter = IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        ContextCompat.registerReceiver(
            context, usbReceiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED
        )
        isRegistered = true

        // Perform initial detection
        currentSource = detectBestInputSource()
        listener?.onInputSourceChanged(currentSource)
    }

    /**
     * Stop monitoring for input source changes and unregister receivers.
     * Must be called to avoid context leaks.
     */
    fun stopMonitoring() {
        if (!isRegistered) return
        context.unregisterReceiver(usbReceiver)
        isRegistered = false
    }

    /**
     * Get the currently detected best input source.
     * @return The highest-priority available input source.
     */
    fun getCurrentSource(): InputSource = currentSource

    /**
     * Detect the best available input source by checking for connected
     * audio input devices in priority order.
     *
     * Priority: USB Audio > Analog Adapter > Phone Mic
     *
     * @return The highest-priority detected input source.
     */
    fun detectBestInputSource(): InputSource {
        // Check for USB audio devices first (highest priority)
        if (hasUsbAudioDevice()) {
            return InputSource.USB_AUDIO
        }

        // Check for analog adapter (wired headset with mic)
        if (hasAnalogAdapter()) {
            return InputSource.ANALOG_ADAPTER
        }

        // Fallback to phone microphone
        return InputSource.PHONE_MIC
    }

    /**
     * Check if a USB audio input device is currently connected.
     *
     * Uses AudioManager.getDevices() which returns audio-capable devices
     * including USB audio interfaces. Also cross-references with UsbManager
     * for more reliable detection on some devices.
     */
    private fun hasUsbAudioDevice(): Boolean {
        val inputDevices = audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)
        val hasUsbAudioInput = inputDevices.any { device ->
            device.type == AudioDeviceInfo.TYPE_USB_DEVICE ||
            device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_USB_ACCESSORY
        }

        if (hasUsbAudioInput) return true

        // Fallback: check UsbManager for audio-class devices that AudioManager
        // might not yet report (brief window during USB enumeration)
        return usbManager.deviceList.values.any { device ->
            isUsbAudioClass(device)
        }
    }

    /**
     * Check if an analog audio adapter with microphone input is connected.
     * This covers TRRS-to-1/4" adapters like the iRig 2, as well as
     * standard wired headsets with inline microphones.
     */
    private fun hasAnalogAdapter(): Boolean {
        val inputDevices = audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)
        return inputDevices.any { device ->
            device.type == AudioDeviceInfo.TYPE_WIRED_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_LINE_ANALOG
        }
    }

    /**
     * Get the Android device ID for the first connected USB audio input device.
     * This ID can be passed to Oboe's setDeviceId() to explicitly route
     * the recording stream to this USB device.
     *
     * @return AudioDeviceInfo.getId() for the USB input, or 0 if none found.
     */
    fun getUsbInputDeviceId(): Int {
        val inputDevices = audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)
        val usbInput = inputDevices.firstOrNull { device ->
            device.type == AudioDeviceInfo.TYPE_USB_DEVICE ||
            device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_USB_ACCESSORY
        }
        return usbInput?.id ?: 0
    }

    /**
     * Get the Android device ID for the first connected USB audio output device.
     * This ID can be passed to Oboe's setDeviceId() to explicitly route
     * the playback stream to this USB device, ensuring processed audio goes
     * back through the USB interface (e.g., to iRig headphone/amp output)
     * instead of the phone speaker.
     *
     * @return AudioDeviceInfo.getId() for the USB output, or 0 if none found.
     */
    fun getUsbOutputDeviceId(): Int {
        val outputDevices = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
        val usbOutput = outputDevices.firstOrNull { device ->
            device.type == AudioDeviceInfo.TYPE_USB_DEVICE ||
            device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_USB_ACCESSORY
        }
        return usbOutput?.id ?: 0
    }

    /**
     * Check if a USB device belongs to the USB Audio Device Class (0x01).
     * USB audio interfaces declare class 0x01 at either the device level
     * or the interface level.
     */
    private fun isUsbAudioClass(device: UsbDevice): Boolean {
        // Check device-level class
        if (device.deviceClass == 0x01) return true

        // Check interface-level class (more common for composite USB devices)
        for (i in 0 until device.interfaceCount) {
            if (device.getInterface(i).interfaceClass == 0x01) {
                return true
            }
        }
        return false
    }
}
