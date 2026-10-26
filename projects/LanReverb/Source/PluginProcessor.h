#pragma once
#include "ReverbEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class LanReverbProcessor final : public juce::AudioProcessor
{
public:
    LanReverbProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override { engine.release(); }
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
    double getTailLengthSeconds() const override { return 65; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    lan::Settings settings() const;
    bool readAnalysis (lan::Analysis& a) const { return engine.readAnalysis(a); }
    float reductionDb() const { return engine.reductionDb(); }
    float peakLevel() const { return engine.peakLevel(); }
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    lan::ReverbEngine engine;
    void process (juce::AudioBuffer<float>&, bool hostBypass);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LanReverbProcessor)
};
