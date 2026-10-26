# Lan Reverb

A stereo reverb/delay for long, wide tails and high wet mixes, with automatic
feedback resonance control. Version **0.5.0**, by **J-Sonic Team**, **2026**.

![Native editor](Previews/lan-reverb.png)

## Controls

| Control | Range / default | Function |
|---|---|---|
| Time | 20–500 ms / 90 ms | Repeat time at zero Scatter; reference room size at full Scatter |
| Decay | 0.3–40 s / 10 s | Nominal RT60, with further loss from damping, interpolation and automatic EQ |
| Pre-delay | 0–200 ms / 20 ms | Delay before the wet network; dry remains immediate |
| Scatter | 0–100% / 100% | Morphs regular stereo delay into a dense stereo reverb |
| Diffusion | 0–100% / 85% | Fine mixing density as Scatter increases |
| Width | 0–200% / 125% | Normalized wet mid/side balance; zero is mono |
| Tap Shift | −100–100% / 0% | Offsets feedback read positions by different amounts per line |
| Phase | 0–100% / 60% | All-pass phase dispersion and automatic phase/tap steering |
| Tame | 0–100% / 75% | Strength of automatic, attenuation-only feedback EQ |
| Dry / Wet | 0–100% / 80% | Linear mix, including true full-wet operation for sends |
| Output | −24–12 dB / 0 dB | Final output gain |
| Auto | On | Enables input-aware spectral peak analysis and automatic correction |
| Bypass | Off | Returns immediate dry audio while keeping the tail and analysis hot |

The ebony-and-gold editor matches the collection, scales from 50% to 120%, and
provides one-sentence control hints. Hover over its title for developer, year,
and version. The plot compares input and feedback power spectra; gold markers
indicate tracked resonance bands. The reduction readout shows the deepest
requested EQ cut, not a broadband gain-reduction meter.

## Delay to reverb with Scatter

**Scatter 0%** aligns all delay lines to Time, disables inter-line mixing and
phase spreading, and reads them through input-aligned stereo pickups. This gives
regular, independent left/right delay repeats rather than eight unrelated echoes.
Pre-delay still offsets the first repeat; Tap Shift moves their common feedback
delay, and automatic EQ can still tame ringing. Phase and Diffusion introduce no
spreading at this endpoint. Mono input stays mono before intentional Width changes.

As Scatter rises, delay lengths separate, phase spreading enters the loop, and
the output pickups rotate toward the stereo reverb field. **Scatter 100%** uses
the full spread and phase network. Diffusion fine-tunes feedback mixing from 25%
to 100% of the full rotation at that endpoint, so its minimum does not disable
the reverb. Scatter defaults to 100% and is smoothed over 50 ms; tap changes keep
their queued 50 ms crossfades. Intermediate settings may have additional decay
loss from the phase-path blend, so Decay remains a nominal control.

## Signal path

`input → wet pre-delay → input injection → delay-line writes`

`delay-line wet taps → stereo pickups → Width → dry/wet mix → Output`

`separate feedback taps → EQ → phase dispersion → damping → orthogonal mixing → feedback gains → delay-line writes`

The wet return is read **before** EQ, phase processing and loop damping. Those
stages process only the recirculated signal, which is added to the independent
input injection at the next delay-line write. They can change later repeats,
but never act as inserts on the wet output or dry signal. Manual Tap Shift and
automatic phase-guided tap steering move the feedback read heads only; the wet
read heads retain the Time/Scatter spacing. Automatic feedback gain adjustments
also do not change the source injection gain.

Two different projections of the network form the wet stereo output. Width is
applied **outside** the feedback loop, then mixed with the immediate dry path.
Stereo input is retained, and matched mono input/output is also supported.

Three stages of Givens rotations mix the feedback lines orthogonally at every
Diffusion setting. This avoids the loss of tail energy caused by interpolating
between identity and a fixed matrix. Per-line gains use
`min(0.9995, 10^(−3 × nominal_loop_seconds / Decay))`, including the Scatter-scaled feedback phase-delay
buffer length. This describes a nominal RT60, not an exact broadband decay:
18 Hz DC/bass damping, a high-frequency low-pass, fractional delay interpolation,
EQ, and time variation change the measured result.

