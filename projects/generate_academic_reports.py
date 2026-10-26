#!/usr/bin/env python3
"""Generate the synthetic ablation reports from native CSV outputs."""
import argparse
from collections import defaultdict
import csv
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import statistics

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from generate_audio_spectrograms import report as audio_report

ROOT = Path(__file__).resolve().parent


def read(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def mean(rows, field):
    values = [float(r[field]) for r in rows if r[field]]
    return statistics.mean(values) if values else float("nan")


def table(rows, fields):
    groups = defaultdict(list)
    for row in rows:
        groups[row["variant"]].append(row)
    lines = ["| Variant | " + " | ".join(label for _, label in fields) + " |", "|---|" + "---:|" * len(fields)]
    for variant, values in groups.items():
        lines.append("| " + variant + " | " + " | ".join(f"{mean(values, field):.4f}" for field, _ in fields) + " |")
    return "\n".join(lines)


def plot_groups(rows, fields, destination):
    groups = defaultdict(list)
    for row in rows:
        groups[row["variant"]].append(row)
    fig, axes = plt.subplots(1, len(fields), figsize=(max(10, 5*len(fields)), 5), constrained_layout=True)
    if len(fields) == 1:
        axes = [axes]
    for ax, (field, label) in zip(axes, fields):
        ax.bar(range(len(groups)), [mean(values, field) for values in groups.values()], color="#b58a42")
        ax.set_xticks(range(len(groups)), list(groups), rotation=65, ha="right", fontsize=8)
        ax.set_ylabel(label)
        ax.grid(axis="y", alpha=.25)
    fig.savefig(destination, dpi=160)
    plt.close(fig)


def archive(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path.suffix == ".csv":
            # Deterministic gzip headers keep repeat runs diffable; timings are
            # deliberately retained and must not be used as portable assertions.
            with (destination/(path.name+".gz")).open("wb") as file:
                with gzip.GzipFile(filename="", mode="wb", fileobj=file, mtime=0) as compressed:
                    compressed.write(path.read_bytes())
        elif path.name == "provenance.json":
            shutil.copyfile(path, destination/path.name)


def kuo(source, destination):
    rows = read(source/"haas.csv")
    fields = [("mid_log_error", "Mid log-magnitude error"), ("side_mid_rms", "Side/mid RMS"), ("regret", "Score regret")]
    plot_groups(rows, fields, destination/"ablation.png")
    # One identical near-zero-range, mono pad case isolates the C -> Am change.
    selected = [r for r in rows if r["rate"] == "48000" and r["corpus"] == "C_to_Am_pad" and r["center_ms"] == "0" and r["variant"] in {"current", "mid_only", "side_only", "exhaustive"}]
    fig, ax = plt.subplots(figsize=(10, 3.5), constrained_layout=True)
    for variant in ["current", "mid_only", "side_only", "exhaustive"]:
        group = [r for r in selected if r["variant"] == variant]
        ax.plot([float(r["time_s"]) for r in group], [float(r["delay_ms"]) for r in group], label=variant)
    ax.axvline(3, ls="--", color="gray", label="C to Am (125 ms fade)")
    ax.set(xlabel="Time (s)", ylabel="Selected target (ms)")
    ax.legend(ncol=3, fontsize=8); fig.savefig(destination/"pad-targets.png", dpi=160); plt.close(fig)
    changes = defaultdict(lambda: [0, 0.])
    for r in rows:
        key = (r["rate"], r["corpus"], r["center_ms"], r["range_ms"], r["variant"])
        changes[key][0] += int(r["changed"])
        changes[key][1] = max(changes[key][1], float(r["time_s"]))
    with (destination/"switching.csv").open("w", newline="") as f:
        writer = csv.writer(f); writer.writerow(["rate", "corpus", "center_ms", "range_ms", "variant", "changes_per_second"])
        for key, (count, duration) in changes.items():
            writer.writerow([*key, count/duration])
    weight_variants = {"mid_only": 0., "penalty_0.05": .05, "penalty_0.10": .10, "penalty_0.175": .175, "current": .35, "penalty_0.50": .5, "penalty_0.70": .7, "penalty_1.00": 1., "penalty_2.00": 2.}
    weights = sorted((value, name, [r for r in rows if r["variant"] == name]) for name, value in weight_variants.items())
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.8), constrained_layout=True)
    for ax, (field, label) in zip(axes, fields[:2]):
        ax.plot([value for value, _, _ in weights], [mean(group, field) for _, _, group in weights], "o-", color="#a27830")
        ax.set(xlabel="Side-shortfall coefficient λ (mid-error weight = 1)", ylabel=label)
        ax.set_xscale("symlog", linthresh=.05); ax.grid(alpha=.25)
    fig.savefig(destination/"score-weights.png", dpi=160); plt.close(fig)
    with (destination/"weight-sweep.csv").open("w", newline="") as f:
        writer = csv.writer(f); writer.writerow(["side_coefficient", "variant", "mean_mid_error", "mean_side_mid_rms", "mean_regret"])
        for value, name, group in weights:
            writer.writerow([value, name, mean(group,"mid_log_error"), mean(group,"side_mid_rms"), mean(group,"regret")])
    return f"""# Kuo Stereo: synthetic score ablations

{len(rows):,} snapshot/variant observations. All signals are generated locally; no recordings or listening results are implied.

![Ablation metrics](ablation.png)

{table(rows, [("mid_log_error", "Mid log error"), ("side_mid_rms", "Side/mid RMS"), ("regret", "Regret"), ("changed", "Change fraction")])}

Means pool frames, sample rates, ranges and fixtures equally **per row**, not per recording or listener. The 96 kHz cases have more frames. Fixed-zero rows exist only for the 0 ± 6 ms search range, so compare them with the corresponding subset of other methods rather than their full-matrix means. Inspect `haas.csv.gz` for per-case results; these are descriptive statistics without independent-trial confidence intervals.

![C-to-Am selected targets](pad-targets.png)

## Protocol and score

The native worker's real `process` method receives chronological snapshots synchronously after its thread is stopped. No timing sleeps, worker races or audio-stream scheduling determine the result. Analysis size and hop are 2,048 samples. Four fixtures are a mono C-to-Am pad, correlated stereo pad, already-wide stereo pad, and a pad with 400 ms-spaced, 6.3 kHz exponentially decaying short bursts. The deterministic existing SynthPad generator changes chord at 3 s over 125 ms. Six seconds are processed at 44.1/48/96 kHz. Center ± Variability settings are 0 ± 6, 12 ± 6, and −28 ± 2 ms. The last case reaches the negative boundary. All candidates are on the production 0.2 ms grid.

For each candidate d, the reference and processed mid signals are Hann-windowed and transformed. With FFT size N, r_k=|FFT(mid)|/N, p_k=|FFT(mid_d)|/N, and w_k=r_k/(r_k+0.01), the spectral error is

`E(d) = sum(w_k * abs(ln((p_k+0.0001)/(r_k+0.0001)))) / max(sum(w_k), 1)`

Bins 2 through N/2−1 are used. A reference-magnitude sum below 0.001 takes the center-delay fallback rather than searching. Let R be the square root of Hann-weighted candidate side energy divided by max(candidate mid energy, 0.000001). The combined objective is `J(d)=E(d)+0.35*max(0,1−R/0.20)`. This is a width-shortfall penalty, not an unlimited reward for added side energy. Newly measured scores use `alpha=1−exp(−elapsedFrames*2048/(0.25*sampleRate))`; unmeasured candidates retain their previous score. A switch normally requires the proposed averaged score to be <0.98 of the incumbent score.

Ablations remove each term, averaging, or the margin independently. Sensitivity pairs vary target ratio (0.10/0.30), penalty (0.175/0.70), averaging (125/500 ms), and margin (0.96/0.99). All reported quality metrics still use the original combined objective. The exhaustive baseline evaluates every candidate on the identical snapshot without averaging or margin. Its raw minimum is the oracle; **regret is instantaneous raw combined score minus this minimum**. It intentionally includes temporal smoothing lag as well as incomplete search. Fixed-center and in-range fixed-zero baselines use that same spectral evaluation, without searching.

Exact-mono mirrored scores must match within 1e−6, verifying that this objective cannot infer the preferred delay sign. An anti-phase input with silent mid must take the center-delay fallback. These invariants are asserted, as are index/range bounds, finite scores, and nonnegative regret. No assertion requires a research variant to sound better.

## Mid-difference versus side-energy weight sweep

![Relative score weights](score-weights.png)

With spectral-error weight fixed at 1, λ spans **0, 0.05, 0.10, 0.175, 0.35, 0.50, 0.70, 1.00, 2.00** in `E + λ*max(0,1−R/0.20)`. The side-only ablation supplies the complementary zero-spectral-weight extreme. `weight-sweep.csv` and the full frame table retain the measured results. Each weight uses exactly the same input snapshots and production temporal/search logic. The criterion is the production side/mid **shortfall**, not an unbounded side-energy reward. These paired comparisons measure a trade-off; they do not select an optimum by listening preference. This mechanism is in **Kuo Stereo**, while Xuan Phase's current alignment algorithm has no such weighted score.

## Limits

The snapshot ablations above isolate target selection. Their score metrics do **not** render delay slew, the Dynamic transient envelope, Velvet, the 1.5 ms lookahead, or final mono fold-down. The separate audio experiment below renders fixed Haas and Velvet through the full path; adaptive delay slew remains outside that experiment. An exact zero delay can remain suitable for already-stereo input with enough side energy. Magnitude deviation is not waveform or phase preservation. `switching.csv` measures target changes; it does not call those changes perceptually useful. `search_us` is a single synchronous wall-clock observation including FFT work and research instrumentation, not an audio-thread CPU benchmark or a portable timing threshold. No loudness normalization is applied.
"""


