#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double twoPi=juce::MathConstants<double>::twoPi;
constexpr std::array<const char*,3> prefixes {"osc1","osc2","osc3"};
}
juce::AudioProcessorValueTreeState::ParameterLayout QinSynthProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto add=[&result](const juce::String& id,const juce::String& name,float lo,float hi,float step,float initial,const char* unit)
    {
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>{lo,hi,step},initial,juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const juce::String prefix(prefixes[size_t(osc)]), title="Oscillator "+juce::String(osc+1);
        result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{prefix+"Wave",1},title+" wave",
            juce::StringArray{"Saw","Pulse","Triangle"},osc));
        add(prefix+"Level",title+" level",0,100,.1f,osc==0 ? 80.f : 0.f,"%");
        add(prefix+"Octave",title+" octave",-2,2,1,0,"oct");
        add(prefix+"Unison",title+" unison",1,8,1,1,"voices");
        add(prefix+"Detune",title+" detune",0,30,.1f,10,"cents");
    }
    add("attack","Attack",.001f,2,.001f,.01f,"s");
    add("release","Release",.01f,5,.001f,.3f,"s");
    add("keyPan","Key pan",0,100,.1f,50,"%");
    add("drive","Zao drive",0,100,.1f,0,"%");
    add("time","Delay time",25,1000,.1f,180,"ms");
    add("decay","Reverb decay",.3f,20,.1f,3,"s");
    add("scatter","Scatter",0,100,.1f,70,"%");
    add("verbMix","Delay-verb mix",0,100,.1f,20,"%");
    add("tapMotion","Onset tap",0,100,.1f,50,"%");
    add("phaseMotion","Onset phase",0,100,.1f,50,"%");
    add("output","Output",-24,12,.1f,0,"dB");
    return result;
}
QinSynthProcessor::QinSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"QinSynthState",layout()) {}
bool QinSynthProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    return buses.getMainInputChannelSet().isDisabled()
        && buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void QinSynthProcessor::prepareToPlay(double rate,int)
{
    sampleRate=std::isfinite(rate) && rate>=8000 ? rate : 48000;
    voices={}; voiceAge=onsetCount=0; onsetTap=onsetPhase=0; sustain=false;
    for(auto& circuit:driveCircuit) circuit.prepare(sampleRate);
    // The MIDI onset is the only source of tap/phase motion. Lan's spectral
    // resonance analyser and tame filters remain dormant in this instrument.
    reverb.prepare(sampleRate,false);
    setLatencySamples(0);
}
void QinSynthProcessor::getStateInformation(juce::MemoryBlock& data)
{ if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void QinSynthProcessor::setStateInformation(const void* data,int size)
{ if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessorEditor* QinSynthProcessor::createEditor() { return new QinSynthEditor(*this); }
QinSynthProcessor::Settings QinSynthProcessor::readSettings() const noexcept
{
    Settings settings;
    const auto get=[this](const juce::String& id) { return parameters.getRawParameterValue(id)->load(std::memory_order_relaxed); };
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const juce::String prefix(prefixes[size_t(osc)]);
        auto& target=settings.oscillator[size_t(osc)];
        target.wave=std::clamp(juce::roundToInt(get(prefix+"Wave")),0,2);
        target.octave=std::clamp(juce::roundToInt(get(prefix+"Octave")),-2,2);
        target.unison=std::clamp(juce::roundToInt(get(prefix+"Unison")),1,maxUnison);
        target.level=std::clamp(get(prefix+"Level")*.01f,0.f,1.f);
        target.detune=std::clamp(get(prefix+"Detune"),0.f,30.f);
    }
    settings.attack=std::clamp(get("attack"),.001f,2.f);
    settings.release=std::clamp(get("release"),.01f,5.f);
    settings.pan=std::clamp(get("keyPan")*.01f,0.f,1.f);
    settings.drive=std::clamp(get("drive")*.01f,0.f,1.f);
    settings.verbTime=std::clamp(get("time"),25.f,1000.f);
    settings.verbDecay=std::clamp(get("decay"),.3f,20.f);
    settings.verbScatter=std::clamp(get("scatter")*.01f,0.f,1.f);
    settings.verbMix=std::clamp(get("verbMix")*.01f,0.f,1.f);
    settings.tapMotion=std::clamp(get("tapMotion")*.01f,0.f,1.f);
    settings.phaseMotion=std::clamp(get("phaseMotion")*.01f,0.f,1.f);
    settings.output=juce::Decibels::decibelsToGain(std::clamp(get("output"),-24.f,12.f));
    return settings;
}
float QinSynthProcessor::notePan(int note,float amount) noexcept
{ return std::clamp((float(note)-64.f)/44.f*amount,-.95f,.95f); }
double QinSynthProcessor::polyBlep(double phase,double step) noexcept
{
    if(phase<step) { const auto t=phase/step; return t+t-t*t-1; }
    if(phase>1-step) { const auto t=(phase-1)/step; return t*t+t+t+1; }
    return 0;
}
double QinSynthProcessor::oscillatorSample(Subvoice& state,int wave,double step) noexcept
{
    const auto phase=state.phase;
    double value=0;
    if(wave==0) value=2*phase-1-polyBlep(phase,step);
    else
    {
        const auto pulse=(phase<.5 ? 1.0 : -1.0)+polyBlep(phase,step)
                         -polyBlep(std::fmod(phase+.5,1.0),step);
        if(wave==1) value=pulse;
        else { state.triangle=.9998*state.triangle+4*step*pulse; value=state.triangle; }
    }
    state.phase+=step;
    if(state.phase>=1) state.phase-=1;
    return std::clamp(value,-1.5,1.5);
}
void QinSynthProcessor::handleMidi(const juce::MidiMessage& message,const Settings& settings)
{
    if(message.isNoteOn())
    {
        const auto note=message.getNoteNumber();
        const auto velocity=message.getFloatVelocity();
        Voice* selected=nullptr;
        for(auto& voice:voices) if(!voice.active) { selected=&voice; break; }
        if(selected==nullptr)
            selected=&*std::min_element(voices.begin(),voices.end(),[](const Voice& a,const Voice& b)
            {
                if(a.keyDown!=b.keyDown) return !a.keyDown;
                return a.age<b.age;
            });
        *selected={}; selected->active=selected->keyDown=true;
        selected->note=note; selected->channel=message.getChannel(); selected->velocity=velocity;
        selected->age=++voiceAge; selected->pan=notePan(note,settings.pan);
        for(int osc=0;osc<oscillatorCount;++osc)
            for(int u=0;u<maxUnison;++u)
                selected->oscillators[size_t(osc)][size_t(u)].phase=
                    std::fmod(.179*double(u)+.137*double(osc)+.073*double(selected->age),1.0);
        // Change only on MIDI note-ons. Alternating direction prevents the
        // feedback tap from repeatedly reinforcing the same frequencies.
        onsetTap=std::clamp((onsetCount++%2==0 ? 1.f : -1.f)
            *(.35f+.35f*std::abs(notePan(note,1.f))+.3f*velocity),-1.f,1.f);
        onsetPhase=.2f+.7f*velocity;
    }
    else if(message.isNoteOff())
    {
        for(auto& voice:voices)
            if(voice.active && voice.note==message.getNoteNumber() && voice.channel==message.getChannel() && voice.keyDown)
            { voice.keyDown=false; voice.sustained=sustain; }
    }
    else if(message.isController() && message.getControllerNumber()==64)
    {
        sustain=message.getControllerValue()>=64;
        if(!sustain) for(auto& voice:voices) voice.sustained=false;
    }
    else if(message.isAllNotesOff() || message.isAllSoundOff())
    {
        sustain=false;
        for(auto& voice:voices)
        {
            voice.keyDown=voice.sustained=false;
            if(message.isAllSoundOff()) voice.active=false;
        }
    }
}
void QinSynthProcessor::renderSegment(juce::AudioBuffer<float>& buffer,int start,int count,const Settings& settings,bool bypass)
{
    if(count<=0) return;
    const auto attackStep=1.0-std::exp(-1.0/(sampleRate*settings.attack));
    const auto releaseStep=std::exp(-1.0/(sampleRate*settings.release));
    for(int i=start;i<start+count;++i)
    {
        double left=0,right=0;
        for(auto& voice:voices)
        {
            if(!voice.active) continue;
            if(voice.keyDown || voice.sustained)
                voice.envelope+=float((1.0-voice.envelope)*attackStep);
            else
            {
                voice.envelope*=float(releaseStep);
                if(voice.envelope<1.e-4f) { voice.active=false; continue; }
            }
            const auto base=juce::MidiMessage::getMidiNoteInHertz(voice.note);
            double voiceLeft=0,voiceRight=0;
            for(int osc=0;osc<oscillatorCount;++osc)
            {
                const auto& control=settings.oscillator[size_t(osc)];
                if(control.level<=0) continue;
                const auto center=base*std::ldexp(1.0,control.octave);
                for(int u=0;u<control.unison;++u)
                {
                    const auto offset=control.unison==1 ? 0.0 : (2.0*u/(control.unison-1)-1.0);
                    const auto detune=std::pow(2.0,offset*control.detune/1200.0);
                    const auto step=std::min(.49,center*detune/sampleRate);
                    const auto value=oscillatorSample(voice.oscillators[size_t(osc)][size_t(u)],control.wave,step)
                        *control.level/control.unison;
                    const auto pan=std::clamp(double(voice.pan)+offset*.12,-.98,.98);
                    voiceLeft+=value*std::sqrt((1-pan)*.5);
                    voiceRight+=value*std::sqrt((1+pan)*.5);
                }
            }
            const auto gain=.16*voice.velocity*voice.envelope;
            left+=gain*voiceLeft; right+=gain*voiceRight;
        }
        buffer.setSample(0,i,float(std::clamp(left,-4.0,4.0)));
        buffer.setSample(1,i,float(std::clamp(right,-4.0,4.0)));
    }
    if(!bypass && settings.drive>1.e-4f)
        for(int i=start;i<start+count;++i)
            for(int ch=0;ch<2;++ch)
            {
                const auto original=buffer.getSample(ch,i);
                const auto driven=driveCircuit[size_t(ch)].process(original*(1+5*settings.drive));
                const auto result=original+settings.drive*(driven-original);
                buffer.setSample(ch,i,std::isfinite(result) ? float(std::clamp(result,-4.0,4.0)) : original);
            }
    float const outputGain=settings.output;
    for(int ch=0;ch<2;++ch) buffer.applyGain(ch,start,count,outputGain);
    lan::Settings verb;
    verb.timeMs=settings.verbTime; verb.decaySeconds=settings.verbDecay;
    verb.scatter=settings.verbScatter; verb.diffusion=.85f; verb.width=1.25f;
    verb.mix=bypass ? 0.f : settings.verbMix;
    verb.tap=onsetTap*settings.tapMotion;
    verb.phase=onsetPhase*settings.phaseMotion;
    verb.tame=0; verb.automatic=false; verb.shimmer=0; verb.predelayMs=0;
    juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(),2,start,count);
    reverb.process(slice,verb,bypass);
}
void QinSynthProcessor::process(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi,bool bypass)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const auto settings=readSettings();
    int position=0;
    for(const auto metadata:midi)
    {
        const auto next=std::clamp(metadata.samplePosition,position,buffer.getNumSamples());
        renderSegment(buffer,position,next-position,settings,bypass);
        handleMidi(metadata.getMessage(),settings);
        position=next;
    }
    renderSegment(buffer,position,buffer.getNumSamples()-position,settings,bypass);
}
void QinSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { process(buffer,midi,false); }
void QinSynthProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { process(buffer,midi,true); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QinSynthProcessor(); }
