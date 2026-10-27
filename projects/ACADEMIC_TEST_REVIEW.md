# Academic test review

This branch currently proposes academic experiments in **three** project folders:
[Kuo Stereo](KuoStereo/ACADEMIC_TEST_PLAN.md), [Zao Drive](ZaoDrive/ACADEMIC_TEST_PLAN.md),
and [APRiCOT](XuanPhase/ACADEMIC_TEST_PLAN.md). Yinyang Compressor, Shun De-Esser,
Ren Transformer and Kuo Velvet have existing engineering tests/reports but no
`ACADEMIC_TEST_PLAN.md` in this checkout. Their existing tests are not relabelled
as academic validation.

## Implemented first: synthetic ablations

| Project | Implemented experiments | Results |
|---|---|---|
| Kuo Stereo | Spectrum-only, side-shortfall-only, combined score, no averaging, no margin; nine relative spectrum/side weights and target/averaging/margin sensitivity; exhaustive same-frame oracle and regret; fixed center/zero baselines; target-switch rate; mono sign ties and anti-phase fallback | [Report](KuoStereo/Tests/Reports/Academic/README.md) |
| Zao Drive | Off/current/energy-only; no input protection; no wet threshold; half/double bandwidth; all four actual circuits; input-partial retention, measured target reduction and wet/total RMS | [Report](ZaoDrive/Tests/Reports/Academic/README.md) |
| APRiCOT | Bypass/fixed manual/current; no residual anchor; no phase-slope estimates; six rather than 24 peaks; no unwrapping; linear interpolation; rendered phase error/ripple, band-envelope arrival dispersion, peak/RMS/crest and windowed energy | [Report](XuanPhase/Tests/Reports/Academic/README.md) |

All three use 44.1/48/96 kHz. They use production estimators and filtering rather
than independent copies of the algorithms. Experimental controls compile only
into dedicated executables, with identical class layouts and no new plugin
parameters or processing branches. Ordinary plugin builds contain none of the
experimental globals. The tests assert bounds, determinism and exercised
mechanisms; they do **not** require the full algorithm to beat every ablation.
Raw measurements, including unfavorable outcomes, are preserved.

The reports define every metric and distinguish heuristic predicted costs from
measured samples, phase accuracy from perceived punch, and spectral magnitude
from exact mid waveform preservation. This is a controlled synthetic study,
not a validation on recordings or listeners.

## Remaining plan review

| Workstream | Assessment and prerequisites |
|---|---|
| Real-audio corpora in all three plans | Pending: obtain licensed recordings, document provenance, choose a representative split, and annotate events for detector precision/recall. Generated tones/noise cannot substitute for this evidence. |
| Listening studies and correlations with judgments | Pending: recruit listeners, preregister protocol, randomize and loudness-match stimuli; no participant ratings are invented. |
| Commercial processor baselines | Pending access to named/licensed processors, exact versions/settings and reproducible host rendering. |
| Zao SPICE/hardware fidelity | Pending independent matching schematics, component models and/or measured hardware. Solver regression alone is insufficient. |
| Alternative psychoacoustic models and adaptive transfer-function baseline | Further implementation needed: choose and cite precise published models, implement them independently, then compare at controlled drive/loudness. The energy-only baseline is now available, but is not a substitute for those models. |
| APRiCOT explicit group-delay optimization and simple-rule baseline | Further implementation needed as separate algorithms. The fixed/manual baseline now exists; it is not represented as a group-delay optimizer. |
| Full sensitivity matrices | Haas target ratio, penalty, averaging and margin, Zao bandwidth, and APRiCOT peak budget are now sampled. Candidate-grid resolution and other Zao thresholds/curve parameters still need dedicated sweeps. |
| Dynamic runtime behavior | Existing integration tests cover selected transitions and worker operation. The new frozen/snapshot experiments do not replace streamed crossfade, worker-starvation, latency/CPU distributions, transient-protection ablation or stereo-link evaluations. Those need separate timing-controlled protocols. |
| Haas waveform/fold-down measurements | Still pending for the full time-varying audio path. Search-score regret is now measured, but does not measure delay slew or exact mono fold-down. |
| APRiCOT FIR length/depth sweep, phase/timing accuracy on a broader corpus | Current ablations measure actual default-length FIRs at full Depth and three sample rates. Other lengths/depths and broader input classes remain pending. |
| Literature/patent/novelty review | Requires a systematic research workflow and citations; synthetic regression results make no novelty claim. |

## Reproduce

From the repository root:

```sh
python3 projects/run_academic_tests.py --output build/academic-results
python3 projects/generate_academic_reports.py build/academic-results --publish
```

