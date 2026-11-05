#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout NianSaturatorProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    auto add = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit)
    { result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>{lo,hi,.1f},def,juce::AudioParameterFloatAttributes().withLabel(unit))); };
    add("crossover","Crossover",80,8000,1000,"Hz"); add("lowDrive","Low",0,100,30,"%");
    add("highDrive","High",0,100,30,"%"); add("mix","Mix",0,100,100,"%");
    add("output","Output",-12,12,0,"dB");
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"F / Peak","T / Drive"},0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false)); return result;
}
NianSaturatorProcessor::NianSaturatorProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)), parameters(*this,nullptr,"NianSaturatorState",createParameterLayout()) {}
juce::AudioProcessorParameter* NianSaturatorProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
juce::AudioProcessorEditor* NianSaturatorProcessor::createEditor() { return new NianSaturatorEditor(*this); }
void NianSaturatorProcessor::getStateInformation(juce::MemoryBlock& data) { if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void NianSaturatorProcessor::setStateInformation(const void* data,int size) { if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NianSaturatorProcessor(); }
void NianSaturatorProcessor::prepareToPlay(double rate,int) { sampleRate=4.f*float(std::isfinite(rate)&&rate>=8000?rate:48000); ring.fill(0); spectrum.fill(0); lowState={}; z1={}; z2={}; coefficientState={}; ringPosition=analysisCounter=0; peakFrequency={150,3000};
    displaySampleRate.store(sampleRate/4.f,std::memory_order_relaxed);
    for (auto& converter : converters) converter.prepare();
    dryHistory = {}; dryPosition = 0;
    displayFifo.reset(); setLatencySamples (latency);
    for(size_t i=0;i<window.size();++i)
        window[i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*float(i)/float(window.size()-1));
    crossoverAlpha.reset(sampleRate,.02); modeBlend.reset(sampleRate,.02); bypassBlend.reset(sampleRate/4.f,.02); mixBlend.reset(sampleRate/4.f,.02);
    const auto frequency=std::clamp(parameters.getRawParameterValue("crossover")->load(),80.f,std::min(8000.f,sampleRate*.4f));
    crossoverAlpha.setCurrentAndTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*frequency/sampleRate));
    modeBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("mode")->load()>.5f ? 1.f : 0.f);
    bypassBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("bypass")->load()>.5f ? 1.f : 0.f);
    mixBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load()*.01f); }
