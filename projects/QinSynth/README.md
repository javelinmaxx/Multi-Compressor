# Qin Synth

Qin Synth is a stereo MIDI instrument with 16-note polyphony, three virtual-analog oscillators per note, and up to eight unison copies per oscillator. It is version 0.7.5 in the J-Sonic suite.

![Qin Synth with OSC, ENV, FLT and tabbed FX panels](Previews/qin-synth.png)

![Qin Synth with Zao Drive selected in FX](Previews/qin-synth-fx.png)

![Qin Synth with MIDI-steered Lan delay-reverb selected in FX](Previews/qin-synth-delay.png)

![Qin Synth with MOD envelope and effect destination](Previews/qin-synth-mod.png)

Qin follows the section hierarchy of Spire: a tabbed **OSC** sound module on the left, **FLT** above **ENV** in the center, and tabbed **FX** on the right. OSC, FLT, and ENV stay visible while FX switches between Drive and Delay. Every control remains available to host automation.

Each oscillator has a band-limited saw, pulse, or triangle waveform, with level, octave, unison count, detune, stereo spread, and whole-oscillator pan in the same OSC panel. Spread positions detuned copies around the oscillator's pan center. Key Pan separately maps lower MIDI notes left and higher notes right, as in a piano-key recording.

ENV has three independent ADSR envelopes. AMP shapes voice amplitude. FLT shifts the cutoff by up to four octaves in either direction, set by the FLT panel's bipolar Env Amount. MOD can target voice drive or unison spread; its Depth sets the amount. FLT provides cutoff, resonance, key follow, Env Amount, and a choice of bypass, a nonlinear TB-303-style four-pole ladder, an 18 dB/oct resonant Sallen-Key-style section, and 12 or 24 dB/oct digital state-variable low-passes. Cutoff changes glide over 5 ms per voice. New notes take a free voice first, then steal a released or oldest voice when all 16 are occupied. Sustain pedal (MIDI CC64), note-off, all-notes-off, and all-sound-off are supported.

The Drive control uses Zao Drive's op-amp feedback clipper circuit. The delay-reverb reuses Lan Reverb's eight-line feedback network and Scatter control. Qin disables Lan's spectral resonance analyser and tamer. A MIDI note-on changes the feedback tap target and phase-scatter depth according to note pitch, velocity, and alternating onset direction; Tap Motion and Phase Motion set the amount. Lan's existing tap crossfades and phase smoothing soften these changes. No audio-spectrum detection controls the tap or phase.

The instrument outputs stereo VST3 and Standalone formats on Windows, macOS, and Linux, plus AU on macOS. The shared suite version is defined in `../PluginVersion.cmake`.

Build the native processor tests with:

```bash
cmake -S projects/QinSynth -B build/qin -DCMAKE_BUILD_TYPE=Release
cmake --build build/qin --target QinSynthTests QinSynth_VST3
```

The tests cover MIDI sample offsets, the 16-voice limit, three oscillators at eightfold unison, oscillator and key-based stereo placement, all filter modes, release, Zao drive, Lan's delay tail, the MIDI-only reverb steering path, and editor tabs.
