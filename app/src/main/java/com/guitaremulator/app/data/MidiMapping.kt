package com.guitaremulator.app.data

import java.util.UUID

/**
 * Curve type for mapping MIDI CC values to parameter ranges.
 *
 * Controls how the raw CC value (0-127) is interpolated between
 * [MidiMapping.minValue] and [MidiMapping.maxValue]:
 * - [LINEAR]: Straight-line interpolation (CC/127).
 * - [LOGARITHMIC]: Logarithmic curve, more resolution at low CC values.
 * - [EXPONENTIAL]: Exponential curve, more resolution at high CC values.
 */
enum class MappingCurve {
    LINEAR,
    LOGARITHMIC,
    EXPONENTIAL
}

/**
 * Maps a MIDI CC message to an effect parameter.
 *
 * When a MIDI CC message matching [ccNumber] and [midiChannel] is received,
 * the CC value (0-127) is scaled to [minValue]..[maxValue] using the
 * specified [curve] and applied to the target effect parameter.
 *
 * @property id Unique identifier (UUID string).
 * @property ccNumber MIDI CC number (0-127).
 * @property midiChannel MIDI channel (0-15, where 0 = omni/any channel).
 * @property targetEffectIndex Index of the target effect in the signal chain (0-20).
 * @property targetParamId Parameter ID within the target effect.
 * @property minValue Parameter value when CC = 0.
 * @property maxValue Parameter value when CC = 127.
 * @property curve Mapping curve type.
 */
data class MidiMapping(
    val id: String = UUID.randomUUID().toString(),
    val ccNumber: Int,
    val midiChannel: Int = 0,
    val targetEffectIndex: Int,
    val targetParamId: Int,
    val minValue: Float,
    val maxValue: Float,
    val curve: MappingCurve = MappingCurve.LINEAR
)
