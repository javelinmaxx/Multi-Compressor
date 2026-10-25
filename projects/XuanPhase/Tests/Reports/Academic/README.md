# Xuan Phase: synthetic alignment ablations

72 signal/variant conditions at 44.1/48/96 kHz, including the production finite FIR and partitioned audio convolution.

![Ablation metrics](ablation.png)

| Variant | Phase error ° | Arrival spread ms | Ripple dB | Crest dB |
|---|---:|---:|---:|---:|
| bypass | 48.3460 | 1.3881 | 0.0000 | 21.2981 |
| manual_fixed | 46.8574 | 1.3777 | 0.6464 | 21.7604 |
| current | 12.0602 | 1.6716 | 0.4640 | 23.1078 |
| no_residual_anchor | 28.4877 | 1.2354 | 0.3574 | 23.6325 |
| no_slope | 12.0286 | 1.8759 | 0.4517 | 22.9533 |
| six_peaks | 21.8310 | 1.6137 | 0.1229 | 22.1935 |
| no_unwrap | 0.8275 | 1.6663 | 0.4672 | 23.6728 |
| linear_interpolation | 12.4079 | 1.7288 | 0.4608 | 23.0699 |

These averages are descriptive and combine very different fixtures. In particular, phase-target accuracy and envelope arrival spread need not improve together.

![Broadband response and phase cap interaction](broadband-response.png)

## Protocol and metrics

Fixtures are four Gaussian wave packets with staggered centers, a broadband attack with more than 24 spectral peaks, and the dispersed attack emerging over a stationary 220 Hz sustain. The source specifies every frequency, envelope and phase. Analysis length is the next power of two ≥40 ms, matching production. Eighty identical background-only snapshots settle the spectral novelty history before an otherwise identical snapshot adds the attack. This **paired-snapshot experiment** isolates estimation/anchoring; it is not a streamed detector precision/recall trial. For the sustain case the new attack is deliberately near the frame edge, where the detector can see it. No synthetic event labels are presented as manually annotated musical audio.

Baselines are latency-matched bypass and a fixed manual +45° low/−45° high response at 1 kHz crossover. Auto is full positive Depth, unity strength, default Stereo Link +100%; manual rotation is zero for all adaptive variants. The same analysed event is frozen before rendering, excluding worker scheduling and release from this experiment. Ablations remove residual anchoring, set local phase-slope delay estimates to zero, retain only the six strongest selected peaks (restoring ascending frequency order), disable adjacent-angle unwrapping, or replace smoothstep interpolation with linear interpolation. The real ±150° cap remains enabled in **every** condition. No output gain or RMS normalization is applied.

Phase error is mean absolute wrapped residual between the **measured FIR** phase (25 ms FIR reference removed) and the *full method's original target angles at its detected frequencies*. The common reference avoids rewarding reduced-peak variants by evaluating only their own retained targets. FIR magnitude ripple is max−min gain across 97 logarithmic points from 40 Hz to min(18 kHz, 0.4×sample rate); this is sampled ripple, not a continuous-frequency bound. Audio rendering includes the separate 25 ms lookahead and the full 50 ms host latency.

Arrival spread is the population standard deviation of energy-centroid arrival times in analytic Gaussian frequency bands centered on those same full-method peaks, using bandwidth sigma=max(70 Hz, 0.06×frequency). Each centroid uses the complete zero-padded rendered buffer; positive-frequency FFT bins form an analytic envelope. It is an envelope-centroid metric, not onset timing precision or perceived punch. RMS and crest use that entire common buffer. `pre_window_energy` integrates before 15 ms relative to the 50 ms-delayed input reference, and `tail_energy` after 33 ms; sustained background is included in both, so these columns must not be interpreted as pure processing artifacts.

## Finding: unwrapping and the cap

On the broadband fixture, disabling unwrapping can produce **less residual phase error** than the full method. The connected unwrapped angle can accumulate beyond one cycle, while the final requested phase is clipped to ±150° without first choosing an equivalent principal angle. This experiment exposes a cap/interpolation interaction; it does not establish that disabling unwrapping is a good production fix. No DSP correction is silently included in this research commit. Inspect the requested-phase traces and per-frequency response before choosing a remedy.

A separate exact seam probe demonstrates why unwrapping exists: adjacent +179°/−179° targets yield π at the midpoint with unwrapping and zero without it. Another off-center probe confirms linear and smooth interpolation differ. Tests assert these mechanisms, finite audio, capped requests, bounded timing, detected targets, residual-anchor differences, nonzero slope contributions and the six-peak budget. They do not demand universal peak or concentration improvement.

## Limits

These are deterministic synthetic frozen-response measurements. The experiment does not measure live crossfade artifacts, release, stereo decorrelation, detector precision/recall on recordings, or worker starvation. Bypass/manual/adaptive results use the same output length but no perceptual loudness matching. Existing streamed engineering tests remain separate. A finite FIR is not an exact all-pass; the measured ripple is reported rather than assumed absent.

## Reproduce

From the repository root, run:

```sh
python3 projects/run_academic_tests.py --output build/academic-results
python3 projects/generate_academic_reports.py build/academic-results --publish
```

CMake 3.22+, a C++17 compiler and the normal JUCE system dependencies are required. Plot generation additionally needs Python `matplotlib`. Native tests and CTest do not require Python plotting packages. Detailed CSV files are gzip-compressed alongside this report; `provenance.json` records source hashes and build information when run through the launcher. Timing values are machine-dependent.
