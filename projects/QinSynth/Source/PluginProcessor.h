#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../LanReverb/Source/ReverbEngine.h"
#include "../../ZaoDrive/Source/Circuits.h"
#include <array>

class QinSynthProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int maxVoices=16, oscillatorCount=3, maxUnison=8;
    QinSynthProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override { reverb.release(); }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 40.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState parameters;
private:
    friend struct QinSynthTestAccess;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    struct Subvoice { double phase=0, triangle=0; };
    struct Voice
    {
        std::array<std::array<Subvoice,maxUnison>,oscillatorCount> oscillators {};
        int note=-1, channel=1;
        uint64_t age=0;
        float velocity=0, envelope=0, pan=0;
        bool keyDown=false, sustained=false, active=false;
    };
    struct OscillatorSettings { int wave=0, octave=0, unison=1; float level=0, detune=0; };
    struct Settings
    {
        std::array<OscillatorSettings,oscillatorCount> oscillator;
        float attack=0.01f, release=0.3f, pan=0, drive=0, output=0;
        float verbTime=180, verbDecay=3, verbScatter=.7f, verbMix=.2f;
        float tapMotion=.5f, phaseMotion=.5f;
    };
    Settings readSettings() const noexcept;
    void handleMidi(const juce::MidiMessage&,const Settings&);
    void renderSegment(juce::AudioBuffer<float>&,int,int,const Settings&,bool);
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
    static double oscillatorSample(Subvoice&,int,double) noexcept;
    static double polyBlep(double,double) noexcept;
    static float notePan(int,float) noexcept;
    std::array<Voice,maxVoices> voices {};
    std::array<zao::OpAmpFeedbackClipper,2> driveCircuit {};
    lan::ReverbEngine reverb;
    uint64_t voiceAge=0, onsetCount=0;
    double sampleRate=48000;
    float onsetTap=0, onsetPhase=0;
    bool sustain=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthProcessor)
};
