# Mono-to-stereo routing checks

Kuo Stereo, Kuo Velvet and Lan Reverb are tested at 48 kHz with 127-sample
blocks. For each plugin, a mono-to-stereo instance receives a sine plus an
impulse while its output-only channel is prefilled with nonzero garbage. Its
two outputs must match, within 1e-6 per sample, a separate instance receiving
the same input duplicated to both stereo channels. Each comparison runs for
256 blocks, both normally and through host bypass.

Kuo Stereo's adaptive Haas search and Lan Reverb's automatic analysis are off
for this deterministic comparison. Kuo Stereo now processes host bypass through
its existing latency-matched dry path, keeping the DSP hot.

Linux native builds and processor suites passed: Kuo Stereo (269 reported checks),
Kuo Velvet (44 reported checks), and Lan Reverb (25,330,188 reported checks).
The routing comparison additionally checks every output sample and throws on
any mismatch; those sample comparisons are not included in the printed counts.
Logic Pro / Audio Unit behavior still requires macOS host validation.
