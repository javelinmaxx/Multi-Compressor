# APRiCOT: finite FIR core

## Finite FIR core: requested, synthesized and realized

![Desired phase, actual FIR and realized response: dispersed_packets](fir-core-dispersed_packets.png)

[Vector PDF](fir-core-dispersed_packets.pdf)

![Desired phase, actual FIR and realized response: broadband_attack](fir-core-broadband_attack.png)

[Vector PDF](fir-core-broadband_attack.pdf)

![Desired phase, actual FIR and realized response: attack_over_sustain](fir-core-attack_over_sustain.png)

[Vector PDF](fir-core-attack_over_sustain.pdf)

Panel **(a)** evaluates the production `bandValue()` smooth interpolation densely on logarithmic frequency, with the detected spectral peaks marked at their connected/interpolated target angles. It separately shows the actual `phaseAt()` request after the ±150° cap. The original principal target angles are preserved in `fir-peaks.csv.gz`; interpolation uses connected angles and local phase-slope terms, so its curve can exceed the cap. No Python interpolation substitutes for the production implementation.

Panel **(b)** plots every actual coefficient returned by `PhaseEngine::impulse()`. At 48 kHz there are **2,401 taps**, indexed 0–2,400, with a **25 ms causal centre** and **−25 ms to +25 ms relative support** (0–50 ms causal time). Gray areas lie outside the finite support. These coefficients include the production taper and are not normalized or replaced by an ideal impulse. The separate input lookahead is not part of this kernel. At other sample rates, `centre = juce::roundToInt(0.025*fs)` and length is `2*centre+1`; at 44.1 kHz the centre/support radius in this run is 24.988662 ms due to sample rounding. The plotter derives the centre from the exported odd kernel length rather than imposing a rounding convention.

Panel **(c)** compares the ideal request with a direct DTFT of those same coefficients: `Hc(f)=sum_n h[n]*exp(−j*2π*f*(n−centre)/fs)`. Only the declared FIR centre delay is removed; there is no fitted delay, phase offset, gain correction or loudness matching. Measured phase is displayed on the branch nearest the requested phase. Phase error is the wrapped difference `arg(Hc*exp(−j*requested))`; magnitude deviation is `20*log10(abs(Hc))`, relative to the ideal **0 dB**. Both favorable and unfavorable approximation errors remain visible.

All three synthetic fixtures are checked at **44.1/48/96 kHz**. The figures show 48 kHz; `fir-approximation-metrics.csv` retains all nine conditions. Each response uses 2,049 log-spaced frequencies from 20 Hz to min(20 kHz, 0.49×fs), plus every exact selected-peak frequency. The maxima/RMSE describe this frequency grid, not a continuous-frequency error bound. `fir-response.csv.gz` stores native interpolated/requested phase and measured complex response; `fir-kernels.csv.gz` stores all actual taps and both time coordinates.

Native tests verify finite taps, exact support length and tapered boundaries, the neutral unit impulse at the declared centre, and reproduction of each kernel by the actual partitioned-convolution audio path driven by a unit pulse (including the correctly rounded separate lookahead). The report independently recomputes the dense DTFT from the exported taps and checks complex response, phase error, magnitude deviation and exact peak/curve correspondence. These figures establish the finite-FIR approximation; they do not impose an unsupported universal error tolerance or claim better perceived transients.
