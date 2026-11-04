# CPU usage tips

These tips describe available controls; they are not benchmark results.
Measure with the same audio, sample rate, buffer size and host before comparing.

- Close plugin editors when you do not need live meters or analysis displays.
- Kuo Stereo: set Haas Variability to Off to stop the adaptive delay search.
- Yinyang Compressor: switch the output transformer off when its coloration is not needed.
- Ren Transformer: remove the instance when transformer coloration is not needed.
- Lan Reverb: switch Shimmer off when the octave layer is not needed, and switch Auto off when automatic resonance tracking is not needed.
- Zao Drive: switch Smart and Clear off when their processing is not needed.
- Raise the host audio buffer size when latency requirements permit; this can reduce callback overhead and dropouts.

A larger buffer does not necessarily reduce the amount of DSP work per sample.
Host bypass may intentionally keep plugin processing active for click-free return;
removing or disabling an instance in the host is a separate operation.
