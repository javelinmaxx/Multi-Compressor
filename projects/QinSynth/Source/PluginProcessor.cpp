#include "PluginProcessor.h"
#include "UnisonDetune.h"
#include "PluginEditor.h"
#include "LegatoCurve.h"
#include "CurveLookup.h"
#include <algorithm>
#include <cmath>
#include <limits>

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
    const auto addEnvelopeTime=[&result](const juce::String& id,const juce::String& name,bool milliseconds,float initial)
    {
        const auto low=milliseconds ? 1.f : .001f;
        const auto high=milliseconds ? 5000.f : 5.f;
        juce::NormalisableRange<float> range {low,high,
            [](float lo,float hi,float position) { return lo*std::pow(hi/lo,position); },
            [](float lo,float hi,float value) { return std::log(value/lo)/std::log(hi/lo); },
            [](float lo,float,float value) { return std::round(value/lo)*lo; }};
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            range,initial,juce::AudioParameterFloatAttributes().withLabel(milliseconds ? "ms" : "s")));
    };
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const juce::String prefix(prefixes[size_t(osc)]), title="Oscillator "+juce::String(osc+1);
        add(prefix+"WaveX",title+" waveform horizontal",0,100,.1f,osc==1 ? 100.f : 0.f,"%");
        add(prefix+"WaveY",title+" waveform vertical",0,100,.1f,osc==2 ? 100.f : 0.f,"%");
        add(prefix+"Level",title+" level",0,100,.1f,osc==0 ? 80.f : 0.f,"%");
        add(prefix+"Octave",title+" octave",-2,2,1,0,"oct");
        add(prefix+"Fine",title+" fine pitch",-1200,1200,.1f,0,"cents");
        add(prefix+"Unison",title+" unison",1,8,1,1,"voices");
        add(prefix+"Detune",title+" detune",0,30,.1f,10,"cents");
        add(prefix+"DetuneShape",title+" detune shape",-100,100,.1f,0,"%");
        add(prefix+"Spread",title+" spread",0,100,.1f,35,"%");
        add(prefix+"Pan",title+" pan",-100,100,.1f,0,"%");
        add(prefix+"FmAmount",title+" FM amount",0,100,.1f,0,"%");
        juce::StringArray fmSources;
        fmSources.add("Off");
        for(int source=0;source<oscillatorCount;++source)
            fmSources.add("OSC "+juce::String(source+1));
        fmSources.add("Noise");
        result.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix+"FmSource",1},title+" FM source",fmSources,0));
    }
    addEnvelopeTime("attack","Attack",false,.01f);
    addEnvelopeTime("release","Release",false,.3f);
    addEnvelopeTime("ampDecay","Amp decay",false,.2f);
    add("ampSustain","Amp sustain",0,100,.1f,100,"%");
    add("ampCurve","Amp curve",-100,100,.1f,0,"%");
    add("ampMerge","Amp envelope merge",0,100,.1f,0,"%");
    for(const char* prefix:{"flt","mod"})
    {
        const juce::String id(prefix);
        if(id=="flt")
        {
            addEnvelopeTime(id+"Attack",id+" attack",true,10.f);
            addEnvelopeTime(id+"Decay",id+" decay",true,200.f);
            juce::NormalisableRange<float> sustainRange {0.f,100.f,
                [](float,float,float position) { return std::expm1(position*std::log(101.f)); },
                [](float,float,float value) { return std::log1p(value)/std::log(101.f); },
                [](float,float,float value) { return std::round(value*10.f)*.1f; }};
            result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fltSustain",1},
                "flt sustain",sustainRange,0.f,juce::AudioParameterFloatAttributes().withLabel("%")));
            addEnvelopeTime(id+"Release",id+" release",true,300.f);
        }
        else
        {
            addEnvelopeTime(id+"Attack",id+" attack",false,.01f);
            addEnvelopeTime(id+"Decay",id+" decay",false,.2f);
            add(id+"Sustain",id+" sustain",0,100,.1f,0,"%");
            addEnvelopeTime(id+"Release",id+" release",false,.3f);
        }
        add(id+"Curve",id+" curve",-100,100,.1f,0,"%");
        add(id+"Merge",id+" envelope merge",0,100,.1f,0,"%");
    }
    add("modDepth","Mod depth",-100,100,.1f,0,"%");
    juce::StringArray modTargets {"Drive","Spread","Pitch","Filter"};
    for(const auto& target:qin::continuousModTargets) modTargets.add(target.label);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"modTarget",1},"Mod destination",
        modTargets,0));
    juce::NormalisableRange<float> cutoffRange {30.f,18000.f,
        [](float low,float high,float value) { return low*std::pow(high/low,value); },
        [](float low,float high,float value) { return std::log(value/low)/std::log(high/low); },
        [](float,float,float value) { return std::round(value); }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterCutoff",1},
        "Filter cutoff",cutoffRange,18000.f,juce::AudioParameterFloatAttributes().withLabel("Hz")));
    juce::NormalisableRange<float> resonanceRange {0.f,100.f,
        [](float,float,float position)
        { return 100.f*std::log1p(100.f*std::clamp(position,0.f,1.f))/std::log(101.f); },
        [](float,float,float value)
        { return std::expm1(std::clamp(value,0.f,100.f)*std::log(101.f)/100.f)/100.f; },
        [](float,float,float value) { return std::round(value*10.f)*.1f; }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterResonance",1},
        "Filter resonance",resonanceRange,0.f,juce::AudioParameterFloatAttributes().withLabel("%")));
    add("filterKeyFollow","Filter key follow",0,100,.1f,0,"%");
    add("filterEnvAmount","Filter envelope amount",-100,100,.1f,0,"%");
    add("filterMerge","Filter merge",0,100,.1f,0,"%");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"filterType",1},"Filter type",
        juce::StringArray{"Bypass","TB-303 ladder","Sallen-Key 18 dB","Digital 12 dB","Digital 24 dB"},0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"noise",1},"Noise oscillator",false));
    add("keyPan","Key pan",-100,100,.1f,50,"%");
    add("drive","Drive",0,100,.1f,0,"%");
    add("driveMix","Drive mix",0,100,.1f,100,"%");
    add("time","Delay time",25,1000,.1f,180,"ms");
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"verbSync",1},"Delay tempo sync",false));
    juce::StringArray verbNotes;
    for(const auto& note:lan::noteLengths) verbNotes.add(note.label);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"verbNote",1},
        "Delay note length",verbNotes,lan::defaultNote));
    add("decay","Reverb decay",.3f,20,.1f,3,"s");
    add("scatter","Scatter",0,100,.1f,70,"%");
    add("verbMix","Delay-verb mix",0,100,.1f,20,"%");
    add("tapMotion","Onset tap",0,100,.1f,50,"%");
    add("phaseMotion","Onset phase",0,100,.1f,50,"%");
    add("output","Output",-24,12,.1f,0,"dB");
    for(const auto* id:{"ampTimesFive","fltTimesFive","modTimesFive"})
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},
            juce::String(id).replace("TimesFive"," envelope times x5"),false));
    add("noiseLevel","Noise level",0,100,.1f,30,"%");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tuneRoot",1},
        "Tuning root",juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},0));
    add("tuneA4","Tuning A4 reference",400,480,.1f,440,"Hz");
    for(int degree=1;degree<12;++degree)
        add("tune"+juce::String(degree),"Tuning degree "+juce::String(degree),-100,100,.1f,0,"cents");
    add("noiseTune","Noise tune",-24,24,.1f,0,"st");
    juce::NormalisableRange<float> noiseHighPassRange {20.f,12000.f,
        [](float low,float high,float position) { return low*std::pow(high/low,position); },
        [](float low,float high,float value) { return std::log(value/low)/std::log(high/low); },
        [](float,float,float value) { return std::round(value); }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"noiseHpf",1},
        "Noise high pass",noiseHighPassRange,20.f,juce::AudioParameterFloatAttributes().withLabel("Hz")));
    add("noisePan","Noise pan",-100,100,.1f,0,"%");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"noiseStyle",1},
        "Noise style",juce::StringArray{"White","Pink","Velvet"},0));
    add("voiceCount","Voice count",1,maxVoices,1,maxVoices,"voices");
    for(int osc=0;osc<oscillatorCount;++osc)
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{juce::String(prefixes[size_t(osc)])+"Retrig",1},
            "Oscillator "+juce::String(osc+1)+" retrig",false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"driveMode",1},"Drive circuit",
        juce::StringArray{"BJT CE","Op-Amp Feedback","Diode Shunt","Series Crossover"},1));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"transformer",1},
        "Output transformer",false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"polyLegato",1},
        "Polyphonic legato",false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"legatoMode",1},
        "Polyphonic legato rule",juce::StringArray{"Closest","Farthest","Upwards","Downwards"},0));
    juce::NormalisableRange<float> slideRange {0.f,2000.f,
        [](float,float,float position) { return 2000.f*std::pow(std::clamp(position,0.f,1.f),2.5f); },
        [](float,float,float value) { return std::pow(std::clamp(value/2000.f,0.f,1.f),.4f); },
        [](float,float,float value) { return std::round(value); }};
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"slideTime",1},
        "Legato slide time",slideRange,80.f,juce::AudioParameterFloatAttributes().withLabel("ms")));
    add("slideShape","Legato slide shape",0,100,.1f,50,"%");
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
    hostTempo.store(120.0,std::memory_order_relaxed);
    glideShapeSmoothing=float(1.0-std::exp(-1.0/(sampleRate*.01)));
    waveSmoothing=float(1.0-std::exp(-1.0/(sampleRate*.005)));
    voices={}; sharedEnvelopes={}; smoothedEnvelopeMerge={}; heldNotes={}; heldNoteCount=0; fmWasActive=false;
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const juce::String prefix(prefixes[size_t(osc)]);
        fmAmounts[size_t(osc)].reset(sampleRate,.005);
        fmAmounts[size_t(osc)].setCurrentAndTargetValue(std::clamp(
            parameters.getRawParameterValue(prefix+"FmAmount")->load()*.01f,0.f,1.f));
        smoothedWaves[size_t(osc)]=qin::clampWavePosition(
            parameters.getRawParameterValue(prefix+"WaveX")->load()*.01f,
            parameters.getRawParameterValue(prefix+"WaveY")->load()*.01f);
        const auto octave=juce::roundToInt(parameters.getRawParameterValue(prefix+"Octave")->load());
        const auto fine=parameters.getRawParameterValue(prefix+"Fine")->load();
        const auto unison=juce::roundToInt(parameters.getRawParameterValue(prefix+"Unison")->load());
        const auto cents=parameters.getRawParameterValue(prefix+"Detune")->load();
        const auto shape=parameters.getRawParameterValue(prefix+"DetuneShape")->load()*.01f;
        for(int u=0;u<maxUnison;++u)
            detuneRatios[size_t(osc)][size_t(u)]=u<unison
                ? std::ldexp(std::exp2((qin::detunePosition(u,unison,shape)*cents+fine)/1200.0),octave) : 1.0;
    }
    for(int i=0;i<=coefficientTableSize;++i)
        coefficientTable[size_t(i)]=std::tan(juce::MathConstants<double>::pi*.44*double(i)/coefficientTableSize);
    masterFilter={}; masterCutoff=smoothedFilterMerge=0;
    smoothedModDepth=std::clamp(parameters.getRawParameterValue("modDepth")->load()*.01f,-1.f,1.f);
    voiceAge=onsetCount=0; onsetTap=onsetPhase=0; sustain=false;
    activeVoiceCount.store(0,std::memory_order_relaxed);
    driveCircuit.prepare(sampleRate);
    lastDriveMode=std::clamp(juce::roundToInt(parameters.getRawParameterValue("driveMode")->load()),0,3);
    for(size_t mode=0;mode<driveWeights.size();++mode)
    {
        driveWeights[mode].reset(sampleRate,.015);
        driveWeights[mode].setCurrentAndTargetValue(int(mode)==lastDriveMode ? 1.f : 0.f);
    }
    for(auto& point:driveScopeInput) point.store(0,std::memory_order_relaxed);
    for(auto& point:driveScopeOutput) point.store(0,std::memory_order_relaxed);
    driveScopeWrite=driveScopeCounter=0; driveScopeEpoch.store(0,std::memory_order_relaxed);
    liveFilterCutoff.store(0,std::memory_order_relaxed);
    liveFilterResonance.store(parameters.getRawParameterValue("filterResonance")->load()*.01f,
                              std::memory_order_relaxed);
    liveModTarget.store(-1,std::memory_order_relaxed);
    // The MIDI onset is the only source of tap/phase motion. Lan's spectral
    // resonance analyser and tame filters remain dormant in this instrument.
    reverb.prepare(sampleRate,false);
    transformerStage.prepare(sampleRate);
    setLatencySamples(ren::TransformerStage::latency);
}
void QinSynthProcessor::getStateInformation(juce::MemoryBlock& data)
{ if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void QinSynthProcessor::setStateInformation(const void* data,int size)
{ if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }
QinSynthProcessor::DriveScope QinSynthProcessor::getDriveScope() const noexcept
{
    DriveScope result;
    result.epoch=driveScopeEpoch.load(std::memory_order_acquire);
    const auto cursor=result.epoch%driveScopeCapturePoints;
    for(size_t index=0;index<driveScopeCapturePoints;++index)
    {
        const auto source=(cursor+index)%driveScopeCapturePoints;
        result.input[index]=driveScopeInput[source].load(std::memory_order_relaxed);
        result.output[index]=driveScopeOutput[source].load(std::memory_order_relaxed);
    }
    return result;
}
juce::AudioProcessorEditor* QinSynthProcessor::createEditor() { return new QinSynthEditor(*this); }
QinSynthProcessor::Settings QinSynthProcessor::readSettings(const char* overrideId,float overrideValue) const noexcept
{
    Settings settings;
    const auto get=[this,overrideId,overrideValue](const juce::String& id)
    {
        return overrideId!=nullptr && id==overrideId ? overrideValue
            : parameters.getRawParameterValue(id)->load(std::memory_order_relaxed);
    };
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const juce::String prefix(prefixes[size_t(osc)]);
        auto& target=settings.oscillator[size_t(osc)];
        target.wave=qin::clampWavePosition(get(prefix+"WaveX")*.01f,get(prefix+"WaveY")*.01f);
        target.retrig=get(prefix+"Retrig")>=.5f;
        target.octave=std::clamp(juce::roundToInt(get(prefix+"Octave")),-2,2);
        target.fineCents=std::clamp(get(prefix+"Fine"),-1200.f,1200.f);
        target.unison=std::clamp(juce::roundToInt(get(prefix+"Unison")),1,maxUnison);
        target.level=std::clamp(get(prefix+"Level")*.01f,0.f,1.f);
        target.detune=std::clamp(get(prefix+"Detune"),0.f,30.f);
        target.detuneShape=std::clamp(get(prefix+"DetuneShape")*.01f,-1.f,1.f);
        target.spread=std::clamp(get(prefix+"Spread")*.01f,0.f,1.f);
        target.pan=std::clamp(get(prefix+"Pan")*.01f,-1.f,1.f);
        target.fmAmount=std::clamp(get(prefix+"FmAmount")*.01f,0.f,1.f);
        const auto selection=std::clamp(juce::roundToInt(get(prefix+"FmSource")),0,oscillatorCount+1);
        target.fmSource=selection-1;
    }
    const auto ampTimeMultiplier=get("ampTimesFive")>=.5f ? 5.f : 1.f;
    settings.env[0]={std::clamp(get("attack")*ampTimeMultiplier,.001f,25.f),std::clamp(get("ampDecay")*ampTimeMultiplier,.001f,25.f),
        std::clamp(get("ampSustain")*.01f,0.f,1.f),std::clamp(get("release")*ampTimeMultiplier,.001f,25.f),
        std::clamp(get("ampCurve")*.01f,-1.f,1.f)};
    for(int index=1;index<3;++index)
    {
        const juce::String prefix(index==1 ? "flt" : "mod");
        const auto scale=(index==1 ? .001f : 1.f)
            *(get(index==1 ? "fltTimesFive" : "modTimesFive")>=.5f ? 5.f : 1.f);
        settings.env[size_t(index)]={std::clamp(get(prefix+"Attack")*scale,.001f,25.f),
            std::clamp(get(prefix+"Decay")*scale,.001f,25.f),std::clamp(get(prefix+"Sustain")*.01f,0.f,1.f),
            std::clamp(get(prefix+"Release")*scale,.001f,25.f),std::clamp(get(prefix+"Curve")*.01f,-1.f,1.f)};
    }
    settings.modDepth=std::clamp(get("modDepth")*.01f,-1.f,1.f);
    for(int index=0;index<3;++index)
        settings.envelopeMerge[size_t(index)]=std::clamp(get(index==0 ? "ampMerge" : index==1 ? "fltMerge" : "modMerge")*.01f,0.f,1.f);
    settings.modTarget=std::clamp(juce::roundToInt(get("modTarget")),0,
        qin::legacyModTargetCount+qin::continuousModTargetCount-1);
    settings.cutoff=std::clamp(get("filterCutoff"),30.f,18000.f);
    settings.resonance=std::clamp(get("filterResonance")*.01f,0.f,1.f);
    settings.keyFollow=std::clamp(get("filterKeyFollow")*.01f,0.f,1.f);
    settings.filterEnv=std::clamp(get("filterEnvAmount")*.01f,-1.f,1.f);
    settings.filterMerge=std::clamp(get("filterMerge")*.01f,0.f,1.f);
    settings.filterType=std::clamp(juce::roundToInt(get("filterType")),0,4);
    settings.noise=get("noise")>=.5f;
    settings.noiseOscillator.level=std::clamp(get("noiseLevel")*.01f,0.f,1.f);
    settings.noiseOscillator.tune=std::clamp(get("noiseTune"),-24.f,24.f);
    settings.noiseOscillator.highPass=std::clamp(get("noiseHpf"),20.f,12000.f);
    settings.noiseOscillator.pan=std::clamp(get("noisePan")*.01f,-1.f,1.f);
    settings.noiseOscillator.style=std::clamp(juce::roundToInt(get("noiseStyle")),0,2);
    settings.tuning.root=std::clamp(juce::roundToInt(get("tuneRoot")),0,11);
    settings.tuning.setReferenceA4(std::clamp(get("tuneA4"),400.f,480.f));
    settings.tuning.equalTempered=std::abs(settings.tuning.referenceA4-440.f)<1.e-6f;
    for(int degree=1;degree<12;++degree)
    {
        const auto cents=std::clamp(get("tune"+juce::String(degree)),-100.f,100.f);
        settings.tuning.cents[size_t(degree)]=cents;
        settings.tuning.equalTempered&=std::abs(cents)<1.e-6f;
    }
    settings.voiceCount=std::clamp(juce::roundToInt(get("voiceCount")),1,maxVoices);
    settings.pan=std::clamp(get("keyPan")*.01f,-1.f,1.f);
    settings.drive=std::clamp(get("drive")*.01f,0.f,1.f);
    settings.driveMix=std::clamp(get("driveMix")*.01f,0.f,1.f);
    settings.driveMode=std::clamp(juce::roundToInt(get("driveMode")),0,3);
    settings.transformer=get("transformer")>=.5f;
    settings.polyLegato=get("polyLegato")>=.5f;
    settings.legatoMode=std::clamp(juce::roundToInt(get("legatoMode")),0,3);
    settings.slideMs=std::clamp(get("slideTime"),0.f,2000.f);
    settings.slideShape=std::clamp(get("slideShape")*.01f,0.f,1.f);
    settings.verbSync=get("verbSync")>=.5f;
    settings.verbNote=std::clamp(juce::roundToInt(get("verbNote")),0,int(lan::noteLengths.size())-1);
    settings.verbTime=settings.verbSync
        ? lan::noteTimeMs(settings.verbNote,hostTempo.load(std::memory_order_relaxed))
        : std::clamp(get("time"),25.f,1000.f);
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
    const auto shaped=qin::curveLookup.envelopeAt(envelope.progress,settings.curve,envelope.stage==0);
    envelope.value=envelope.start+(target-envelope.start)*shaped;
    if(envelope.progress>=1.f && envelope.stage<2)
    { envelope.start=envelope.value; envelope.progress=0; ++envelope.stage; }
    return envelope.value;
}
float QinSynthProcessor::filterCoefficient(float hz) const noexcept
{
    const auto index=std::clamp(double(hz)/sampleRate/.44*coefficientTableSize,0.0,double(coefficientTableSize));
    const auto low=int(index);
    const auto high=std::min(low+1,coefficientTableSize);
    const auto fraction=float(index-low);
    return coefficientTable[size_t(low)]+fraction*(coefficientTable[size_t(high)]-coefficientTable[size_t(low)]);
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
    if(type==2)
    {
        // Equal-R/C non-inverting Sallen-Key low-pass: K controls its feedback,
        // while the output is normalized to unity gain at DC. Trapezoidal
        // integration solves both capacitor voltages together.
        const auto k=1.f+1.7f*std::clamp(resonance,0.f,1.f);
        const auto denominator=1.f+(3.f-k)*g+g*g;
        const auto v1=((state.s1+g*input)*(1.f+g)+g*(1.f-k)*state.s2)/denominator;
        const auto v2=(state.s2+g*v1)/(1.f+g);
        state.s1=2.f*v1-state.s1;
        state.s2=2.f*v2-state.s2;
        const auto third=onePole*(v2-state.pole);
        const auto output=state.pole+third;
        state.pole=output+third;
        if(!std::isfinite(output) || !std::isfinite(state.s1) || !std::isfinite(state.s2)
           || std::abs(state.s1)>64.f || std::abs(state.s2)>64.f)
        { state={}; return 0; }
        return 2.f*std::tanh(output*.5f);
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
double QinSynthProcessor::oscillatorSample(Subvoice& state,const std::array<float,4>& weights,double step) noexcept
{
    const auto phase=state.phase;
    const auto edge=polyBlep(phase,step);
    const auto saw=2*phase-1-edge;
    const auto pulse=(phase<.5 ? 1.0 : -1.0)+edge-polyBlep(std::fmod(phase+.5,1.0),step);
    state.triangle=.9998*state.triangle+4*step*pulse;
    const auto sine=weights[3]>1.e-6f ? qin::curveLookup.sineAt(phase) : 0.0;
    const auto value=weights[0]*saw+weights[1]*pulse+weights[2]*state.triangle+weights[3]*sine;
    state.phase+=step;
    if(state.phase>=1) state.phase-=1;
    return std::clamp(value,-1.5,1.5);
}
void QinSynthProcessor::retargetVoice(Voice& voice,int note,int channel,const Settings& settings) noexcept
{
    const auto start=std::isfinite(voice.pitchNote) ? voice.pitchNote : double(voice.note);
    voice.glideStart=start;
    voice.note=note; voice.channel=channel; voice.pan=notePan(note,settings.pan);
    // Keep the current coefficient for continuity, but retarget immediately
    // if a new legato note arrives before the previous interpolation finishes.
    voice.noiseCoefficientSamples=0;
    voice.keyDown=true; voice.sustained=false;
    voice.glideShape=settings.slideShape;
    if(settings.slideMs<=0.f || std::abs(start-double(note))<1.e-9)
    {
        voice.pitchNote=note; voice.glideProgress=1.f; voice.glideIncrement=1.f;
    }
    else
    {
        voice.glideProgress=0.f;
        voice.glideIncrement=std::min(1.f,float(1000.0/(sampleRate*settings.slideMs)));
    }
}
double QinSynthProcessor::advancePitch(Voice& voice,const Settings& settings) const noexcept
{
    if(voice.glideProgress>=1.f) return voice.pitchNote=voice.note;
    voice.glideShape+=glideShapeSmoothing*(settings.slideShape-voice.glideShape);
    const auto t=voice.glideProgress=std::min(1.f,voice.glideProgress+voice.glideIncrement);
    const auto curve=qin::legatoCurve(t,voice.glideShape);
    voice.pitchNote=voice.glideStart+(double(voice.note)-voice.glideStart)*curve;
    if(t>=1.f) voice.pitchNote=voice.note;
    return voice.pitchNote;
}
void QinSynthProcessor::handleMidi(const juce::MidiMessage& message,const Settings& settings)
{
    if(message.isNoteOn())
    {
        const auto note=message.getNoteNumber();
        const auto velocity=message.getFloatVelocity();
        const auto overlapping=heldNoteCount>0;
        if(!std::any_of(voices.begin(),voices.end(),[](const Voice& voice)
           { return voice.active && (voice.keyDown || voice.sustained); }))
            for(auto& envelope:sharedEnvelopes)
            { envelope.start=envelope.value; envelope.progress=0.f; envelope.stage=0; }
        if(heldNoteCount==int(heldNotes.size()))
        {
            std::move(heldNotes.begin()+1,heldNotes.end(),heldNotes.begin());
            --heldNoteCount;
        }
        heldNotes[size_t(heldNoteCount++)]={note,message.getChannel(),-1};
        int selectedIndex=0;
        if(settings.voiceCount==1 && overlapping && voices[0].active)
        {
            // A legato note changes pitch without restarting oscillators,
            // envelopes, or filters, even when oscillator Retrig is enabled.
            retargetVoice(voices[0],note,message.getChannel(),settings);
        }
        else
        {
            Voice* selected=nullptr;
            const auto end=voices.begin()+settings.voiceCount;
            if(settings.polyLegato && settings.voiceCount>1)
            {
                const auto match=[&](bool releasedOnly)
                {
                    for(int pass=0;pass<2;++pass)
                    {
                        int best=-1;
                        double bestScore=std::numeric_limits<double>::infinity();
                        for(int index=0;index<settings.voiceCount;++index)
                        {
                            const auto& candidate=voices[size_t(index)];
                            if(!candidate.active || (releasedOnly && candidate.keyDown)) continue;
                            const auto difference=note-candidate.note;
                            if(pass==0 && settings.legatoMode==2 && difference<=0) continue;
                            if(pass==0 && settings.legatoMode==3 && difference>=0) continue;
                            const auto score=settings.legatoMode==1
                                ? -double(std::abs(difference)) : double(std::abs(difference));
                            if(score<bestScore) { bestScore=score; best=index; }
                        }
                        if(best>=0) return best;
                        if(settings.legatoMode<2) break;
                    }
                    return -1;
                };
                if(const auto index=match(true);index>=0) selected=&voices[size_t(index)];
                if(selected==nullptr)
                    for(auto it=voices.begin();it!=end;++it)
                        if(!it->active) { selected=&*it; break; }
                if(selected==nullptr)
                    if(const auto index=match(false);index>=0) selected=&voices[size_t(index)];
            }
            else
            {
                for(auto it=voices.begin();it!=end;++it)
                    if(!it->active) { selected=&*it; break; }
                if(selected==nullptr)
                    selected=&*std::min_element(voices.begin(),end,[](const Voice& a,const Voice& b)
                    {
                        if(a.keyDown!=b.keyDown) return !a.keyDown;
                        return a.age<b.age;
                    });
            }
            selectedIndex=int(selected-voices.data());
            if(settings.polyLegato && settings.voiceCount>1 && selected->active)
            {
                if(!selected->keyDown && !selected->sustained)
                {
                    for(auto& envelope:selected->env)
                    { envelope.start=envelope.value; envelope.progress=0.f; envelope.stage=0; }
                }
                retargetVoice(*selected,note,message.getChannel(),settings);
                selected->age=++voiceAge;
            }
            else
            {
                const auto previousOscillators=selected->oscillators;
                const auto hadPreviousNote=selected->age!=0;
                *selected={}; selected->active=selected->keyDown=true;
                selected->note=note; selected->channel=message.getChannel(); selected->velocity=velocity;
                selected->pitchNote=selected->glideStart=note;
                selected->glideShape=settings.slideShape;
                selected->age=++voiceAge; selected->pan=notePan(note,settings.pan);
                selected->noiseState=0x9e3779b9u^uint32_t(selected->age*747796405u)^(uint32_t(note)*2891336453u);
                if(selected->noiseState==0) selected->noiseState=1;
                for(int osc=0;osc<oscillatorCount;++osc)
                    for(int u=0;u<maxUnison;++u)
                    {
                        auto& subvoice=selected->oscillators[size_t(osc)][size_t(u)];
                        if(settings.oscillator[size_t(osc)].retrig)
                            subvoice={};
                        else if(hadPreviousNote)
                            subvoice=previousOscillators[size_t(osc)][size_t(u)];
                        else
                            subvoice.phase=std::fmod(.179*double(u)+.137*double(osc)
                                +.073*double(selected->age),1.0);
                    }
            }
        }
        heldNotes[size_t(heldNoteCount-1)].voiceIndex=selectedIndex;
        // Change only on MIDI note-ons. Alternating direction prevents the
        // feedback tap from repeatedly reinforcing the same frequencies.
        onsetTap=std::clamp((onsetCount++%2==0 ? 1.f : -1.f)
            *(.35f+.35f*std::abs(notePan(note,1.f))+.3f*velocity),-1.f,1.f);
        onsetPhase=.2f+.7f*velocity;
    }
    else if(message.isNoteOff())
    {
        int removedVoice=-1;
        for(int index=heldNoteCount-1;index>=0;--index)
            if(heldNotes[size_t(index)].note==message.getNoteNumber()
               && heldNotes[size_t(index)].channel==message.getChannel())
            {
                removedVoice=heldNotes[size_t(index)].voiceIndex;
                std::move(heldNotes.begin()+index+1,heldNotes.begin()+heldNoteCount,heldNotes.begin()+index);
                --heldNoteCount;
                break;
            }
        if(removedVoice<0) return;
        if(settings.voiceCount==1 && voices[0].active && voices[0].keyDown
           && voices[0].note==message.getNoteNumber() && voices[0].channel==message.getChannel()
           && heldNoteCount>0)
        {
            const auto& previous=heldNotes[size_t(heldNoteCount-1)];
            retargetVoice(voices[0],previous.note,previous.channel,settings);
            return;
        }
        if(settings.polyLegato && settings.voiceCount>1 && removedVoice<settings.voiceCount)
        {
            auto& voice=voices[size_t(removedVoice)];
            if(voice.active && voice.keyDown && voice.note==message.getNoteNumber()
               && voice.channel==message.getChannel())
            {
                for(int index=heldNoteCount-1;index>=0;--index)
                    if(heldNotes[size_t(index)].voiceIndex==removedVoice)
                    {
                        const auto& previous=heldNotes[size_t(index)];
                        retargetVoice(voice,previous.note,previous.channel,settings);
                        return;
                    }
                voice.keyDown=false; voice.sustained=sustain;
            }
            return;
        }
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
        sustain=false; heldNoteCount=0;
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
    const auto mergeSmoothing=float(1-std::exp(-1.0/(sampleRate*.01)));
    const auto modDepthIncrement=(settings.modDepth-smoothedModDepth)/count;
    std::array<std::array<double,maxUnison>,oscillatorCount> ratioIncrements {};
    bool pitchMoving=false;
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const auto& control=settings.oscillator[size_t(osc)];
        for(int u=0;u<control.unison;++u)
        {
            const auto offset=qin::detunePosition(u,control.unison,control.detuneShape);
            const auto target=std::ldexp(std::exp2((offset*control.detune+control.fineCents)/1200.0),control.octave);
            auto& current=detuneRatios[size_t(osc)][size_t(u)];
            ratioIncrements[size_t(osc)][size_t(u)]=(target-current)/count;
            pitchMoving|=std::abs(target-current)>1.e-12;
        }
    }
    std::array<std::array<double,oscillatorCount>,maxVoices> baseSteps {};
    std::array<double,maxVoices> tunedBasePitch {};
    constexpr int noiseUpdateSamples=16;
    std::array<bool,oscillatorCount> fmSourceNeeded {};
    bool noiseFmNeeded=false;
    std::array<int,oscillatorCount> fmSources {};
    bool anyFmActive=false;
    for(int osc=0;osc<oscillatorCount;++osc)
    {
        const auto& control=settings.oscillator[size_t(osc)];
        fmSources[size_t(osc)]=-1;
        auto& amount=fmAmounts[size_t(osc)];
        amount.setTargetValue(control.fmAmount);
        if(control.fmAmount<=0.f && amount.getCurrentValue()<=0.f) continue;
        fmSources[size_t(osc)]=control.fmSource;
        anyFmActive|=control.fmSource>=0;
        if(control.fmSource>=0 && control.fmSource<oscillatorCount)
            fmSourceNeeded[size_t(control.fmSource)]=true;
        else if(control.fmSource==oscillatorCount) noiseFmNeeded=true;
    }
    const auto fmRouting=qin::FmRouting::calculate(fmSources);
    if(anyFmActive && !fmWasActive)
        for(auto& voice:voices) voice.previousFmOutput={};
    fmWasActive=anyFmActive;
    const auto noiseOutputActive=settings.noise && settings.noiseOscillator.level>0;
    const auto noiseActive=noiseOutputActive || noiseFmNeeded;
    const auto noiseTuneRatio=noiseActive ? std::exp2(settings.noiseOscillator.tune/12.f) : 1.0;
    const auto noiseHighPass=noiseActive ? float(1-std::exp(-twoPi
        *std::min(double(settings.noiseOscillator.highPass),sampleRate*.44)/sampleRate)) : 0.f;
    const auto noiseAtPitch=[&](double pitch) noexcept
    {
        const auto hz=settings.tuning.frequency(pitch);
        const auto center=std::clamp(hz*noiseTuneRatio,30.0,sampleRate*.4);
        const auto g=double(filterCoefficient(float(center)));
        constexpr double damping=1.5;
        const auto a1=1/(1+g*(g+damping));
        return NoiseFilterCoefficients {float(a1),float(g*a1),float(g*g*a1),noiseHighPass};
    };
    std::array<std::array<std::array<qin::StereoPanGains,maxUnison>,oscillatorCount>,maxVoices> panGains {};
    std::array<qin::StereoPanGains,maxVoices> noisePanGains {};
    std::array<bool,maxVoices> movingPan {};
    for(size_t voiceIndex=0;voiceIndex<voices.size();++voiceIndex)
    {
        auto& voice=voices[voiceIndex];
        if(!voice.active) continue;
        if(voice.glideProgress>=1.f) voice.pan=notePan(voice.note,settings.pan);
        tunedBasePitch[voiceIndex]=settings.tuning.tunedPitch(voice.note);
        const auto base=440.0*std::exp2((tunedBasePitch[voiceIndex]-69.0)/12.0);
        movingPan[voiceIndex]=voice.glideProgress<1.f
            || (settings.modTarget==1 && (std::abs(settings.modDepth)>0.f || std::abs(smoothedModDepth)>0.f));
        for(int osc=0;osc<oscillatorCount;++osc)
        {
            const auto& control=settings.oscillator[size_t(osc)];
            if(control.level<=0 && !fmSourceNeeded[size_t(osc)]) continue;
            baseSteps[voiceIndex][size_t(osc)]=base/sampleRate;
            if(control.level>0 && !movingPan[voiceIndex])
                for(int u=0;u<control.unison;++u)
                {
                    const auto offset=control.unison==1 ? 0.0 : (2.0*u/(control.unison-1)-1.0);
                    const auto pan=std::clamp(double(voice.pan)+control.pan
                        +offset*.35*control.spread,-.98,.98);
                    panGains[voiceIndex][size_t(osc)][size_t(u)]=qin::stereoPanLookup.evaluate(float(pan));
                }
        }
        if(noiseActive)
        {
            if(!movingPan[voiceIndex])
                noisePanGains[voiceIndex]=qin::stereoPanLookup.evaluate(
                    std::clamp(voice.pan+settings.noiseOscillator.pan,-.98f,.98f));
            if(voice.glideProgress>=1.f)
            {
                voice.noiseCoefficients=noiseAtPitch(voice.note);
                voice.noiseCoefficientSamples=0;
                voice.noiseCoefficientsReady=true;
                voice.noiseWasGliding=false;
            }
            else if(!voice.noiseCoefficientsReady)
            {
                voice.noiseCoefficients=noiseAtPitch(voice.glideStart);
                voice.noiseCoefficientsReady=true;
                voice.noiseCoefficientSamples=0;
            }
            voice.noiseCoefficients.highPass=noiseHighPass;
        }
        else voice.noiseCoefficientsReady=false;
    }
    for(int i=start;i<start+count;++i)
    {
        const auto sharedHeld=std::any_of(voices.begin(),voices.end(),[](const Voice& voice)
        { return voice.active && (voice.keyDown || voice.sustained); });
        std::array<float,3> sharedValues {};
        for(size_t env=0;env<sharedEnvelopes.size();++env)
        {
            sharedValues[env]=advanceEnvelope(sharedEnvelopes[env],settings.env[env],sharedHeld,coefficients[env]);
            smoothedEnvelopeMerge[env]+=mergeSmoothing*(settings.envelopeMerge[env]-smoothedEnvelopeMerge[env]);
        }
        smoothedModDepth+=modDepthIncrement;
        std::array<float,oscillatorCount> fmDepth {};
        for(int osc=0;osc<oscillatorCount;++osc)
            fmDepth[size_t(osc)]=fmAmounts[size_t(osc)].getNextValue();
        if(pitchMoving)
            for(int osc=0;osc<oscillatorCount;++osc)
                for(int u=0;u<settings.oscillator[size_t(osc)].unison;++u)
                    detuneRatios[size_t(osc)][size_t(u)]+=ratioIncrements[size_t(osc)][size_t(u)];
        std::array<std::array<float,4>,oscillatorCount> waveMixes {};
        for(int osc=0;osc<oscillatorCount;++osc)
        {
            auto& current=smoothedWaves[size_t(osc)];
            const auto target=settings.oscillator[size_t(osc)].wave;
            current.x+=waveSmoothing*(target.x-current.x);
            current.y+=waveSmoothing*(target.y-current.y);
            waveMixes[size_t(osc)]=qin::waveWeights(current);
        }
        double left=0,right=0,masterInputLeft=0,masterInputRight=0;
        double cutoffSum=0,cutoffWeight=0;
        for(size_t voiceIndex=0;voiceIndex<voices.size();++voiceIndex)
        {
            auto& voice=voices[voiceIndex];
            if(!voice.active) continue;
            const auto held=voice.keyDown || voice.sustained;
            for(size_t env=0;env<voice.env.size();++env)
            {
                const auto individual=advanceEnvelope(voice.env[env],settings.env[env],held,coefficients[env]);
                voice.effectiveEnv[env]=individual+(sharedValues[env]-individual)*smoothedEnvelopeMerge[env];
            }
            // A shared AMP shape must not hold a released note open while another key remains down.
            const auto releaseGate=(held || !sharedHeld) ? 1.f : std::clamp(voice.env[0].value
                /std::max(.001f,voice.env[0].start),0.f,1.f);
            const auto amp=voice.env[0].value*(1.f-smoothedEnvelopeMerge[0])
                +sharedValues[0]*smoothedEnvelopeMerge[0]*releaseGate;
            voice.effectiveEnv[0]=amp;
            const auto filterEnv=voice.effectiveEnv[1];
            const auto modEnv=voice.effectiveEnv[2];
            if(!held && amp<1.e-4f) { voice.active=false; continue; }
            const auto gliding=voice.glideProgress<1.f;
            const auto currentPitch=gliding ? advancePitch(voice,settings) : double(voice.note);
            const auto pitchFactor=voice.glideProgress>=1.f ? 1.0
                : std::exp2((settings.tuning.tunedPitch(currentPitch)-tunedBasePitch[voiceIndex])/12.0);
            if(gliding) voice.pan=std::clamp(float((currentPitch-64.0)/44.0)*settings.pan,-.95f,.95f);
            double voiceLeft=0,voiceRight=0;
            const auto spreadMod=settings.modTarget==1 ? smoothedModDepth*modEnv : 0.f;
            const auto pitchMod=settings.modTarget==2 ? std::exp2(double(smoothedModDepth*modEnv)) : 1.0;
            float noiseRaw=0;
            if(noiseActive)
            {
                auto& random=voice.noiseState;
                random^=random<<13; random^=random>>17; random^=random<<5;
                const auto white=float(random>>8)*(1.f/8388607.5f)-1.f;
                auto& pink=voice.pink;
                pink[0]=.99765f*pink[0]+.099046f*white;
                pink[1]=.963f*pink[1]+.2965164f*white;
                pink[2]=.57f*pink[2]+1.0526913f*white;
                const auto pinkSample=.08f*(pink[0]+pink[1]+pink[2]+.1848f*white);
                const auto velvet=(random&1023u)<64u ? white*4.f : 0.f;
                const auto source=settings.noiseOscillator.style==1 ? pinkSample
                    : settings.noiseOscillator.style==2 ? velvet : white;
                if(gliding)
                {
                    if(voice.noiseCoefficientSamples==0)
                    {
                        const auto ahead=std::min(1.f,voice.glideProgress
                            +noiseUpdateSamples*voice.glideIncrement);
                        const auto pitch=voice.glideStart+(double(voice.note)-voice.glideStart)
                            *qin::legatoCurve(ahead,voice.glideShape);
                        const auto target=noiseAtPitch(pitch);
                        auto& step=voice.noiseCoefficientStep;
                        const auto& current=voice.noiseCoefficients;
                        step.a1=(target.a1-current.a1)/noiseUpdateSamples;
                        step.a2=(target.a2-current.a2)/noiseUpdateSamples;
                        step.a3=(target.a3-current.a3)/noiseUpdateSamples;
                        voice.noiseCoefficientSamples=noiseUpdateSamples;
                    }
                    auto& current=voice.noiseCoefficients;
                    const auto& step=voice.noiseCoefficientStep;
                    current.a1+=step.a1; current.a2+=step.a2; current.a3+=step.a3;
                    --voice.noiseCoefficientSamples;
                    voice.noiseWasGliding=true;
                }
                else if(voice.noiseWasGliding)
                {
                    voice.noiseCoefficients=noiseAtPitch(currentPitch);
                    voice.noiseCoefficientSamples=0;
                    voice.noiseWasGliding=false;
                }
                const auto& noiseFilter=voice.noiseCoefficients;
                const auto v3=source-voice.noiseLowState;
                const auto band=noiseFilter.a1*voice.noiseBandState+noiseFilter.a2*v3;
                const auto low=voice.noiseLowState+noiseFilter.a2*voice.noiseBandState+noiseFilter.a3*v3;
                voice.noiseBandState=2*band-voice.noiseBandState;
                voice.noiseLowState=2*low-voice.noiseLowState;
                voice.noiseHighPassState+=noiseFilter.highPass*(band-voice.noiseHighPassState);
                noiseRaw=band-voice.noiseHighPassState;
            }
            if(!anyFmActive)
            {
                for(int osc=0;osc<oscillatorCount;++osc)
                {
                    const auto& control=settings.oscillator[size_t(osc)];
                    if(control.level<=0) continue;
                    for(int u=0;u<control.unison;++u)
                    {
                        const auto offset=control.unison==1 ? 0.0 : (2.0*u/(control.unison-1)-1.0);
                        const auto value=oscillatorSample(voice.oscillators[size_t(osc)][size_t(u)],waveMixes[size_t(osc)],
                            std::min(.49,baseSteps[voiceIndex][size_t(osc)]
                                *detuneRatios[size_t(osc)][size_t(u)]*pitchFactor*pitchMod))
                            *control.level/control.unison;
                        auto gains=panGains[voiceIndex][size_t(osc)][size_t(u)];
                        if(movingPan[voiceIndex])
                        {
                            const auto pan=std::clamp(double(voice.pan)+control.pan
                                +offset*.35*std::clamp(control.spread+spreadMod,0.f,1.f),-.98,.98);
                            gains=qin::stereoPanLookup.evaluate(float(pan));
                        }
                        voiceLeft+=value*gains.left;
                        voiceRight+=value*gains.right;
                    }
                }
            }
            else
            {
                // Process a source before its targets. An edge within a feedback
                // cycle reads the source's previous sample instead of recursing.
                std::array<float,oscillatorCount> fmOutput {};
                for(int position=0;position<oscillatorCount;++position)
                {
                    const auto osc=fmRouting.order[size_t(position)];
                    const auto& control=settings.oscillator[size_t(osc)];
                    if(control.level<=0 && !fmSourceNeeded[size_t(osc)]) continue;
                    float source=0;
                    const auto sourceIndex=fmSources[size_t(osc)];
                    if(sourceIndex==oscillatorCount) source=noiseRaw;
                    else if(sourceIndex>=0 && sourceIndex<oscillatorCount)
                        source=fmRouting.delayedEdge[size_t(osc)]
                            ? voice.previousFmOutput[size_t(sourceIndex)]
                            : fmOutput[size_t(sourceIndex)];
                    const auto frequencyScale=1.0+fmDepth[size_t(osc)]*std::clamp(source,-1.f,1.f);
                    double sum=0;
                    for(int u=0;u<control.unison;++u)
                    {
                        const auto raw=oscillatorSample(voice.oscillators[size_t(osc)][size_t(u)],waveMixes[size_t(osc)],
                            std::clamp(baseSteps[voiceIndex][size_t(osc)]
                                *detuneRatios[size_t(osc)][size_t(u)]*pitchFactor*pitchMod*frequencyScale,1.e-7,.49));
                        sum+=raw;
                        if(control.level<=0) continue;
                        const auto offset=control.unison==1 ? 0.0 : (2.0*u/(control.unison-1)-1.0);
                        const auto value=raw*control.level/control.unison;
                        auto gains=panGains[voiceIndex][size_t(osc)][size_t(u)];
                        if(movingPan[voiceIndex])
                        {
                            const auto pan=std::clamp(double(voice.pan)+control.pan
                                +offset*.35*std::clamp(control.spread+spreadMod,0.f,1.f),-.98,.98);
                            gains=qin::stereoPanLookup.evaluate(float(pan));
                        }
                        voiceLeft+=value*gains.left;
                        voiceRight+=value*gains.right;
                    }
                    fmOutput[size_t(osc)]=std::clamp(float(sum/control.unison),-1.f,1.f);
                }
                voice.previousFmOutput=fmOutput;
            }
            if(noiseOutputActive)
            {
                const auto gains=movingPan[voiceIndex]
                    ? qin::stereoPanLookup.evaluate(std::clamp(voice.pan+settings.noiseOscillator.pan,-.98f,.98f))
                    : noisePanGains[voiceIndex];
                voiceLeft+=noiseRaw*settings.noiseOscillator.level*gains.left;
                voiceRight+=noiseRaw*settings.noiseOscillator.level*gains.right;
            }
            if(settings.modTarget==0 && std::abs(smoothedModDepth)>1.e-6f)
            {
                const auto amount=smoothedModDepth*modEnv;
                if(amount>0)
                {
                    voiceLeft=voiceLeft+amount*(std::tanh(voiceLeft*(1+3*amount))-voiceLeft);
                    voiceRight=voiceRight+amount*(std::tanh(voiceRight*(1+3*amount))-voiceRight);
                }
                else { voiceLeft*=1+amount; voiceRight*=1+amount; }
            }
            if(settings.filterType!=0)
            {
                const auto octaves=settings.keyFollow*float((currentPitch-60.0)/12.0)
                    +settings.filterEnv*filterEnv*4.f
                    +(settings.modTarget==3 ? smoothedModDepth*modEnv*4.f : 0.f);
                const auto target=std::clamp(settings.cutoff*(std::abs(octaves)<1.e-6f ? 1.f : std::exp2(octaves)),
                                             30.f,float(sampleRate*.44));
                voice.cutoff+=cutoffSmoothing*(target-voice.cutoff);
                const auto hz=std::clamp(voice.cutoff,25.f,float(sampleRate*.44));
                const auto coefficient=settings.filterType==1
                    ? std::clamp(float(2*juce::MathConstants<double>::pi*hz/sampleRate),.001f,.9f)
                    : filterCoefficient(hz);
                const auto onePole=settings.filterType==2
                    ? coefficient/(1.f+coefficient) : 0.f;
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
                : filterCoefficient(hz);
            const auto onePole=settings.filterType==2
                ? coefficient/(1.f+coefficient) : 0.f;
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
                const auto weight=double(voice.velocity)*voice.effectiveEnv[0];
                cutoffLogSum+=weight*std::log(voice.cutoff); cutoffWeight+=weight;
            }
    const auto voiceCutoff=cutoffWeight>1.e-5 ? float(std::exp(cutoffLogSum/cutoffWeight)) : 0.f;
    liveFilterCutoff.store(voiceCutoff>0 ? voiceCutoff+(masterCutoff-voiceCutoff)*smoothedFilterMerge : 0.f,
                           std::memory_order_relaxed);
    liveFilterResonance.store(settings.resonance,std::memory_order_relaxed);
    if(settings.driveMode!=lastDriveMode)
    {
        if(driveWeights[size_t(settings.driveMode)].getCurrentValue()<=0.f)
            driveCircuit.reset(settings.driveMode);
        for(size_t mode=0;mode<driveWeights.size();++mode)
            driveWeights[mode].setTargetValue(int(mode)==settings.driveMode ? 1.f : 0.f);
        lastDriveMode=settings.driveMode;
    }
    // Around 12 kHz of scope samples spans roughly 85 ms, enough to see two
    // complete cycles of a low piano note without processing every sample.
    const auto stride=std::max(1u,uint32_t(sampleRate/12000.0));
    for(int i=start;i<start+count;++i)
    {
        const auto capture=driveScopeCounter+1>=stride;
        const auto original=capture ? .5f*(buffer.getSample(0,i)+buffer.getSample(1,i)) : 0.f;
        std::array<float,zao::circuitCount> weights {};
        for(size_t mode=0;mode<weights.size();++mode) weights[mode]=driveWeights[mode].getNextValue();
        if(settings.drive>1.e-4f && settings.driveMix>1.e-4f)
            for(int ch=0;ch<2;++ch)
            {
                const auto dry=buffer.getSample(ch,i);
                double driven=0;
                for(size_t mode=0;mode<weights.size();++mode)
                    if(weights[mode]>1.e-6f)
                        driven+=weights[mode]*driveCircuit.process(int(mode),ch,dry*(1+5*settings.drive));
                const auto result=dry+settings.driveMix*(driven-dry);
                buffer.setSample(ch,i,!bypass && std::isfinite(result) ? float(std::clamp(result,-4.0,4.0)) : dry);
            }
        if(capture)
        {
            driveScopeCounter=0;
            const auto driven=.5f*(buffer.getSample(0,i)+buffer.getSample(1,i));
            driveScopeInput[driveScopeWrite].store(original,std::memory_order_relaxed);
            driveScopeOutput[driveScopeWrite].store(driven,std::memory_order_relaxed);
            driveScopeWrite=(driveScopeWrite+1)%driveScopeCapturePoints;
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
    // Ren's delayed clean branch remains aligned; its nonlinear core stops
    // once the off transition has faded out.
    transformerStage.process(slice,6.f,0.f,0.f,1.f,settings.transformer,bypass);
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
    if(auto* transport=getPlayHead())
        if(const auto position=transport->getPosition())
            if(const auto bpm=position->getBpm())
                if(std::isfinite(*bpm) && *bpm>=lan::minimumTempo && *bpm<=lan::maximumTempo)
                    hostTempo.store(*bpm,std::memory_order_relaxed);
    const auto settings=readSettings();
    liveModTarget.store(-1,std::memory_order_release);
    const auto render=[this,&buffer,&settings,bypass](int start,int count)
    {
        if(count<=0) return;
        if(settings.modTarget<qin::legacyModTargetCount
           || (std::abs(settings.modDepth)<1.e-6f && std::abs(smoothedModDepth)<1.e-6f)
           || std::none_of(voices.begin(),voices.end(),[](const Voice& voice) { return voice.active; }))
        { renderSegment(buffer,start,count,settings,bypass); return; }
        const auto& route=qin::continuousModTargets[settings.modTarget-qin::legacyModTargetCount];
        auto* parameter=parameters.getParameter(route.id);
        auto* raw=parameters.getRawParameterValue(route.id);
        if(parameter==nullptr || raw==nullptr)
        { renderSegment(buffer,start,count,settings,bypass); return; }
        const auto baseline=parameter->convertTo0to1(raw->load(std::memory_order_relaxed));
        const auto publish=[this,&settings](float normalized)
        {
            liveModNormalized.store(std::clamp(normalized,0.f,1.f),std::memory_order_relaxed);
            liveModTarget.store(settings.modTarget,std::memory_order_release);
        };
        const auto stableSustain=std::abs(smoothedModDepth-settings.modDepth)<1.e-6f
            && settings.modTarget!=qin::modTargetIndex("modSustain")
            && settings.modTarget!=qin::modTargetIndex("modDepth")
            && std::all_of(voices.begin(),voices.end(),[](const Voice& voice)
               { return !voice.active || ((voice.keyDown || voice.sustained) && voice.env[2].stage==2); });
        if(stableSustain)
        {
            float envelope=0;
            for(const auto& voice:voices)
                if(voice.active) envelope=std::max(envelope,voice.effectiveEnv[2]);
            const auto shift=std::clamp(smoothedModDepth*envelope,-1.f,1.f);
            if(std::abs(shift)<1.e-6f)
            { publish(baseline); renderSegment(buffer,start,count,settings,bypass); }
            else
            {
                const auto value=parameter->convertFrom0to1(std::clamp(baseline+shift,0.f,1.f));
                publish(parameter->convertTo0to1(value));
                renderSegment(buffer,start,count,readSettings(route.id,value),bypass);
            }
            return;
        }
        constexpr int modulationSlice=64;
        for(int offset=0;offset<count;offset+=modulationSlice)
        {
            float envelope=0;
            for(const auto& voice:voices)
                if(voice.active) envelope=std::max(envelope,voice.effectiveEnv[2]);
            const auto shift=std::clamp(smoothedModDepth*envelope,-1.f,1.f);
            const auto size=std::min(modulationSlice,count-offset);
            if(std::abs(shift)<1.e-6f)
            {
                publish(baseline);
                renderSegment(buffer,start+offset,size,settings,bypass);
            }
            else
            {
                const auto value=parameter->convertFrom0to1(std::clamp(baseline+shift,0.f,1.f));
                publish(parameter->convertTo0to1(value));
                auto modulated=readSettings(route.id,value);
                renderSegment(buffer,start+offset,size,modulated,bypass);
            }
        }
    };
    int position=0;
    for(const auto metadata:midi)
    {
        const auto next=std::clamp(metadata.samplePosition,position,buffer.getNumSamples());
        render(position,next-position);
        if(settings.modTarget==qin::modTargetIndex("slideTime")
           || settings.modTarget==qin::modTargetIndex("slideShape"))
        {
            const auto& route=qin::continuousModTargets[settings.modTarget-qin::legacyModTargetCount];
            auto* parameter=parameters.getParameter(route.id);
            auto* raw=parameters.getRawParameterValue(route.id);
            float envelope=0;
            for(const auto& voice:voices)
                if(voice.active) envelope=std::max(envelope,voice.effectiveEnv[2]);
            if(parameter!=nullptr && raw!=nullptr && envelope>0)
            {
                const auto baseline=parameter->convertTo0to1(raw->load(std::memory_order_relaxed));
                const auto value=parameter->convertFrom0to1(std::clamp(baseline+smoothedModDepth*envelope,0.f,1.f));
                handleMidi(metadata.getMessage(),readSettings(route.id,value));
            }
            else handleMidi(metadata.getMessage(),settings);
        }
        else handleMidi(metadata.getMessage(),settings);
        position=next;
    }
    render(position,buffer.getNumSamples()-position);
    int active=0;
    for(const auto& voice:voices) if(voice.active) ++active;
    activeVoiceCount.store(active,std::memory_order_relaxed);
}
void QinSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { process(buffer,midi,false); }
void QinSynthProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { process(buffer,midi,true); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QinSynthProcessor(); }
