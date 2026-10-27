# XuanPhase Method: Transient-Guided Phase Rotation

This note describes the implemented signal path for the XuanPhase research project. It documents the current algorithm; it does not claim an exact all-pass implementation or an iterative group-delay optimizer.

## 1. Transient detection

XuanPhase analyzes overlapping, windowed FFT frames on a background worker. Its detector looks for **newly emerging spectral content**, rather than relying only on the whole-window crest factor.

For each frame, it compares each spectral-bin magnitude with the maximum of the previous frame's same and neighboring bins. A positive novelty residual is formed when a bin grows sufficiently beyond this local reference and a noise-relative floor. Log-compressed growth lets quieter new partials contribute without allowing low-level FFT sidelobes to dominate.

The streaming detector also tracks:
- A slowly adapting spectral baseline (approximately 60 ms time constant).
- An adaptive threshold derived from recent novelty history (64-frame history).
- Local attack evidence from short-window energy rise.
- A 20 ms re-arm interval and brief event follow-up to avoid repeated triggers.
- Neighboring-bin comparison to reduce false triggers from vibrato or moving partials.

This detector is designed to find a new attack even when it is quieter than an already sustained component. It remains a heuristic: overlapping transients, very low signal-to-noise ratio, and late worker responses can limit accuracy.

## 2. Spectral-residual anchoring

Once an onset is detected, the algorithm combines novelty evidence from the short event-collection interval. It builds a **positive spectral residual**: the phase of each current FFT bin is retained, while its magnitude is replaced by the amount of newly emerging energy assigned to that bin.

The residual is transformed back to the time domain. The sample with the largest absolute real-valued residual is selected as the transient anchor. This differs from selecting the loudest sample in the original analysis frame, which could belong to an older, louder sound.

The anchor is a local reference for constructing phase targets; it is not an external reference track, and it is not a perfect source separator. It depends on the onset detector and the novelty estimate.

## 3. Local phase-slope timing estimates

After anchoring, XuanPhase selects local spectral peaks between 60 Hz and the smaller of 20 kHz or 45% of the sample rate. Selection reserves a strong peak in each of 24 logarithmic frequency regions, then fills remaining slots with the strongest candidates. This avoids allowing low frequencies to consume every available anchor.

For each selected FFT bin (k), the algorithm estimates:
- **Relative phase:** the bin's complex phase is measured relative to the selected transient anchor, including the phase sign implied by the anchor sample.
- **Local phase slope:** the phase difference between neighboring bins (k-1) and (k+1) is unwrapped after removing the anchor's time origin. The slope is converted to a relative-time estimate.

In code, the approximate relative-time estimate is

```text
tau_k = -slope_k / (4 * pi * sampleRate / FFTSize)
```

The estimate is clamped to ±25 ms, consistent with the finite FIR's ±25 ms support. Local phase slopes provide a local group-delay-like timing cue; they are not a complete or globally optimized group-delay curve. Estimates can be ambiguous for weak, overlapping, or poorly resolved partials.

## 4. Smooth target interpolation

Each selected peak defines a target with frequency, local width, relative phase angle, and relative-time estimate. Targets are sorted by frequency.

Between neighboring targets, phase angles are unwrapped using the shortest angular difference so a pair near +180° and −180° is treated as a continuous transition rather than a jump through zero. A smoothstep interpolation,

```text
s(u) = u^2 * (3 - 2u)
```

blends neighboring phase and timing targets. Within a local target region, the phase estimate includes a frequency-offset term derived from the local relative-time estimate. The response tapers toward neutral only outside the outermost target regions, reducing abrupt boundary behavior.

The requested phase combines this automatic correction with the user's low/high manual phase controls and crossover. Auto Depth scales the automatic correction from reversed correction at negative values, through no correction at 0%, to the full estimated correction at +100%. The combined request is capped at ±150° to leave margin from the ±180° phase-wrap boundary.

## 5. Finite-FIR phase-response synthesis

The desired positive-frequency response is sampled as a complex spectrum with **unit magnitude and the requested phase**. The negative-frequency bins are constructed by conjugate symmetry so the inverse FFT produces a real-valued impulse response. DC and Nyquist are constrained to real values because they cannot represent arbitrary quadrature phase.

The impulse response is centered at 25 ms and has finite support from −25 ms to +25 ms relative to that center. Its outer 20% is tapered to reduce truncation artifacts. The resulting kernel is a **finite, tapered FIR approximation to a phase-only response**, not an exact all-pass filter. Magnitude ripple is measurable, particularly at low frequencies and intermediate phase angles, so measured magnitude and phase responses must be reported.

The audio path adds 25 ms of input lookahead before the centered FIR, resulting in a fixed reported latency of 50 ms (2,400 samples at 48 kHz). A direct convolution head plus partitioned FFT tail avoids additional block latency. Filter responses are designed on the worker thread and transferred as immutable kernels; transitions use a 5 ms sigmoid crossfade. Coefficient/kernel design, heap allocation, and waiting for the worker do not occur in the audio callback.

## Scope and limitations

- No external reference track is used; the transient anchor is estimated from the input itself.
- The method estimates phase targets from detected transient context; it does not currently optimize a general group-delay loss.
- The finite FIR is an approximation and can change magnitude slightly.
- The detector and local phase-slope estimates can fail on overlapping or ambiguous events.
- A concentrated waveform peak is not, by itself, evidence of improved perceived punch or sound quality.

## Related implementation and test files

- [Project README](README.md)
- [Phase engine](Source/PhaseEngine.cpp)
- [Phase engine interface](Source/PhaseEngine.h)
- [Onset detection report](Tests/Reports/onset-detection.md)
- [Automatic alignment report](Tests/Reports/AUTO_ALIGNMENT.md)
- [Academic test plan](ACADEMIC_TEST_PLAN.md)