Each phase stage uses a normalized scattering all-pass with a short delay. Its
junction obeys `y=a*x+sqrt(1−a²)*state`,
`new_state=sqrt(1−a²)*x−a*state`, preserving the joint input/state energy even
while `a` changes. Auto evaluates bounded coefficient candidates against the
tracked peak's phase, using opposing phase offsets across lines. It also shifts
the feedback taps by a small, peak-dependent amount. These operations scatter
existing loop modes; EQ provides explicit attenuation rather than assuming phase
rotation alone can remove their energy.

**Tap changes take 50 ms** through a convex quintic crossfade between old and new
read positions, with later requests queued until the current fade finishes.
Pre-delay uses the same transition. This avoids abrupt read-head jumps and
continuous Doppler sweeps, although changing a populated delay network can still
produce audible coloration or modulation.

## Automatic resonance control

A background worker transforms a 4,096-sample Hann-windowed frame at up to
10 updates/s. It sums channel powers rather than cancelling anti-phase stereo.
Input peaks are protected before feedback candidates are scored. A robust
neighboring-bin median preserves quieter notes beside loud partials; peaks
within 60 dB of the strongest input bin and above the absolute noise floor are
eligible. Peaks at least 12 dB above that local median are remembered for the
nominal Decay plus 1.25 s of delay allowance after they leave the input. Weaker
6–12 dB peaks receive a short 250 ms guard. Protection covers neighboring bins
and vetoes any EQ band that overlaps the protected region, including held cuts.
An arriving input note therefore revokes an old overlapping EQ/automatic-phase
target rather than waiting for its normal release.

Only unprotected feedback peaks can become automatic EQ or phase/tap targets;
they must exceed the surrounding feedback by at least 8 dB beyond instantaneous
input prominence and persist for approximately 300 ms. Up to three separated bands receive cuts capped at 9 dB before
Tame scaling. Their minimum width is limited by FFT resolution, and their
nominal Q is at most 14. The effective low-frequency resolution changes with
sample rate; this is not a continuous-frequency resonance detector.

Unprotected notches release over roughly 2.5 s, avoiding immediate removal of a successful
cut. Two fixed filter banks warm for 20 ms and crossfade over 80 ms, preventing
coefficient jumps. The callback neither runs FFTs nor waits for the worker; it
uses a bounded single job slot and lock-free atomic result publication. Missed
jobs are dropped rather than queued without bound. Auto Off stops analysis submissions; so does Tame at zero when Phase is zero
or Scatter is zero. Phase still supplies its static
manual dispersion when Auto is off and Scatter is above zero.

Protection favors preserving observed musical content, so a resonance too
close to a protected source note is deliberately left alone. Source history is
retained across Auto toggles and aged by stream time; preparing a new stream
clears it. Source peaks must have been observed while analysis was enabled.

The output stage is linear at normal levels. A smooth, bounded feedback-write
guard activates only above magnitude 2, with an asymptotic limit of 3 per delay
line. Invalid input samples are replaced with zero. These safeguards and
sub-unity feedback gains protect against runaway numerical behavior; this plugin
cannot guarantee the absence of resonances on arbitrary material, distinguish
all intentional tones from unwanted ringing, or control external routing feedback.

## Build and verify

```sh
cmake -S projects/LanReverb -B build/lan -DCMAKE_BUILD_TYPE=Release -DLAN_REVERB_BUILD_TESTS=ON
cmake --build build/lan --target LanReverb_VST3 LanReverb_Standalone LanReverbTests --parallel 3
./build/lan/LanReverbTests --report /tmp/lan-reverb-report
python3 projects/LanReverb/Tests/generate_report.py /tmp/lan-reverb-report --publish
```

On Linux, run GUI-linked tests under `xvfb-run -a` if there is no display. For a
native preview, use `LanReverbTests --editor /tmp/lan-reverb.png`; the repository's
`projects/ZaoDrive/Tests/with_x11_atoms.py` helper seeds window-manager atoms for
minimal Xvfb environments. Plot generation needs Python `numpy` and `matplotlib`.

macOS also provides `LanReverb_AU`; the root `build-macos-all.sh` discovers this
project automatically. Windows workflows are unchanged. The latency reported to
the host is **zero samples**; wet pre-delay and feedback delay remain intentional
effect timing. A conservative 65 s tail is reported to the host.

See the [test report](Tests/Reports/README.md) for reproducible measurements and
scope, and [TOOLTIPS.md](TOOLTIPS.md) for the complete hover text.