def zao(source, destination):
    rows = read(source/"smart.csv"); partials = read(source/"partials.csv")
    plot_groups(rows, [("targets", "Selected bands"), ("total_rms_db", "Total RMS change (dB)")], destination/"ablation.png")
    plot_groups(partials, [("wet_change_db", "Original partial: wet change (dB)"), ("total_change_db", "Original partial: total change (dB)")], destination/"partial-retention.png")
    return f"""# Zao Drive: synthetic Smart ablations

{len(rows):,} frame/variant conditions across all four production circuits at 44.1/48/96 kHz.

![Ablation results](ablation.png)

{table(rows, [("targets", "Bands"), ("protected_targets", "Bands near input partials"), ("wet_rms_db", "Wet RMS Δ dB"), ("total_rms_db", "Total RMS Δ dB")])}

![Measured partial retention](partial-retention.png)

{table(partials, [("wet_change_db", "Input partial: wet Δ dB"), ("total_change_db", "Input partial: total Δ dB")])}

## Protocol

A 4,096-sample periodic frame contains a single tone, a C-like chord, an input-harmonic collision, an inharmonic complex, or seeded periodic noise. Frequencies are exact FFT bins; the source lists their indices, so absolute pitches vary with sample rate. Each tonal mixture has a sum-of-amplitudes ceiling of 0.25, not an equal-RMS normalization. Drive is 24 dB, Input and Output 0 dB, Tone 16 kHz, Mix 100%; Clear, Auto and Transformer are off. BJT Bias is zero and diode Knee 0.5. Six frames warm the actual oversampled circuit and tone path. A research-only tap in `SmartHarmonics::process` captures the aligned dry reference and **actual post-tone driver-minus-reference wet residual**, before Smart filtering. This does not presume the residual contains only harmonics.

Every variant receives the same captured frame. The production RBJ filter bank is loaded through its existing deterministic preview interface; four repeated frames settle its state and the last frame is measured. There is no bank transition or scheduler dependency. Baselines are Smart Off and energy-only selection (`cost=1`, otherwise the same candidate generation and protection). Ablations disable input-peak protection, disable the relative wet-energy threshold, or scale candidate bandwidth by 0.5/2.0. The no-protection fixture includes a harmonic coinciding with one input partial and another non-octave partial, ensuring the dissonance score does not trivially vanish at every protected frequency.

Partial retention is measured by complex sinusoidal projection at each known input frequency in the **filtered wet signal** and in **dry+wet total output**, relative to the identical unfiltered capture. No gain matching or automatic normalization masks attenuation. Total and wet RMS are reported separately. `targets.csv.gz` contains measured Hann-windowed three-bin energy before/after around every selected target, bandwidth, order and cut. `estimated_cost_before/after` are the analyser's own predicted heuristic costs; `measured_target_power_ratio` comes from the real filtered samples and is deliberately separate. Overlapping measurement windows can double-count energy; they are not total distortion power. Protection proximity is evaluated against known fixture frequencies and the selected bandwidth; noisy fixtures have no labelled partials.

The tests assert deterministic target counts/costs, finite output, ≤16 bands, cuts within −6..0 dB, Off identity, noise neutrality, and that the corpus actually changes when protection, threshold or interval weighting is removed. They do not assert a preferred musical outcome.

## Limits

Means pool these synthetic conditions and partials; they are not perceptual scores. This is a frozen stationary filter experiment, not 20 Hz adaptation/crossfade or worker-starvation measurement. The energy-only baseline is deliberately aggressive and is not loudness-matched to the current heuristic. No SPICE, hardware, licensed external processor, established sensory-roughness model, or real-audio validation is represented. A reduction in selected-band power is not proof of greater clarity.
"""


