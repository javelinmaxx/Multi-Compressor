#include "../Source/PluginProcessor.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
void set(QinSynthProcessor& processor,const char* id,float value)
{
    auto* parameter=processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

double run(int scenario)
{
    QinSynthProcessor instrument;
    set(instrument,"verbMix",0); set(instrument,"driveMix",0);
    set(instrument,"transformer",0); set(instrument,"filterType",0);
    set(instrument,"voiceCount",scenario==0 ? 16 : 1);
    set(instrument,"slideTime",200); set(instrument,"keyPan",100);
    if(scenario==2)
    {
        set(instrument,"osc1Level",0); set(instrument,"osc2Level",0); set(instrument,"osc3Level",0);
        set(instrument,"noise",1); set(instrument,"noiseLevel",100);
        set(instrument,"noiseTune",12);
    }
    else
    {
        for(const auto* id:{"osc1Unison","osc2Unison","osc3Unison"}) set(instrument,id,8);
        for(const auto* id:{"osc1Level","osc2Level","osc3Level"}) set(instrument,id,40);
        if(scenario==1) { set(instrument,"modTarget",1); set(instrument,"modDepth",50); }
    }
    instrument.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);
    juce::MidiBuffer midi;
    double checksum=0;
    const auto start=std::chrono::steady_clock::now();
    for(int block=0;block<375;++block)
    {
        if(block==0)
        {
            const auto count=scenario==0 ? 16 : 1;
            for(int voice=0;voice<count;++voice)
                midi.addEvent(juce::MidiMessage::noteOn(1,48+voice,.8f),0);
        }
        else if(scenario!=0 && block%16==0)
            midi.addEvent(juce::MidiMessage::noteOn(1,block%32==0 ? 84 : 48,.8f),0);
        instrument.processBlock(audio,midi);
        midi.clear();
        checksum+=std::abs(audio.getSample(0,block%128));
    }
    const auto elapsed=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count();
    if(!std::isfinite(checksum) || checksum<1.e-6) std::abort();
    return elapsed;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr std::array<const char*,3> names {"static_pan","moving_pan","noise_glide"};
    for(int scenario=0;scenario<3;++scenario)
    {
        run(scenario);
        std::array<double,5> times {};
        for(auto& result:times) result=run(scenario);
        std::sort(times.begin(),times.end());
        std::printf("%s,%.3f\n",names[size_t(scenario)],times[2]);
    }
}
