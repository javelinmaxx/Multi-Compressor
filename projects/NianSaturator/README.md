# Nian Saturator

Two-band processor with a shared crossover, preventing overlapping cutoff
settings. F mode tracks spectrally prominent peaks in each band and adds narrow
bandpass emphasis; T mode applies soft drive independently to the two bands.
Low and High set their respective amounts and Output sets the final level.

The editor follows the shared J-Sonic style and supports 50%–120% scaling.
The bottom display plots the latency-aligned input waveform, the generated
delta, and their sum. Its logarithmic frequency bar marks the shared crossover:
Low is below the marker and High is above it. The delta includes output gain
and follows bypass, so the plotted sum corresponds to the actual output.

The split is complementary: the high-band signal is the input minus a
one-pole low-pass. Its bands therefore overlap around the single crossover,
but their unprocessed sum reconstructs the input. F mode uses a 4096-point
Hann-windowed spectrum and a local spectral-floor test to track one prominent
peak per band, then adds a narrow Q=12 bandpass. The filter coefficients,
crossover, mode and bypass transitions are smoothed over 20 ms. T mode uses
two independent soft-clipping curves. The complete split, F detector and both
saturation modes run at 4× the host sample rate through a linear-phase FIR
converter. The converter reports 64 host samples of latency; bypass uses an
equally delayed dry path. The audio thread sends a decimated waveform to a
fixed-size queue, and only the display repaints at up to 30 Hz.

[Editor preview](Previews/nian-saturator.png) · [Processor test report](Tests/Reports/processor.md). Processor tests check neutral
reconstruction, both-band F peak enhancement, T drive, settled bypass and
editor hit targets at 50% and 120%, mode attachment, and finite output while automating mode and crossover.
