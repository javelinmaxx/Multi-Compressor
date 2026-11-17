# Qin Synth control hints

| Control | Hint |
| --- | --- |
| Plugin title | Developer: J-Sonic Team; Year: 2026; Version: 0.7.5 |
| OSC 1/2/3 tabs | Opens the selected oscillator in the OSC panel. |
| Voices | Drag up or down to set the maximum number of simultaneous notes from one to sixteen. |
| Active Voices | Shows the number of allocated voices, including release tails. |
| Poly Legato | Enables voice matching and pitch slides when polyphony exceeds one voice. |
| Voice Match | Closest and Farthest use pitch distance, while Upwards and Downwards prefer voices below or above the new note. |
| Slide | Sets mono and polyphonic legato slide time from instant to two seconds. |
| Shape | Morphs the pitch glide from logarithmic through linear to sigmoid. |
| Retrig | Restarts this oscillator's waveform phase at each note onset when enabled. |
| N | Enables the noise oscillator and opens its tuning, filter, and style controls. |
| Noise Style | Selects white, pink, or sparse velvet noise before the tuned filter. |
| Noise Level | Sets the noise oscillator level independently of the three tonal oscillators. |
| Noise Tune | Shifts the note-tracked noise resonance by up to two octaves in semitones. |
| Noise HPF | Removes noise below the selected high-pass frequency. |
| Noise Pan | Positions the noise oscillator independently in stereo. |
| AMP tab | Opens the amplitude envelope. |
| FLT tab | Opens the filter envelope. |
| MOD tab | Opens the effect modulation envelope. |
| DRIVE tab | Opens the drive controls. |
| Drive Circuit | Selects BJT, op-amp, diode shunt, or series crossover drive from Zao. |
| Transformer | Adds Ren's transformer to the output and saves CPU when off. |
| DELAY tab | Opens Lan's delay and reverb controls. |
| Osc Wave | Drag within the square to blend the selected oscillator between saw, pulse, triangle, and sine waves. |
| Osc Level | Sets the selected oscillator's contribution to the mix. |
| Osc Octave | Transposes the selected oscillator by whole octaves. |
| Osc Unison | Sets the number of copies of the selected oscillator. |
| Osc Detune | Offsets the selected oscillator's unison copies in cents. |
| Osc Detune Shape | Moves inner unison pitches toward center at -100% or toward the outer voices at +100%. |
| Osc Spread | Sets how far the selected oscillator's detuned copies spread across stereo. |
| Osc Pan | Positions the selected oscillator as a whole in stereo. |
| AMP Attack | Sets the amplitude envelope rise from 1 to 5000 ms on a logarithmic scale. |
| AMP Decay | Sets the amplitude envelope decay from 1 to 5000 ms on a logarithmic scale. |
| AMP Sustain | Sets amplitude while the key remains held. |
| AMP Release | Sets the amplitude envelope release from 1 to 5000 ms on a logarithmic scale. |
| AMP Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| FLT Attack | Sets the filter envelope rise from 1 to 5000 ms with logarithmic control. |
| FLT Decay | Sets the filter envelope decay from 1 to 5000 ms with logarithmic control. |
| FLT Sustain | Sets the held filter-envelope level with finer control near zero. |
| FLT Release | Sets the filter envelope release from 1 to 5000 ms with logarithmic control. |
| *5 | Multiplies Attack, Decay, and Release of all three envelopes by five while enabled, up to 25000 ms. |
| FLT Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| MOD Attack | Sets the modulation envelope rise from 1 to 5000 ms on a logarithmic scale. |
| MOD Decay | Sets the modulation envelope decay from 1 to 5000 ms on a logarithmic scale. |
| MOD Sustain | Sets the held level of effect modulation. |
| MOD Release | Sets the modulation envelope release from 1 to 5000 ms on a logarithmic scale. |
| MOD Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| MOD Depth | Sets how strongly the modulation envelope affects its destination. |
| MOD Target | Routes the modulation envelope to Drive, Spread, Pitch, or Filter cutoff. |
| FLT Type | Selects the TB-303, 18 dB Sallen-Key, or 12/24 dB digital filter model. |
| FLT Freq | Sets the filter's base cutoff on a logarithmic frequency scale. |
| FLT Resonance | Emphasizes the cutoff with finer control at high resonance. |
| FLT Key Follow | Raises the cutoff with MIDI note pitch. |
| FLT Env Amount | Sets how far the filter envelope moves the cutoff. |
| FLT Merge | Blends per-voice filters into one shared master filter at 100%. |
| Key Pan | Sets pitch panning from high notes left at -100% to high notes right at +100%. |
| Output | Sets the final synth level. |
| Drive | Sets how hard the oscillator signal drives the selected circuit. |
| Drive Mix | Blends the driven signal with the dry oscillator signal. |
| Time | Sets Lan's delay and reverb time. |
| Decay | Sets how long Lan's feedback tail lasts. |
| Scatter | Morphs Lan's delay into a diffuse reverb. |
| Mix | Blends the Lan effect with the synth. |
| Tap Motion | Sets how far note onsets move Lan's feedback taps. |
| Phase Motion | Sets how strongly note onsets change Lan's phase scatter. |
