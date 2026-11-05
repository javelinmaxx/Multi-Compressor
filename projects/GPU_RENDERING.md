# Experimental GPU editors

All nine plugin editors attach a JUCE OpenGL context after their existing JUCE
controls are laid out. The context paints the component tree, including meter
traces and knob glows, while the audio callback remains independent of the UI.
If no frame is rendered within one second of becoming visible, the context is
detached and JUCE software painting resumes. Set `J_SONIC_SOFTWARE_UI=1`
before launching a host to force software painting for every plugin. Yinyang
also accepts its earlier `YINYANG_SOFTWARE_UI=1` switch.

This uses JUCE's OpenGL renderer, not the Visage UI library. Accelerated
drawing may reduce UI CPU use on some
systems, but driver overhead can increase it, and audio DSP CPU use is
unchanged. A virtual Linux display can verify OpenGL attachment and visible
frames but cannot establish hardware acceleration or Logic Pro AU behavior.

Each project builds its usual processor tests and a `<Plugin>GpuSmoke` target.
Run the smoke target in a graphical session, then repeat with
`J_SONIC_SOFTWARE_UI=1`; it opens the editor, changes scale from 50% to 120%,
and checks the selected render path. The [Yinyang Logic acceptance plan](YinyangCompressor/Tests/Visage/LOGIC_TEST_PLAN.md) lists the remaining AU host
checks, including editor reopen, automation, and multiple instances.

## Linux verification

Release VST3 builds and the existing processor tests passed for Yinyang
Compressor, Shun De-Esser, Kuo Stereo, Zao Drive, Ren Transformer, Kuo Velvet,
Xuan Phase, Lan Reverb, and Nian Saturator. Each plugin's visible-editor smoke
test passed under Xvfb with OpenGL attached and with the global software switch
enabled. The tests resize between 50% and 120% and destroy the editor after
rendering. The OpenGL callback is checked by the Yinyang-specific test, and
the shared renderer returns to software painting if no frame arrives after a
visible window is created. Mac AU and Logic Pro behavior remain untested.
