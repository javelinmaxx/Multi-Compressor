# Academic Test Plan — Zao Drive

**Purpose:** List the additional experiments needed to develop Zao Drive's Smart harmonic processing and circuit models into an academically defensible paper. This checklist distinguishes completed synthetic engineering checks from experiments still required.

Implementation references:
- [Project README](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/ZaoDrive/README.md)
- [Smart design and limitations](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/ZaoDrive/SMART.md)
- [Smart harmonic implementation](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/ZaoDrive/Source/SmartHarmonics.cpp)
- [Measured Smart report](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/ZaoDrive/Tests/Reports/smart-report.md)
- [Circuit specification](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/ZaoDrive/CIRCUITS.md)

## Current research framing

Smart does not change the nonlinear circuit transfer function or prevent the circuit from generating harmonics. It analyzes the input and measured wet residual, scores candidate harmonic products against existing input partials using a continuous approximation of a 12-TET dissonance curve, then attenuates selected frequency bands in the wet residual.

The wet residual is not a perfect harmonic separator: circuit gain, phase, compression and intermodulation may leave fundamental or non-harmonic components. The current interval curve is a musical heuristic, not a universal psychoacoustic clarity model.

## Priority 1 — Compare psychoacoustic scoring models

- [ ] Formalize the current score, including peak selection, frequency refinement, candidate harmonic orders, interval-dissonance curve, protected input-peak bandwidth, energy threshold, target priority, and attenuation bounds.
- [ ] Compare the current 12-TET interval heuristic against a neutral energy-only strategy, a simple distance-to-nearest-input-peak strategy, and an established pairwise sensory-roughness model.
- [ ] Perform sensitivity analysis on the 12-TET curve, 40 Hz–12 kHz input peak range, orders 2–12, four-bandwidth protection rule, -40 dB wet threshold, 0.035 cost threshold, 18× cost-to-cut mapping, 6 dB maximum cut, and 16-band limit.
- [ ] Measure false targets, missed targets, target stability, and filter selection differences for each model.
- [ ] Test single-note, chordal, polyphonic, inharmonic, noisy and transient-heavy inputs. Include intentionally dissonant or rough material, since dissonance reduction is not always a desirable musical objective.
- [ ] Determine whether the selected targets correspond to perceptually objectionable products in blinded listening tests rather than assuming the score predicts clarity.

## Priority 2 — Establish baselines and run ablations

- [ ] Compare Smart Off, current Smart, fixed/manual peaking-EQ cuts, energy-only harmonic attenuation, and candidate selection without dissonance weighting.
- [ ] Compare against an adaptive transfer-function or drive-scaling baseline informed by prior work such as De Man and Reiss (AES 2014), “Adaptive Control of Amplitude Distortion Effects.”
- [ ] Remove input-peak protection in an ablation to quantify its effect on desired partial retention.
- [ ] Remove the wet-energy threshold and vary candidate bandwidth to measure false suppression and collateral attenuation.
- [ ] Compare current residual-based processing with a model-derived or independently measured reference where feasible.
- [ ] Compare the four circuit models at matched input level and output loudness; document whether the same objective behaves consistently across models.

## Priority 3 — Validate on real musical material

- [ ] Assemble a documented corpus of guitar, bass, vocals, piano/keys, synths, chords, dense arrangements, and noisy/inharmonic sources.
- [ ] Test multiple registers, chord changes, input levels, drive settings, circuit modes, Tone settings, Mix levels, and transformer states.
- [ ] Report when Smart leaves the signal unchanged due to low confidence and when it selects filters that cause undesirable tonal loss.
- [ ] Measure performance on polyphonic intermodulation products that are not simple integer multiples of a single detected input partial.
- [ ] Report target changes and worker/update behavior during sustained notes, rapid chord changes, and short transients.

## Priority 4 — Measure the actual audio output

- [ ] Measure retention of original input partials in dB, not just the number of protected targets.
- [ ] Measure the energy and dissonance-weighted cost of selected target bands before and after filtering.
- [ ] Measure total output spectrum, RMS/loudness, crest factor, harmonic distortion, intermodulation products, and unintended attenuation near selected bands.
- [ ] Quantify fundamental leakage and intermodulation contamination in the driver-minus-reference wet residual across drive levels, circuit modes, and input frequencies.
- [ ] Verify that the measured target reduction is not an artifact of overall gain reduction or normalization.
- [ ] Measure response artifacts during 50 ms filter-bank crossfades, CPU cost, update delay, latency, and worker starvation.
- [ ] Repeat at supported sample rates and document FFT/window settings and analysis thresholds.

## Priority 5 — Perceptual evaluation

- [ ] Run a blinded, randomized listening test comparing Smart On/Off and the strongest baseline at controlled loudness.
- [ ] Ask separately about roughness, clarity, distortion intensity, timbral naturalness, note separation, and preference.
- [ ] Include both harmonically consonant material and intentionally dissonant/rough examples.
- [ ] Include multiple listeners and report protocol, material, level matching, statistical analysis, uncertainty, and exclusions.
- [ ] Test whether the current score or alternative roughness model predicts subjective judgments.

## Priority 6 — Circuit-model fidelity (if writing a separate circuit paper)

- [ ] Compare each modeled circuit's DC transfer curve and transient response against a SPICE implementation with matching component models.
- [ ] Where possible, build or measure physical reference circuits and characterize component tolerances and bias conditions.
- [ ] Quantify transfer error, harmonic spectra, intermodulation distortion, frequency response, bias sensitivity, stability, aliasing and CPU cost.
- [ ] Document the existing model limitations: no bench calibration, temperature drift, component tolerances, some junction capacitance/recovery effects, and a simplified finite-current behavioral driver for external diode circuits.
- [ ] Separate solver consistency with the specified equations from fidelity to a real device or pedal.

## Priority 7 — Reproducibility and paper readiness

- [ ] Regenerate and archive measurement reports from a clean checkout with exact commands and version information.
- [ ] Provide machine-readable input/output spectra, selected targets, score values, filter parameters and test-signal provenance where licensing permits.
- [ ] Cite and compare psychoacoustic roughness/consonance models, adaptive distortion control, harmonic mixing, and relevant patents.
- [ ] State the contribution narrowly: input-conditioned, dissonance-weighted attenuation of selected distortion products in the wet residual, not prevention of harmonic generation by transfer-function adjustment.

## Suggested minimum experiment matrix

| Dimension | Conditions |
|---|---|
| Scoring | Current 12-TET heuristic, energy-only, distance heuristic, roughness model |
| Input | Single notes, chords, polyphony, noise, inharmonic material, real recordings |
| Circuit | BJT CE, op-amp feedback, diode shunt, series crossover |
| Metrics | Input partial retention, target cost reduction, total spectrum, IMD, loudness |
| Runtime | CPU, update behavior, transition artifacts, latency |
| Perception | Roughness, clarity, timbral naturalness, distortion intensity, preference |

## Claims to avoid until verified

- That Smart prevents harmonics from being generated.
- That the wet residual contains only generated harmonics.
- That the 12-TET heuristic is a universal psychoacoustic measure.
- That reduced weighted harmonic cost necessarily means better perceived clarity.
- That the circuit models reproduce real hardware without SPICE and/or hardware validation.
