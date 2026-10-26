#!/usr/bin/env python3
"""Sample-derived spectrograms for deterministic academic audio captures."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load(path, *, constant_rate=True):
    data = np.genfromtxt(path, delimiter=",", names=True, dtype=float)
    if data.ndim != 1 or len(data) < 2 or not all(np.isfinite(data[n]).all() for n in data.dtype.names):
        raise ValueError(f"Invalid or nonfinite audio data: {path}")
    if "sample" in data.dtype.names and not np.array_equal(data["sample"], np.arange(len(data))):
        raise ValueError(f"Non-contiguous capture: {path}")
    if "rate" in data.dtype.names and (np.any(data["rate"] <= 0) or (constant_rate and not np.all(data["rate"] == data["rate"][0]))):
        raise ValueError(f"Inconsistent or nonpositive sample rate: {path}")
    return data


def stft(signal, rate, size=2048, hop=256):
    """Unpadded Hann STFT; one-sided bin powers sum to window-weighted mean square."""
    signal = np.asarray(signal, dtype=float)
    if signal.ndim != 1 or len(signal) < size or not np.isfinite(signal).all() or rate <= 0:
        raise ValueError("STFT needs finite mono audio, positive rate and a complete window")
    window = np.hanning(size)
    frames = np.lib.stride_tricks.sliding_window_view(signal, size)[::hop]
    spectrum = np.fft.rfft(frames * window, axis=1).T
    power = abs(spectrum)**2 / (size * np.sum(window**2))
    power[1:-1] *= 2
    times = (np.arange(len(frames))*hop + (size-1)/2)/rate
    return np.fft.rfftfreq(size, 1/rate), times, spectrum, power


def db(power):
    return 10*np.log10(np.maximum(power, 1e-16))


def phase_pair(before, after, floor_db=-50):
    # The same absolute threshold for both signals prevents phase in silence
    # and weak bins becoming visually persuasive random colour.
    threshold = max(np.max(abs(before)**2), np.max(abs(after)**2))*10**(floor_db/10)
    valid_before = abs(before)**2 > max(threshold, 1e-30)
    valid_after = abs(after)**2 > max(threshold, 1e-30)
    return (np.ma.array(np.angle(before), mask=~valid_before),
            np.ma.array(np.angle(after), mask=~valid_after),
            np.ma.array(np.angle(after*np.conj(before)), mask=~(valid_before & valid_after)))


def mesh(ax, f, t, values, title, limits, phase=False, delta=False, max_hz=16000):
    im = ax.pcolormesh(t, f, values, shading="auto", rasterized=True,
                       cmap="twilight" if phase else "RdBu_r" if delta else "magma",
                       vmin=limits[0], vmax=limits[1])
    ax.set(title=title, xlabel="Input-aligned time (s)", ylabel="Frequency (Hz)", ylim=(100,max_hz))
    ax.set_yscale("log")
    ax.figure.colorbar(im, ax=ax, label="Phase (rad)" if phase else "Δ power (dB)" if delta else "Bin power (dBFS)")


def save(fig, destination, name):
    fig.savefig(destination/name, dpi=160)
    plt.close(fig)


def kuo(source, destination):
    settings = load(source/"audio-settings.csv")
    assert np.all(settings["velvet_width_pct"] == 0), "Academic Kuo audio requires Velvet off"
    neutral, wide = load(source/"audio-neutral.csv"), load(source/"audio-wide.csv")
    if not np.array_equal(neutral["input_left"], wide["input_left"]):
        raise AssertionError("Kuo renders must have identical input")
    rate = wide["rate"][0]
    spectra = []
    for a, b in [(wide["input_left"],wide["input_right"]), (wide["output_left"],wide["output_right"])]:
        spectra.extend([stft((a+b)/2,rate), stft((a-b)/2,rate)])
    assert np.max(abs(neutral["output_left"]-neutral["input_left"])) < 1e-5
    assert np.max(abs(neutral["output_left"]-neutral["output_right"])) < 1e-8
    assert np.sum(spectra[3][3]) > 1e-5, "Widening must create side energy"
    fig, axes = plt.subplots(3,2,figsize=(13,11),constrained_layout=True)
    for ax, spectrum, title in zip(axes[:2].flat,spectra,["Input mid: (L+R)/2","Input side: (L−R)/2 (mono)","Processed mid","Processed side"]):
        f,t,_,power = spectrum
        mesh(ax,f,t,db(power),title,(-110,-20))
        ax.axvline(3,color="cyan",ls="--",lw=.8)
    f,t,_,_ = spectra[0]
    for spectrum, label in [(spectra[0],"Input mid"),(spectra[2],"Output mid"),(spectra[3],"Output side")]:
        axes[2,0].plot(t,db(spectrum[3].sum(axis=0)),label=label)
    axes[2,0].set(xlabel="Time (s)",ylabel="Window mean-square (dBFS)",ylim=(-90,0)); axes[2,0].legend()
    ratio = db(spectra[3][3].sum(axis=0)/np.maximum(spectra[2][3].sum(axis=0),1e-16))
    ratio = np.ma.array(ratio, mask=spectra[2][3].sum(axis=0) < 1e-8)
    axes[2,1].plot(t,ratio); axes[2,1].set(xlabel="Time (s)",ylabel="Side / mid energy (dB)",ylim=(-60,30))
    fig.suptitle("Kuo Stereo • mono transient pad → fixed 12 ms Haas • Velvet OFF")
    save(fig,destination,"audio-mid-side-spectrogram.png")
    mid = (wide["output_left"]+wide["output_right"])/2
    side = (wide["output_left"]-wide["output_right"])/2
    return f"""## Rendered mid and side audio

