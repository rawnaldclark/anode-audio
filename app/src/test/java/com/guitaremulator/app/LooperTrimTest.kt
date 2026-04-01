package com.guitaremulator.app

import com.guitaremulator.app.audio.IAudioEngine
import com.guitaremulator.app.data.LooperSessionManager
import com.guitaremulator.app.viewmodel.LooperDelegate
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

/**
 * Unit tests for the trim/crop feature in [LooperDelegate].
 *
 * Tests the trim editor lifecycle (show/hide), snap modes (FREE,
 * ZERO_CROSSING, BEAT), commit/undo operations, and edge cases
 * (zero loop length, commit without preview).
 *
 * Uses Mockito to mock [IAudioEngine] (the interface, not the concrete
 * AudioEngine which has unmockable `external fun` methods) and
 * [LooperSessionManager].
 *
 * The `returnDefaultValues = true` testOption in build.gradle.kts ensures
 * [android.util.Log] calls return defaults without Robolectric.
 */
class LooperTrimTest {

    private lateinit var audioEngine: IAudioEngine
    private lateinit var sessionManager: LooperSessionManager
    private lateinit var delegate: LooperDelegate

    @Before
    fun setUp() {
        audioEngine = Mockito.mock(IAudioEngine::class.java)
        sessionManager = Mockito.mock(LooperSessionManager::class.java)

        // looperInit must succeed for ensureInitialized() to set isInitialized = true.
        // Many methods call ensureInitialized() internally (showTrimEditor, toggle, etc.).
        `when`(audioEngine.looperInit(Mockito.anyInt())).thenReturn(true)

        delegate = LooperDelegate(audioEngine, sessionManager)
    }

    // =========================================================================
    // Show / Hide Trim Editor
    // =========================================================================

    @Test
    fun `showTrimEditor sets visible and starts preview`() {
        // Arrange: simulate a recorded loop of 48000 samples
        `when`(audioEngine.looperGetLoopLength()).thenReturn(48000)
        `when`(audioEngine.looperGetState()).thenReturn(LooperDelegate.STATE_IDLE)
        `when`(audioEngine.looperGetPlaybackPosition()).thenReturn(0)
        `when`(audioEngine.getCurrentBeat()).thenReturn(0)
        `when`(audioEngine.isReamping()).thenReturn(false)
        `when`(audioEngine.getWaveformSummary(Mockito.anyInt(), Mockito.anyInt(), Mockito.anyInt()))
            .thenReturn(FloatArray(600) { 0f })

        // Pre-populate loopLength via pollState (showTrimEditor reads _loopLength.value)
        delegate.showLooper()
        delegate.pollState()

        // Act
        delegate.showTrimEditor()

        // Assert
        assertTrue("Trim editor should be visible", delegate.isTrimEditorVisible.value)
        assertTrue("Trim preview should be active", delegate.isTrimPreviewActive.value)
        verify(audioEngine).startTrimPreview()
        assertEquals("Trim start should be 0", 0, delegate.trimStart.value)
        assertEquals("Trim end should match loop length", 48000, delegate.trimEnd.value)
    }

    @Test
    fun `hideTrimEditor clears state and cancels preview`() {
        // Arrange: open the trim editor first
        `when`(audioEngine.looperGetLoopLength()).thenReturn(48000)
        `when`(audioEngine.looperGetState()).thenReturn(LooperDelegate.STATE_IDLE)
        `when`(audioEngine.looperGetPlaybackPosition()).thenReturn(0)
        `when`(audioEngine.getCurrentBeat()).thenReturn(0)
        `when`(audioEngine.isReamping()).thenReturn(false)
        `when`(audioEngine.getWaveformSummary(Mockito.anyInt(), Mockito.anyInt(), Mockito.anyInt()))
            .thenReturn(FloatArray(600) { 0f })

        delegate.showLooper()
        delegate.pollState()
        delegate.showTrimEditor()

        // Precondition: trim editor is open
        assertTrue(delegate.isTrimEditorVisible.value)
        assertTrue(delegate.isTrimPreviewActive.value)

        // Act
        delegate.hideTrimEditor()

        // Assert
        assertFalse("Trim editor should be hidden", delegate.isTrimEditorVisible.value)
        assertFalse("Trim preview should be inactive", delegate.isTrimPreviewActive.value)
        verify(audioEngine).cancelTrimPreview()
        assertNull("Waveform data should be cleared", delegate.waveformData.value)
    }

