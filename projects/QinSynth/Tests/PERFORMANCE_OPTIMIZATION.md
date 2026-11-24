# Qin Synth pan and noise-glide optimization

## Parallel voice synthesis and filters

The audio thread and one persistent JUCE worker split active voices when a
render segment contains at least four voices and 128 samples. Each lane computes
its voices' oscillators, FM chains, noise, envelopes, filter coefficients, and
filter outputs. Shared controls are snapshotted per sample, then the audio
thread sums voice contributions in their original order and runs the merged
filter and output effects. The worker requests real-time scheduling and falls
back to high priority if that request fails. Worker scratch buffers are
allocated during `prepareToPlay`.

Linux Release build, 48 kHz, 128-sample blocks, one second of audio per run.
Each value is the median of five timed runs after one warmup. Serial and
threaded figures use the same build and settings; wall-clock time includes
the synchronization cost. DAW CPU meters may differ.

| Scenario | Serial | Threaded | Reduction |
| --- | ---: | ---: | ---: |
| 16 voices, 3 oscillators × 8 unison, filter bypassed | 272.000 ms | 170.416 ms | 37.3% |
| Same voices, Sallen-Key filter, 55% resonance, 50% merge | 343.686 ms | 210.747 ms | 38.7% |

The serial mono legato and noise glide cases measured 70.870 ms and 47.304 ms.
Processor tests compare threaded and serial output sample by sample under FM,
noise, per-voice and merged filtering, note-on, and note-off. The maximum
allowed difference is `1e-5`, and the test checks that the worker actually
renders dense polyphony.

## CPU comparison

Linux Release build, 48 kHz stereo, 128-sample blocks, one second of rendered
audio per run. Each result is the median of five timed runs after one warmup;
instrument construction and parameter setup are excluded. The baseline is
commit `583526e01c`. Times are local wall-clock measurements, so DAW CPU
meters and other machines may differ.

| Scenario | Baseline | Updated | Time reduction |
| --- | ---: | ---: | ---: |
| 16 voices, three oscillators at 8× unison, stable pan | 418.804 ms | 282.611 ms | 32.5% |
| Mono legato and moving stereo spread, three oscillators at 8× unison | 79.447 ms | 75.069 ms | 5.5% |
| Mono noise oscillator gliding between notes | 49.467 ms | 48.087 ms | 2.8% |

Run `QinSynthPerformanceBench` to repeat these cases. Stationary oscillator
and noise pan gains are cached per render segment; moving gains use an
interpolated equal-power lookup. The noise filter updates its note-tracked
coefficients every 16 samples and interpolates them between updates. At 48 kHz,
the update interval is 0.33 ms. MIDI retargets restart the interpolation from
the current coefficient, preserving continuity.

The processor suite passes, including a 10,001-point pan-gain comparison to
the analytic square-root curve and a noise-glide comparison across 17- and
128-sample host blocks. The latter allows at most `1e-4` sample difference.

## TB-303 ladder tanh lookup probe

A separate probe substituted the shared transformer tanh table into the four
ladder poles and feedback input while retaining the production ladder's
coefficient and arithmetic. It exercised 72 combinations of 44.1, 48, and
96 kHz; 30, 500, 5,000, and 18,000 Hz cutoff; 80% and 100% resonance; and
0.5, 2, and 8 input amplitude. Each case ran 48,000 samples of mixed tones
and periodic impulses.

All outputs and pole states stayed finite and bounded. In the 18 kHz,
100%-resonance, amplitude-2 cases, however, the lookup and analytic versions
diverged by up to **0.657** in output, at all three sample rates. The maximum
difference in the other 69 cases was `5.11e-5`. The ladder is sensitive to tiny
nonlinear changes in this extreme feedback setting, so its production `tanh`
calls remain analytic.
