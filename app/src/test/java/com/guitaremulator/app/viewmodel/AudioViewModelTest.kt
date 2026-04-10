package com.guitaremulator.app.viewmodel

import android.app.Application
import android.content.Context
import android.hardware.usb.UsbManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import com.guitaremulator.app.audio.AudioFocusManager
import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.audio.InputSourceManager
import com.guitaremulator.app.data.EffectRegistry
import com.guitaremulator.app.data.PresetManager
import com.guitaremulator.app.data.UserPreferences
import com.guitaremulator.app.data.UserPreferencesRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.junit.MockitoJUnitRunner

/**
 * Unit tests for [AudioViewModel].
 *
 * Uses Mockito to mock the JNI-backed [AudioEngine], the file-backed
 * [PresetManager], the DataStore-backed [UserPreferencesRepository], and
 * the Android [Application] context. All dependencies are injected via
 * the constructor DI pattern introduced by the @JvmOverloads refactor.
 *
 * Because [AudioViewModel] extends [AndroidViewModel], the init block
 * requires a functioning Application context to create [AudioFocusManager]
 * (needs AudioManager) and [InputSourceManager] (needs AudioManager,
 * UsbManager, and BroadcastReceiver registration). These are satisfied
 * by stubbing the mock Application's getSystemService calls.
 *
 * The test dispatcher (UnconfinedTestDispatcher) is installed via
 * Dispatchers.setMain so that viewModelScope.launch blocks execute
 * immediately and synchronously during tests.
 */
@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(MockitoJUnitRunner::class)
class AudioViewModelTest {

    // =========================================================================
    // Mocks
    // =========================================================================

    @Mock
    lateinit var mockApplication: Application

    @Mock
    lateinit var mockAudioEngine: IAudioEngine

    @Mock
    lateinit var mockPresetManager: PresetManager

    @Mock
    lateinit var mockUserPrefsRepo: UserPreferencesRepository

    @Mock
    lateinit var mockAudioManager: AudioManager

    @Mock
    lateinit var mockUsbManager: UsbManager

    @Mock
    lateinit var mockAudioFocusManager: AudioFocusManager

    @get:Rule
    val tempFolder = TemporaryFolder()

    private lateinit var viewModel: AudioViewModel

    private val testDispatcher = UnconfinedTestDispatcher()

    // =========================================================================
    // Setup & Teardown
    // =========================================================================

