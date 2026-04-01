// tremolo.dsp -- Amplitude modulation (tremolo) effect
//
// A classic tremolo effect that modulates the signal amplitude using an LFO.
// The LFO shape blends between sine and square waveforms.
//
// Parameters:
//   Rate  : LFO frequency in Hz (0.5 - 20.0, default 4.0)
//   Depth : Modulation depth (0.0 - 1.0, default 0.5)
//   Shape : LFO waveform (0.0 = pure sine, 1.0 = pure square, default 0.0)
//
// To compile with FAUST:
//   faust -lang cpp -cn FaustTremolo -single -inpl -nvi \
//         -a ../guitar_emulator.arch tremolo.dsp -o ../generated/FaustTremolo.h
//
// Or without architecture file (self-contained with -i):
//   faust -lang cpp -cn FaustTremolo -i -single -inpl -nvi \
//         tremolo.dsp -o ../generated/FaustTremolo.h

import("stdfaust.lib");

rate  = hslider("Rate", 4.0, 0.5, 20.0, 0.1);
depth = hslider("Depth", 0.5, 0.0, 1.0, 0.01);
shape = hslider("Shape", 0.0, 0.0, 1.0, 0.01);

// LFO: blend between sine and square, normalized to [0, 1]
sine_lfo = (os.osc(rate) + 1.0) / 2.0;
square_lfo = (os.square(rate) + 1.0) / 2.0;
lfo = sine_lfo * (1.0 - shape) + square_lfo * shape;

// Tremolo: modulate amplitude between (1 - depth) and 1.0
process = _ * (1.0 - depth * lfo);
