#!/usr/bin/env python3
"""Validate and plot Xuan's selected targets, actual finite kernel and DTFT error."""
import csv

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def response(kernel, frequencies, rate, centre):
    """Direct DTFT, with only the FIR centre delay removed; no normalization."""
    kernel = np.asarray(kernel, dtype=float)
    frequencies = np.asarray(frequencies, dtype=float)
    time = (np.arange(len(kernel))-centre)/rate
    measured = np.empty(len(frequencies), dtype=complex)
    for at in range(0,len(frequencies),128):
        measured[at:at+128] = np.exp(-2j*np.pi*np.outer(frequencies[at:at+128],time)) @ kernel
    return measured


def phase_error(measured, requested_degrees):
    return np.rad2deg(np.angle(measured*np.exp(-1j*np.deg2rad(requested_degrees))))


def read(path):
    return np.genfromtxt(path,delimiter=",",names=True,dtype=None,encoding="utf-8")


def report(source,destination):
    kernels, curves, peaks = [read(source/name) for name in ["fir-kernels.csv","fir-response.csv","fir-peaks.csv"]]
    summaries = []
    figures = []
    for rate in sorted(set(kernels["rate"])):
        for corpus in ["dispersed_packets","broadband_attack","attack_over_sustain"]:
            h = kernels[(kernels["rate"] == rate) & (kernels["corpus"] == corpus)]
            r = curves[(curves["rate"] == rate) & (curves["corpus"] == corpus)]
            p = peaks[(peaks["rate"] == rate) & (peaks["corpus"] == corpus)]
            centre = (len(h)-1)//2
            if len(h) != 2*centre+1 or abs(centre-rate*.025) > .500001 or len(r) < 2049 or not len(p):
                raise AssertionError("Each FIR condition needs the full kernel, dense response and detected peaks")
            np.testing.assert_array_equal(h["sample"],np.arange(len(h)))
            np.testing.assert_allclose(h["causal_ms"],1000*np.arange(len(h))/rate,atol=1e-7)
            np.testing.assert_allclose(h["relative_ms"],1000*(np.arange(len(h))-centre)/rate,atol=1e-7)
            assert np.isfinite(h["coefficient"]).all() and np.max(abs(h["coefficient"][[0,-1]])) < 1e-12
            assert np.all(np.diff(r["frequency_hz"]) > 0)
            assert np.all(abs(r["requested_deg"]) <= 150.00001)
            assert set(p["frequency_hz"]).issubset(set(r["frequency_hz"]))
            measured = response(h["coefficient"],r["frequency_hz"],rate,centre)
            # Recompute from actual coefficients instead of trusting the native
            # response table or reconstructing an ideal unit-magnitude kernel.
            np.testing.assert_allclose(measured,r["real"]+1j*r["imag"],atol=2e-8,rtol=2e-8)
            gain = 20*np.log10(np.maximum(abs(measured),1e-15))
            error = phase_error(measured,r["requested_deg"])
            np.testing.assert_allclose(gain,r["gain_db"],atol=2e-6)
            np.testing.assert_allclose(error,r["phase_error_deg"],atol=2e-5)
            # At the exact peak frequencies, the exported interpolation values
            # must coincide with the requested curve, including cap differences.
            indices = np.searchsorted(r["frequency_hz"],p["frequency_hz"])
            np.testing.assert_allclose(r["interpolated_deg"][indices],p["interpolated_deg"],atol=1e-7)
            np.testing.assert_allclose(r["requested_deg"][indices],p["requested_deg"],atol=1e-7)
            summary = [int(rate),corpus,len(h),float(np.sqrt(np.mean(error**2))),float(np.max(abs(error))),float(np.max(abs(gain))),float(np.ptp(gain))]
            summaries.append(summary)
            if rate != 48000:
                continue
            fig = plt.figure(figsize=(11,11),constrained_layout=True)
            grid = fig.add_gridspec(3,1,height_ratios=[1,1,1.6])
            a,b = fig.add_subplot(grid[0]),fig.add_subplot(grid[1])
            c = grid[2].subgridspec(2,1,height_ratios=[1.2,1])
            cp = fig.add_subplot(c[0]); cm = fig.add_subplot(c[1],sharex=cp)
            frequency = r["frequency_hz"]
            a.semilogx(frequency,r["interpolated_deg"],color="#24748d",label="Smooth interpolated target (before cap)")
            a.semilogx(frequency,r["requested_deg"],"--",color="#bd6e17",label="Requested phase (±150° cap)")
            a.scatter(p["frequency_hz"],p["interpolated_deg"],s=22,color="black",zorder=4,label="Selected spectral peaks")
            for bound in [-150,150]: a.axhline(bound,color="gray",lw=.6,ls=":")
            a.set(title="(a) Desired phase response",xlabel="Frequency (Hz, logarithmic)",ylabel="Phase (degrees)",xlim=(frequency[0],frequency[-1]))
            a.legend(fontsize=8,ncol=2); a.grid(alpha=.2)
            b.plot(h["causal_ms"],h["coefficient"],color="#24748d",lw=.9,label="Actual PhaseEngine::impulse() coefficients")
            centre_ms = centre*1000/rate
            for boundary in [0,2*centre_ms]: b.axvline(boundary,color="gray",ls="--",lw=1)
            b.axvline(centre_ms,color="#bd6e17",ls="--",lw=1.2,label="25 ms centre")
            b.axvspan(-2,0,color="gray",alpha=.12); b.axvspan(2*centre_ms,2*centre_ms+2,color="gray",alpha=.12)
            b.set(title="(b) Synthesized finite FIR impulse response",xlabel="Causal kernel time (ms)",ylabel="Coefficient",xlim=(-2,2*centre_ms+2))
            relative = b.secondary_xaxis("top",functions=(lambda t:t-25,lambda t:t+25))
            relative.set_xticks([-25,0,25]); relative.set_xlabel("Support relative to centre (ms): −25 to +25")
            b.legend(fontsize=8); b.grid(alpha=.2)
            # Put measured phase on the nearest requested branch. Error remains
            # principal circular phase error; no fitted time/phase offset is used.
            cp.semilogx(frequency,r["requested_deg"],color="#bd6e17",label="Ideal requested phase")
            cp.semilogx(frequency,r["requested_deg"]+error,"--",color="#24748d",label="Measured FIR phase (centre delay removed)")
            cp.set(title="(c) Realized versus ideal frequency response",ylabel="Phase (degrees)",xlim=(frequency[0],frequency[-1]))
            cp.legend(fontsize=8); cp.grid(alpha=.2)
            cm.semilogx(frequency,gain,color="#24748d",label="Measured FIR magnitude deviation")
            cm.axhline(0,color="black",ls=":",lw=1,label="Ideal magnitude: 0 dB")
            cm.set(xlabel="Frequency (Hz, logarithmic)",ylabel="Deviation (dB)")
            cm.legend(fontsize=8); cm.grid(alpha=.2)
            cm.text(.02,.05,f"Sampled max |phase error| {summary[4]:.3f}°; max |magnitude deviation| {summary[5]:.3f} dB",transform=cm.transAxes,fontsize=8,bbox={"facecolor":"white","alpha":.85,"edgecolor":"none"})
            fig.suptitle(f"Xuan Phase • {corpus} • 48 kHz / {len(h)} taps / Auto Depth +100%")
            name = "fir-core-"+corpus
            fig.savefig(destination/(name+".png"),dpi=170)
            fig.savefig(destination/(name+".pdf"),metadata={"CreationDate":None,"ModDate":None})
            plt.close(fig)
            figures.append(f"![Desired phase, actual FIR and realized response: {corpus}]({name}.png)\n\n[Vector PDF]({name}.pdf)")
    with (destination/"fir-approximation-metrics.csv").open("w",newline="") as stream:
        writer = csv.writer(stream,lineterminator="\n")
        writer.writerow(["rate","corpus","taps","phase_rmse_deg","max_phase_error_deg","max_magnitude_deviation_db","magnitude_ripple_db"])
        writer.writerows(summaries)
    radius_44100 = next((row[2]-1)*500/44100 for row in summaries if row[0] == 44100)
    text = "## Finite FIR core: requested, synthesized and realized\n\n"+"\n\n".join(figures)+f"""

Panel **(a)** evaluates the production `bandValue()` smooth interpolation densely on logarithmic frequency, with the detected spectral peaks marked at their connected/interpolated target angles. It separately shows the actual `phaseAt()` request after the ±150° cap. The original principal target angles are preserved in `fir-peaks.csv.gz`; interpolation uses connected angles and local phase-slope terms, so its curve can exceed the cap. No Python interpolation substitutes for the production implementation.

Panel **(b)** plots every actual coefficient returned by `PhaseEngine::impulse()`. At 48 kHz there are **2,401 taps**, indexed 0–2,400, with a **25 ms causal centre** and **−25 ms to +25 ms relative support** (0–50 ms causal time). Gray areas lie outside the finite support. These coefficients include the production taper and are not normalized or replaced by an ideal impulse. The separate input lookahead is not part of this kernel. At other sample rates, `centre = juce::roundToInt(0.025*fs)` and length is `2*centre+1`; at 44.1 kHz the centre/support radius in this run is {radius_44100:.6f} ms due to sample rounding. The plotter derives the centre from the exported odd kernel length rather than imposing a rounding convention.

Panel **(c)** compares the ideal request with a direct DTFT of those same coefficients: `Hc(f)=sum_n h[n]*exp(−j*2π*f*(n−centre)/fs)`. Only the declared FIR centre delay is removed; there is no fitted delay, phase offset, gain correction or loudness matching. Measured phase is displayed on the branch nearest the requested phase. Phase error is the wrapped difference `arg(Hc*exp(−j*requested))`; magnitude deviation is `20*log10(abs(Hc))`, relative to the ideal **0 dB**. Both favorable and unfavorable approximation errors remain visible.

All three synthetic fixtures are checked at **44.1/48/96 kHz**. The figures show 48 kHz; `fir-approximation-metrics.csv` retains all nine conditions. Each response uses 2,049 log-spaced frequencies from 20 Hz to min(20 kHz, 0.49×fs), plus every exact selected-peak frequency. The maxima/RMSE describe this frequency grid, not a continuous-frequency error bound. `fir-response.csv.gz` stores native interpolated/requested phase and measured complex response; `fir-kernels.csv.gz` stores all actual taps and both time coordinates.

Native tests verify finite taps, exact support length and tapered boundaries, the neutral unit impulse at the declared centre, and reproduction of each kernel by the actual partitioned-convolution audio path driven by a unit pulse (including the correctly rounded separate lookahead). The report independently recomputes the dense DTFT from the exported taps and checks complex response, phase error, magnitude deviation and exact peak/curve correspondence. These figures establish the finite-FIR approximation; they do not impose an unsupported universal error tolerance or claim better perceived transients.
"""
    (destination/"fir-core.md").write_text("# Xuan Phase: finite FIR core\n\n"+text)
    return text
