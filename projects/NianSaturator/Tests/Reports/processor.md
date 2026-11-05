# Nian Saturator processor checks

Linux x86_64, Release build. The VST3 and Standalone targets and the native
processor test are built from the project CMake file.

- A stereo sine with zero F amounts verifies complementary crossover
  reconstruction against the latency-aligned input; an impulse confirms that
  the measured peak delay matches the reported 64 samples.
- A driven 10 kHz sine checks that the folded 30 kHz third harmonic at 18 kHz
  remains below 3% of the fundamental after 4× processing and downsampling.
- Simultaneous 700 Hz and 4 kHz tones, split at 1 kHz, verify that F mode
  enhances each detected component by at least 20% relative to zero amounts.
- Twelve simultaneous tones verify that all six low and six high F cells track
  distinct peaks, while an 88 Hz onset reaches the new band-gain target within
  512 host samples without a waveform jump above 0.08.
- T mode at maximum amounts must change the signal while its 3 kHz output
  remains within 1.05–1.6 times dry RMS and below 0.01 average DC level; its
  second harmonic must be present and its third below 10% of the fundamental.
- Mix at 0% must produce latency-aligned dry audio, while 50% must sit midway
  between the dry and fully processed outputs; the spectrum queue receives
  full-rate host samples rather than a decimated waveform.
- Bypass must settle to the dry samples delayed by 64 samples after its fade.
- Repeated mode and crossover changes must keep stereo samples finite and
  bounded; the editor mode control must update the host parameter.
- The native editor is rendered and its controls are checked for visibility
  and click targets at 50% and 120% scaling.

These synthetic tests do not measure perceived sound quality or establish a
complete aliasing bound across frequencies and drive settings.
Audio Unit and Logic Pro behavior require a macOS host test.
