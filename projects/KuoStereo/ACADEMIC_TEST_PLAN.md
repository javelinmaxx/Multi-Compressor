# Academic Test Plan — Kuo Stereo

**Purpose:** List the experiments still required to develop Smart Haas and the associated mid-channel integrity claims into an academically defensible investigation. This is a test plan; items are not claims of completed work.

Implementation references:
- [Project README](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/KuoStereo/README.md)
- [Processor implementation](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/KuoStereo/Source/PluginProcessor.cpp)
- [Feature measurement report](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/KuoStereo/Measurements/Report/REPORT.md)
- [Feature measurement generator](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/KuoStereo/Measurements/FeatureMeasurements.cpp)

## Implemented synthetic experiment suite

The first ablation suite is implemented in [`Tests/AblationTests.cpp`](Tests/AblationTests.cpp),
with reproducible [results and limitations](Tests/Reports/Academic/README.md).
See the [cross-project review](../ACADEMIC_TEST_REVIEW.md) for what remains.
The checklist below retains broader research requirements: synthetic completion
must not be mistaken for real-audio or perceptual validation.

## Current research framing

Evaluate a content-adaptive signed Haas-delay selection method that trades off weighted mid-channel spectral-magnitude deviation against a target side/mid RMS ratio, with score averaging, a switching margin, bounded delay slew, and transient-specific delay reduction.

Be precise: the score penalizes mid-spectrum magnitude deviation; it does not guarantee exact preservation of the original mid waveform or phase. Velvet Width preserves the mid entering its stage, which may already have been changed by the Haas stage.

## Priority 1 — Strengthen the adaptive objective

- [x] Document the scoring function mathematically, including spectral weighting, log-magnitude floor, side/mid target ratio, and all constants.
- [x] Run a deterministic synthetic ablation study: mid-spectrum error only; side-energy reward only; combined score; combined score without temporal averaging; and combined score without the switching margin.
- [ ] Perform sensitivity analysis for the side/mid target ratio (currently 0.20), penalty coefficient (currently 0.35), score averaging time (currently 250 ms), switching margin (currently 2%), and candidate delay step (currently 0.2 ms).
- [ ] Measure how often the selected target changes and whether those changes improve the objective consistently rather than tracking noise or chord transients.
- [ ] Test tie behavior and sign selection. For an exactly mono input, positive and negative delay candidates can have identical mid and side energy magnitudes, so the current objective cannot infer the preferred delay sign from mono content alone.
- [ ] Compare the coarse survey/local refinement search with an exhaustive offline grid search on identical snapshots. Quantify score regret, convergence time, and CPU cost.

## Priority 2 — Broaden the input corpus

- [ ] Build a reproducible corpus of real vocals, guitars, bass, drums, synth pads, keys, dense chords, and already-stereo recordings.
- [ ] Include sustained harmonic content, chord changes, vibrato, modulation, transients, low-level input, noise, anti-phase material, and sources with substantial existing side energy.
- [ ] Evaluate a range of Center Delay and Variability values, including negative delays, values near zero, and boundary-clamped searches.
- [ ] Document failure cases, such as excessive spectral coloration, unstable target movement, or choices that increase width at the expense of center solidity.
- [ ] Distinguish mono-input behavior from stereo-input behavior; the algorithm may have less information for selecting a delay direction on mono input.

## Priority 3 — Objective signal and stereo measurements

- [ ] Measure weighted and unweighted mid-spectrum magnitude error, and add time-domain mid error after latency alignment.
- [ ] Measure side/mid RMS ratio, zero-lag L/R correlation, frequency-dependent correlation, and interchannel delay.
- [ ] Measure mono fold-down spectrum and the depth/frequency of comb-filter notches for each delay choice.
- [ ] Quantify output level, transient peak changes, spectral change, and delay-target stability separately.
- [ ] Verify that the delay search's spectral metric predicts the chosen outcome across material and parameter settings.
- [ ] Report the 1.5 ms lookahead latency and the transient detector's response and release behavior.
- [ ] Measure the contribution of Smart Haas separately from Velvet Width; do not attribute the latter's center-preserving construction to the full chain's original input mid.

## Priority 4 — Compare with baselines

- [ ] Compare against fixed Haas delays using the same center setting and a grid of manually selected delays.
- [ ] Compare against exhaustive search using the exact current score to isolate the impact of the low-CPU search strategy.
- [ ] Compare against a search minimizing only mid spectral deviation, and against a simpler width target without the spectral penalty.
- [ ] Compare against a commercial Haas widener at documented, approximately comparable settings where feasible.
- [ ] Ablate transient protection and test whether it improves attack integrity without introducing audible switching artifacts.
- [ ] Report differences in width, mid integrity, mono fold-down, stability, latency, and CPU cost.

## Priority 5 — Perceptual validation

- [ ] Conduct a blinded, randomized listening test at matched loudness comparing fixed delay, adaptive delay, and suitable commercial/reference methods.
- [ ] Ask listeners to rate perceived width, center solidity, timbral coloration, transient clarity, mono compatibility where relevant, and overall preference separately.
- [ ] Include cases where the widest result is not necessarily the preferred result.
- [ ] Report participants, test material, protocol, statistical analysis, uncertainty, and exclusions.
- [ ] Test whether reductions in mid-spectrum error and changes in side/mid ratio correlate with subjective judgments; do not treat either metric as a substitute for listening data.

## Priority 6 — Reproducibility and paper readiness

- [ ] Extend the deterministic feature-measurement harness to generate tables and plots for the full corpus.
- [ ] Record exact plugin version, sample rate, block size, parameter values, analysis window, and test-signal provenance.
- [ ] Provide reproducible scripts and machine-readable outputs where licensing permits.
- [ ] Compare the method with prior art in Haas widening, adaptive distortion/width control, and mid/side processing before claiming novelty.

## Suggested minimum experiment matrix

| Dimension | Conditions |
|---|---|
| Input | Mono, correlated stereo, already-wide stereo, transients, sustained and polyphonic material |
| Search | Fixed/manual, current heuristic, exhaustive same-score oracle, ablated scores |
| Delay | Positive, negative, near-zero, center and range boundaries |
| Metrics | Mid spectral/time error, side/mid ratio, correlation, comb filtering, mono fold-down |
| Runtime | Target stability, CPU, 1.5 ms latency, update cadence |
| Perception | Width, center solidity, coloration, transient integrity, preference |

## Claims to avoid until verified

- “Perfect mid preservation” across the complete Haas path.
- That the chosen delay is perceptually optimal solely because it minimizes the current score.
- General superiority inferred from a synthetic pad test.
- Novelty claims based only on combining Haas delay, mid/side processing, or transient protection.