Use `--only KuoStereo` (or another project) to run a subset. Use repeated
`--build-dir Project=/path/to/configured/build` arguments to reuse existing
builds, and `--skip-build` only when those binaries match the sources. Every
native target is also registered with CTest as `<Project>Ablations`; CTest
writes results into its build directory. No DAW, network access, audio device,
GUI/X server, or Python plotting dependency is needed for the native ablations.
Python `matplotlib` is needed only for report figures.

The launcher records the source revision, dirty state, source hashes, compiler,
flags and configuration. The report archive stores gzip-compressed CSVs and
provenance; timings are observations of that machine, not CI pass thresholds.

## Validation for this implementation

Linux/GCC 14.2 Release builds passed all three native ablation executables:
134,200 Haas snapshot/variant observations, 420 Smart conditions, and 72 phase
conditions. The ordinary processor regression suites also passed (269 Kuo,
3,144 Zao, 308,437 APRiCOT assertions). Assertion counts include per-sample safety
checks and are not counts of independent experimental trials. CTest registration
was checked for all three new targets. Symbol inspection of each production
shared-code archive confirmed that research-control globals were absent.

The Kuo weight sweep demonstrates the expected measured trade-off on this
corpus: mean mid log-magnitude error rose from 0.1328 at side weight zero to
0.1536 at side weight 2.0, while mean side/mid RMS rose from 0.4874 to 0.5482.
These pooled synthetic means do not identify a perceptually optimal setting.
The academic branch now removes the phase cap from combined manual, Auto and
Stereo Link requests; current APRiCOT figures are regenerated from the uncapped
response. Finite-FIR error and unfavorable estimator outcomes remain reported.

## Sample-derived audio spectrogram tests

The native ablation executables now export real audio samples at 48 kHz for
Kuo's neutral and fixed-Haas renders with Velvet always off, Zao's paired Smart Off/On wet and
total outputs, and APRiCOT's three transient fixtures before/after automatic phase
processing. `generate_academic_reports.py` validates and publishes the resulting
spectrograms alongside each existing report. Kuo shows **mid and side energy**;
Zao shows **on-minus-off power and measured dissonance-weighted wet energy**;
APRiCOT shows **waveforms, transient window energy, magnitude and masked phase**.

The audio protocols and limitations are in each report's `audio-spectrograms.md`.
Native tests check finite samples, neutral reference identity, and exercised
processing. Report generation checks nonzero side energy, identical paired
inputs, masked valid phase, and >0.1 dB attenuation of actual weighted Smart
target-bin energy on both documented fixtures. These sample-derived measures
are separate from the analyser's predicted cost; the dissonance weighting is
still the plugin's own heuristic, not listener evidence.

Run `python3 projects/test_audio_spectrograms.py` to calibrate STFT power against
Parseval's identity, a known −6.0206 dB gain, a known +π/2 phase shift, mid/side
energy conservation, and silence masking. Python figures/tests require NumPy
and Matplotlib; native ablations still need neither. The usual two-command
reproduction workflow above includes all three sets of figures. CSV sample
captures and frequency weights are archived with deterministic gzip headers.

### Required isolation settings

All academic Kuo measurements must keep **Velvet OFF (Width = 0%)**, including
neutral and widened audio spectrogram renders. Widening in those renders comes
from Haas alone. The native test checks Width and verifies both output channels
against the expected fixed-delay taps; report generation also checks the saved
`audio-settings.csv`.

All academic Zao measurements must keep **Clear, Auto compensation, Transformer
and Bypass OFF**, Input/Output at 0 dB and Mix at 100%. Smart is the only switch
varied for the Smart on/off comparison; circuit/analysis ablations remain as
documented. The settings are explicit in the native harness and exported to
`audio-settings.csv`, which report generation checks.

### APRiCOT finite-FIR core figure

[APRiCOT's three-panel FIR report](XuanPhase/Tests/Reports/Academic/fir-core.md)
links production smooth target interpolation and detected peaks to the actual
finite kernel and its measured approximation error. It includes all three
48 kHz fixtures as PNG/PDF figures and checks all nine fixture/sample-rate
conditions numerically. Native unit-pulse convolution checks and independent
Python DTFT/FFT calibration cover support, delay reference, phase sign, gain
and wrapped error. Regenerate it with the ordinary academic workflow, or use
`--only APRiCOT` on both the native runner and report generator.

### APRiCOT spectral-novelty handoff

The [measured handoff figure](XuanPhase/Tests/Reports/Academic/novelty-phase-handoff.md)
connects chronological input FFTs and adaptive novelty threshold crossings to
the actual accumulated residual, transient anchor and anchor-referenced phase
and phase-slope delay estimates. Two softer attacks emerge over a louder
sustained background. All intermediates come from a test-only observer in the
production analysis function; native and independent Python checks validate
both detections and their phase-estimator inputs. This deterministic streamed
analysis test does not claim to measure asynchronous worker handoff timing.
