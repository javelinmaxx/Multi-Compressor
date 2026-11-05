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
NianSaturatorProcessor::~NianSaturatorProcessor() { stopAnalysisWorker(); }
void NianSaturatorProcessor::releaseResources() { stopAnalysisWorker(); }
juce::AudioProcessorParameter* NianSaturatorProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
juce::AudioProcessorEditor* NianSaturatorProcessor::createEditor() { return new NianSaturatorEditor(*this); }
void NianSaturatorProcessor::getStateInformation(juce::MemoryBlock& data) { if(auto xml=parameters.copyState().createXml()) copyXmlToBinary(*xml,data); }
void NianSaturatorProcessor::setStateInformation(const void* data,int size) { if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NianSaturatorProcessor(); }
void NianSaturatorProcessor::prepareToPlay(double rate,int) { stopAnalysisWorker(); sampleRate=4.f*float(std::isfinite(rate)&&rate>=8000?rate:48000); ring.fill(0); spectrum.fill(0); lowState={}; z1={}; z2={}; coefficientState={}; tubeDcState={}; tubeRmsState={}; tubeGainState={{{1,1},{1,1}}}; peakTarget={}; peakEnvelope={}; ringPosition=analysisCounter=0; fSuspended=tubeSuspended=false; recentInputPeak=0; recoveryMix=1; analysisGeneration=0; lastPeakMode=parameters.getRawParameterValue("mode")->load()<.5f;
    displaySampleRate.store(sampleRate/4.f,std::memory_order_relaxed);
    for (auto& converter : converters) converter.prepare();
    dryHistory = {}; dryPosition = 0;
    lookaheadSamples=std::max(1,juce::roundToInt(sampleRate*.25f*.005f));
    for(auto& channel : lookaheadHistory) channel.assign(size_t(lookaheadSamples),0.f);
    lookaheadPosition=0;
    displayFifo.reset(); setLatencySamples(latency+lookaheadSamples);
    for(size_t i=0;i<window.size();++i)
        window[i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*float(i)/float(window.size()-1));
    crossoverAlpha.reset(sampleRate,.02); modeBlend.reset(sampleRate,.02); bypassBlend.reset(sampleRate/4.f,.02); mixBlend.reset(sampleRate/4.f,.02);
    const auto frequency=std::clamp(parameters.getRawParameterValue("crossover")->load(),80.f,std::min(8000.f,sampleRate*.1f));
    const auto maxFrequency=std::min(20000.f,sampleRate*.1125f);
    for(int slot=0;slot<fBandCount;++slot)
    {
        const auto lo=slot<6 ? 40.f : frequency;
        const auto hi=slot<6 ? frequency : maxFrequency;
        peakFrequency[size_t(slot)]=lo*std::pow(hi/lo,(float(slot%6)+.5f)/6.f);
    }
    crossoverAlpha.setCurrentAndTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*frequency/sampleRate));
    modeBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("mode")->load()>.5f ? 1.f : 0.f);
    bypassBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("bypass")->load()>.5f ? 1.f : 0.f);
    mixBlend.setCurrentAndTargetValue(parameters.getRawParameterValue("mix")->load()*.01f);
    startAnalysisWorker(); }
