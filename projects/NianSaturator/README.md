# Nian Saturator

Two-band processor with a shared crossover, preventing overlapping cutoff
settings. F mode tracks twelve spectrally prominent peaks in separate frequency
cells and adds narrow bandpass emphasis; T mode applies gentle tube-style drive
independently to the two bands.
Low and High set their respective amounts, Mix blends the dry and processed
signals, and Output sets the final level.

The editor follows the shared J-Sonic style and supports 50%–120% scaling.
The bottom display shows the left-channel input, generated-delta, and output
spectra on a logarithmic 40 Hz–20 kHz axis. A gold marker separates the Low
and High regions at the shared crossover. The displayed delta includes Mix,
Output, and bypass, so input plus delta matches the actual output.

The split is complementary: the high-band signal is the input minus a
one-pole low-pass. Its bands therefore overlap around the single crossover,
but their unprocessed sum reconstructs the input. F mode analyzes a 4096-point
Hann-windowed host-rate spectrum, allowing useful resolution in the bass. Six
logarithmic cells below the crossover and six above it each select one prominent
peak and add a Q=24 bandpass. The detector refines peaks between FFT bins;
band gain rises in 4 ms and falls in 25 ms, while retuning takes 5 ms.
Crossover, mode, Mix and bypass transitions are smoothed over 20 ms. T mode
uses a lightly biased arctangent curve with a DC-blocked residual, at a lower
drive than the former tanh curve. The split and both saturation modes run at
4× the host sample rate through a linear-phase FIR
converter. The converter reports 64 host samples of latency; bypass uses an
equally delayed dry path. If an internal filter or oversampling state becomes
invalid, the processor resets that state, passes latency-aligned dry audio,
and fades the wet path back over 10 ms. The audio thread sends host-rate samples through a
fixed-size queue; the editor computes a 2048-point Hann-windowed spectrum at
up to 30 Hz without an FFT on the audio callback.

[Editor preview](Previews/nian-saturator.png) · [Processor test report](Tests/Reports/processor.md). Processor tests cover all twelve F bands, bass onset,
the gentler T curve, Mix, bypass, fault recovery, alias suppression, and scaled editor controls.