def xuan(source, destination):
    rows = read(source/"phase.csv")
    plot_groups(rows, [("mean_phase_error_deg", "Residual phase error (degrees)"), ("arrival_dispersion_ms", "Band-envelope arrival spread (ms)"), ("magnitude_ripple_db", "FIR magnitude ripple (dB)")], destination/"ablation.png")
    fig, axes = plt.subplots(2, 1, figsize=(10, 6), constrained_layout=True)
    responses = read(source/"response.csv")
    for variant in ["current", "no_unwrap", "no_slope", "six_peaks"]:
        group = [r for r in responses if r["rate"] == "48000" and r["corpus"] == "broadband_attack" and r["variant"] == variant]
        axes[0].semilogx([float(r["frequency_hz"]) for r in group], [float(r["requested_deg"]) for r in group], label=variant)
        axes[1].semilogx([float(r["frequency_hz"]) for r in group], [float(r["gain_db"]) for r in group], label=variant)
    axes[0].set_ylabel("Requested phase (degrees)"); axes[1].set(xlabel="Frequency (Hz)", ylabel="Measured FIR gain (dB)")
    axes[0].legend(ncol=2); fig.savefig(destination/"broadband-response.png", dpi=160); plt.close(fig)
    return f"""# Xuan Phase: synthetic alignment ablations

{len(rows)} signal/variant conditions at 44.1/48/96 kHz, including the production finite FIR and partitioned audio convolution.

![Ablation metrics](ablation.png)

{table(rows, [("mean_phase_error_deg", "Phase error °"), ("arrival_dispersion_ms", "Arrival spread ms"), ("magnitude_ripple_db", "Ripple dB"), ("crest_db", "Crest dB")])}

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
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--publish", action="store_true", help="Write reports into each project's Tests/Reports/Academic folder")
    args = parser.parse_args()
    for project, generate in [("KuoStereo", kuo), ("ZaoDrive", zao), ("XuanPhase", xuan)]:
        source = args.results/project
        if not source.exists():
            continue
        destination = ROOT/project/"Tests/Reports/Academic" if args.publish else source/"report"
        destination.mkdir(parents=True, exist_ok=True)
        archive(source, destination)
        (destination/"README.md").write_text(generate(source, destination)+audio_report(project, source, destination)+"\n## Reproduce\n\nFrom the repository root, run:\n\n```sh\npython3 projects/run_academic_tests.py --output build/academic-results\npython3 projects/generate_academic_reports.py build/academic-results --publish\n```\n\nCMake 3.22+, a C++17 compiler and the normal JUCE system dependencies are required. Plot generation additionally needs Python `numpy` and `matplotlib`. Native tests and CTest do not require Python plotting packages. Detailed CSV files are gzip-compressed alongside this report; `provenance.json` records source hashes and build information when run through the launcher. Timing values are machine-dependent.\n")
        (destination/"report-provenance.json").write_text(json.dumps({
            "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "audio_generator_sha256": hashlib.sha256((ROOT/"generate_audio_spectrograms.py").read_bytes()).hexdigest(),
            "input_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.glob("*.csv"))},
            "matplotlib_version": matplotlib.__version__,
            "numpy_version": np.__version__,
        }, indent=2)+"\n")
        print(destination/"README.md")


if __name__ == "__main__":
    main()
