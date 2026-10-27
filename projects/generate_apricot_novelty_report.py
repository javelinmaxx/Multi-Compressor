#!/usr/bin/env python3
"""Measured APRiCOT spectral novelty → residual anchor → phase-target handoff."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def positive_novelty(magnitude, previous_neighbour, largest, floor):
    return np.where(magnitude > largest*.01,np.maximum(0,magnitude-previous_neighbour*1.25-floor*.5),0)


def spectral_residual(spectrum, novelty):
    size = len(spectrum)
    selected = np.zeros(size,dtype=complex)
    bins = np.arange(1,size//2)
    selected[bins] = spectrum[bins]*novelty[bins]/np.maximum(abs(spectrum[bins]),1e-12)
    selected[size-bins] = np.conj(selected[bins])
    return np.fft.ifft(selected).real


def phase_targets(spectrum, bins, anchor, anchor_value, rate):
    size = len(spectrum)
    wrap = lambda phase: np.angle(np.exp(1j*phase))
    sign = np.pi if anchor_value < 0 else 0
    angle = wrap(sign-2*np.pi*bins*anchor/size-np.angle(spectrum[bins]))
    slope = wrap(np.angle(spectrum[bins+1])-np.angle(spectrum[bins-1])+4*np.pi*anchor/size)
    delay = np.clip(-slope/(4*np.pi*rate/size),-.025,.025)
    return angle,delay


def read(path):
    return np.genfromtxt(path,delimiter=",",names=True)


def report(source,destination):
    audio,frames,spectra,context,peaks,curve = [read(source/(name+".csv")) for name in
        ["novelty-audio","novelty-frames","novelty-spectrum","novelty-context","novelty-peaks","novelty-target-curve"]]
    rate = int(frames["rate"][0]); size = int(frames["frame_size"][0])
    assert np.all(frames["rate"] == rate) and np.all(frames["frame_size"] == size)
    assert np.all(np.diff(frames["frame_sample"]) == 128)
    events = frames[frames["onset"] != 0]
    expected = audio["sample"][audio["expected_onset"] != 0]
    assert len(events) == len(expected) == 2
    assert np.all(events["novelty"] > events["threshold"])
    assert np.all((events["frame_sample"] >= expected) & (events["frame_sample"] < expected+.03*rate))
    assert np.all(frames["targets"][frames["estimating"] == 0] == 0)
    frame_samples = frames["frame_sample"].astype(int)
    matrix = {}
    for field in ["input_magnitude","previous_neighbour_max","novelty_magnitude","event_novelty_magnitude"]:
        matrix[field] = spectra[field].reshape(len(frames),size//2-1).T
    np.testing.assert_array_equal(spectra["frame_sample"],np.repeat(frame_samples,size//2-1))
    np.testing.assert_array_equal(spectra["bin"],np.tile(np.arange(1,size//2),len(frames)))
    prediction = positive_novelty(matrix["input_magnitude"],matrix["previous_neighbour_max"],frames["largest_magnitude"],frames["noise_floor"])
    np.testing.assert_allclose(prediction,matrix["novelty_magnitude"],atol=2e-6,rtol=2e-5)
    selected_frame = int(context["frame_sample"][0])
    assert np.all(context["frame_sample"] == selected_frame)
    assert selected_frame == events["frame_sample"][0]
    np.testing.assert_array_equal(context["sample"],np.arange(selected_frame-size,selected_frame))
    anchor = int(np.flatnonzero(context["is_anchor"])[0])
    assert np.sum(context["is_anchor"]) == 1 and anchor == np.argmax(abs(context["residual"]))
    fft = np.fft.fft(context["windowed_input"])
    index = int(np.flatnonzero(frame_samples == selected_frame)[0])
    novelty = np.zeros(size); novelty[1:size//2] = matrix["event_novelty_magnitude"][:,index]
    np.testing.assert_allclose(spectral_residual(fft,novelty),context["residual"],atol=2e-7,rtol=2e-4)
    bins = peaks["bin"].astype(int)
    assert np.all(peaks["anchor_index"] == anchor)
    np.testing.assert_allclose(peaks["anchor_value"],context["residual"][anchor],atol=1e-9)
    np.testing.assert_allclose(fft[bins],peaks["real"]+1j*peaks["imag"],atol=3e-5,rtol=3e-5)
    angle,delay = phase_targets(fft,bins,anchor,context["residual"][anchor],rate)
    np.testing.assert_allclose(np.angle(np.exp(1j*(angle-peaks["target_angle_rad"]))),0,atol=2e-5)
    np.testing.assert_allclose(delay,peaks["relative_delay_s"],atol=2e-7)
    assert len(peaks) > 0 and np.all(abs(curve["requested_deg"]) <= 150.00001)

    fig = plt.figure(figsize=(14,14))
    grid = fig.add_gridspec(5,2,height_ratios=[1,.95,1,1.3,.4],left=.07,right=.92,top=.94,bottom=.045,hspace=.65,wspace=.5)
    waveform = fig.add_subplot(grid[0,:]); spectrum_ax = fig.add_subplot(grid[1,0]); novel_ax = fig.add_subplot(grid[1,1])
    detector = fig.add_subplot(grid[2,:]); anchor_ax = fig.add_subplot(grid[3,0]); estimator = fig.add_subplot(grid[3,1])
    flow = fig.add_subplot(grid[4,:]); flow.axis("off")
    time = audio["sample"]/rate
    waveform.plot(time,audio["background"],color="gray",alpha=.4,lw=.5,label="Louder sustained 220/330 Hz background")
    waveform.plot(time,audio["input"],color="#24748d",lw=.5,label="Input mixture")
    waveform.plot(time,audio["attack"],color="#bd6e17",lw=1,label="Known new attack component (not detector input)")
    for i,onset in enumerate(expected/rate):
        waveform.axvline(onset,color="#268b69",ls=":",lw=1,label="Constructed attack start" if i == 0 else None)
    for i,event in enumerate(events["frame_sample"]/rate):
        waveform.axvline(event,color="#b43c42",ls="--",lw=1,label="Detected new event" if i == 0 else None)
    waveform.axvspan((selected_frame-size)/rate,selected_frame/rate,color="#b43c42",alpha=.12,label="Selected phase-estimation frame")
    waveform.set(title="(a) Soft transients emerging under a louder sustained sound",xlabel="Input stream time (s)",ylabel="Amplitude",xlim=(.2,1))
    waveform.legend(fontsize=8,ncol=3,loc="upper right"); waveform.grid(alpha=.15)
    frequencies = np.arange(1,size//2)*rate/size
    for ax,field,title in [(spectrum_ax,"input_magnitude","(b) Input FFT magnitude: first-attack zoom"),(novel_ax,"novelty_magnitude","Positive spectral novelty N(k,t): first attack")]:
        values = 20*np.log10(np.maximum(2*matrix[field]/size,1e-10))
        im = ax.pcolormesh(frame_samples/rate,frequencies,values,shading="auto",cmap="magma",vmin=-90,vmax=-10,rasterized=True)
        ax.set(title=title,xlabel="Analysis frame end (s)",ylabel="Frequency (Hz)",ylim=(60,16000),xlim=(.225,.285))
        ax.set_yscale("log")
        ax.axvline(expected[0]/rate,color="#268b69",ls=":",lw=.8)
        ax.axvline(events["frame_sample"][0]/rate,color="#b43c42",ls="--",lw=.8)
        fig.colorbar(im,ax=ax,label="2M/N magnitude (dB re 1)")
    detector.plot(frame_samples/rate,frames["novelty"],color="#24748d",label="Production spectral novelty score")
    detector.plot(frame_samples/rate,frames["threshold"],"--",color="#b43c42",label="Actual adaptive threshold")
    detector.scatter(events["frame_sample"]/rate,events["novelty"],color="#b43c42",s=35,zorder=5,label="New onset flag")
    for event in events["frame_sample"]:
        detector.axvspan(event/rate,(event/rate)+.006,color="#bd6e17",alpha=.15)
    target_axis = detector.twinx()
    target_axis.step(frame_samples/rate,frames["targets"],where="post",color="gray",alpha=.5,label="Returned phase targets")
    target_axis.set(ylabel="Number of phase targets",ylim=(0,max(4,float(frames["targets"].max())+1)))
    detector.set(title="(c) Threshold crossing + event gates → new event and up to 6 ms context collection",xlabel="Analysis frame end (s)",ylabel="Novelty / threshold",xlim=(.2,1))
    detector.legend(fontsize=8,loc="upper left",ncol=3); target_axis.legend(fontsize=8,loc="upper right"); detector.grid(alpha=.15)
    context_time = context["sample"]/rate*1000
    anchor_time = context_time[anchor]
    input_peak = int(np.flatnonzero(context["is_input_peak"])[0])
    anchor_ax.plot(context_time,context["input"],color="gray",alpha=.6,lw=.8,label="Input mixture")
    residual_axis = anchor_ax.twinx()
    residual_axis.plot(context_time,context["residual"],color="#b43c42",lw=1,label="Actual IFFT spectral residual")
    residual_axis.scatter([anchor_time],[context["residual"][anchor]],color="#b43c42",marker="*",s=100,zorder=5)
    anchor_ax.axvline(anchor_time,color="#b43c42",ls="--",label=f"Residual anchor: {anchor_time:.2f} ms")
    anchor_ax.scatter([context_time[input_peak]],[context["input"][input_peak]],color="black",s=25,label="Whole-window |input| maximum")
    anchor_ax.set(title="(d) Residual peak → transient anchor",xlabel="Absolute input time (ms)",ylabel="Input amplitude")
    residual_axis.set_ylabel("Residual amplitude",color="#b43c42")
    anchor_ax.legend(fontsize=7,loc="upper left"); residual_axis.legend(fontsize=7,loc="upper right"); anchor_ax.grid(alpha=.15)
    estimator.semilogx(curve["frequency_hz"],curve["interpolated_deg"],color="#24748d",label="Interpolated phase targets")
    estimator.semilogx(curve["frequency_hz"],curve["requested_deg"],"--",color="#b43c42",label="Requested phase (±150° cap)")
    estimator.scatter(peaks["frequency_hz"],peaks["interpolated_deg"],color="black",s=30,label="Selected spectral peaks")
    delay_axis = estimator.twinx()
    delay_axis.scatter(peaks["frequency_hz"],1000*peaks["relative_delay_s"],marker="s",color="#bd6e17",s=25,label="Local phase-slope delay τₖ")
    delay_axis.set_ylabel("Relative delay τₖ (ms)",color="#bd6e17")
    estimator.set(title="(e) Anchor-referenced phase and delay targets",xlabel="Frequency (Hz, logarithmic)",ylabel="Phase (degrees)",xlim=(60,20000))
    estimator.legend(fontsize=7,loc="upper left"); delay_axis.legend(fontsize=7,loc="upper right"); estimator.grid(alpha=.15)
    boxes = ["N(k,t) → novelty score\nadaptive threshold + event gates",f"New event + first phase estimate\nframe end {selected_frame/rate*1000:.2f} ms",f"Positive residual IFFT\nanchor = {anchor_time:.2f} ms","Anchor-referenced θₖ, τₖ\n→ interpolated phase request"]
    for i,text in enumerate(boxes):
        x = .115+i*.255
        flow.text(x,.5,text,ha="center",va="center",fontsize=9,bbox={"boxstyle":"round,pad=.6","facecolor":"#eef4f5","edgecolor":"#24748d"})
        if i < 3: flow.annotate("",xy=(x+.135,.5),xytext=(x+.1,.5),arrowprops={"arrowstyle":"->","color":"#24748d","lw":1.5})
    fig.suptitle("APRiCOT • measured spectral-novelty detection and phase-estimator handoff • 48 kHz")
    fig.savefig(destination/"novelty-phase-handoff.png",dpi=170)
    fig.savefig(destination/"novelty-phase-handoff.pdf",metadata={"CreationDate":None,"ModDate":None})
    plt.close(fig)
    followups = frames[(frames["frame_sample"] >= selected_frame) & (frames["frame_sample"]-selected_frame <= .006*rate) & (frames["estimating"] != 0)]
    peak_rows = "\n".join(f"| {row['frequency_hz']:.2f} | {np.rad2deg(row['target_angle_rad']):.3f} | {row['relative_delay_s']*1000:.3f} |" for row in peaks)
    unexpected = [float(hz) for hz in peaks["frequency_hz"] if min(abs(hz-np.array([2437.5,6750.]))) > rate/size]
    peak_note = ("Selected "+", ".join(f"{hz:g} Hz" for hz in unexpected)+" is outside one FFT bin of either constructed attack partial (2,437.5/6,750 Hz)." if unexpected else "All first-frame targets lie within one FFT bin of a constructed attack partial.")
    drift_note = ("On this fixture a later context frame relocates the residual anchor into the earlier sustained background. Detection of the new attack therefore does not guarantee that every subsequent phase estimate stays anchored to it. This unfavorable outcome is preserved." if np.any(followups["anchor_sample"] < expected[0]) else "All shown follow-up anchors remain after the constructed attack start on this fixture; this does not establish robust separation for other inputs.")
    followup_rows = "\n".join(f"| {row['frame_sample']/rate*1000:.3f} | {row['anchor_sample']/rate*1000:.3f} | {int(row['targets'])} | {'yes' if row['anchor_sample'] >= expected[0] else 'no'} |" for row in followups)
    rows = "\n".join(f"| {start/rate*1000:.2f} | {event/rate*1000:.2f} | {(event-start)/rate*1000:.2f} |" for start,event in zip(expected,events["frame_sample"]))
    text = f"""## Spectral novelty → detected transient → phase estimator

