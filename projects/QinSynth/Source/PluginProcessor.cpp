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
        add(prefix+"Spread",title+" spread",0,100,.1f,35,"%");
        add(prefix+"Pan",title+" pan",-100,100,.1f,0,"%");
    }
    add("attack","Attack",.001f,2,.001f,.01f,"s");
    add("release","Release",.01f,5,.001f,.3f,"s");
    add("ampDecay","Amp decay",.01f,5,.001f,.2f,"s");
    add("ampSustain","Amp sustain",0,100,.1f,100,"%");
    add("ampCurve","Amp curve",-100,100,.1f,0,"%");
    for(const char* prefix:{"flt","mod"})
    {
        const juce::String id(prefix);
        if(id=="flt")
        {
            const auto addTime=[&result,&id](const char* suffix,float initial)
            {
                juce::NormalisableRange<float> range {1.f,5000.f,
                    [](float low,float high,float position) { return low*std::pow(high/low,position); },
                    [](float low,float high,float value) { return std::log(value/low)/std::log(high/low); },
                    [](float,float,float value) { return std::round(value); }};
                result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id+suffix,1},
                    id+" "+suffix,range,initial,juce::AudioParameterFloatAttributes().withLabel("ms")));
            };
            addTime("Attack",10.f); addTime("Decay",200.f);
            juce::NormalisableRange<float> sustainRange {0.f,100.f,
                [](float,float,float position) { return std::expm1(position*std::log(101.f)); },
                [](float,float,float value) { return std::log1p(value)/std::log(101.f); },
                [](float,float,float value) { return std::round(value*10.f)*.1f; }};
            result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fltSustain",1},
                "flt sustain",sustainRange,0.f,juce::AudioParameterFloatAttributes().withLabel("%")));
            addTime("Release",300.f);
        }
        else
        {
            add(id+"Attack",id+" attack",.001f,2,.001f,.01f,"s");
            add(id+"Decay",id+" decay",.01f,5,.001f,.2f,"s");
            add(id+"Sustain",id+" sustain",0,100,.1f,0,"%");
            add(id+"Release",id+" release",.01f,5,.001f,.3f,"s");
        }
        add(id+"Curve",id+" curve",-100,100,.1f,0,"%");
    }
    add("modDepth","Mod depth",0,100,.1f,0,"%");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"modTarget",1},"Mod destination",
        juce::StringArray{"Drive","Spread"},0));
    juce::NormalisableRange<float> cutoffRange {30.f,18000.f,
        [](float low,float high,float value) { return low*std::pow(high/low,value); },
        [](float low,float high,float value) { return std::log(value/low)/std::log(high/low); },
        [](float,float,float value) { return std::round(value); }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterCutoff",1},
        "Filter cutoff",cutoffRange,18000.f,juce::AudioParameterFloatAttributes().withLabel("Hz")));
    juce::NormalisableRange<float> resonanceRange {0.f,100.f,
        [](float,float,float value) { return std::exp(value*std::log(101.f))-1.f; },
        [](float,float,float value) { return std::log1p(value)/std::log(101.f); },
        [](float,float,float value) { return std::round(value*10.f)*.1f; }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterResonance",1},
        "Filter resonance",resonanceRange,0.f,juce::AudioParameterFloatAttributes().withLabel("%")));
    add("filterKeyFollow","Filter key follow",0,100,.1f,0,"%");
    add("filterEnvAmount","Filter envelope amount",-100,100,.1f,0,"%");
    add("filterMerge","Filter merge",0,100,.1f,0,"%");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"filterType",1},"Filter type",
        juce::StringArray{"Bypass","TB-303 ladder","Sallen-Key 18 dB","Digital 12 dB","Digital 24 dB"},0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"noise",1},"Noise oscillator",false));
    add("keyPan","Key pan",0,100,.1f,50,"%");
    add("drive","Drive",0,100,.1f,0,"%");
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
      parameters(*this,nullptr,"QinSynthState",layout())
{
    for(auto& point:driveScopeInput) point.store(0,std::memory_order_relaxed);
    for(auto& point:driveScopeOutput) point.store(0,std::memory_order_relaxed);
}
bool QinSynthProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    return buses.getMainInputChannelSet().isDisabled()
        && buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void QinSynthProcessor::prepareToPlay(double rate,int)
{
    sampleRate=std::isfinite(rate) && rate>=8000 ? rate : 48000;
    voices={}; masterFilter={}; masterCutoff=smoothedFilterMerge=0;
    voiceAge=onsetCount=0; onsetTap=onsetPhase=0; sustain=false;
    for(auto& circuit:driveCircuit) circuit.prepare(sampleRate);
    for(auto& point:driveScopeInput) point.store(0,std::memory_order_relaxed);
    for(auto& point:driveScopeOutput) point.store(0,std::memory_order_relaxed);
    driveScopeWrite=driveScopeCounter=0; driveScopeEpoch.store(0,std::memory_order_relaxed);
    liveFilterCutoff.store(0,std::memory_order_relaxed);
    // The MIDI onset is the only source of tap/phase motion. Lan's spectral
    // resonance analyser and tame filters remain dormant in this instrument.
    reverb.prepare(sampleRate,false);
    setLatencySamples(0);
}
void QinSynthProcessor::getStateInformation(juce::MemoryBlock& data)
{ if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void QinSynthProcessor::setStateInformation(const void* data,int size)
{ if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }
QinSynthProcessor::DriveScope QinSynthProcessor::getDriveScope() const noexcept
{
    DriveScope result;
    result.epoch=driveScopeEpoch.load(std::memory_order_acquire);
    const auto cursor=result.epoch%driveScopePoints;
    for(size_t index=0;index<driveScopePoints;++index)
    {
        const auto source=(cursor+index)%driveScopePoints;
        result.input[index]=driveScopeInput[source].load(std::memory_order_relaxed);
        result.output[index]=driveScopeOutput[source].load(std::memory_order_relaxed);
    }
    return result;
}
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
        target.spread=std::clamp(get(prefix+"Spread")*.01f,0.f,1.f);
        target.pan=std::clamp(get(prefix+"Pan")*.01f,-1.f,1.f);
    }
    settings.env[0]={std::clamp(get("attack"),.001f,2.f),std::clamp(get("ampDecay"),.01f,5.f),
        std::clamp(get("ampSustain")*.01f,0.f,1.f),std::clamp(get("release"),.01f,5.f),
        std::clamp(get("ampCurve")*.01f,-1.f,1.f)};
    for(int index=1;index<3;++index)
    {
        const juce::String prefix(index==1 ? "flt" : "mod");
        const auto scale=index==1 ? .001f : 1.f;
        settings.env[size_t(index)]={std::clamp(get(prefix+"Attack")*scale,.001f,5.f),
            std::clamp(get(prefix+"Decay")*scale,.001f,5.f),std::clamp(get(prefix+"Sustain")*.01f,0.f,1.f),
            std::clamp(get(prefix+"Release")*scale,.001f,5.f),std::clamp(get(prefix+"Curve")*.01f,-1.f,1.f)};
    }
    settings.modDepth=std::clamp(get("modDepth")*.01f,0.f,1.f);
    settings.modTarget=std::clamp(juce::roundToInt(get("modTarget")),0,1);
    settings.cutoff=std::clamp(get("filterCutoff"),30.f,18000.f);
    settings.resonance=std::clamp(get("filterResonance")*.01f,0.f,1.f);
    settings.keyFollow=std::clamp(get("filterKeyFollow")*.01f,0.f,1.f);
    settings.filterEnv=std::clamp(get("filterEnvAmount")*.01f,-1.f,1.f);
    settings.filterMerge=std::clamp(get("filterMerge")*.01f,0.f,1.f);
    settings.filterType=std::clamp(juce::roundToInt(get("filterType")),0,4);
    settings.noise=get("noise")>=.5f;
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
float QinSynthProcessor::advanceEnvelope(Envelope& envelope,const EnvelopeSettings& settings,bool held,
                                        const std::array<float,3>& step) const noexcept
{
    if(!held && envelope.stage!=3)
    { envelope.stage=3; envelope.start=envelope.value; envelope.progress=0; }
    if(envelope.stage==2) { envelope.value=settings.sustain; return envelope.value; }
    const auto target=envelope.stage==0 ? 1.f : envelope.stage==1 ? settings.sustain : 0.f;
    const auto increment=step[size_t(envelope.stage==0 ? 0 : envelope.stage==1 ? 1 : 2)];
    envelope.progress=std::min(1.f,envelope.progress+increment);
    const auto slope=envelope.stage==0 ? 6.9f : 5.f;
    const auto base=(1.f-std::exp(-slope*envelope.progress))/(1.f-std::exp(-slope));
    const auto shaped=std::pow(base,std::exp2(settings.curve*2.f));
    envelope.value=envelope.start+(target-envelope.start)*shaped;
    if(envelope.progress>=1.f && envelope.stage<2)
    { envelope.start=envelope.value; envelope.progress=0; ++envelope.stage; }
    return envelope.value;
}
float QinSynthProcessor::filterSample(FilterState& state,float input,float g,float onePole,float resonance,int type) const noexcept
{
    if(type==0) return input;
    if(type==1)
    {
        auto value=std::tanh(input*1.8f-3.5f*resonance*state.ladder[3]);
        for(auto& pole:state.ladder) { pole+=g*(value-pole); value=std::tanh(pole); }
        return std::clamp(state.ladder[3]*1.3f,-4.f,4.f);
    }
    const auto k=std::clamp(1.4f-1.1f*resonance,.3f,1.4f);
    const auto section=[g](float x,float damping,float& s1,float& s2)
    {
        // Trapezoidal SVF: the first integrator state belongs in both the
        // input and denominator terms. Omitting g*s1 destabilizes high Q.
        const auto a1=1.f/(1.f+g*(g+damping));
        const auto a2=g*a1;
        const auto v3=x-s2;
        const auto band=a1*s1+a2*v3;
        const auto low=s2+a2*s1+g*a2*v3;
        s1=2*band-s1; s2=2*low-s2;
        return low;
    };
    auto value=section(input,k,state.s1,state.s2);
    if(type==2)
    {
        state.pole+=onePole*(value-state.pole);
        value=state.pole;
    }
    if(type==4) value=section(value,1.4f-.55f*resonance,state.t1,state.t2);
    if(!std::isfinite(value) || !std::isfinite(state.s1) || !std::isfinite(state.s2)
       || !std::isfinite(state.t1) || !std::isfinite(state.t2))
    { state={}; return 0; }
    return 2.f*std::tanh(value*.5f);
}
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
        selected->noiseState=0x9e3779b9u^uint32_t(selected->age*747796405u)^(uint32_t(note)*2891336453u);
        if(selected->noiseState==0) selected->noiseState=1;
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
    std::array<std::array<float,3>,3> coefficients {};
    for(size_t env=0;env<3;++env)
    {
        const auto& e=settings.env[env];
        const auto filterEnvelope=env==1;
        const std::array<float,3> times {e.attack*(filterEnvelope ? 1.f : 6.9f),
                                         e.decay*(filterEnvelope ? 1.f : 5.f),
                                         e.release*(filterEnvelope ? 1.f : 5.f)};
        for(size_t stage=0;stage<3;++stage)
            coefficients[env][stage]=float(1.0/(sampleRate*std::max(.001f,times[stage])));
    }
    const auto cutoffSmoothing=float(1-std::exp(-1.0/(sampleRate*.005)));
    std::array<std::array<std::array<double,maxUnison>,oscillatorCount>,maxVoices> steps {};
    for(size_t voiceIndex=0;voiceIndex<voices.size();++voiceIndex)
    {
        const auto& voice=voices[voiceIndex];
        if(!voice.active) continue;
        const auto base=juce::MidiMessage::getMidiNoteInHertz(voice.note);
        for(int osc=0;osc<oscillatorCount;++osc)
        {
            const auto& control=settings.oscillator[size_t(osc)];
            if(control.level<=0) continue;
            const auto center=base*std::ldexp(1.0,control.octave);
            for(int u=0;u<control.unison;++u)
            {
                const auto offset=control.unison==1 ? 0.0 : 2.0*u/(control.unison-1)-1.0;
                steps[voiceIndex][size_t(osc)][size_t(u)]=std::min(.49,center*std::pow(2.0,offset*control.detune/1200.0)/sampleRate);
            }
        }
    }
    for(int i=start;i<start+count;++i)
    {
        double left=0,right=0,masterInputLeft=0,masterInputRight=0;
        double cutoffSum=0,cutoffWeight=0;
        for(size_t voiceIndex=0;voiceIndex<voices.size();++voiceIndex)
        {
            auto& voice=voices[voiceIndex];
            if(!voice.active) continue;
            const auto held=voice.keyDown || voice.sustained;
            const auto amp=advanceEnvelope(voice.env[0],settings.env[0],held,coefficients[0]);
            const auto filterEnv=advanceEnvelope(voice.env[1],settings.env[1],held,coefficients[1]);
            const auto modEnv=advanceEnvelope(voice.env[2],settings.env[2],held,coefficients[2]);
            if(!held && amp<1.e-4f) { voice.active=false; continue; }
            double voiceLeft=0,voiceRight=0;
            const auto spreadMod=settings.modTarget==1 ? settings.modDepth*modEnv : 0.f;
            for(int osc=0;osc<oscillatorCount;++osc)
            {
                const auto& control=settings.oscillator[size_t(osc)];
                if(control.level<=0) continue;
                for(int u=0;u<control.unison;++u)
                {
                    const auto offset=control.unison==1 ? 0.0 : (2.0*u/(control.unison-1)-1.0);
                    const auto value=oscillatorSample(voice.oscillators[size_t(osc)][size_t(u)],control.wave,
                        steps[voiceIndex][size_t(osc)][size_t(u)])
                        *control.level/control.unison;
                    const auto pan=std::clamp(double(voice.pan)+control.pan
                        +offset*.35*std::clamp(control.spread+spreadMod,0.f,1.f),-.98,.98);
                    voiceLeft+=value*std::sqrt((1-pan)*.5);
                    voiceRight+=value*std::sqrt((1+pan)*.5);
                }
            }
            if(settings.noise)
            {
                auto& random=voice.noiseState;
                random^=random<<13; random^=random>>17; random^=random<<5;
                const auto noise=(float(random>>8)*(1.f/8388607.5f)-1.f)*.3f;
                voiceLeft+=noise*std::sqrt((1.f-voice.pan)*.5f);
                voiceRight+=noise*std::sqrt((1.f+voice.pan)*.5f);
            }
            if(settings.modTarget==0 && settings.modDepth>0)
            {
                const auto amount=settings.modDepth*modEnv;
                voiceLeft=voiceLeft+amount*(std::tanh(voiceLeft*(1+3*amount))-voiceLeft);
                voiceRight=voiceRight+amount*(std::tanh(voiceRight*(1+3*amount))-voiceRight);
            }
            if(settings.filterType!=0)
            {
                const auto octaves=settings.keyFollow*(voice.note-60)/12.f+settings.filterEnv*filterEnv*4.f;
                const auto target=std::clamp(settings.cutoff*(std::abs(octaves)<1.e-6f ? 1.f : std::exp2(octaves)),
                                             30.f,float(sampleRate*.44));
                voice.cutoff+=cutoffSmoothing*(target-voice.cutoff);
                const auto hz=std::clamp(voice.cutoff,25.f,float(sampleRate*.44));
                const auto coefficient=settings.filterType==1
                    ? std::clamp(float(2*juce::MathConstants<double>::pi*hz/sampleRate),.001f,.9f)
                    : std::tan(float(juce::MathConstants<double>::pi*hz/sampleRate));
                const auto onePole=settings.filterType==2
                    ? std::clamp(float(1-std::exp(-2*juce::MathConstants<double>::pi*hz/sampleRate)),0.f,1.f) : 0.f;
                const auto gain=.16*voice.velocity*amp;
                masterInputLeft+=gain*voiceLeft; masterInputRight+=gain*voiceRight;
                cutoffSum+=gain*hz; cutoffWeight+=gain;
                voiceLeft=filterSample(voice.filter[0],float(voiceLeft),coefficient,onePole,settings.resonance,settings.filterType);
                voiceRight=filterSample(voice.filter[1],float(voiceRight),coefficient,onePole,settings.resonance,settings.filterType);
            }
            const auto gain=.16*voice.velocity*amp;
            left+=gain*voiceLeft; right+=gain*voiceRight;
        }
        if(settings.filterType!=0)
        {
            const auto target=cutoffWeight>1.e-8 ? float(cutoffSum/cutoffWeight) : masterCutoff;
            masterCutoff+=cutoffSmoothing*(target-masterCutoff);
            const auto hz=std::clamp(masterCutoff,25.f,float(sampleRate*.44));
            const auto coefficient=settings.filterType==1
                ? std::clamp(float(2*juce::MathConstants<double>::pi*hz/sampleRate),.001f,.9f)
                : std::tan(float(juce::MathConstants<double>::pi*hz/sampleRate));
            const auto onePole=settings.filterType==2
                ? std::clamp(float(1-std::exp(-2*juce::MathConstants<double>::pi*hz/sampleRate)),0.f,1.f) : 0.f;
            const auto masterLeft=filterSample(masterFilter[0],float(masterInputLeft),coefficient,onePole,
                                               settings.resonance,settings.filterType);
            const auto masterRight=filterSample(masterFilter[1],float(masterInputRight),coefficient,onePole,
                                                settings.resonance,settings.filterType);
            smoothedFilterMerge+=cutoffSmoothing*(settings.filterMerge-smoothedFilterMerge);
            left=left+(masterLeft-left)*smoothedFilterMerge;
            right=right+(masterRight-right)*smoothedFilterMerge;
        }
        buffer.setSample(0,i,float(std::clamp(left,-4.0,4.0)));
        buffer.setSample(1,i,float(std::clamp(right,-4.0,4.0)));
    }
    double cutoffLogSum=0, cutoffWeight=0;
    if(settings.filterType!=0)
        for(const auto& voice:voices)
            if(voice.active && voice.cutoff>0)
            {
                const auto weight=double(voice.velocity)*voice.env[0].value;
                cutoffLogSum+=weight*std::log(voice.cutoff); cutoffWeight+=weight;
            }
    const auto voiceCutoff=cutoffWeight>1.e-5 ? float(std::exp(cutoffLogSum/cutoffWeight)) : 0.f;
    liveFilterCutoff.store(voiceCutoff>0 ? voiceCutoff+(masterCutoff-voiceCutoff)*smoothedFilterMerge : 0.f,
                           std::memory_order_relaxed);
    const auto stride=std::max(1u,uint32_t(sampleRate/(driveScopePoints*50.0)));
    for(int i=start;i<start+count;++i)
    {
        const auto capture=driveScopeCounter+1>=stride;
        const auto original=capture ? .5f*(buffer.getSample(0,i)+buffer.getSample(1,i)) : 0.f;
        if(settings.drive>1.e-4f)
            for(int ch=0;ch<2;++ch)
            {
                const auto dry=buffer.getSample(ch,i);
                const auto driven=driveCircuit[size_t(ch)].process(dry*(1+5*settings.drive));
                const auto result=dry+settings.drive*(driven-dry);
                buffer.setSample(ch,i,!bypass && std::isfinite(result) ? float(std::clamp(result,-4.0,4.0)) : dry);
            }
        if(capture)
        {
            driveScopeCounter=0;
            const auto driven=.5f*(buffer.getSample(0,i)+buffer.getSample(1,i));
            driveScopeInput[driveScopeWrite].store(original,std::memory_order_relaxed);
            driveScopeOutput[driveScopeWrite].store(driven,std::memory_order_relaxed);
            driveScopeWrite=(driveScopeWrite+1)%driveScopePoints;
            driveScopeEpoch.fetch_add(1,std::memory_order_release);
        }
        else ++driveScopeCounter;
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
    for(int channel=0;channel<2;++channel)
        for(int sample=start;sample<start+count;++sample)
        {
            const auto value=buffer.getSample(channel,sample);
            if(!std::isfinite(value)) { buffer.setSample(channel,sample,0.f); continue; }
            const auto magnitude=std::abs(value);
            if(magnitude>2.f)
                buffer.setSample(channel,sample,std::copysign(2.f+2.f*std::tanh((magnitude-2.f)*.5f),value));
        }
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
