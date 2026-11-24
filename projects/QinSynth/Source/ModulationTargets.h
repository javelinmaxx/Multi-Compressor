#pragma once
#include <string_view>

namespace qin
{
struct ModulationTarget
{
    const char* id;
    const char* label;
};

// Only continuously variable controls are destinations. Switches, stepped
// selectors, polyphony, and octave/unison counts are deliberately excluded.
inline constexpr ModulationTarget continuousModTargets[] {
    {"osc1WaveX","OSC 1 / Wave X"},{"osc1WaveY","OSC 1 / Wave Y"},
    {"osc1Level","OSC 1 / Level"},{"osc1Fine","OSC 1 / Fine"},
    {"osc1Detune","OSC 1 / Detune"},{"osc1DetuneShape","OSC 1 / Detune Shape"},
    {"osc1Spread","OSC 1 / Spread"},{"osc1Pan","OSC 1 / Pan"},
    {"osc1FmAmount","OSC 1 / FM Amount"},
    {"osc2WaveX","OSC 2 / Wave X"},{"osc2WaveY","OSC 2 / Wave Y"},
    {"osc2Level","OSC 2 / Level"},{"osc2Fine","OSC 2 / Fine"},
    {"osc2Detune","OSC 2 / Detune"},{"osc2DetuneShape","OSC 2 / Detune Shape"},
    {"osc2Spread","OSC 2 / Spread"},{"osc2Pan","OSC 2 / Pan"},
    {"osc2FmAmount","OSC 2 / FM Amount"},
    {"osc3WaveX","OSC 3 / Wave X"},{"osc3WaveY","OSC 3 / Wave Y"},
    {"osc3Level","OSC 3 / Level"},{"osc3Fine","OSC 3 / Fine"},
    {"osc3Detune","OSC 3 / Detune"},{"osc3DetuneShape","OSC 3 / Detune Shape"},
    {"osc3Spread","OSC 3 / Spread"},{"osc3Pan","OSC 3 / Pan"},
    {"osc3FmAmount","OSC 3 / FM Amount"},
    {"noiseLevel","Noise / Level"},{"noiseTune","Noise / Tune"},
    {"noiseHpf","Noise / HPF"},{"noisePan","Noise / Pan"},
    {"attack","AMP / Attack"},{"ampDecay","AMP / Decay"},
    {"ampSustain","AMP / Sustain"},{"release","AMP / Release"},
    {"ampCurve","AMP / Curve"},
    {"ampMerge","AMP / Merge"},
    {"fltAttack","FLT ENV / Attack"},{"fltDecay","FLT ENV / Decay"},
    {"fltSustain","FLT ENV / Sustain"},{"fltRelease","FLT ENV / Release"},
    {"fltCurve","FLT ENV / Curve"},{"fltMerge","FLT ENV / Merge"},
    {"modAttack","MOD ENV / Attack"},{"modDecay","MOD ENV / Decay"},
    {"modSustain","MOD ENV / Sustain"},{"modRelease","MOD ENV / Release"},
    {"modCurve","MOD ENV / Curve"},{"modDepth","MOD ENV / Depth"},
    {"modMerge","MOD ENV / Merge"},
    {"filterCutoff","FLT / Frequency"},{"filterResonance","FLT / Resonance"},
    {"filterKeyFollow","FLT / Key Follow"},{"filterEnvAmount","FLT / Env Amount"},
    {"filterMerge","FLT / Merge"},
    {"keyPan","PLAY / Key Pan"},{"slideTime","PLAY / Slide"},
    {"slideShape","PLAY / Shape"},
    {"drive","FX / Drive"},{"driveMix","FX / Drive Mix"},
    {"time","FX / Delay Time"},{"decay","FX / Decay"},
    {"scatter","FX / Scatter"},{"verbMix","FX / Mix"},
    {"tapMotion","FX / Tap Motion"},{"phaseMotion","FX / Phase Motion"},
    {"output","OUTPUT / Level"},
    {"tuneA4","TUNE / A4 Reference"},
    {"tune1","TUNE / Degree 1"},{"tune2","TUNE / Degree 2"},
    {"tune3","TUNE / Degree 3"},{"tune4","TUNE / Degree 4"},
    {"tune5","TUNE / Degree 5"},{"tune6","TUNE / Degree 6"},
    {"tune7","TUNE / Degree 7"},{"tune8","TUNE / Degree 8"},
    {"tune9","TUNE / Degree 9"},{"tune10","TUNE / Degree 10"},
    {"tune11","TUNE / Degree 11"},
    {"driveLowCut","FX / Pre Low Cut"},{"driveHighCut","FX / Pre High Cut"},
    {"drivePostLowCut","FX / Post Low Cut"},{"drivePostHighCut","FX / Post High Cut"},
    {"osc1Duty","OSC 1 / Duty"},{"osc2Duty","OSC 2 / Duty"},{"osc3Duty","OSC 3 / Duty"}
};
inline constexpr int legacyModTargetCount=4;
inline constexpr int continuousModTargetCount=int(sizeof(continuousModTargets)/sizeof(continuousModTargets[0]));

constexpr int modTargetIndex(std::string_view id) noexcept
{
    for(int index=0;index<continuousModTargetCount;++index)
        if(id==continuousModTargets[index].id) return legacyModTargetCount+index;
    return -1;
}
}
