# Nian Saturator processor checks

Linux x86_64, Release build. The VST3 and Standalone targets and the native
processor test are built from the project CMake file.

- A stereo sine with zero F amounts verifies complementary crossover
  reconstruction to within 1e-5 per sample.
- Simultaneous 700 Hz and 4 kHz tones, split at 1 kHz, verify that F mode
  enhances each detected component by at least 20% relative to zero amounts.
- T mode at maximum amounts must change the test signal.
- Bypass must settle to the original samples after its fade.
- Repeated mode and crossover changes must keep stereo samples finite and
  bounded; the editor mode control must update the host parameter.
- The native editor is rendered and its controls are checked for visibility
  and click targets at 50% and 120% scaling.

These synthetic tests do not measure perceived sound quality or establish a
complete aliasing bound. T mode runs at host rate and can alias at high drive.
Audio Unit and Logic Pro behavior require a macOS host test.
