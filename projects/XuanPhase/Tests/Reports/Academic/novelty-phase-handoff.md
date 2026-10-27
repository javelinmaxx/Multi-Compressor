# APRiCOT: spectral-novelty transient detection and phase estimation

## Spectral novelty → detected transient → phase estimator

![Measured novelty, residual anchor and phase-estimator handoff](novelty-phase-handoff.png)

[Vector PDF](novelty-phase-handoff.pdf)

This deterministic streamed fixture uses the **production `PhaseEngine::analyse()`** at 48 kHz, 2,048-sample frames and 128-sample hops. Two soft two-partial attacks (2,437.5/6,750 Hz, amplitude coefficient 0.035, 1.5 ms rise, 22 ms decay) emerge over a louder stationary 220/330 Hz background (amplitudes 0.25/0.10). The first 200 ms warm the detector. Ground-truth attack starts are fixture labels, not detector inputs.

| Constructed attack start (ms) | Detector frame end (ms) | Detection delay (ms) |
|---|---|---|
| 250.00 | 256.00 | 6.00 |
| 750.00 | 757.33 | 7.33 |

**(a–b)** show the original mixture and newly emerging spectral content; the heatmaps zoom to the first attack (225–285 ms), while the waveform and detector trace show both events. At each bin, the observed positive residual magnitude is `N=max(0, M−1.25*max(previous neighbour magnitudes)−0.5*floor)` when `M>0.01*largest`, otherwise zero. The figure exports this intermediate value from the real detector. Both heatmaps share the same scale, `20 log10(2*magnitude/2048)`, without separate gain normalization. The FFT uses production's short edge tapers, not a substitute Hann analysis. Spectral images are timestamped at the **end** of the analysis window.

**(c)** shows the actual novelty score and adaptive threshold. The threshold is the maximum of 0.125, median+4×max(0.003,MAD), and mean+3.5×standard deviation over the current history. A threshold crossing alone is insufficient: production energy/attack/emergence, release and 20 ms re-arm gates also apply. Red dots are the actual new-onset flags. Orange spans show the up-to-6 ms event-context collection; gray steps count targets actually returned to the phase estimator. Follow-up frames can return targets without flagging another onset.

**(d)** selects the first detected-event frame at **256.000 ms**. Production preserves the current complex FFT phase, replaces magnitudes with the event's accumulated positive novelty, imposes conjugate symmetry, and takes the IFFT. Its largest absolute real sample becomes the transient anchor at **253.500 ms**, rather than automatically using the mixture's whole-window peak. The residual has its own labeled amplitude axis; it is not secretly gain-matched to the louder input.

**(e)** uses that **same exported anchor and FFT** to show selected targets and the resulting interpolation. For FFT peak bin k and frame-relative anchor a, production computes `θₖ=wrap(sign(anchor)−2π*k*a/N−arg(Xₖ))` (sign term π for a negative anchor, 0 otherwise) and `τₖ=clip(−wrap(arg(Xₖ₊₁)−arg(Xₖ₋₁)+4π*a/N)/(4π*fs/N), ±25 ms)`. Those targets enter the actual smooth interpolation and ±150° capped phase request at full positive Auto Depth. This is a measured connection between detection and phase estimation, not a schematic arrow attached to independently synthesized targets.

The first returned targets are retained exactly, including peaks that differ from the known synthesized attack partials:

| Selected frequency (Hz) | Anchor-referenced θₖ (degrees) | Local τₖ (ms) |
|---|---:|---:|
| 562.50 | 43.839 | 1.879 |
| 2437.50 | -11.404 | -0.728 |

Selected 562.5 Hz is outside one FFT bin of either constructed attack partial (2,437.5/6,750 Hz). A successful onset flag therefore does not guarantee perfect separation of the new attack or selection of every intended partial.

### Follow-up context: observed anchor drift

The figure selects the **first new-event frame by rule**, not the best-looking later frame. The complete trace also retains the estimator's following context frames:

| Frame end (ms) | Absolute anchor (ms) | Returned targets | Anchor at/after the new attack start? |
|---|---|---:|---|
| 256.000 | 253.500 | 2 | yes |
| 258.667 | 257.792 | 2 | yes |
| 261.333 | 218.958 | 3 | no |

On this fixture a later context frame relocates the residual anchor into the earlier sustained background. Detection of the new attack therefore does not guarantee that every subsequent phase estimate stays anchored to it. This unfavorable outcome is preserved. The plot is an initial-handoff demonstration, not proof of robust separation or uniformly correct phase estimation.

The observer compiles **only into the academic executable**. Native tests require exactly two post-warmup detections, both within 30 ms, no phase targets outside event/context frames, and an anchor belonging to the new attack at the actual residual peak. Python independently reconstructs positive novelty, the residual IFFT, and the anchor-referenced phase/delay targets from exported windowed samples and compares them with native results. This synchronous chronological analysis experiment excludes worker scheduling, kernel handoff deadlines and audio-output latency; it does not establish detector accuracy on recordings or listeners.

Raw data: `novelty-audio.csv.gz`, `novelty-frames.csv.gz`, `novelty-spectrum.csv.gz`, `novelty-context.csv.gz`, `novelty-peaks.csv.gz` and `novelty-target-curve.csv.gz`.