![Input and output mid/side spectrograms](audio-mid-side-spectrogram.png)

Six seconds of the deterministic mono C→Am pad plus 400 ms-spaced bursts pass through the **full production audio path** at 48 kHz, in 128-sample blocks. A neutral 0 ms Haas/0% Velvet render must reproduce the latency-aligned input and have silent side. The illustrated condition uses fixed 12 ms Haas, zero Variability, **Velvet OFF (Width 0%)**, 80% Transient protection, zero Dynamic and Attack, all Velvet bands 100%, Output 0 dB. Only the declared 72-sample host latency is removed; the intentional Haas delay remains. The render includes a 100 ms zero-input tail. The cyan line marks the 3 s chord change.

`M=(L+R)/2`, `S=(L−R)/2`; no channel or image normalization is applied. The ratio trace is hidden where mid energy is below −80 dBFS. Integrated rendered mid energy is {db(np.mean(mid**2)):.2f} dBFS and side energy {db(np.mean(side**2)):.2f} dBFS (side/mid {db(np.sum(side**2)/np.sum(mid**2)):.2f} dB). This Haas-only full-path example complements the search snapshots: it does not evaluate adaptive worker scheduling or claim exact mid preservation with Haas enabled. Raw aligned samples are in `audio-neutral.csv.gz` and `audio-wide.csv.gz`.
"""


def zao(source, destination):
    settings = load(source/"audio-settings.csv", constant_rate=False)
    for switch in ["clear","auto_compensate","transformer","bypass","capture_smart"]:
        assert np.all(settings[switch] == 0), f"Academic Zao capture requires {switch} off"
    assert np.all(settings["input_db"] == 0) and np.all(settings["output_db"] == 0) and np.all(settings["mix"] == 1)
    lines = ["## Rendered Smart on/off spectrograms", "", "| Fixture | Whole wet clash-energy Δ dB | Targeted clash-energy Δ dB |", "|---|---:|---:|"]
    images = []
    for corpus in ["C_chord","harmonic_collision"]:
        audio, weights = load(source/f"audio-{corpus}.csv"), load(source/f"clash-{corpus}.csv")
        rate = audio["rate"][0]
        results = {name:stft(audio[name],rate,4096,256) for name in ["wet_off","wet_on","total_off","total_on"]}
        f,t,_,off = results["wet_off"]; on = results["wet_on"][3]
        assert np.array_equal(f,weights["frequency_hz"])
        w = weights["weight"][:,None]
        targeted = w*weights["target"][:,None]
        energy = [(off*weight).sum(axis=0) for weight in [w,targeted]]
        after = [(on*weight).sum(axis=0) for weight in [w,targeted]]
        changes = [float(db(a.sum()/b.sum())) for a,b in zip(after,energy)]
        assert energy[1].sum() > 1e-12, "Selected harmonics must carry measured audio energy"
        assert changes[1] < -.1, "Smart must attenuate actual weighted target-bin energy"
        lines.append(f"| {corpus} | {changes[0]:.3f} | {changes[1]:.3f} |")
        fig,axes = plt.subplots(4,2,figsize=(13,14),constrained_layout=True)
        for ax,name in zip(axes[:2].flat,["total_off","total_on","wet_off","wet_on"]):
            mesh(ax,f,t,db(results[name][3]),name.replace("_"," "),(-110,-15))
        for ax,kind in zip(axes[2],["total","wet"]):
            p,q = results[kind+"_off"][3],results[kind+"_on"][3]
            mask = p < max(p.max()*1e-8,1e-16)
            delta = np.ma.array(db(q)-db(p),mask=mask)
            mesh(ax,f,t,delta,f"{kind}: Smart on − off",(-6,6),delta=True)
            if kind == "wet":
                selected = f[weights["target"] > 0]
                ax.plot(np.full(len(selected),t[0]), selected, ">", color="black", markersize=2)
        for i,label in enumerate(["Whole wet","Selected harmonic bins"]):
            axes[3,0].plot(t,db(energy[i]),label=label+" off")
            axes[3,0].plot(t,db(after[i]),ls="--",label=label+" on")
        axes[3,0].set(xlabel="Time (s)",ylabel="Weighted wet energy (dBFS)"); axes[3,0].legend(fontsize=8)
        axes[3,1].bar(["Whole wet","Targeted"],changes,color=["#bb8533","#427b99"])
        axes[3,1].set(ylabel="Measured clash-energy on/off (dB)"); axes[3,1].axhline(0,color="gray")
        fig.suptitle(f"Zao Drive • op-amp 24 dB • {corpus} • frozen production Smart bank")
        filename = f"audio-smart-{corpus}.png"; save(fig,destination,filename)
        images.append(f"![Smart spectral difference: {corpus}]({filename})")
    return "\n".join(lines)+"\n\n"+"\n\n".join(images)+"""

