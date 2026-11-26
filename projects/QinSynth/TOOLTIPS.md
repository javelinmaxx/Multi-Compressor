# Qin Synth control hints

| Control | Hint |
| --- | --- |
| Plugin title | Developer: J-Sonic Team; Year: 2026; Version: 0.7.5 |
| Preset Menu | Opens factory and saved presets, with commands to save or delete your own presets. |
| Preset Name | Shows the current preset name saved with this project. |
| Previous Preset | Loads the previous preset in the list. |
| Next Preset | Loads the next preset in the list. |
| OSC 1/2/3 tabs | Opens the selected oscillator in the OSC panel. |
| Voices | Drag up or down to set the maximum number of simultaneous notes from one to sixteen. |
| TUNE | Opens the custom tuning table and A4 reference controls. |
| Tuning root | Transposes the deviation pattern to this pitch class and locks it at zero cents. |
| Tuning A4 | Sets the reference frequency for A4 from 400 to 480 Hz. |
| Tuning key | Drag this piano key to adjust its pitch deviation in cents. |
| Root key | The selected root is locked to zero cents. |
| Active Voices | Shows the number of allocated voices, including release tails. |
| Poly Legato | Enables voice matching and pitch slides when polyphony exceeds one voice. |
| Voice Match | Closest and Farthest use pitch distance, while Upwards and Downwards prefer voices below or above the new note. |
| Slide | Sets mono and polyphonic legato slide time from instant to two seconds. |
| Shape | Morphs the pitch glide from logarithmic through linear to sigmoid. |
| Retrig | Restarts this oscillator's waveform phase at each note onset when enabled. |
| N | Enables the noise oscillator and opens its tuning, filter, and style controls. |
| Noise Style | Selects white, pink, or sparse velvet noise before the tuned filter. |
| Noise Level | Sets the noise oscillator level, shown in dB from silence to 0 dB. |
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
| Osc Duty | Changes pulse width, narrows triangle peaks, and adds a flat end to saw without changing sine. |
| OSC Phase Start | Drag the phase line to choose where this oscillator starts when Retrig is enabled. |
| Osc Level | Drag to set this oscillator's level from silence to 0 dB; double-click to reset. |
| Osc Octave | Transposes the selected oscillator by whole octaves. |
| Osc Fine | Drag slowly to shift this oscillator by up to 700 cents in either direction; double-click to reset. |
| Osc Unison | Sets the number of copies of the selected oscillator. |
| Osc Detune | Offsets the selected oscillator's unison copies in cents. |
| Osc Detune Shape | Moves inner unison pitches toward center at -100% or toward the outer voices at +100%. |
| Osc Spread | Sets how far the selected oscillator's detuned copies spread across stereo. |
| Osc Pan | Positions the selected oscillator as a whole in stereo. |
| FM Amt | Sets this oscillator's bipolar frequency deviation up to its base frequency. |
| FM Source | Selects an oscillator or noise as the FM source, using the prior sample for feedback cycles. |
| AMP Attack | Sets the amplitude envelope rise from 1 to 5000 ms on a logarithmic scale. |
| AMP Decay | Sets the amplitude envelope decay from 1 to 5000 ms on a logarithmic scale. |
| AMP Sustain | Sets amplitude while the key remains held. |
| AMP Release | Sets the amplitude envelope release from 1 to 5000 ms on a logarithmic scale. |
| AMP Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| FLT Attack | Sets the filter envelope rise from 1 to 5000 ms with logarithmic control. |
| FLT Decay | Sets the filter envelope decay from 1 to 5000 ms with logarithmic control. |
| FLT Sustain | Sets the held filter-envelope level with finer control near zero. |
| FLT Release | Sets the filter envelope release from 1 to 5000 ms with logarithmic control. |
| *5 | Multiplies Attack, Decay, and Release of the selected envelope by five while enabled, up to 25000 ms. |
| AMP Merge | Blends each note's amplitude envelope with one shared envelope, from separate at zero to merged at 100 percent. |
| FLT Envelope Merge | Blends each note's filter envelope with one shared envelope, from separate at zero to merged at 100 percent. |
| MOD Merge | Blends each note's modulation envelope with one shared envelope, from separate at zero to merged at 100 percent. |
| FLT Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| MOD Attack | Sets the modulation envelope rise from 1 to 5000 ms on a logarithmic scale. |
| MOD Decay | Sets the modulation envelope decay from 1 to 5000 ms on a logarithmic scale. |
| MOD Sustain | Sets the held level of effect modulation. |
| MOD Release | Sets the modulation envelope release from 1 to 5000 ms on a logarithmic scale. |
| MOD Curve | Negative curve values act sooner, while positive values delay attack, decay, and release movement. |
| MOD Depth | Sets the modulation amount, with negative values moving the destination downward. |
| MOD Target | Routes the modulation envelope to any continuous synth control. |
| FLT Type | Selects the TB-303, 18 dB Sallen-Key, or 12/24 dB digital filter model. |
| FLT Freq | Sets the filter's base cutoff on a logarithmic frequency scale. |
| FLT Resonance | Emphasizes the cutoff while a red marker shows the MOD envelope’s effective resonance. |
| FLT Key Follow | Raises the cutoff with MIDI note pitch. |
| FLT Env Amount | Moves the filter cutoff by up to thirty octaves in either direction. |
| FLT Merge | Blends per-voice filters into one shared master filter at 100%. |
| Key Pan | Sets pitch panning from high notes left at -100% to high notes right at +100%. |
| Output | Sets the final synth level. |
| Drive | Sets how hard the oscillator signal drives the selected circuit. |
| Drive Mix | Blends the driven signal with the dry oscillator signal. |
| Drive Low Cut | Click to select the low cut before or after drive, then drag; the inner and outer rings show their settings. |
| Drive High Cut | Click to select the high cut before or after drive, then drag; the inner and outer rings show their settings. |
| Time | Sets Lan's delay and reverb time. |
| Sync | Synchronizes Delay Time to the host tempo using straight, dotted, or triplet notes. |
| Delay Note | Selects the note length for tempo-synchronized delay and reverb time. |
| Decay | Sets how long Lan's feedback tail lasts. |
| Scatter | Morphs Lan's delay into a diffuse reverb. |
| Mix | Blends the Lan effect with the synth. |
| Tap Motion | Sets how far note onsets move Lan's feedback taps. |
| Phase Motion | Sets how strongly note onsets change Lan's phase scatter. |