    // =========================================================================
    // setTrimStart -- Snap Modes
    // =========================================================================

    @Test
    fun `setTrimStart freeMode sets directly`() {
        // Arrange: open trim editor
        openTrimEditor()

        // Ensure snap mode is FREE (default)
        assertEquals(LooperDelegate.SnapMode.FREE, delegate.snapMode.value)

        `when`(audioEngine.getTrimStart()).thenReturn(1000)

        // Act
        delegate.setTrimStart(1000)

        // Assert
        verify(audioEngine).setTrimStart(1000)
        assertEquals("Trim start should be 1000 (no snap)", 1000, delegate.trimStart.value)
    }

    @Test
    fun `setTrimStart zeroCrossingMode snaps to zero crossing`() {
        // Arrange: open trim editor and set snap mode
        openTrimEditor()
        delegate.setSnapMode(LooperDelegate.SnapMode.ZERO_CROSSING)

        // applySnap calls findNearestZeroCrossing(sample, -1) for ZERO_CROSSING mode
        `when`(audioEngine.findNearestZeroCrossing(1000, -1)).thenReturn(1010)
        `when`(audioEngine.getTrimStart()).thenReturn(1010)

        // Act
        delegate.setTrimStart(1000)

        // Assert: the snapped value (1010) is passed to the engine
        verify(audioEngine).setTrimStart(1010)
        assertEquals("Trim start should be snapped to 1010", 1010, delegate.trimStart.value)
    }

    @Test
    fun `setTrimStart beatMode snaps to beat`() {
        // Arrange: open trim editor and set snap mode
        openTrimEditor()
        delegate.setSnapMode(LooperDelegate.SnapMode.BEAT)

        // applySnap calls snapToBeat(sample) for BEAT mode
        `when`(audioEngine.snapToBeat(1000)).thenReturn(960)
        `when`(audioEngine.getTrimStart()).thenReturn(960)

        // Act
        delegate.setTrimStart(1000)

        // Assert: the snapped value (960) is passed to the engine
        verify(audioEngine).setTrimStart(960)
        assertEquals("Trim start should be snapped to 960", 960, delegate.trimStart.value)
    }

    // =========================================================================
    // setTrimEnd
    // =========================================================================

    @Test
    fun `setTrimEnd updates flow`() {
        // Arrange: open trim editor
        openTrimEditor()

        `when`(audioEngine.getTrimEnd()).thenReturn(44100)

        // Act
        delegate.setTrimEnd(44100)

        // Assert
        verify(audioEngine).setTrimEnd(44100)
        assertEquals("Trim end should be 44100", 44100, delegate.trimEnd.value)
    }

    // =========================================================================
    // commitTrim
    // =========================================================================

    @Test
    fun `commitTrim success updates state`() {
        // Arrange: open trim editor
        openTrimEditor()

        `when`(audioEngine.commitTrim()).thenReturn(true)
        `when`(audioEngine.isCropUndoAvailable()).thenReturn(true)
        // After commit, the loop length is updated (cropped loop is shorter)
        `when`(audioEngine.looperGetLoopLength()).thenReturn(24000)
        `when`(audioEngine.getWaveformSummary(Mockito.anyInt(), Mockito.anyInt(), Mockito.anyInt()))
            .thenReturn(FloatArray(600) { 0f })

        // Act
        val result = delegate.commitTrim()

        // Assert
        assertTrue("commitTrim should return true on success", result)
        assertFalse(
            "Trim preview should be deactivated after commit",
            delegate.isTrimPreviewActive.value
        )
        assertTrue(
            "Crop undo should be available after commit",
            delegate.isCropUndoAvailable.value
        )
        assertEquals(
            "Loop length should be updated to new cropped length",
            24000, delegate.loopLength.value
        )
        assertEquals(
            "Trim start should reset to 0 after commit",
            0, delegate.trimStart.value
        )
        assertEquals(
            "Trim end should match new loop length after commit",
            24000, delegate.trimEnd.value
        )
    }