bool NianSaturatorProcessor::isBusesLayoutSupported(const BusesLayout& layout) const { return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet() && (layout.getMainInputChannelSet()==juce::AudioChannelSet::mono() || layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo()); }
void NianSaturatorProcessor::startAnalysisWorker()
{
    stopAnalysis=false; hasJob=hasResult=false;
    analysisThread=std::thread([this]
    {
        AnalysisJob job;
        for(;;)
        {
            {
                std::unique_lock<std::mutex> lock(jobMutex);
                jobReady.wait(lock,[this]{return stopAnalysis || hasJob;});
                if(stopAnalysis) break;
                job=pendingJob;
                hasJob=false;
            }
            auto result=analyse(job);
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                pendingResult=result;
                hasResult=true;
            }
        }
    });
}
void NianSaturatorProcessor::stopAnalysisWorker()
{
    if(!analysisThread.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(jobMutex);
        stopAnalysis=true;
    }
    jobReady.notify_one();
    analysisThread.join();
    hasJob=hasResult=false;
}
void NianSaturatorProcessor::submitAnalysis(float crossover) noexcept
{
    if(!jobMutex.try_lock()) return;
    if(!hasJob)
    {
        for(size_t i=0;i<ring.size();++i)
            pendingJob.samples[i]=ring[(size_t(ringPosition)+i)%ring.size()];
        pendingJob.crossover=crossover;
        pendingJob.rate=sampleRate*.25f;
        pendingJob.generation=analysisGeneration;
        hasJob=true;
        jobMutex.unlock();
        jobReady.notify_one();
    }
    else jobMutex.unlock();
}
bool NianSaturatorProcessor::takeAnalysis(AnalysisResult& result) noexcept
{
    if(!resultMutex.try_lock()) return false;
    const auto available=hasResult;
    if(available) { result=pendingResult; hasResult=false; }
    resultMutex.unlock();
    return available;
}
NianSaturatorProcessor::AnalysisResult NianSaturatorProcessor::analyse(const AnalysisJob& job)
{
    constexpr int size=4096;
    const auto analysisRate=job.rate, crossover=job.crossover;
    AnalysisResult result; result.generation=job.generation; result.crossover=crossover;
    const auto maxFrequency=std::min(20000.f,analysisRate*.45f);
    spectrum.fill(0);
    for(int i=0;i<size;++i) spectrum[size_t(i)]=job.samples[size_t(i)]*window[size_t(i)];
    fft.performFrequencyOnlyForwardTransform(spectrum.data());
    for(int slot=0;slot<fBandCount;++slot)
    {
        const auto sideLo=slot<6 ? 40.f : crossover;
        const auto sideHi=slot<6 ? crossover : maxFrequency;
        const auto ratio=sideHi/sideLo;
        const auto cell=slot%6;
        const auto lo=sideLo*std::pow(ratio,float(cell)/6.f);
        const auto hi=sideLo*std::pow(ratio,float(cell+1)/6.f);
        const auto start=std::max(1,int(std::ceil(lo*size/analysisRate)));
        const auto end=std::min(size/2-1,int(std::ceil(hi*size/analysisRate))-1);
        float best=0, selected=0;
        for(int bin=start;bin<=end;++bin)
        {
            float floor=0; int neighbors=0;
            for(int offset=-5;offset<=5;++offset)
                if(std::abs(offset)>1 && bin+offset>0 && bin+offset<size/2)
                { floor+=spectrum[size_t(bin+offset)]; ++neighbors; }
            floor/=float(std::max(1,neighbors));
            // Low FFT bins have only a few neighbors; adjacent bass notes can
            // otherwise be mistaken for the local floor of each other.
            const auto score=spectrum[size_t(bin)]-(bin*analysisRate/size<120.f ? 1.25f : 4.f)*floor;
            if(score>best)
            {
                best=score;
                const auto left=spectrum[size_t(bin-1)], center=spectrum[size_t(bin)], right=spectrum[size_t(bin+1)];
                const auto curvature=left-2*center+right;
                const auto offset=std::abs(curvature)>1.e-6f ? std::clamp(.5f*(left-right)/curvature,-.5f,.5f) : 0.f;
                selected=(float(bin)+offset)*analysisRate/size;
            }
        }
        if(best>1.e-3f)
        { result.frequency[size_t(slot)]=std::clamp(selected,lo,hi); result.target[size_t(slot)]=1; }
    }
    return result;
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
    const auto crossover=std::clamp(get("crossover"),80.f,std::min(8000.f,sampleRate*.1f));
    const std::array<float,2> amount {get("lowDrive")*.01f,get("highDrive")*.01f};
    const auto gain=juce::Decibels::decibelsToGain(get("output"));
    const std::array<float,2> drives {1+amount[0]*2,1+amount[1]*2};
    const std::array<float,2> biases {.04f*amount[0],.04f*amount[1]};
    const std::array<float,2> centers {std::atan(drives[0]*biases[0]),std::atan(drives[1]*biases[1])};
    const std::array<float,2> normalizers {
        1.f/(std::atan(drives[0]*(1.f+biases[0]))-centers[0]),
        1.f/(std::atan(drives[1]*(1.f+biases[1]))-centers[1])};
    const bool peakMode=get("mode")<.5f, bypass=hostBypass || get("bypass")>.5f;
    if(peakMode!=lastPeakMode) { ++analysisGeneration; lastPeakMode=peakMode; }
    crossoverAlpha.setTargetValue(1-std::exp(-juce::MathConstants<float>::twoPi*crossover/sampleRate));
    modeBlend.setTargetValue(peakMode ? 0.f : 1.f); bypassBlend.setTargetValue(bypass ? 1.f : 0.f);
    mixBlend.setTargetValue(std::clamp(get("mix")*.01f,0.f,1.f));
    int writeStart1,writeSize1,writeStart2,writeSize2;
    displayFifo.prepareToWrite(b.getNumSamples(),writeStart1,writeSize1,writeStart2,writeSize2);
    const auto coefficientStep=1-std::exp(-1.f/(.005f*sampleRate));
    const auto peakAttackStep=1-std::exp(-1.f/(.004f*sampleRate));
    const auto peakReleaseStep=1-std::exp(-1.f/(.025f*sampleRate));
    const auto dcStep=1-std::exp(-juce::MathConstants<float>::twoPi*8.f/sampleRate);
    const auto tubeDetectorStep=1-std::exp(-1.f/(.005f*sampleRate));
    const auto tubeAttackStep=1-std::exp(-1.f/(.002f*sampleRate));
    const auto tubeReleaseStep=1-std::exp(-1.f/(.075f*sampleRate));
    const auto inputPeakDecay=std::exp(-1.f/(.1f*sampleRate*.25f));
    const auto recoveryStep=1.f/(.01f*sampleRate*.25f);
    struct Coefficients { float b0, a1, a2; };
    std::array<Coefficients,fBandCount> coefficients {};
    const auto updateCoefficients = [&]
    {
        for(size_t band=0;band<coefficients.size();++band)
        {
            const auto frequency=std::clamp(peakFrequency[band],40.f,std::min(20000.f,sampleRate*.1125f));
            const auto omega=juce::MathConstants<float>::twoPi*frequency/sampleRate;
            const auto a=std::sin(omega)/48.f, inv=1.f/(1.f+a);
            coefficients[band]={a*inv,-2*std::cos(omega)*inv,(1-a)*inv};
            if(peakEnvelope[band]<1.e-4f)
                coefficientState[band]={coefficients[band].b0,coefficients[band].a1,coefficients[band].a2};
        }
    };
    updateCoefficients();
    for(int i=0;i<b.getNumSamples();++i)
    {
        float signal=0;
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto input=b.getSample(ch,i);
            if(std::isfinite(input) && std::abs(input)>std::abs(signal)) signal=input;
        }
        ring[size_t(ringPosition)]=signal; ringPosition=(ringPosition+1)%int(ring.size());
        if((analysisCounter&63)==0)
        {
            AnalysisResult result;
            if(takeAnalysis(result) && peakMode && result.generation==analysisGeneration
               && std::abs(result.crossover-crossover)<1.f)
            {
                for(int slot=0;slot<fBandCount;++slot)
                {
                    const auto index=size_t(slot);
                    if(result.target[index]>.5f)
                    {
                        if(peakTarget[index]<.5f)
                        {
                            peakEnvelope[index]=0;
                            for(int ch=0;ch<2;++ch) z1[size_t(ch)][index]=z2[size_t(ch)][index]=0;
                        }
                        peakFrequency[index]=result.frequency[index];
                    }
                    peakTarget[index]=result.target[index];
                }
                fSuspended=false;
                updateCoefficients();
            }
        }
        if(peakMode && ++analysisCounter>=256) { analysisCounter=0; submitAnalysis(crossover); }
        std::array<character::Oversampling4x::Frame,2> frames {};
        std::array<float,2> dry {};
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto index=size_t(ch);
            auto input=b.getSample(ch,i); if(!std::isfinite(input)) input=0;
            recentInputPeak=std::max(std::abs(input),recentInputPeak*inputPeakDecay);
            const auto delayed=lookaheadHistory[index][size_t(lookaheadPosition)];
            lookaheadHistory[index][size_t(lookaheadPosition)]=input;
            dry[index]=dryHistory[index][size_t(dryPosition)];
            dryHistory[index][size_t(dryPosition)]=delayed;
            frames[index]=converters[index].upsample(delayed);
        }
        lookaheadPosition=(lookaheadPosition+1)%lookaheadSamples;
        for(int phase=0;phase<4;++phase)
        {
            const auto alpha=crossoverAlpha.getNextValue(), mode=modeBlend.getNextValue();
            if(!peakMode && mode>=.9999f && !fSuspended)
            {
                peakTarget={}; peakEnvelope={}; z1={}; z2={}; fSuspended=true;
            }
            if(peakMode && mode<=.0001f && !tubeSuspended)
            {
                tubeDcState={}; tubeRmsState={}; tubeGainState={{{1,1},{1,1}}}; tubeSuspended=true;
            }
            if(!peakMode) tubeSuspended=false;
            for(size_t band=0;band<coefficients.size() && !fSuspended;++band)
            {
                const auto target=coefficients[band];
                coefficientState[band][0]+=coefficientStep*(target.b0-coefficientState[band][0]);
                coefficientState[band][1]+=coefficientStep*(target.a1-coefficientState[band][1]);
                coefficientState[band][2]+=coefficientStep*(target.a2-coefficientState[band][2]);
                auto& envelope=peakEnvelope[band];
                envelope+=(peakTarget[band]>envelope ? peakAttackStep : peakReleaseStep)*(peakTarget[band]-envelope);
            }
            for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
            {
                const auto index=size_t(ch);
                const auto x=frames[index][size_t(phase)];
                lowState[index]+=alpha*(x-lowState[index]); const std::array<float,2> bands {lowState[index],x-lowState[index]};
                float peakDelta=0;
                if(!fSuspended && mode<.9999f)
                    for(size_t band=0;band<coefficients.size();++band)
                    {
                        if(peakTarget[band]<.5f && peakEnvelope[band]<1.e-5f) continue;
                        const auto& c=coefficientState[band];
                        const auto side=band/6;
                        const auto filtered=c[0]*bands[side]+z1[index][band];
                        z1[index][band]=-c[1]*filtered+z2[index][band];
                        z2[index][band]=-c[0]*bands[side]-c[2]*filtered;
                        peakDelta+=filtered*amount[side]*2.f*peakEnvelope[band];
                    }
                float tube=x;
                if(mode>0 && !tubeSuspended)
                    for(size_t band=0;band<2;++band)
                    {
                        // As in a feed-forward compressor, the band level
                        // controls only its added harmonic residual. This
                        // softens loud attacks without compressing dry audio.
                        auto& rms=tubeRmsState[index][band];
                        rms+=tubeDetectorStep*(bands[band]*bands[band]-rms);
                        const auto level=std::sqrt(std::max(0.f,rms));
                        const auto targetGain=level>.18f ? std::sqrt(.18f/level) : 1.f;
                        auto& dynamicGain=tubeGainState[index][band];
                        dynamicGain+=(targetGain<dynamicGain ? tubeAttackStep : tubeReleaseStep)*(targetGain-dynamicGain);
                        const auto shaped=(std::atan(drives[band]*(bands[band]+biases[band]))-centers[band])*normalizers[band];
                        const auto residual=shaped-bands[band];
                        auto& dc=tubeDcState[index][band];
                        dc+=dcStep*(residual-dc);
                        tube+=.45f*amount[band]*dynamicGain*(residual-dc);
                    }
                frames[index][size_t(phase)]=x+peakDelta+mode*(tube-x-peakDelta);
            }
        }
        const auto bypassAmount=bypassBlend.getNextValue();
        const auto mix=mixBlend.getNextValue();
        std::array<float,2> wet {};
        bool fault=false;
        const auto wetLimit=std::max(8.f,8.f*recentInputPeak);
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto index=size_t(ch);
            for(const auto value : frames[index])
                if(!std::isfinite(value) || std::abs(value)>wetLimit) fault=true;
            if(!fault)
            {
                wet[index]=converters[index].downsample(frames[index]);
                if(!std::isfinite(wet[index]) || std::abs(wet[index])>wetLimit) fault=true;
            }
        }
        if(fault)
        {
            // A poisoned IIR or oversampling history would otherwise produce
            // non-finite output indefinitely. Fall back to latency-aligned dry
            // audio and fade the rebuilt processing path back in.
            for(auto& converter : converters) converter.reset();
            lowState={}; z1={}; z2={}; tubeDcState={}; tubeRmsState={}; tubeGainState={{{1,1},{1,1}}}; peakTarget={}; peakEnvelope={}; coefficientState={}; tubeSuspended=false;
            const auto maximum=std::min(20000.f,sampleRate*.1125f);
            for(int slot=0;slot<fBandCount;++slot)
                if(!std::isfinite(peakFrequency[size_t(slot)]) || peakFrequency[size_t(slot)]<40.f || peakFrequency[size_t(slot)]>maximum)
                {
                    const auto lo=slot<6 ? 40.f : crossover;
                    const auto hi=slot<6 ? crossover : maximum;
                    peakFrequency[size_t(slot)]=lo*std::pow(hi/lo,(float(slot%6)+.5f)/6.f);
                }
            updateCoefficients();
            recoveryMix=0;
        }
        else recoveryMix=std::min(1.f,recoveryMix+recoveryStep);
        for(int ch=0;ch<b.getNumChannels() && ch<2;++ch)
        {
            const auto index=size_t(ch);
            const auto mixed=(dry[index]+mix*(wet[index]-dry[index]))*gain;
            const auto delta=fault ? 0.f : recoveryMix*(1-bypassAmount)*(mixed-dry[index]);
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