The paired inputs are the existing 48 kHz op-amp **C-like chord and harmonic-collision fixtures**, not hand-picked output bins. **Clear, Auto compensation, Transformer and Bypass always stay OFF** in these academic tests; Smart alone is varied for the comparison. Input and Output are explicitly 0 dB. At 24 dB drive, Tone 16 kHz and Mix 100%, the real oversampled circuit supplies an aligned dry reference and post-tone wet residual. The captured periodic 4,096-sample frame feeds the real Smart filter bank continuously for 20 cycles; discard four settling cycles and show the remaining 16 (~1.37 s). Off is that same unfiltered wet capture; total output is dry+wet. The frozen production selection removes worker/crossfade timing from this comparison. Repeated material intentionally yields horizontal spectral lines, not a musical passage.

Blue in the difference maps means attenuation, red enhancement, both computed from **actual samples**. Wet and total changes are separate because phase cancellation can increase some dry+wet bins even when the wet bank cuts. Difference bins below −80 dB of the off image maximum are hidden. All on/off levels use identical scales and no gain matching.

“Clash energy” is measured STFT wet power weighted by the production `intervalDissonance(f,input_partial)` averaged over the known, equally weighted input partials. Weights remain fixed for both renders. The table includes the whole wet spectrum and, separately, bins within one bandwidth of any selected production harmonic target (black marks at the left edge). It reports **10 log10(sum weighted on power / sum weighted off power)**; there is no output-dependent normalization. Tests require nonzero target energy and measured attenuation greater than 0.1 dB on both fixtures. This is an operational check of the plugin's own 12-TET heuristic on synthetic signals, not an independent psychoacoustic model or proof that listeners perceive less clash. CSV captures and fixed frequency weights accompany the plots.
"""


def xuan(source,destination):
    sections = ["## Rendered transients and phase spectrograms"]
    for corpus in ["dispersed_packets","broadband_attack","attack_over_sustain"]:
        a = load(source/f"audio-{corpus}-current.csv")
        b = load(source/f"audio-{corpus}-bypass.csv")
        assert np.max(abs(b["input_left"]-b["output_left"])) < 1e-5
        assert np.array_equal(a["input_left"],b["input_left"])
        rate = a["rate"][0]
        f,t,z,p = stft(a["input_left"],rate,512,32)
        _,_,y,q = stft(a["output_left"],rate,512,32)
        before,after,delta = phase_pair(z,y)
        assert before.count() > 0 and after.count() > 0 and delta.count() > 0
        assert np.max(abs(a["output_left"]-a["input_left"])) > 1e-5
        fig,axes = plt.subplots(4,2,figsize=(13,13),constrained_layout=True)
        time = np.arange(len(a))/rate
        axes[0,0].plot(time,a["input_left"],label="Input",lw=.8)
        axes[0,0].plot(time,a["output_left"],label="Processed",lw=.8,alpha=.8)
        axes[0,0].set(xlabel="Input-aligned time (s)",ylabel="Amplitude",xlim=(0,.08)); axes[0,0].legend()
        for powers,label in [(p,"Input"),(q,"Processed")]:
            axes[0,1].plot(t,db(powers.sum(axis=0)),label=label)
        axes[0,1].set(xlabel="Time (s)",ylabel="Transient window energy (dBFS)",xlim=(0,.08),ylim=(-100,0)); axes[0,1].legend()
        for ax,power,title in [(axes[1,0],p,"Input magnitude"),(axes[1,1],q,"Processed magnitude")]:
            mesh(ax,f,t,db(power),title,(-100,-20),max_hz=18000)
        for ax,values,title in [(axes[2,0],before,"Input phase"),(axes[2,1],after,"Processed phase"),(axes[3,0],delta,"Phase change: processed − input")]:
            mesh(ax,f,t,values,title,(-np.pi,np.pi),phase=True,max_hz=18000)
        axes[3,1].axis("off")
        axes[3,1].text(0,.9,"Same input timeline; 2,400-sample host latency removed.\n512-sample Hann / 32-sample hop (10.67 / 0.67 ms).\nCyclic phase colour: −π to +π.\nHide phase >50 dB below common peak bin power.\nNo per-image or loudness normalization.\nFrozen automatic correction, full positive Depth.\nEnvelope alignment and carrier phase are distinct measures.",va="top",fontsize=10,linespacing=1.8)
        for ax in axes[1:,:].flat:
            if ax.axison: ax.set_xlim(0,.08)
        fig.suptitle(f"Xuan Phase • {corpus} • actual partitioned convolution output")
        filename = f"audio-phase-{corpus}.png"; save(fig,destination,filename)
        sections.append(f"![Transient and phase before/after: {corpus}]({filename})")
    return "\n\n".join(sections)+"""

