# KuoStereo: audio spectrogram measurements

## Rendered mid and side audio

![Input and output mid/side spectrograms](audio-mid-side-spectrogram.png)

Six seconds of the deterministic mono C→Am pad plus 400 ms-spaced bursts pass through the **full production audio path** at 48 kHz, in 128-sample blocks. A neutral 0 ms Haas/0% Velvet render must reproduce the latency-aligned input and have silent side. The illustrated condition uses fixed 12 ms Haas, zero Variability, 100% Velvet, 80% Transient protection, zero Dynamic and Attack, all Velvet bands 100%, Output 0 dB. Only the declared 72-sample host latency is removed; the intentional Haas delay remains. The render includes a 100 ms zero-input tail. The cyan line marks the 3 s chord change.

`M=(L+R)/2`, `S=(L−R)/2`; no channel or image normalization is applied. The ratio trace is hidden where mid energy is below −80 dBFS. Integrated rendered mid energy is -23.03 dBFS and side energy -24.85 dBFS (side/mid -1.83 dB). This full-path example complements the search snapshots: it does not evaluate adaptive worker scheduling or claim exact mid preservation with Haas enabled. Raw aligned samples are in `audio-neutral.csv.gz` and `audio-wide.csv.gz`.

All spectrograms derive from native sample captures, using NumPy FFTs and Hann windows without boundary padding. One-sided per-bin power is `|FFT(x*w)|²/(N*sum(w²))`, doubling interior bins; its sum is the window-weighted mean square (0 dBFS = mean square 1). Kuo uses 2,048/256 samples (window/hop), Zao 4,096/256, Xuan 512/32, all at 48 kHz. Phase is masked in silence. Raw captures are gzip-compressed beside this report.
