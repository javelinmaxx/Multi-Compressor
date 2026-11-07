# Nian Saturator processor checks

Linux x86_64, Release build. The VST3 and Standalone targets and the native
processor test are built from the project CMake file.

- A stereo sine with zero F amounts verifies complementary crossover
  reconstruction against the latency-aligned input; an impulse confirms that
  the measured peak delay matches the reported 5 ms lookahead plus 64 samples.
- A driven 10 kHz sine checks that the folded 30 kHz third harmonic at 18 kHz
  remains below 3% of the fundamental after 4× processing and downsampling.
- Simultaneous 700 Hz and 4 kHz tones, split at 1 kHz, verify that F mode
  produces a brief transient delta in both regions, then returns both band
  envelopes below 0.05 during the sustained tones. A second 700 Hz strike
  retriggers a short pulse and decays again, even when the peak was recently
  tracked by the detector.
- A phase-continuous 700→850→700 Hz note verifies that one active F cell follows
  the new pitch without a sample jump above 0.16.
- Twelve simultaneous tones verify that all six low and six high F cells track
  distinct peaks, while an 88 Hz onset acquires its short band-gain pulse
  without a waveform jump above 0.08.
- Peak-selection checks reject the weaker of two candidates within two semitones
  below 500 Hz while retaining separated bass peaks and candidates above 500 Hz.
  A 40 Hz second-order low cut on the F addition attenuates a 20 Hz test tone
  while passing a 100 Hz tone; the dry signal is outside this filter path.
- T mode at maximum amounts must change the signal while its 3 kHz output
  remains within 1.05–1.6 times dry RMS and below 0.01 average DC level; its
  second harmonic must be present and its third below 10% of the fundamental.
  The feed-forward detector must ease the added harmonics on a loud note and
  return smoothly after it ends.
- F and T mode are stressed at 44.1, 48, 96, and 192 kHz with changing buffer
  sizes and rapid crossover/mode automation. An injected non-finite filter
  state must fall back to dry audio and restore finite, audible output after
  the 10 ms recovery fade. The F-mode FFT worker must also restart after
  release and prepare without corrupting the audio path.
- Three synthetic piano notes at 48 kHz exercise F mode with a sharp hammer
  attack, inharmonic partials, and staggered harmonic decays. Their output
  stays finite and audible without tripping fault recovery or producing an
  excessive sample jump; the first 5 ms of the attack stays close to the
  delayed dry reference.
- Mix at 0% must produce latency-aligned dry audio, while 50% must sit midway
  between the dry and fully processed outputs; the spectrum queue receives
  full-rate host samples rather than a decimated waveform.
- Bypass must settle to the dry samples delayed by lookahead plus 64 samples
  after its fade.
- Repeated mode and crossover changes must keep stereo samples finite and
  bounded; the editor mode control must update the host parameter.
- The native editor is rendered and its controls are checked for visibility
  and click targets at 50% and 120% scaling.

These synthetic tests do not measure perceived sound quality or establish a
complete aliasing bound across frequencies and drive settings.
Audio Unit and Logic Pro behavior require a macOS host test.
