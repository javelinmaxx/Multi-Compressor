#!/usr/bin/env python3
"""Plot measurements emitted by LanReverbTests without reimplementing the DSP."""
import argparse
import csv
import gzip
import hashlib
import json
from pathlib import Path
import shutil

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load(root, name):
    return np.genfromtxt(root/name, delimiter=",", names=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--publish", action="store_true")
    args = parser.parse_args()
    source = args.results
    destination = Path(__file__).resolve().parent/"Reports" if args.publish else source/"report"
    destination.mkdir(parents=True, exist_ok=True)
    decay = load(source,"decay.csv")
    fig, ax = plt.subplots(figsize=(10,4), constrained_layout=True)
    for setting in np.unique(decay["decay_s"]):
        rows = decay[decay["decay_s"] == setting]
        ax.plot(rows["time_s"],20*np.log10(np.maximum(rows["rms"],1e-12)),label=f"{setting:g} s nominal")
    ax.set(xlabel="Time after impulse (s)",ylabel="100 ms RMS (dBFS)",ylim=(-120,0)); ax.legend(); ax.grid(alpha=.25)
    fig.savefig(destination/"decay.png",dpi=160); plt.close(fig)
    ringing = load(source,"resonance.csv"); spectral = load(source,"spectra.csv")
    fig, axes = plt.subplots(2,1,figsize=(10,7),constrained_layout=True)
    labels = ["Auto Off", "EQ only", "Phase / tap only", "Combined (Tame 75 / Phase 60)"]
    energies = []
    for mode,label in enumerate(labels):
        rows = ringing[ringing["mode"] == mode]
        axes[0].plot(rows["time_s"],20*np.log10(np.maximum(rows["rms"],1e-12)),label=label)
        energies.append(float(np.mean(rows[(rows["time_s"] >= 3)&(rows["time_s"] < 8)]["rms"]**2)))
    for column,label in zip(["off_power","eq_power","phase_power","combined_power"],labels):
        axes[1].semilogx(spectral["frequency_hz"],10*np.log10(np.maximum(spectral[column],1e-20)),label=label)
    axes[0].set(xlabel="Time (s)",ylabel="100 ms RMS (dBFS)"); axes[0].axvline(1,ls="--",color="gray",label="Excitation stops")
    axes[1].set(xlabel="Frequency (Hz)",ylabel="Late-tail power (dBFS)",xlim=(60,16000),ylim=(-150,-10))
    for ax in axes: ax.grid(alpha=.25); ax.legend(fontsize=9)
    fig.savefig(destination/"resonance.png",dpi=160); plt.close(fig)
    taps = load(source,"tap-shift.csv")
    fig, ax = plt.subplots(figsize=(10,3.5),constrained_layout=True)
    for column,label in [("fixed_power","Fixed taps"),("shifted_power","Tap Shift +70% after 2 s")]:
        ax.semilogx(taps["frequency_hz"],10*np.log10(np.maximum(taps[column],1e-20)),label=label)
    ax.set(xlabel="Frequency (Hz)",ylabel="Late-tail power (dBFS)",xlim=(60,16000),ylim=(-150,-10)); ax.legend(); ax.grid(alpha=.25)
    fig.savefig(destination/"tap-shift.png",dpi=160); plt.close(fig)
    spread = load(source,"stereo-scatter.csv"); stereo = load(source,"stereo.csv")
    fig,axes = plt.subplots(1,2,figsize=(10,4),constrained_layout=True)
    axes[0].scatter(spread["left"],spread["right"],s=2,alpha=.3,color="#b58a42"); axes[0].set(xlabel="Wet left",ylabel="Wet right",title="Mono noise → stereo reverb",aspect="equal")
    axes[1].plot(stereo["width"]*100,stereo["side_mid_rms"],"o-",color="#b58a42"); axes[1].set(xlabel="Width (%)",ylabel="Side/mid RMS")
    fig.savefig(destination/"width.png",dpi=160); plt.close(fig)
    automation = load(source,"automation.csv"); detector = load(source,"detector.csv")
    fig,axes = plt.subplots(1,2,figsize=(10,4),constrained_layout=True)
    for rate in np.unique(automation["rate"]):
        rows = automation[automation["rate"] == rate]
        axes[0].plot(rows["time_s"],20*np.log10(np.maximum(rows["rms"],1e-12)),label=f"{rate/1000:g} kHz")
    axes[0].set(xlabel="Time (s)",ylabel="Automated full-wet RMS (dBFS)"); axes[0].legend(fontsize=8)
    axes[1].plot(detector["time_s"],detector["cut_db"],color="#b58a42"); axes[1].set(xlabel="Time after feedback tone stops (s)",ylabel="Requested EQ cut (dB)")
    fig.savefig(destination/"automation.png",dpi=160); plt.close(fig)
    maximum = float(np.max(ringing["max_cut_db"]))
    changes = [10*np.log10(max(e,1e-30)/max(energies[0],1e-30)) for e in energies]
    rows = "\n".join(f"| {label} | {energy:.8g} | {change:+.2f} dB |" for label,energy,change in zip(labels,energies,changes))
    text = f'''# Lan Reverb engineering measurements

Version 0.5.0. These are reproducible synthetic engineering tests, not listening results or a claim that every resonance can be removed.

## Long, fully wet tails

![Decay settings](decay.png)

A stereo-linked 0.8-amplitude impulse feeds the real eight-line engine at 48 kHz, with Time 90 ms, Pre-delay 20 ms, Diffusion 85%, Phase 60%, Width 125%, Mix 100%, Output 0 dB and Auto Off. Twelve seconds are rendered at nominal Decay 2/10/30 s. Each plotted point is absolute two-channel RMS over 100 ms; there is no gain normalization. The tests require substantially more 3–4 s tail energy at 30 s than at 2 s. Damping and fractional delay interpolation make the controls nominal, especially at high frequencies. This is not a measured 30 s broadband RT60 fit.

[Eight-second fully wet impulse example](wet-impulse.wav).

## Automatic ringing control

![Tail and spectral comparison](resonance.png)

The input contains 220 Hz and 997 Hz tones at amplitudes 0.14 and 0.10 for one second, followed by nine seconds of silence. Time is 90 ms, Decay 30 s, Diffusion 10%, Pre-delay 20 ms, Width 125%, Mix 100%, and Output 0 dB. Conditions are Auto Off with Phase 0; Auto On with Tame 100% / Phase 0; Auto On with Tame 0 / Phase 100%; and Auto On with the default Tame 75% / Phase 60%. These compare isolated EQ, isolated phase/tap processing, and their combined result. Phase zero retains the phase stage's pure delay.

| Condition | Mean late-tail power, 3–8 s | Change from Off |
|---|---:|---:|
{rows}

The deepest requested single-band cut was **{maximum:.2f} dB**. Tests require a genuine correction request and at least 10% less late energy with EQ and with the combined controls on this deliberately tonal fixture. The spectrum uses overlapping 4,096-sample Hann windows with 2,048-sample hops over 3–8 s, averaging both channel powers. No output matching conceals attenuation. Phase/tap processing redistributes loop modes and may move energy between frequencies; its result is reported independently rather than required to beat EQ or reduce total energy.

The analyser and feedback filters are production code. Tests run the analyser synchronously at callback boundaries for deterministic results; the shipped plugin runs it in a background worker, whose scheduling can delay or drop updates. The plotted 100 ms RMS windows are independent of the internal FFT windows. The CSV's `max_cut_db` column is the maximum over the entire render, not a time-varying reduction trace.

[Tamed tail example](tamed-tail.wav).

## Manual feedback-tap change during the tail

![Tap shift redistributes existing ringing](tap-shift.png)

The same tone excitation and settings as the Auto Off condition are rendered again, but Tap Shift changes from 0 to +70% at 2 s, after the source has stopped. Other controls stay fixed. The plot compares 3–8 s tail spectra. A normalized spectral dot-product check requires the shifted result to differ from the unchanged network. This directly exercises existing ringing rather than merely changing the settings before excitation. Energy can move into other frequencies; a tap change alone is not an attenuation guarantee.

## Width from mono

![Stereo field](width.png)

Seeded uniform mono noise (MT19937 seed 72463, amplitude ±0.25) plays for two seconds, followed by a one-second tail. Other controls are defaults, except Auto Off and Mix 100%. Side/mid RMS is measured over 0.5–2.5 s, including the beginning of the tail. Width 0 must be mono; 100% and 200% must produce substantial side energy. The scatter uses every third sample from 0.5–0.625 s at Width 100%. Width is outside the feedback loop and uses normalization to avoid increasing its maximum mid/side gain.

## Tap automation, detector behavior and host integration

![Automation and detector release](automation.png)

At 8/44.1/48/96/192 kHz, two seconds of mono noise feed a full-wet 40 s-decay engine while Time alternates between 20/500 ms, Tap Shift between −100/+100%, Phase between 0/100%, and Diffusion across five settings. New settings arrive every 17 blocks of 128 samples, so their rate varies with sample rate. Four seconds are rendered. Every sample must remain finite and bounded, and the late tail must remain bounded after the source stops. This is a stress test, not proof against every automation schedule.

A separate read-head test changes the requested tap every 31 samples while a 50 ms fade is in progress. It verifies exact DC unity and no abrupt discontinuity on a stationary periodic tone. New requests wait for the current fade endpoint. Spectral tests verify persistence, detection of anti-phase feedback without mid cancellation, protection of a matching input partial, no cuts on seeded broadband noise, and release of held bands during silence. The displayed release curve follows a previously detected tone; it illustrates the approximately 2.5 s release constant, not onset latency.

Host checks cover all twelve parameters and state recall, mono/stereo layouts, zero dry latency, exact dry output at zero Mix and initial host bypass, static DSP invariance at 1/127/512-sample blocks, FFT submission Off/On, analysis staying hot during bypass, and containment of nonfinite input. Native editor checks cover control bindings, title information, one-sentence hints and 50%–120% bounds.

## Reproduce

From the repository root:

```sh
cmake -S projects/LanReverb -B build/lan -DCMAKE_BUILD_TYPE=Release -DLAN_REVERB_BUILD_TESTS=ON
cmake --build build/lan --target LanReverbTests --parallel 3
xvfb-run -a ./build/lan/LanReverbTests --report /tmp/lan-reverb-report
python3 projects/LanReverb/Tests/generate_report.py /tmp/lan-reverb-report --publish
```

Omit `xvfb-run -a` on a desktop system. Linux GUI previews can additionally use the existing X11-atom helper described in the project README. Python plots require NumPy and Matplotlib. Compressed CSVs preserve raw measurements. No DAW, external processor, hardware reference, or listening panel is part of these tests. AU/macOS and real DAW behavior require platform validation.
'''
    (destination/"README.md").write_text(text)
    for file in source.glob("*.csv"):
        with (destination/(file.name+".gz")).open("wb") as stream:
            with gzip.GzipFile(fileobj=stream,filename="",mode="wb",mtime=0) as compressed:
                compressed.write(file.read_bytes())
    for file in source.glob("*.wav"):
        if file.resolve() != (destination/file.name).resolve(): shutil.copyfile(file,destination/file.name)
    (destination/"provenance.json").write_text(json.dumps({"generator_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"data_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.glob("*.csv"))},"numpy":np.__version__,"matplotlib":matplotlib.__version__},indent=2)+"\n")
    print(destination/"README.md")


if __name__ == "__main__":
    main()
