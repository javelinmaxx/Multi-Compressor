# Academic Test Plan — XuanPhase

**Purpose:** List the experiments still needed to develop the XuanPhase investigation into a defensible academic paper. This is a research checklist, not a claim that the proposed tests have already been run.

Implementation references:
- [Project README](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/XuanPhase/README.md)
- [Phase engine](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/XuanPhase/Source/PhaseEngine.cpp)
- [Onset detection report](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/XuanPhase/Tests/Reports/onset-detection.md)
- [Automatic alignment report](https://github.com/javelinmaxx/Multi-Compressor/blob/master/projects/XuanPhase/Tests/Reports/AUTO_ALIGNMENT.md)

## Implemented synthetic experiment suite

The first ablation suite is implemented in [`Tests/AblationTests.cpp`](Tests/AblationTests.cpp),
with reproducible [results and limitations](Tests/Reports/Academic/README.md).
See the [cross-project review](../ACADEMIC_TEST_REVIEW.md) for what remains.
The checklist below retains broader research requirements: synthetic completion
must not be mistaken for real-audio or perceptual validation.

## Current research framing

Investigate transient-guided adaptive phase rotation using spectral-residual anchoring, local phase-slope timing estimates, frequency-spanning peak selection, smooth target interpolation, and finite-FIR phase-response synthesis.

The current implementation is a finite, tapered FIR approximation to a phase-only response, not an exact all-pass filter or a general iterative group-delay optimizer. Report measured magnitude ripple honestly.

## Priority 1 — Validate on real audio

- [ ] Assemble and document a reproducible corpus of kick drums, snares, cymbals, plucked strings, piano, bass, vocals, synths, sustained tones, noisy signals, and polyphonic mixtures.
- [ ] Measure transient detector precision, recall, false-positive rate, onset timing error, and confidence calibration against manually annotated events.
- [ ] Include quiet attacks over loud sustained material, vibrato, repeated transients, overlapping attacks, slow fades, noisy attacks, and low-SNR events.
- [ ] Report failure cases where a transient is missed, an unrelated event triggers adaptation, or the phase target damages a musically useful attack.
- [ ] Separate synthetic regression results from results on real recordings.

## Priority 2 — Compare against meaningful baselines

- [ ] Compare bypass, a fixed/manual phase rotator, a fixed phase-response profile, and the current transient-guided method at matched output level.
- [ ] Include a commercially available phase-dispersion effect as a black-box reference if licensing and reproducibility permit; document exact settings and avoid claims about its internal algorithm.
- [ ] Implement a simple transient detector with a preselected phase profile to test whether the spectral-residual anchor and local phase-slope estimate add value.
- [x] Run deterministic synthetic ablations that remove spectral-residual anchoring, remove phase-slope timing estimates, reduce the number of spectral peaks, disable phase unwrapping, and replace smooth interpolation with simpler interpolation.
- [ ] Compare the current approach with an explicit group-delay optimization only if that alternative is implemented as a separate algorithm.

## Priority 3 — Quantify signal behavior

- [ ] Measure magnitude-response ripple and phase error over frequency for multiple rotation depths, spectral peak configurations, sample rates, and FIR lengths.
- [ ] Report low-frequency behavior explicitly, including the current documented deviations near 20 Hz and 50 Hz.
- [ ] Measure transient concentration using a predefined bandwise arrival-time dispersion metric and transient-envelope measures; specify event windows and band definitions.
- [ ] Measure peak amplitude, crest factor, RMS, pre-onset energy, post-onset/tail energy, ringing, and spectral coloration separately. Do not equate peak increase with perceived punch.
- [ ] Test phase-response and timing-target accuracy on single tones, harmonic complexes, inharmonic signals, and polyphonic material.
- [ ] Test mono compatibility and the effect on intentional source coloration.
- [ ] Measure update artifacts, magnitude deviation during response crossfades, clipping/peak excursions, latency, CPU cost, and behavior when the analysis worker is delayed.
- [ ] Repeat the measurements at supported sample rates and document the software version, settings, and exact test signals.

## Priority 4 — Perceptual validation

- [ ] Design a blinded, randomized, loudness-controlled listening test comparing bypass, static phase rotation, and adaptive XuanPhase.
- [ ] Ask separately about transient sharpness, punch, body, ringing, timbral change, and overall preference.
- [ ] Include examples where concentration is beneficial and examples where phase dispersion is intentional or undesirable.
- [ ] Document participants, material, protocol, level matching, statistical method, uncertainty intervals, and exclusions.
- [ ] Check whether objective reductions in bandwise arrival-time dispersion predict listener judgments; do not assume that they do.

## Priority 5 — Reproducibility and paper readiness

- [ ] Publish scripts or exact commands that regenerate the plots and metrics from the test signals.
- [ ] Store machine-readable result tables and the test corpus provenance; ensure audio use and distribution rights permit sharing.
- [ ] Report the fixed 50 ms host latency, lookahead, FIR support, and response-transition strategy, with a clear real-time versus offline distinction.
- [ ] State the research question and novelty relative to static phase dispersion, peak-reduction phase rotators, group-delay design, and time-varying all-pass methods.
- [ ] Conduct a systematic literature and patent search before making a priority claim.

## Suggested minimum experiment matrix

| Dimension | Conditions |
|---|---|
| Input material | Synthetic attacks plus varied real musical sources |
| Method | Bypass, static/manual, simple rule-based, current adaptive method |
| Depth | Multiple settings from neutral to maximum |
| Signal metrics | Phase error, magnitude ripple, arrival dispersion, peak/RMS, tail energy |
| Runtime metrics | Latency, CPU, update safety, sample-rate behavior |
| Perception | Blinded ratings of sharpness, punch, body, ringing, timbral change |

## Claims to avoid until verified

- Exact all-pass or perfect magnitude preservation.
- Universal improvement in transient clarity or punch.
- General detector accuracy inferred only from synthetic tests.
- Novelty based solely on automatic phase rotation.
- Perceptual superiority inferred from phase-target accuracy alone.
