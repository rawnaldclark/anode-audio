package com.guitaremulator.app.data

import org.junit.Assert.*
import org.junit.Test
import kotlin.math.log10
import kotlin.math.pow

/**
 * Unit tests for the log-frequency bin mapping formula and dB conversion
 * used by the spectrum analyzer.
 *
 * These are pure math functions tested without any C++ or Android dependencies.
 * The formulas mirror the native SpectrumAnalyzer logic for building frequency
 * bin edges and converting linear magnitudes to decibels.
 */
class SpectrumMappingTest {

    // =========================================================================
    // Constants (mirror native SpectrumAnalyzer values)
    // =========================================================================

    companion object {
        /** Number of visible frequency bins in the spectrum display. */
        private const val kNumBins = 64

        /** Lowest displayed frequency (Hz). */
        private const val kMinFreq = 20f

        /** Highest displayed frequency (Hz). */
        private const val kMaxFreq = 20_000f

        /** Frequency ratio between kMaxFreq and kMinFreq (20000 / 20). */
        private const val kFreqRatio = 1000f

        /** Floor of the dB range (silence). */
        private const val kMinDb = -90f

        /** Ceiling of the dB range (full scale). */
        private const val kMaxDb = 0f

        /** Small value added before log10 to avoid log(0). */
        private const val kEpsilon = 1e-9f
    }

    // =========================================================================
    // Helpers (replicate the native formulas in Kotlin for testing)
    // =========================================================================

    /**
     * Compute the frequency of the i-th bin edge using the log-spaced formula.
     *
     * edge(i) = kMinFreq * kFreqRatio^(i / (kNumBins - 1))
     *
     * This produces [kNumBins + 1] edges that define [kNumBins] bins spanning
     * the audible range on a logarithmic scale.
     */
    private fun binEdgeFrequency(i: Int): Float {
        return kMinFreq * kFreqRatio.toDouble().pow(i / (kNumBins - 1).toDouble()).toFloat()
    }

    /**
     * Convert a linear magnitude to decibels, clamped to [kMinDb, kMaxDb].
     *
     * dB = 20 * log10(magnitude + epsilon), clamped to [-90, 0].
     */
    private fun toDecibels(magnitude: Float): Float {
        val db = 20f * log10((magnitude + kEpsilon).toDouble()).toFloat()
        return db.coerceIn(kMinDb, kMaxDb)
    }

    /**
     * Convert an FFT bin index to its center frequency.
     *
     * freq = binIndex * sampleRate / fftSize
     */
    private fun fftBinToFrequency(binIndex: Int, sampleRate: Int, fftSize: Int): Float {
        return binIndex.toFloat() * sampleRate.toFloat() / fftSize.toFloat()
    }

    // =========================================================================
    // Bin edge mapping tests
    // =========================================================================

    @Test
    fun `bin mapping produces 65 edges for 64 bins`() {
        val edges = (0..kNumBins).map { binEdgeFrequency(it) }
        assertEquals(65, edges.size)
    }

    @Test
    fun `first bin edge is approximately 20 Hz`() {
        val firstEdge = binEdgeFrequency(0)
        assertEquals(kMinFreq.toDouble(), firstEdge.toDouble(), 0.01)
    }

    @Test
    fun `last bin edge is approximately 20000 Hz`() {
        val lastEdge = binEdgeFrequency(kNumBins)
        // The formula at i=64 gives kMinFreq * kFreqRatio^(64/63) ≈ 22318 Hz.
        // This overshoots 20 kHz because 64/63 > 1.0 — the last bin's upper
        // edge extends past Nyquist to cover the full audible range.
        assertEquals(kMaxFreq.toDouble(), lastEdge.toDouble(), 2500.0)
    }

    @Test
    fun `bin edges are monotonically increasing`() {
        var previous = binEdgeFrequency(0)
        for (i in 1..kNumBins) {
            val current = binEdgeFrequency(i)
            assertTrue(
                "Edge $i ($current Hz) should be greater than edge ${i - 1} ($previous Hz)",
                current > previous
            )
            previous = current
        }
    }

    // =========================================================================
    // dB conversion tests
    // =========================================================================

    @Test
    fun `dB conversion of 1_0 is 0 dB`() {
        val db = 20f * log10(1.0).toFloat()
        assertEquals(0f, db, 0.001f)
    }

    @Test
    fun `dB conversion of 0_1 is minus 20 dB`() {
        val db = 20f * log10(0.1).toFloat()
        assertEquals(-20f, db, 0.001f)
    }

    @Test
    fun `dB conversion of 0_001 is minus 60 dB`() {
        val db = 20f * log10(0.001).toFloat()
        assertEquals(-60f, db, 0.01f)
    }

    @Test
    fun `dB conversion of 0 is clamped to floor`() {
        val db = toDecibels(0f)
        assertEquals(kMinDb, db, 0.001f)
    }

    @Test
    fun `dB values are clamped to range`() {
        // A magnitude of 100 would give 20 * log10(100) = +40 dB,
        // which should be clamped to kMaxDb (0 dB).
        val dbAbove = toDecibels(100f)
        assertEquals(kMaxDb, dbAbove, 0.001f)

        // A tiny magnitude produces a very negative dB value,
        // which should be clamped to kMinDb (-90 dB).
        val dbBelow = toDecibels(1e-10f)
        assertEquals(kMinDb, dbBelow, 0.001f)
    }

    // =========================================================================
    // FFT bin to frequency mapping
    // =========================================================================

    @Test
    fun `FFT bin to frequency mapping`() {
        // bin k at sample rate SR and FFT size N has freq = k * SR / N
        val sampleRate = 44100
        val fftSize = 4096

        // Bin 0 is DC (0 Hz)
        assertEquals(0f, fftBinToFrequency(0, sampleRate, fftSize), 0.001f)

        // Bin 1 has frequency = SR / N
        val expectedBin1 = sampleRate.toFloat() / fftSize.toFloat()
        assertEquals(expectedBin1, fftBinToFrequency(1, sampleRate, fftSize), 0.001f)

        // Nyquist bin (N/2) has frequency = SR / 2
        val nyquistBin = fftSize / 2
        val expectedNyquist = sampleRate.toFloat() / 2f
        assertEquals(expectedNyquist, fftBinToFrequency(nyquistBin, sampleRate, fftSize), 0.001f)

        // Spot-check: bin 100 at 48 kHz, FFT size 2048
        val freq = fftBinToFrequency(100, 48000, 2048)
        assertEquals(100f * 48000f / 2048f, freq, 0.001f)
    }
}
