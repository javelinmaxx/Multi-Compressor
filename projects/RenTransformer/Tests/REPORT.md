# Ren Transformer validation

The processor tests exercise the production plugin and the original Yinyang
transformer independently. No compressor or sidechain is instantiated by Ren.

| Feature | Verification |
| --- | --- |
| Transformer reuse | Compare every rendered sample with `character::OversampledTransformer` at 44.1, 48 and 96 kHz, with 0/6/18 dB Core Drive, 35% DC Bias and +3 dB Output; maximum allowed absolute error is 1e-6. |
| Response | At 0%, the core matches drive-only `tanh` saturation within 1e-6 even with DC Bias set; Ren's output matches the 0% oversampled reference and differs from the 100% output. |
| Output stage | With Core Off or Mix at zero, a stereo impulse returns at sample 64 with the selected Output gain. |
| Bypass | Both host bypass and the Bypass parameter return the original stereo impulse at sample 64 without Output gain. |
| CPU saving | The wrapper's running state becomes false after the Off fade and restarts after warming when enabled. |
| Host compatibility | Seven exposed parameters survive binary state save/restore, and the host bypass parameter is registered. |
| Layouts and buffer sizes | Mono and stereo processing are exercised; 1/127/512-sample blocks must render the same signal within 1e-6. |
| Editor | Native control initialization, UI/host parameter changes and one-sentence tooltips are checked in the compact single-panel layout. |
| Scaling | Every interactive control remains inside the editor and passes pointer hit testing at 50% and 120%. |

The committed [native preview](../Previews/ren-transformer-tooltip.png) includes
a real JUCE tooltip. Build and preview reproduction commands are in the project
[README](../README.md). macOS AU and native Windows/macOS host validation require
those platforms.

## Results — 2026-11-16

Linux Release VST3 built successfully. The processor run passed 44 checks;
the editor run passed 79 checks, including tooltip rendering and 50%/120%
scaling. The compact editor remains 444 × 260 and displays five knobs in one
row, with Output at the right edge. The shared-core regressions also passed:
1,325 Yinyang DSP checks, 3,370 Yinyang processor checks, and 3,145 Zao
processor checks.