    @Before
    fun setup() {
        // Install test dispatcher so viewModelScope.launch executes immediately
        Dispatchers.setMain(testDispatcher)

        // Stub Application.getSystemService for AudioFocusManager and InputSourceManager.
        // AudioFocusManager needs AudioManager; InputSourceManager needs both
        // AudioManager and UsbManager.
        `when`(mockApplication.getSystemService(Context.AUDIO_SERVICE))
            .thenReturn(mockAudioManager)
        `when`(mockApplication.getSystemService(Context.USB_SERVICE))
            .thenReturn(mockUsbManager)
        `when`(mockApplication.packageName)
            .thenReturn("com.guitaremulator.app")

        // LooperSessionManager accesses context.filesDir to create session storage.
        // Provide a temporary directory so mkdirs() succeeds in tests.
        `when`(mockApplication.filesDir)
            .thenReturn(tempFolder.newFolder("filesDir"))

        // InputSourceManager.detectBestInputSource() calls audioManager.getDevices()
        // which must return a non-null array. Return empty to indicate no external
        // devices connected (falls back to PHONE_MIC).
        `when`(mockAudioManager.getDevices(anyInt()))
            .thenReturn(emptyArray<AudioDeviceInfo>())

        // UsbManager.deviceList must return a non-null map for InputSourceManager
        // to iterate over without NPE.
        `when`(mockUsbManager.deviceList)
            .thenReturn(HashMap())

        // AudioFocusManager.requestFocus() is called in startEngine(). Stub it
        // to return true so the engine proceeds past the audio focus check.
        `when`(mockAudioFocusManager.requestFocus()).thenReturn(true)

        // The polling coroutine in startEngine() calls audioEngine.isRunning()
        // on each iteration. With UnconfinedTestDispatcher it runs immediately,
        // so we must return true to prevent the loop from resetting _isRunning.
        `when`(mockAudioEngine.isRunning()).thenReturn(true)

        // PresetManager.listPresets() is called in refreshPresetList() during init.
        // Must return a non-null list to avoid NPE when assigned to MutableStateFlow.
        `when`(mockPresetManager.listPresets())
            .thenReturn(emptyList())

        // UserPreferencesRepository.userPreferences is collected in init via first().
        // Provide a default-valued Flow so the preference restoration completes cleanly.
        `when`(mockUserPrefsRepo.userPreferences)
            .thenReturn(flowOf(UserPreferences()))

        // Construct the ViewModel with all mocked dependencies
        viewModel = AudioViewModel(
            application = mockApplication,
            audioEngine = mockAudioEngine,
            presetManager = mockPresetManager,
            userPreferencesRepository = mockUserPrefsRepo,
            audioFocusManagerOverride = mockAudioFocusManager
        )
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    // =========================================================================
    // Engine Lifecycle
    // =========================================================================

    @Test
    fun `startEngine sets master gains and bypass before starting`() {
        // Arrange: mock engine start to succeed
        `when`(mockAudioEngine.startEngine()).thenReturn(true)

        // Act
        viewModel.startEngine()

        // Assert: verify gains and bypass were sent to engine before startEngine
        verify(mockAudioEngine).setInputGain(1.0f)
        verify(mockAudioEngine).setOutputGain(1.0f)
        verify(mockAudioEngine).setGlobalBypass(false)
        verify(mockAudioEngine).startEngine()

        // Verify engine is now reported as running
        assertTrue(viewModel.isRunning.value)
    }

    @Test
    fun `stopEngine resets running state and meters`() {
        // Arrange: start the engine first
        `when`(mockAudioEngine.startEngine()).thenReturn(true)
        viewModel.startEngine()
        assertTrue(viewModel.isRunning.value)

        // Act
        viewModel.stopEngine()

        // Assert
        assertFalse(viewModel.isRunning.value)
        assertEquals(0f, viewModel.inputLevel.value, 0.001f)
        assertEquals(0f, viewModel.outputLevel.value, 0.001f)
        assertEquals(0, viewModel.sampleRate.value)
        assertEquals(0, viewModel.framesPerBuffer.value)
        assertEquals(0f, viewModel.estimatedLatencyMs.value, 0.001f)
    }

    @Test
    fun `startEngine when already running is no-op`() {
        // Arrange: start the engine
        `when`(mockAudioEngine.startEngine()).thenReturn(true)
        viewModel.startEngine()

        // Act: try to start again
        viewModel.startEngine()

        // Assert: startEngine was only called once on the native engine
        verify(mockAudioEngine).startEngine()
    }

    @Test
    fun `stopEngine when not running is no-op`() {
        // Act: stop without ever starting
        viewModel.stopEngine()

        // Assert: native stopEngine was never called
        verify(mockAudioEngine, never()).stopEngine()
        assertFalse(viewModel.isRunning.value)
    }

    @Test
    fun `startEngine failure sets error and does not mark running`() {
        // Arrange: mock engine start to fail
        `when`(mockAudioEngine.startEngine()).thenReturn(false)

        // Act
        viewModel.startEngine()

        // Assert
        assertFalse(viewModel.isRunning.value)
        assertEquals("Failed to start audio engine", viewModel.engineError.value)
    }

    // =========================================================================
    // Master Controls
    // =========================================================================

    @Test
    fun `setInputGain clamps value above maximum to 4_0`() {
        // Act: attempt to set gain above max
        viewModel.setInputGain(5.0f)

        // Assert: value is clamped to the 0.0..4.0 range
        assertEquals(4.0f, viewModel.inputGain.value, 0.001f)
        verify(mockAudioEngine).setInputGain(4.0f)
    }

    @Test
    fun `setOutputGain clamps value below minimum to 0_0`() {
        // Act: attempt to set negative gain
        viewModel.setOutputGain(-1.0f)

        // Assert: value is clamped to 0.0
        assertEquals(0.0f, viewModel.outputGain.value, 0.001f)
        verify(mockAudioEngine).setOutputGain(0.0f)
    }

    @Test
    fun `toggleGlobalBypass propagates new state to engine`() {
        // Assert initial state
        assertFalse(viewModel.globalBypass.value)

        // Act: toggle bypass on
        viewModel.toggleGlobalBypass()

        // Assert
        assertTrue(viewModel.globalBypass.value)
        verify(mockAudioEngine).setGlobalBypass(true)

        // Act: toggle bypass off
        viewModel.toggleGlobalBypass()

        // Assert
        assertFalse(viewModel.globalBypass.value)
        verify(mockAudioEngine).setGlobalBypass(false)
    }

    @Test
    fun `setInputGain updates state flow value`() {
        // Act
        viewModel.setInputGain(2.5f)

        // Assert
        assertEquals(2.5f, viewModel.inputGain.value, 0.001f)
    }

    @Test
    fun `setOutputGain updates state flow value`() {
        // Act
        viewModel.setOutputGain(3.0f)

        // Assert
        assertEquals(3.0f, viewModel.outputGain.value, 0.001f)
    }

    // =========================================================================
    // Effect Control
    // =========================================================================

    @Test
    fun `toggleEffect updates state and calls engine when running`() {
        // Arrange: start engine so toggleEffect propagates to native
        `when`(mockAudioEngine.startEngine()).thenReturn(true)
        viewModel.startEngine()

        // Effect index 4 = Overdrive, which is enabled by default
        val initialState = viewModel.effectStates.value[4]
        assertTrue(initialState.enabled)

        // Act: toggle it off
        viewModel.toggleEffect(4)

        // Assert: state flow updated and engine notified
        assertFalse(viewModel.effectStates.value[4].enabled)
        verify(mockAudioEngine).setEffectEnabled(4, false)
    }

    @Test
    fun `setEffectParameter updates parameter map`() {
        // The init block populates effectStates from EffectRegistry defaults.
        // Effect index 4 = Overdrive, param 0 = Drive (default 0.3)
        val initialDrive = viewModel.effectStates.value[4].parameters[0]
        assertEquals(0.3f, initialDrive!!, 0.001f)

        // Act: set drive to 0.8
        viewModel.setEffectParameter(4, 0, 0.8f)

        // Assert
        assertEquals(0.8f, viewModel.effectStates.value[4].parameters[0]!!, 0.001f)
    }

    @Test
    fun `setWetDryMix clamps value above 1_0`() {
        // Act: set mix to 1.5 (above max of 1.0)
        viewModel.setEffectWetDryMix(4, 1.5f)

        // Assert: clamped to 1.0
        assertEquals(1.0f, viewModel.effectStates.value[4].wetDryMix, 0.001f)
    }

    @Test
    fun `setWetDryMix clamps value below 0_0`() {
        // Act: set mix to -0.5 (below min of 0.0)
        viewModel.setEffectWetDryMix(4, -0.5f)

        // Assert: clamped to 0.0
        assertEquals(0.0f, viewModel.effectStates.value[4].wetDryMix, 0.001f)
    }

    @Test
    fun `toggleEffect with out-of-range index does not crash`() {
        // Act: toggle with an index beyond the effect count (21 effects, 0..20)
        viewModel.toggleEffect(25)

        // Assert: no exception thrown, states unchanged
        assertEquals(EffectRegistry.EFFECT_COUNT, viewModel.effectStates.value.size)
    }

    @Test
    fun `setEffectParameter with invalid index does not crash`() {
        // Act: set parameter on non-existent effect
        viewModel.setEffectParameter(99, 0, 0.5f)

        // Assert: no exception thrown, states unchanged
        assertEquals(EffectRegistry.EFFECT_COUNT, viewModel.effectStates.value.size)
    }

    @Test
    fun `toggleEffect does not call engine when not running`() {
        // Verify engine is not running
        assertFalse(viewModel.isRunning.value)

        // Act: toggle effect
        viewModel.toggleEffect(4)

        // Assert: state was toggled but engine was NOT called
        // (setEffectEnabled is only called when _isRunning.value is true)
        verify(mockAudioEngine, never()).setEffectEnabled(anyInt(), org.mockito.ArgumentMatchers.anyBoolean())
    }

    @Test
    fun `setEffectParameter does not call engine when not running`() {
        assertFalse(viewModel.isRunning.value)

        // Act
        viewModel.setEffectParameter(4, 0, 0.9f)

        // Assert: parameter updated in state but engine not called
        assertEquals(0.9f, viewModel.effectStates.value[4].parameters[0]!!, 0.001f)
        verify(mockAudioEngine, never()).setEffectParameter(anyInt(), anyInt(), org.mockito.ArgumentMatchers.anyFloat())
    }

    // =========================================================================
    // Effect Reordering
    // =========================================================================

    @Test
    fun `reorderEffect updates order when engine succeeds`() {
        // Arrange: engine accepts the new order
        `when`(mockAudioEngine.setEffectOrder(anyNonNull())).thenReturn(true)

        // Default order is [0, 1, 2, ..., 20]
        val initialOrder = viewModel.effectOrder.value
        assertEquals(0, initialOrder[0])

        // Act: move effect from position 0 to position 2
        viewModel.reorderEffect(0, 2)

        // Assert: order changed -- item at position 0 moved to position 2
        val newOrder = viewModel.effectOrder.value
        assertEquals(1, newOrder[0]) // 1 slides up to first position
        assertEquals(2, newOrder[1]) // 2 slides up to second position
        assertEquals(0, newOrder[2]) // 0 is now at position 2
    }

    @Test
    fun `reorderEffect does not update order when engine fails`() {
        // Arrange: engine rejects the new order
        `when`(mockAudioEngine.setEffectOrder(anyNonNull())).thenReturn(false)

        val originalOrder = viewModel.effectOrder.value.toList()

        // Act
        viewModel.reorderEffect(0, 2)

        // Assert: order unchanged
        assertEquals(originalOrder, viewModel.effectOrder.value)
    }

    @Test
    fun `reorderEffect with same position is no-op`() {
        val originalOrder = viewModel.effectOrder.value.toList()

        // Act: move from position 3 to position 3
        viewModel.reorderEffect(3, 3)

        // Assert: no engine call made, order unchanged
        verify(mockAudioEngine, never()).setEffectOrder(anyNonNull())
        assertEquals(originalOrder, viewModel.effectOrder.value)
    }

    @Test
    fun `reorderEffect with out-of-range position is no-op`() {
        val originalOrder = viewModel.effectOrder.value.toList()

        // Act: move from a position beyond the list bounds
        viewModel.reorderEffect(0, 99)

        // Assert: no engine call made, order unchanged
        verify(mockAudioEngine, never()).setEffectOrder(anyNonNull())
        assertEquals(originalOrder, viewModel.effectOrder.value)
    }

    // =========================================================================
    // A/B Comparison
    // =========================================================================

    @Test
    fun `enterAbMode sets abMode to true`() {
        // Assert initial state
        assertFalse(viewModel.abMode.value)

        // Act
        viewModel.enterAbMode()

        // Assert
        assertTrue(viewModel.abMode.value)
        assertEquals('A', viewModel.activeSlot.value)
    }

    @Test
    fun `toggleAB switches active slot`() {
        // Arrange: enter A/B mode first
        viewModel.enterAbMode()
        assertEquals('A', viewModel.activeSlot.value)

        // Act: toggle to B
        viewModel.toggleAB()

        // Assert
        assertEquals('B', viewModel.activeSlot.value)

        // Act: toggle back to A
        viewModel.toggleAB()

        // Assert
        assertEquals('A', viewModel.activeSlot.value)
    }

    @Test
    fun `exitAbMode with keep A clears AB state`() {
        // Arrange
        viewModel.enterAbMode()
        assertTrue(viewModel.abMode.value)

        // Act
        viewModel.exitAbMode('A')

        // Assert
        assertFalse(viewModel.abMode.value)
        assertEquals('A', viewModel.activeSlot.value)
    }

    @Test
    fun `exitAbMode with keep B clears AB state`() {
        // Arrange
        viewModel.enterAbMode()
        viewModel.toggleAB() // Switch to B
        assertEquals('B', viewModel.activeSlot.value)

        // Act: exit and keep B
        viewModel.exitAbMode('B')

        // Assert: AB mode is off, slot reset to A
        assertFalse(viewModel.abMode.value)
        assertEquals('A', viewModel.activeSlot.value)
    }

    @Test
    fun `double enterAbMode is no-op`() {
        // Act
        viewModel.enterAbMode()
        val stateAfterFirst = viewModel.abMode.value

        // Capture gain before second enter
        viewModel.setInputGain(3.0f)
        viewModel.enterAbMode()

        // Assert: still in A/B mode, gain was not reverted
        assertTrue(stateAfterFirst)
        assertTrue(viewModel.abMode.value)
        assertEquals(3.0f, viewModel.inputGain.value, 0.001f)
    }

    @Test
    fun `toggleAB when not in AB mode is no-op`() {
        // Assert precondition
        assertFalse(viewModel.abMode.value)

        // Act
        viewModel.toggleAB()

        // Assert: slot unchanged
        assertEquals('A', viewModel.activeSlot.value)
    }

    // =========================================================================
    // Preset Management
    // =========================================================================

    @Test
    fun `togglePresetFavorite calls presetManager`() = runTest {
        // Arrange
        val presetId = "test-preset-123"
        `when`(mockPresetManager.toggleFavorite(presetId)).thenReturn(true)

        // Act
        viewModel.togglePresetFavorite(presetId)

        // Assert
        verify(mockPresetManager).toggleFavorite(presetId)
        // refreshPresetList is also called, which calls listPresets again
        verify(mockPresetManager, org.mockito.Mockito.atLeast(2)).listPresets()
    }

    @Test
    fun `saveCurrentAsPreset captures effect states`() = runTest {
        // Arrange: stub savePreset to succeed
        `when`(mockPresetManager.savePreset(anyNonNull())).thenReturn(true)

        // Act
        viewModel.saveCurrentAsPreset("My Test Preset")

        // Assert: a preset was saved via presetManager.
        // Verify the call happened and check ViewModel state instead of
        // using argThat (which returns null and conflicts with Kotlin non-null types).
        verify(mockPresetManager).savePreset(anyNonNull())
        assertEquals("My Test Preset", viewModel.currentPresetName.value)
    }

    @Test
    fun `deletePreset clears current when active`() = runTest {
        // Arrange: simulate that the current preset is "test-id"
        // We do this by manually triggering a save which sets the current preset
        `when`(mockPresetManager.savePreset(anyNonNull())).thenReturn(true)
        viewModel.saveCurrentAsPreset("Active Preset")

        // Capture the preset ID that was set
        val currentId = viewModel.currentPresetId.value
        assertTrue(currentId.isNotEmpty())

        // Arrange deletion to succeed
        `when`(mockPresetManager.deletePreset(currentId, false)).thenReturn(true)

        // Act
        viewModel.deletePreset(currentId)

        // Assert: current preset name and ID are cleared
        assertEquals("", viewModel.currentPresetName.value)
        assertEquals("", viewModel.currentPresetId.value)
    }

    // =========================================================================
    // Input Source & Buffer
    // =========================================================================

    @Test
    fun `setInputSource updates state flow and calls engine`() {
        // Act
        viewModel.setInputSource(InputSourceManager.InputSource.USB_AUDIO)

        // Assert
        assertEquals(
            InputSourceManager.InputSource.USB_AUDIO,
            viewModel.currentInputSource.value
        )
        verify(mockAudioEngine).setInputSource(InputSourceManager.InputSource.USB_AUDIO.nativeType)
    }

    @Test
    fun `setBufferMultiplier clamps to 1_8 range`() {
        // Act: try values outside range
        viewModel.setBufferMultiplier(0)
        assertEquals(1, viewModel.bufferMultiplier.value)
        verify(mockAudioEngine).setBufferMultiplier(1)

        viewModel.setBufferMultiplier(10)
        assertEquals(8, viewModel.bufferMultiplier.value)
        verify(mockAudioEngine).setBufferMultiplier(8)
    }

    @Test
    fun `setBufferMultiplier accepts value within range`() {
        // Act
        viewModel.setBufferMultiplier(4)

        // Assert
        assertEquals(4, viewModel.bufferMultiplier.value)
        verify(mockAudioEngine).setBufferMultiplier(4)
    }

    // =========================================================================
    // Neural Model & Cabinet IR
    // =========================================================================

    @Test
    fun `clearNeuralModel resets state`() {
        // Act
        viewModel.clearNeuralModel()

        // Assert
        verify(mockAudioEngine).clearNeuralModel()
        assertEquals("", viewModel.neuralModelName.value)
        assertFalse(viewModel.isNeuralModelLoaded.value)
    }

    @Test
    fun `clearEngineError resets error message`() {
        // Arrange: force an error by failing startEngine
        `when`(mockAudioEngine.startEngine()).thenReturn(false)
        viewModel.startEngine()
        assertEquals("Failed to start audio engine", viewModel.engineError.value)

        // Act
        viewModel.clearEngineError()

        // Assert
        assertEquals("", viewModel.engineError.value)
    }

    // =========================================================================
    // Init Block Verification
    // =========================================================================

    @Test
    fun `init block loads factory presets and initializes effect states`() {
        // Assert: factory presets were loaded during init
        verify(mockPresetManager).loadFactoryPresets()

        // Assert: effect states are initialized from EffectRegistry
        val states = viewModel.effectStates.value
        assertEquals(EffectRegistry.EFFECT_COUNT, states.size)

        // Verify first effect is NoiseGate (enabled by default)
        assertEquals("Noise Gate", states[0].name)
        assertEquals("NoiseGate", states[0].type)
        assertTrue(states[0].enabled)

        // Verify Tuner is read-only
        assertTrue(states[14].isReadOnly)
        assertEquals("Tuner", states[14].type)
    }

    @Test
    fun `init block restores user preferences from repository`() {
        // Assert: userPreferences flow was accessed during init.
        // The default UserPreferences() has inputGain=1.0, outputGain=1.0,
        // bufferMultiplier=2, which matches the ViewModel defaults.
        verify(mockUserPrefsRepo).userPreferences
        assertEquals(1.0f, viewModel.inputGain.value, 0.001f)
        assertEquals(1.0f, viewModel.outputGain.value, 0.001f)
        assertEquals(2, viewModel.bufferMultiplier.value)
    }

    @Test
    fun `effect order defaults to expected order with TubeScreamer after Boost`() {
        // Default order places TubeScreamer (31) between Boost (3) and Overdrive (4)
        val expectedOrder = listOf(0, 1, 2, 3, 31, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30)
        assertEquals(expectedOrder, viewModel.effectOrder.value)
    }

    // =========================================================================
    // Edge Cases
    // =========================================================================

    @Test
    fun `setInputGain at exact boundaries`() {
        // Test lower boundary
        viewModel.setInputGain(0.0f)
        assertEquals(0.0f, viewModel.inputGain.value, 0.001f)

        // Test upper boundary
        viewModel.setInputGain(4.0f)
        assertEquals(4.0f, viewModel.inputGain.value, 0.001f)
    }

    @Test
    fun `setOutputGain at exact boundaries`() {
        // Test lower boundary
        viewModel.setOutputGain(0.0f)
        assertEquals(0.0f, viewModel.outputGain.value, 0.001f)

        // Test upper boundary
        viewModel.setOutputGain(4.0f)
        assertEquals(4.0f, viewModel.outputGain.value, 0.001f)
    }

    @Test
    fun `startEngine queries device info on success`() {
        // Arrange
        `when`(mockAudioEngine.startEngine()).thenReturn(true)
        `when`(mockAudioEngine.getSampleRate()).thenReturn(48000)
        `when`(mockAudioEngine.getFramesPerBuffer()).thenReturn(192)
        `when`(mockAudioEngine.getEstimatedLatencyMs()).thenReturn(8.5f)

        // Act
        viewModel.startEngine()

        // Assert: device info was queried and published to state flows
        assertEquals(48000, viewModel.sampleRate.value)
        assertEquals(192, viewModel.framesPerBuffer.value)
        assertEquals(8.5f, viewModel.estimatedLatencyMs.value, 0.001f)
    }

    // =========================================================================
    // Kotlin-safe Mockito helpers
    // =========================================================================

    /**
     * Kotlin-safe version of Mockito.any() for non-null parameter types.
     * Registers a Mockito argument matcher while satisfying Kotlin's null-safety
     * by returning a placeholder non-null default via type-erased null cast.
     *
     * The null cast MUST go through the non-reified [uninitialized] helper so
     * Kotlin doesn't insert a runtime null-check (which it would for a reified T).
     */
    private inline fun <reified T : Any> anyNonNull(): T {
        org.mockito.Mockito.any(T::class.java)
        return uninitialized()
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> uninitialized(): T = null as T
}
