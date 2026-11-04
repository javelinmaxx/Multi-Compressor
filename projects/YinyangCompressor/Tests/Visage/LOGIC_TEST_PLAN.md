# Yinyang Visage integration: Logic Pro test plan

Status: **integration not implemented**. These are acceptance tests, not results.
The local Visage static library configured and built on Linux; this does not
establish that a GPU surface embeds correctly in an Audio Unit host.

## macOS build and validation

1. Build the Yinyang AU and VST3 on the target Mac with the full Xcode toolchain.
2. Validate the AU with `auval -v aufx Chc1 Chra`; record macOS, Logic, GPU,
   architecture, plugin build revision and the complete validation output.
3. In Logic, insert the AU on a stereo audio track and play a signal while
   opening, closing and reopening the editor repeatedly. The GPU surface,
   meter traces and knob-value glows must render without blank or frozen frames.
4. Drag each knob, type values, use switches, automation and tooltips. Verify
   host automation, session recall and touch interactions; compare actual audio
   with the reference build at identical settings.
5. Resize from 50% to 120%, move between displays with different scale factors,
   hide/show Logic and reopen the project. Check crisp text, hit targets,
   waveform interaction, clipping and animations at every size.
6. Exercise all four compressor models, Yin/Yang, transformer on/off, bypass,
   mono/stereo, stopped transport and a sustained signal. Check the waveform
   overlay and gain-reduction history continue updating.
7. Open multiple instances, close individual windows during playback and quit
   Logic. Check for crashes, leaked surfaces, stale callbacks, runaway CPU/GPU
   use and audio dropouts. Repeat with the GPU unavailable or initialization
   failing and verify a usable software fallback.

## Automated checks before Logic

- Build the plugin and existing processor, DSP, upward-compression and
  oversampling tests. Run the editor checks for each supported scale.
- Exercise creation, resize, hide/show, teardown and recreation of the embedded
  surface on the UI thread. Verify no graphics access after editor destruction.
- Test event forwarding and focus for mouse, keyboard, text entry and tooltips;
  test that parameter updates and meter data cross threads without locks on the
  audio callback.
- Compare captured frames for visible meter trace and knob-value glow in both
  software fallback and GPU modes; include a forced GPU-init-failure case.
- Run GPU-backed window tests on macOS and Linux with real graphics devices;
  a virtual display alone is not evidence of Logic or Metal compatibility.

Do not mark the feature complete until both GPU and fallback paths pass these
checks in Logic with the Audio Unit loaded. No Logic or AU result is available
from the current Linux environment.
