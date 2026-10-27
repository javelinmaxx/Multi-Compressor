# APRiCOT: audio spectrogram measurements

## Rendered transients and phase spectrograms

![Transient and phase before/after: dispersed_packets](audio-phase-dispersed_packets.png)

![Transient and phase before/after: broadband_attack](audio-phase-broadband_attack.png)

![Transient and phase before/after: attack_over_sustain](audio-phase-attack_over_sustain.png)

The three existing 48 kHz fixtures are rendered through the actual finite-FIR/partitioned-convolution path with the analysed event frozen, Auto on, Depth +100%, manual rotation zero and Stereo Link +100%. Remove exactly the declared 50 ms host latency from the output, not an estimated best alignment. A separately rendered neutral reference must match the padded input to 1e−5. The output tail remains available in the raw CSV; plots zoom to the first 80 ms. No event-specific time shift or amplitude scaling is applied.

Phase is the wrapped complex **audio STFT phase**, not the requested filter response or the editor meter. Before and after use identical windows and time origins. Each phase map masks bins more than 50 dB below the shared maximum; the difference is `arg(Y*conj(X))`, shown only where both signals pass that threshold. Waveforms, magnitude maps and window energy show how transient structure changes alongside carrier phase. Narrow low-frequency packets need the longer-window response/arrival metrics elsewhere in this report; these short-window images prioritize transient time resolution. Changed phase does not itself prove sharper transients: unfavorable results remain visible. The frozen render does not test live detector scheduling or correction release.

All spectrograms derive from native sample captures, using NumPy FFTs and Hann windows without boundary padding. One-sided per-bin power is `|FFT(x*w)|²/(N*sum(w²))`, doubling interior bins; its sum is the window-weighted mean square (0 dBFS = mean square 1). Kuo uses 2,048/256 samples (window/hop), Zao 4,096/256, APRiCOT 512/32, all at 48 kHz. Phase is masked in silence. Raw captures are gzip-compressed beside this report.
