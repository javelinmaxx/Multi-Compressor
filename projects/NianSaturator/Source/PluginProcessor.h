#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include "../../YinyangCompressor/Source/Oversampling4x.h"

class NianSaturatorProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int fBandCount = 12;
    NianSaturatorProcessor();
    void prepareToPlay (double, int) override;
    ~NianSaturatorProcessor() override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    struct DisplaySample { float input = 0, delta = 0; };
    int readDisplay (DisplaySample* destination, int capacity) noexcept;
    float getDisplaySampleRate() const noexcept { return displaySampleRate.load (std::memory_order_relaxed); }
private:
    friend struct NianSaturatorTestAccess;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::dsp::FFT fft {12};
    std::array<float,8192> spectrum {};
    std::array<float,4096> ring {}, window {};
    std::array<float,2> lowState {};
    std::array<std::array<float,fBandCount>,2> z1 {}, z2 {};
    std::array<float,fBandCount> peakFrequency {}, peakTarget {}, peakEnvelope {};
    std::array<int,fBandCount> peakHoldSamples {}, peakCooldownSamples {};
    std::array<int,fBandCount> recentPeakSamples {};
    std::array<float,fBandCount> pendingPeakFrequency {};
    std::array<bool,fBandCount> retuning {};
    std::array<std::array<double,3>,fBandCount> coefficientState {};
    struct FDeltaHighPass
    {
        double a1 = 0, a2 = 0, a3 = 0, ic1eq = 0, ic2eq = 0;
        void prepare(double rate) noexcept;
        void reset() noexcept { ic1eq=ic2eq=0; }
        float process(float input) noexcept
        {
            constexpr auto damping=1.4142135623730951; // Butterworth Q
            const auto v3=double(input)-ic2eq;
            const auto v1=a1*ic1eq+a2*v3;
            const auto v2=ic2eq+a2*ic1eq+a3*v3;
            ic1eq=2.0*v1-ic1eq;
            ic2eq=2.0*v2-ic2eq;
            return static_cast<float>(double(input)-damping*v1-v2);
        }
    };
    std::array<FDeltaHighPass,2> fDeltaHighPass {};
    std::array<std::array<float,2>,2> tubeDcState {};
    std::array<std::array<float,2>,2> tubeRmsState {}, tubeGainState {};
    juce::SmoothedValue<float> crossoverAlpha, modeBlend, bypassBlend, mixBlend;
    std::array<character::Oversampling4x, 2> converters;
    static constexpr int latency = character::Oversampling4x::latencySamples;
    std::array<std::array<float, latency>, 2> dryHistory {};
    int dryPosition = 0;
    std::array<std::vector<float>, 2> lookaheadHistory;
    int lookaheadSamples = 0, lookaheadPosition = 0;
    juce::AbstractFifo displayFifo { 16384 };
    std::array<DisplaySample, 16384> displayQueue {};
    std::atomic<float> displaySampleRate { 48000.f };
    int ringPosition = 0, analysisCounter = 0;
    float sampleRate = 48000;
    bool fSuspended = false, tubeSuspended = false;
    float recentInputPeak = 0, recoveryMix = 1;
    float onsetFast = 0, onsetSlow = 0;
    int onsetRefractory = 0;
    bool onsetArmed = true;
    struct AnalysisJob { std::array<float,4096> samples {}; float crossover = 1000, rate = 48000; unsigned generation = 0; };
    struct AnalysisResult { std::array<float,fBandCount> frequency {}, target {}; float crossover = 1000; unsigned generation = 0; };
    std::mutex jobMutex, resultMutex;
    std::condition_variable jobReady;
    std::thread analysisThread;
    AnalysisJob pendingJob;
    AnalysisResult pendingResult;
    bool stopAnalysis = false, hasJob = false, hasResult = false, lastPeakMode = true;
    unsigned analysisGeneration = 0;
    void startAnalysisWorker();
    void stopAnalysisWorker();
    void submitAnalysis(float crossover) noexcept;
    bool takeAnalysis(AnalysisResult& result) noexcept;
    AnalysisResult analyse(const AnalysisJob& job);
    static void suppressAdjacentLowPeaks(AnalysisResult&,const std::array<float,fBandCount>& scores) noexcept;
    void process (juce::AudioBuffer<float>&, bool hostBypass);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NianSaturatorProcessor)
};