bool NianSaturatorProcessor::isBusesLayoutSupported(const BusesLayout& layout) const { return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet() && (layout.getMainInputChannelSet()==juce::AudioChannelSet::mono() || layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo()); }
void NianSaturatorProcessor::analyse(float crossover)
{
    constexpr int size=4096;
    spectrum.fill(0);
    for(int i=0;i<size;++i) spectrum[size_t(i)]=ring[size_t((ringPosition+i)%size)]*window[size_t(i)];
    fft.performFrequencyOnlyForwardTransform(spectrum.data());
    for(int band=0;band<2;++band)
    {
        float best=0, selected=peakFrequency[size_t(band)];
        for(int bin=1;bin<size/2;++bin)
        {
            const auto hz=bin*sampleRate/size;
            if(hz<40 || hz>sampleRate*.45f || (band==0 ? hz>=crossover : hz<crossover)) continue;
            float floor=0; int neighbors=0;
            for(int offset=-5;offset<=5;++offset)
                if(std::abs(offset)>1 && bin+offset>0 && bin+offset<size/2)
                { floor+=spectrum[size_t(bin+offset)]; ++neighbors; }
            floor/=float(std::max(1,neighbors)); const auto score=spectrum[size_t(bin)]-4*floor;
            if(score>best) { best=score; selected=hz; }
        }
        if(best>1.e-4f) peakFrequency[size_t(band)]=selected;
        peakFrequency[size_t(band)] = band == 0
            ? std::clamp(peakFrequency[0],40.f,crossover)
            : std::clamp(peakFrequency[1],crossover,sampleRate*.45f);
    }
}
void NianSaturatorProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { process(b,false); }
void NianSaturatorProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { process(b,true); }
int NianSaturatorProcessor::readDisplay(DisplaySample* destination, int capacity) noexcept
{
    const auto count=std::min(capacity,displayFifo.getNumReady());
    int start1,size1,start2,size2;
    displayFifo.prepareToRead(count,start1,size1,start2,size2);
    for(int i=0;i<size1;++i) destination[i]=displayQueue[size_t(start1+i)];
    for(int i=0;i<size2;++i) destination[size1+i]=displayQueue[size_t(start2+i)];
    displayFifo.finishedRead(size1+size2);
    return size1+size2;
}
void NianSaturatorProcessor::process(juce::AudioBuffer<float>& b,bool hostBypass)
{
    juce::ScopedNoDenormals guard;
    auto get=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    const auto crossover=std::clamp(get("crossover"),80.f,std::min(8000.f,sampleRate*.4f));
    const std::array<float,2> amount {get("lowDrive")*.01f,get("highDrive")*.01f};
    const auto gain=juce::Decibels::decibelsToGain(get("output"));
    const std::array<float,2> drives {1+amount[0]*8,1+amount[1]*8};
    const std::array<float,2> driveNormalization {1/std::tanh(drives[0]),1/std::tanh(drives[1])};
    const bool peakMode=get("mode")<.5f, bypass=hostBypass || get("bypass")>.5f;
    crossoverAlpha.setTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*crossover/sampleRate));
    modeBlend.setTargetValue(peakMode ? 0.f : 1.f); bypassBlend.setTargetValue(bypass ? 1.f : 0.f);
    mixBlend.setTargetValue(std::clamp(get("mix")*.01f,0.f,1.f));
    int writeStart1,writeSize1,writeStart2,writeSize2;
    displayFifo.prepareToWrite(b.getNumSamples(),writeStart1,writeSize1,writeStart2,writeSize2);
    const auto coefficientStep=1-std::exp(-1.f/(.02f*sampleRate));
    struct Coefficients { float b0, a1, a2; };
    std::array<Coefficients,2> coefficients {};
    const auto updateCoefficients = [&]
    {
        for(size_t band=0;band<2;++band)
        {
            const auto frequency=std::clamp(peakFrequency[band],40.f,sampleRate*.45f);
            const auto omega=juce::MathConstants<float>::twoPi*frequency/sampleRate;
            const auto a=std::sin(omega)/24.f, inv=1.f/(1.f+a);
            coefficients[band]={a*inv,-2*std::cos(omega)*inv,(1-a)*inv};
        }
    };
    updateCoefficients();
    for(int i=0;i<b.getNumSamples();++i)
    {
        std::array<character::Oversampling4x::Frame,2> frames {};
        std::array<float,2> dry {};
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto index=size_t(ch);
            auto input=b.getSample(ch,i); if(!std::isfinite(input)) input=0;
            dry[index]=dryHistory[index][size_t(dryPosition)];
            dryHistory[index][size_t(dryPosition)]=input;
            frames[index]=converters[index].upsample(input);
        }
        for(int phase=0;phase<4;++phase)
        {
            const auto alpha=crossoverAlpha.getNextValue(), mode=modeBlend.getNextValue();
            for(size_t band=0;band<2;++band)
            {
                const auto target=coefficients[band];
                coefficientState[band][0]+=coefficientStep*(target.b0-coefficientState[band][0]);
                coefficientState[band][1]+=coefficientStep*(target.a1-coefficientState[band][1]);
                coefficientState[band][2]+=coefficientStep*(target.a2-coefficientState[band][2]);
            }
            float signal=0;
            for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
                if(std::abs(frames[size_t(ch)][size_t(phase)])>std::abs(signal)) signal=frames[size_t(ch)][size_t(phase)];
            ring[size_t(ringPosition)]=signal; ringPosition=(ringPosition+1)%4096;
            if(peakMode && ++analysisCounter>=1024) { analysisCounter=0; analyse(crossover); updateCoefficients(); }
            for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
            {
                const auto index=size_t(ch);
                const auto x=frames[index][size_t(phase)];
                lowState[index]+=alpha*(x-lowState[index]); const std::array<float,2> bands {lowState[index],x-lowState[index]};
                float output=0;
                for(size_t band=0;band<2;++band)
                {
                    const auto& c=coefficientState[band];
                    const auto filtered=c[0]*bands[band]+z1[index][band];
                    z1[index][band]=-c[1]*filtered+z2[index][band]; z2[index][band]=-c[0]*bands[band]-c[2]*filtered;
                    const auto peak=bands[band]+filtered*amount[band]*2;
                    const auto driven=mode>0 ? bands[band]+amount[band]*(std::tanh(bands[band]*drives[band])*driveNormalization[band]-bands[band]) : bands[band];
                    output+=peak+mode*(driven-peak);
                }
                frames[index][size_t(phase)]=output;
            }
        }
        const auto bypassAmount=bypassBlend.getNextValue();
        const auto mix=mixBlend.getNextValue();
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto index=size_t(ch);
            const auto wet=converters[index].downsample(frames[index]);
            const auto mixed=(dry[index]+mix*(wet-dry[index]))*gain;
            const auto delta=(1-bypassAmount)*(mixed-dry[index]);
            b.setSample(ch,i,dry[index]+delta);
            if(ch==0 && i<writeSize1+writeSize2)
            {
                const auto position=i<writeSize1 ? writeStart1+i : writeStart2+i-writeSize1;
                displayQueue[size_t(position)]={dry[index],delta};
            }
        }
        dryPosition=(dryPosition+1)%latency;
    }
    displayFifo.finishedWrite(writeSize1+writeSize2);
}