![Measured novelty, residual anchor and phase-estimator handoff](novelty-phase-handoff.png)

[Vector PDF](novelty-phase-handoff.pdf)

This deterministic streamed fixture uses the **production `PhaseEngine::analyse()`** at 48 kHz, 2,048-sample frames and 128-sample hops. Two soft two-partial attacks (2,437.5/6,750 Hz, amplitude coefficient 0.035, 1.5 ms rise, 22 ms decay) emerge over a louder stationary 220/330 Hz background (amplitudes 0.25/0.10). The first 200 ms warm the detector. Ground-truth attack starts are fixture labels, not detector inputs.

| Constructed attack start (ms) | Detector frame end (ms) | Detection delay (ms) |
|---|---|---|
{rows}

**(a–b)** show the original mixture and newly emerging spectral content; the heatmaps zoom to the first attack (225–285 ms), while the waveform and detector trace show both events. At each bin, the observed positive residual magnitude is `N=max(0, M−1.25*max(previous neighbour magnitudes)−0.5*floor)` when `M>0.01*largest`, otherwise zero. The figure exports this intermediate value from the real detector. Both heatmaps share the same scale, `20 log10(2*magnitude/2048)`, without separate gain normalization. The FFT uses production's short edge tapers, not a substitute Hann analysis. Spectral images are timestamped at the **end** of the analysis window.

