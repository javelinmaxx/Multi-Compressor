# Qin Synth tuning in Logic Pro

Logic Pro's project tuning controls are documented for its included software instruments. Apple does not document an AU tuning-table feed to third-party instruments, so Qin does not read Logic's project tuning automatically. To match Logic's **User** tuning, open Qin's **TUNE** menu beside the voice count, select the same root, and copy the twelve cent offsets. Set **A4** to match the project reference pitch. Qin saves these controls with the plug-in state and applies them to tonal oscillators, note-tracked noise, and pitch slides.

Each OSC tab has a draggable **Octave** readout for whole-octave transposition and a **Fine** knob ranging from −1200 to +1200 cents. The two controls add together, so +1200 cents of Fine cancels an Octave setting of −1.

Logic's Hermode and stretch tuning modes are not transferred to Qin. The TUNE menu provides a fixed twelve-degree table with linear interpolation during slides.

Apple reference: [Logic Pro tuning project settings](https://support.apple.com/guide/logicpro/tuning-project-settings-lgcp452f2693/mac).