The three existing 48 kHz fixtures are rendered through the actual finite-FIR/partitioned-convolution path with the analysed event frozen, Auto on, Depth +100%, manual rotation zero and Stereo Link +100%. Remove exactly the declared 50 ms host latency from the output, not an estimated best alignment. A separately rendered neutral reference must match the padded input to 1e−5. The output tail remains available in the raw CSV; plots zoom to the first 80 ms. No event-specific time shift or amplitude scaling is applied.

Phase is the wrapped complex **audio STFT phase**, not the requested filter response or the editor meter. Before and after use identical windows and time origins. Each phase map masks bins more than 50 dB below the shared maximum; the difference is `arg(Y*conj(X))`, shown only where both signals pass that threshold. Waveforms, magnitude maps and window energy show how transient structure changes alongside carrier phase. Narrow low-frequency packets need the longer-window response/arrival metrics elsewhere in this report; these short-window images prioritize transient time resolution. Changed phase does not itself prove sharper transients: unfavorable results remain visible. The frozen render does not test live detector scheduling or correction release.
"""


def report(project,source,destination):
    text = {"KuoStereo":kuo,"ZaoDrive":zao,"XuanPhase":xuan}[project](source,destination)
    text += "\nAll spectrograms derive from native sample captures, using NumPy FFTs and Hann windows without boundary padding. One-sided per-bin power is `|FFT(x*w)|²/(N*sum(w²))`, doubling interior bins; its sum is the window-weighted mean square (0 dBFS = mean square 1). Kuo uses 2,048/256 samples (window/hop), Zao 4,096/256, Xuan 512/32, all at 48 kHz. Phase is masked in silence. Raw captures are gzip-compressed beside this report.\n"
    (destination/"audio-spectrograms.md").write_text(f"# {project}: audio spectrogram measurements\n\n"+text)
    return "\n\n"+text