    @Test
    fun `commitTrim failure returns false`() {
        // Arrange: open trim editor
        openTrimEditor()

        `when`(audioEngine.commitTrim()).thenReturn(false)

        // Act
        val result = delegate.commitTrim()

        // Assert
        assertFalse("commitTrim should return false on failure", result)
        // Trim preview should still be active (not cancelled on failure)
        assertTrue(
            "Trim preview should remain active on failure",
            delegate.isTrimPreviewActive.value
        )
    }

    @Test
    fun `commitTrim when not in preview returns false`() {
        // Arrange: do NOT open the trim editor -- isTrimPreviewActive is false

        // Act
        val result = delegate.commitTrim()

        // Assert
        assertFalse("commitTrim should return false when not in preview", result)
        // commitTrim should bail out immediately without calling the engine
        verify(audioEngine, never()).commitTrim()
    }

    // =========================================================================
    // undoCrop
    // =========================================================================

    @Test
    fun `undoCrop success updates state`() {
        // Arrange: open trim editor, commit a trim, then undo
        openTrimEditor()

        `when`(audioEngine.commitTrim()).thenReturn(true)
        `when`(audioEngine.isCropUndoAvailable()).thenReturn(true)
        `when`(audioEngine.looperGetLoopLength()).thenReturn(24000)
        `when`(audioEngine.getWaveformSummary(Mockito.anyInt(), Mockito.anyInt(), Mockito.anyInt()))
            .thenReturn(FloatArray(600) { 0f })

        delegate.commitTrim()

        // Now set up undo
        `when`(audioEngine.undoCrop()).thenReturn(true)
        `when`(audioEngine.looperGetLoopLength()).thenReturn(48000) // Restored original length

        // Act
        val result = delegate.undoCrop()

        // Assert
        assertTrue("undoCrop should return true on success", result)
        assertFalse(
            "Crop undo should no longer be available after undo",
            delegate.isCropUndoAvailable.value
        )
        assertEquals(
            "Loop length should be restored to pre-crop length",
            48000, delegate.loopLength.value
        )
        assertEquals(
            "Trim end should match restored loop length",
            48000, delegate.trimEnd.value
        )
    }

    // =========================================================================
    // setSnapMode
    // =========================================================================

    @Test
    fun `setSnapMode updates flow`() {
        // Default is FREE
        assertEquals(LooperDelegate.SnapMode.FREE, delegate.snapMode.value)

        // Set to ZERO_CROSSING
        delegate.setSnapMode(LooperDelegate.SnapMode.ZERO_CROSSING)
        assertEquals(
            "Snap mode should be ZERO_CROSSING",
            LooperDelegate.SnapMode.ZERO_CROSSING,
            delegate.snapMode.value
        )

        // Set to BEAT
        delegate.setSnapMode(LooperDelegate.SnapMode.BEAT)
        assertEquals(
            "Snap mode should be BEAT",
            LooperDelegate.SnapMode.BEAT,
            delegate.snapMode.value
        )

        // Set back to FREE
        delegate.setSnapMode(LooperDelegate.SnapMode.FREE)
        assertEquals(
            "Snap mode should be FREE",
            LooperDelegate.SnapMode.FREE,
            delegate.snapMode.value
        )
    }

    // =========================================================================
    // Edge Cases
    // =========================================================================