**(c)** shows the actual novelty score and adaptive threshold. The threshold is the maximum of 0.125, median+4×max(0.003,MAD), and mean+3.5×standard deviation over the current history. A threshold crossing alone is insufficient: production energy/attack/emergence, release and 20 ms re-arm gates also apply. Red dots are the actual new-onset flags. Orange spans show the up-to-6 ms event-context collection; gray steps count targets actually returned to the phase estimator. Follow-up frames can return targets without flagging another onset.

**(d)** selects the first detected-event frame at **{selected_frame/rate*1000:.3f} ms**. Production preserves the current complex FFT phase, replaces magnitudes with the event's accumulated positive novelty, imposes conjugate symmetry, and takes the IFFT. Its largest absolute real sample becomes the transient anchor at **{anchor_time:.3f} ms**, rather than automatically using the mixture's whole-window peak. The residual has its own labeled amplitude axis; it is not secretly gain-matched to the louder input.

**(e)** uses that **same exported anchor and FFT** to show selected targets and the resulting interpolation. For FFT peak bin k and frame-relative anchor a, production computes `θₖ=wrap(sign(anchor)−2π*k*a/N−arg(Xₖ))` (sign term π for a negative anchor, 0 otherwise) and `τₖ=clip(−wrap(arg(Xₖ₊₁)−arg(Xₖ₋₁)+4π*a/N)/(4π*fs/N), ±25 ms)`. Those targets enter the actual smooth interpolation and ±150° capped phase request at full positive Auto Depth. This is a measured connection between detection and phase estimation, not a schematic arrow attached to independently synthesized targets.

