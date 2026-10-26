#include "PluginProcessor.h"
#include "PluginEditor.h"
juce::AudioProcessorValueTreeState::ParameterLayout LanReverbProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto add = [&] (const char* id, const char* name, float low, float high, float step, float initial, const char* unit, float skew = 1)
    {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>{low,high,step,skew},initial,juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    add("time","Time",20,500,.1f,90,"ms",.55f);
    add("decay","Decay",.3f,40,.1f,10,"s",.5f);
    add("predelay","Pre-delay",0,200,.1f,20,"ms");
    add("scatter","Scatter",0,100,.1f,100,"%");
    add("diffusion","Diffusion",0,100,.1f,85,"%");
    add("width","Width",0,200,.1f,125,"%");
    add("tap","Tap shift",-100,100,.1f,0,"%");
    add("phase","Phase scatter",0,100,.1f,60,"%");
    add("tame","Resonance tame",0,100,.1f,75,"%");
    add("mix","Dry wet",0,100,.1f,80,"%");
    add("output","Output",-24,12,.1f,0,"dB");
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"automatic",1},"Auto resonance",true));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync",1},"Time tempo sync",false));
    juce::StringArray notes;
    for (const auto& note : lan::noteLengths) notes.add(note.label);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"note",1},"Time note length",notes,lan::defaultNote));
    add("lowShelfHz","Low shelf frequency",40,2000,1,200,"Hz",.4f);
    add("lowShelfDb","Low shelf gain",-12,0,.1f,0,"dB");
    add("highShelfHz","High shelf frequency",2000,16000,1,8000,"Hz",.6f);
    add("highShelfDb","High shelf gain",-12,0,.1f,0,"dB");
    add("shimmer","Shimmer",0,100,.1f,0,"%");
    return result;
}
LanReverbProcessor::LanReverbProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"LanReverbState",createParameterLayout()) {}
lan::Settings LanReverbProcessor::settings() const
{
    lan::Settings s;
    const auto get = [this] (const char* id) { return parameters.getRawParameterValue(id)->load(); };
    s.timeMs = get("sync") >= .5f ? lan::noteTimeMs(juce::roundToInt(get("note")),hostTempo.load()) : get("time"); s.decaySeconds = get("decay"); s.predelayMs = get("predelay");
    s.scatter = get("scatter")*.01f; s.diffusion = get("diffusion")*.01f; s.width = get("width")*.01f; s.tap = get("tap")*.01f;
    s.phase = get("phase")*.01f; s.tame = get("tame")*.01f; s.mix = get("mix")*.01f; s.outputDb = get("output");
    s.lowShelfHz = get("lowShelfHz"); s.lowShelfDb = std::clamp(get("lowShelfDb"),-12.f,0.f);
    s.highShelfHz = get("highShelfHz"); s.highShelfDb = std::clamp(get("highShelfDb"),-12.f,0.f); s.shimmer = get("shimmer")*.01f;
    s.automatic = get("automatic") >= .5f; s.bypass = get("bypass") >= .5f; return s;
}
juce::AudioProcessorParameter* LanReverbProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
juce::AudioProcessorEditor* LanReverbProcessor::createEditor() { return new LanReverbEditor(*this); }
void LanReverbProcessor::getStateInformation (juce::MemoryBlock& data)
{ if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void LanReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data,size))
        if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
void LanReverbProcessor::prepareToPlay (double rate, int) { engine.prepare(rate); setLatencySamples(0); }
bool LanReverbProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet() == layout.getMainOutputChannelSet()
        && (layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
void LanReverbProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process(b,false); }
void LanReverbProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { process(b,true); }
void LanReverbProcessor::process (juce::AudioBuffer<float>& b, bool hostBypass)
{
    // Hosts provide transport information on the audio thread, including when
    // stopped. Missing or invalid tempo retains the last valid value (120 at
    // startup), so intermittent host data cannot make the delay jump.
    if (auto* transport = getPlayHead())
        if (const auto position = transport->getPosition())
            if (const auto bpm = position->getBpm())
                if (std::isfinite(*bpm) && *bpm >= lan::minimumTempo && *bpm <= lan::maximumTempo)
                    hostTempo.store(*bpm);
    engine.process(b,settings(),hostBypass);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LanReverbProcessor(); }
