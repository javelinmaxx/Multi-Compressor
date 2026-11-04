#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../YinyangCompressor/Source/Oversampling4x.h"

class NianSaturatorProcessor final : public juce::AudioProcessor
{
public:
    NianSaturatorProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
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
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::dsp::FFT fft {12};
    std::array<float,8192> spectrum {};
    std::array<float,4096> ring {}, window {};
    std::array<float,2> lowState {};
    std::array<std::array<float,2>,2> z1 {}, z2 {};
    std::array<float,2> peakFrequency {150,3000};
    std::array<std::array<float,3>,2> coefficientState {};
    juce::SmoothedValue<float> crossoverAlpha, modeBlend, bypassBlend;
    std::array<character::Oversampling4x, 2> converters;
    static constexpr int latency = character::Oversampling4x::latencySamples;
    std::array<std::array<float, latency>, 2> dryHistory {};
    int dryPosition = 0, displayDecimation = 0;
    juce::AbstractFifo displayFifo { 2048 };
    std::array<DisplaySample, 2048> displayQueue {};
    int ringPosition = 0, analysisCounter = 0;
    float sampleRate = 48000;
    void analyse (float crossover);
    void process (juce::AudioBuffer<float>&, bool hostBypass);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NianSaturatorProcessor)
};