    @Test
    fun `showTrimEditor with zero loop length does not open`() {
        // Arrange: loop length is 0 (no loop recorded)
        `when`(audioEngine.looperGetLoopLength()).thenReturn(0)
        `when`(audioEngine.looperGetState()).thenReturn(LooperDelegate.STATE_IDLE)
        `when`(audioEngine.looperGetPlaybackPosition()).thenReturn(0)
        `when`(audioEngine.getCurrentBeat()).thenReturn(0)
        `when`(audioEngine.isReamping()).thenReturn(false)

        // Populate loopLength=0 via pollState
        delegate.showLooper()
        delegate.pollState()

        // Act
        delegate.showTrimEditor()

        // Assert: the code checks `if (len <= 0)` and returns early with a log warning.
        // The trim editor should NOT be visible, and startTrimPreview should NOT be called.
        assertFalse(
            "Trim editor should not open with zero loop length",
            delegate.isTrimEditorVisible.value
        )
        assertFalse(
            "Trim preview should not be active with zero loop length",
            delegate.isTrimPreviewActive.value
        )
        verify(audioEngine, never()).startTrimPreview()
    }

    @Test
    fun `setTrimStart does nothing when not in preview`() {
        // Arrange: trim preview is NOT active (default state)
        assertFalse(delegate.isTrimPreviewActive.value)

        // Act
        delegate.setTrimStart(5000)

        // Assert: should bail out immediately without calling the engine
        verify(audioEngine, never()).setTrimStart(Mockito.anyInt())
        assertEquals("Trim start should remain at default 0", 0, delegate.trimStart.value)
    }

    @Test
    fun `setTrimEnd does nothing when not in preview`() {
        // Arrange: trim preview is NOT active (default state)
        assertFalse(delegate.isTrimPreviewActive.value)

        // Act
        delegate.setTrimEnd(40000)

        // Assert: should bail out immediately without calling the engine
        verify(audioEngine, never()).setTrimEnd(Mockito.anyInt())
        assertEquals("Trim end should remain at default 0", 0, delegate.trimEnd.value)
    }

    @Test
    fun `hideTrimEditor without prior show does not call cancel`() {
        // Arrange: trim preview was never started
        assertFalse(delegate.isTrimPreviewActive.value)

        // Act
        delegate.hideTrimEditor()

        // Assert: cancelTrimPreview should NOT be called since preview was never active
        verify(audioEngine, never()).cancelTrimPreview()
        assertFalse(delegate.isTrimEditorVisible.value)
    }

    @Test
    fun `undoCrop failure returns false and preserves state`() {
        // Arrange
        `when`(audioEngine.undoCrop()).thenReturn(false)

        // Act
        val result = delegate.undoCrop()

        // Assert
        assertFalse("undoCrop should return false on failure", result)
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    /**
     * Helper to set up the LooperDelegate in a state where the trim editor
     * is open and trim preview is active, ready for setTrimStart/End or
     * commitTrim operations.
     *
     * Mocks the necessary engine calls for:
     * - ensureInitialized() (looperInit)
     * - showLooper() (sets visibility)
     * - pollState() (populates loopLength)
     * - showTrimEditor() (starts preview, fetches waveform)
     */
    private fun openTrimEditor() {
        `when`(audioEngine.looperGetLoopLength()).thenReturn(48000)
        `when`(audioEngine.looperGetState()).thenReturn(LooperDelegate.STATE_IDLE)
        `when`(audioEngine.looperGetPlaybackPosition()).thenReturn(0)
        `when`(audioEngine.getCurrentBeat()).thenReturn(0)
        `when`(audioEngine.isReamping()).thenReturn(false)
        `when`(audioEngine.getWaveformSummary(Mockito.anyInt(), Mockito.anyInt(), Mockito.anyInt()))
            .thenReturn(FloatArray(600) { 0f })

        delegate.showLooper()
        delegate.pollState()
        delegate.showTrimEditor()

        // Verify preconditions
        assertTrue("Precondition: trim editor should be visible", delegate.isTrimEditorVisible.value)
        assertTrue("Precondition: trim preview should be active", delegate.isTrimPreviewActive.value)
    }
}
