# Nian Saturator

Two-band processor with a shared crossover, preventing overlapping cutoff
settings. F mode tracks twelve spectrally prominent peaks in separate frequency
cells and adds short bandpass pulses to detected onsets; T mode applies gentle tube-style drive
independently to the two bands.
Low and High set their respective amounts, Mix blends the dry and processed
signals, and Output sets the final level.

The editor follows the shared J-Sonic style and supports 50%–120% scaling.
The bottom display shows the left-channel input, generated-delta, and output
spectra on a logarithmic 40 Hz–20 kHz axis. It smooths neighboring FFT bins
and stacks the delta magnitude in gold above the input magnitude; the white
trace is the measured output spectrum. The gold stack is a visual comparison,
because spectral magnitudes do not add exactly when phases differ. A gold
marker separates the Low and High regions at the shared crossover.

The split is complementary: the high-band signal is the input minus a
one-pole low-pass. Its bands therefore overlap around the single crossover,
but their unprocessed sum reconstructs the input. F mode analyzes a 4096-point
Hann-windowed host-rate spectrum, allowing useful resolution in the bass. Six
logarithmic cells below the crossover and six above it each select one prominent
peak and add a Q=24 bandpass. The first cell starts at 40 Hz; the detector
rejects lower-frequency peaks instead of assigning their FFT leakage to that
cell. The detector refines peaks between FFT bins;
the FFT and candidate search run on a separate worker thread using preallocated
snapshots and a nonblocking audio-thread handoff. A busy worker may skip a
snapshot, and results from an earlier mode or crossover setting are discarded.
Each new peak gets a 2 ms hold with a 1 ms attack and a 6 ms exponential decay,
then returns toward dry even if the tone sustains. A 35 ms cooldown prevents
brief detector dropouts from repeatedly lifting the same tone. A pitch jump
exceeding 3% within an active cell fades that band's gain down over 8 ms,
resets its filter state, and starts a fresh short pulse at the new pitch;
smaller retunes use 5 ms coefficient smoothing.
The coefficient interpolation uses double precision so very narrow low-frequency
bands stay tuned accurately at the 4× processing rate.
At 100% Low or High, each active F band adds up to 0.995× its bandpass signal.
For a fully aligned tone at the band's center, dry plus that band is 1.995× the
original amplitude (+6 dB) before Mix, other bands, and Output.
An independent level-rise detector can retrigger a recently tracked band when
the same note is struck again, even if the long FFT still reports that pitch.
Crossover, mode, Mix and bypass transitions are smoothed over 20 ms. The F
detector reads the live input while the audio path is delayed by 5 ms, so its
peak decisions lead the sound reaching the filters. T mode uses a lightly
biased arctangent curve with a DC-blocked residual; a feed-forward band-level
detector gently reduces only the added harmonic signal on loud notes, with a
5 ms detector, 2 ms attack, and 75 ms release. The split and both saturation
modes run at 4× the host sample rate through a linear-phase FIR converter.
This original residual-control design takes conceptual inspiration from
[Roth-AIR's high-band saturation and compression](https://github.com/danielrothmann/Roth-AIR);
it does not reuse Roth-AIR source code.
The converter adds 64 host samples of latency beyond the 5 ms lookahead;
bypass uses an equally delayed dry path. If an internal filter or oversampling state becomes
invalid, the processor resets that state, passes latency-aligned dry audio,
and fades the wet path back over 10 ms. The audio thread sends host-rate samples through a
fixed-size queue; the editor computes a 2048-point Hann-windowed spectrum at
up to 30 Hz without an FFT on the audio callback.

[Editor preview](Previews/nian-saturator.png) · [Processor test report](Tests/Reports/processor.md). Processor tests cover all twelve F bands, the 40 Hz floor, the +6 dB single-band cap, bass onset,
the gentler T curve, Mix, bypass, fault recovery, alias suppression, and scaled editor controls.