The first returned targets are retained exactly, including peaks that differ from the known synthesized attack partials:

| Selected frequency (Hz) | Anchor-referenced θₖ (degrees) | Local τₖ (ms) |
|---|---:|---:|
{peak_rows}

{peak_note} A successful onset flag therefore does not guarantee perfect separation of the new attack or selection of every intended partial.

### Follow-up context: observed anchor drift

The figure selects the **first new-event frame by rule**, not the best-looking later frame. The complete trace also retains the estimator's following context frames:

| Frame end (ms) | Absolute anchor (ms) | Returned targets | Anchor at/after the new attack start? |
|---|---|---:|---|
{followup_rows}

{drift_note} The plot is an initial-handoff demonstration, not proof of robust separation or uniformly correct phase estimation.

The observer compiles **only into the academic executable**. Native tests require exactly two post-warmup detections, both within 30 ms, no phase targets outside event/context frames, and an anchor belonging to the new attack at the actual residual peak. Python independently reconstructs positive novelty, the residual IFFT, and the anchor-referenced phase/delay targets from exported windowed samples and compares them with native results. This synchronous chronological analysis experiment excludes worker scheduling, kernel handoff deadlines and audio-output latency; it does not establish detector accuracy on recordings or listeners.

Raw data: `novelty-audio.csv.gz`, `novelty-frames.csv.gz`, `novelty-spectrum.csv.gz`, `novelty-context.csv.gz`, `novelty-peaks.csv.gz` and `novelty-target-curve.csv.gz`.
"""
    (destination/"novelty-phase-handoff.md").write_text("# APRiCOT: spectral-novelty transient detection and phase estimation\n\n"+text)
    return text
