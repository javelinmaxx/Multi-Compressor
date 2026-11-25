#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../LanReverb/Source/ReverbEngine.h"
#include "../../RenTransformer/Source/TransformerStage.h"
#include "DriveCircuitBank.h"
#include "DriveInputFilter.h"
#include "WaveBlend.h"
#include "StereoPanLookup.h"
#include "FmRouting.h"
#include "TuningTable.h"
#include "ModulationTargets.h"
#include "../../LanReverb/Source/DelayTiming.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class QinSynthProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int maxVoices=16, oscillatorCount=3, maxUnison=8;
    static constexpr float filterEnvOctaves=10.f;
    static constexpr size_t driveScopePoints=128, driveScopeCapturePoints=1024;
    struct DriveScope
    {
        std::array<float,driveScopeCapturePoints> input {}, output {};
        uint32_t epoch=0;
    };
    QinSynthProcessor();
    ~QinSynthProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
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
    double getTailLengthSeconds() const override
    { return parameters.getRawParameterValue("ampTimesFive")->load()>=.5f ? 150.0 : 40.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    DriveScope getDriveScope() const noexcept;
    float getLiveFilterCutoff() const noexcept { return liveFilterCutoff.load(std::memory_order_relaxed); }
    float getLiveFilterResonance() const noexcept { return liveFilterResonance.load(std::memory_order_relaxed); }
    struct LiveModulation { int target=-1; float normalized=0; };
    LiveModulation getLiveModulation() const noexcept
    { return {liveModTarget.load(std::memory_order_acquire),liveModNormalized.load(std::memory_order_relaxed)}; }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(std::memory_order_relaxed); }
    uint32_t getPresetStateEpoch() const noexcept { return presetStateEpoch.load(std::memory_order_acquire); }
    double getHostTempo() const noexcept { return hostTempo.load(std::memory_order_relaxed); }
    const std::atomic<bool>& bypassActivity() const noexcept { return hostBypassed; }
    juce::AudioProcessorValueTreeState parameters;
private:
    friend struct QinSynthTestAccess;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    struct Subvoice { double phase=0; };
    struct Envelope { float value=0, progress=0, start=0; int stage=0; };
    struct FilterState { float s1=0,s2=0,t1=0,t2=0,pole=0; std::array<float,4> ladder {}; };
    struct NoiseFilterCoefficients { float a1=1, a2=0, a3=0, highPass=0; };
    struct Voice
    {
        std::array<std::array<Subvoice,maxUnison>,oscillatorCount> oscillators {};
        std::array<float,oscillatorCount> previousFmOutput {};
        std::array<Envelope,3> env {};
        std::array<float,3> effectiveEnv {};
        std::array<FilterState,2> filter {};
        int note=-1, channel=1;
        uint64_t age=0;
        float velocity=0, pan=0, cutoff=0, onsetGain=0;
        double pitchNote=0, glideStart=0;
        float glideProgress=1, glideIncrement=1, glideShape=.5f;
        uint32_t noiseState=1;
        std::array<float,3> pink {};
        float noiseBandState=0, noiseLowState=0, noiseHighPassState=0;
        NoiseFilterCoefficients noiseCoefficients {}, noiseCoefficientStep {};
        int noiseCoefficientSamples=0;
        bool noiseCoefficientsReady=false;
        bool noiseWasGliding=false;
        bool keyDown=false, sustained=false, active=false;
    };
    struct SharedFrame
    {
        std::array<float,3> sharedValues, merge;
        float modDepth;
        std::array<float,oscillatorCount> fmDepth;
        std::array<std::array<double,maxUnison>,oscillatorCount> detuneRatios;
        std::array<std::array<float,4>,oscillatorCount> waveMixes;
        std::array<qin::DutyShape,oscillatorCount> dutyShapes;
    };
    struct VoiceFrame
    {
        double left=0,right=0,masterLeft=0,masterRight=0,cutoffSum=0,cutoffWeight=0;
    };
    struct WorkerJob
    {
        void* context=nullptr;
        void (*run)(void*,int,int)=nullptr;
        int first=0,last=0;
    };
    class VoiceWorkerThread final : public juce::Thread
    {
    public:
        explicit VoiceWorkerThread(QinSynthProcessor& processor)
            : juce::Thread("Qin voice render"),owner(processor) {}
        void run() override { owner.voiceWorkerLoop(); }
    private:
        QinSynthProcessor& owner;
    };
    struct HeldNote { int note=-1, channel=1, voiceIndex=-1; };
    struct OscillatorSettings { qin::WavePosition wave; int octave=0, unison=1, fmSource=-1; bool retrig=false; float phaseStart=0, duty=.5f, level=0, detune=0, detuneShape=0, spread=.12f, pan=0, fmAmount=0, fineCents=0; };
    struct NoiseSettings { float level=.3f, tune=0, highPass=20, pan=0; int style=0; };
    struct EnvelopeSettings { float attack=.01f, decay=.2f, sustain=1.f, release=.3f, curve=0; };
    struct Settings
    {
        std::array<OscillatorSettings,oscillatorCount> oscillator;
        NoiseSettings noiseOscillator;
        qin::TuningTable tuning;
        std::array<EnvelopeSettings,3> env;
        std::array<float,3> envelopeMerge {};
        float pan=0, drive=0, driveMix=1, driveLowCut=0, driveHighCut=20000,
              drivePostLowCut=0, drivePostHighCut=20000, output=0, cutoff=18000,
              resonance=0, keyFollow=0, filterEnv=0, filterMerge=0, modDepth=0;
        int filterType=0, modTarget=0, voiceCount=maxVoices, driveMode=1, legatoMode=0;
        bool noise=false, transformer=false, polyLegato=false;
        float slideMs=80, slideShape=.5f;
        float verbTime=180, verbDecay=3, verbScatter=.7f, verbMix=.2f;
        bool verbSync=false;
        int verbNote=lan::defaultNote;
        float tapMotion=.5f, phaseMotion=.5f;
    };
    Settings readSettings(const char* overrideId=nullptr,float overrideValue=0) const noexcept;
    void retargetVoice(Voice&,int,int,const Settings&) noexcept;
    double advancePitch(Voice&,const Settings&) const noexcept;
    void handleMidi(const juce::MidiMessage&,const Settings&);
    void renderSegment(juce::AudioBuffer<float>&,int,int,const Settings&,bool);
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
    static double oscillatorSample(Subvoice&,const std::array<float,4>&,double,const qin::DutyShape&) noexcept;
    static double polyBlep(double,double) noexcept;
    static float notePan(int,float) noexcept;
    float advanceEnvelope(Envelope&,const EnvelopeSettings&,bool,const std::array<float,3>&) const noexcept;
    float filterSample(FilterState&,float,float,float,float,int) const noexcept;
    float filterCoefficient(float hz) const noexcept;
    void stopVoiceWorker();
    void voiceWorkerLoop();
    std::array<Voice,maxVoices> voices {};
    std::atomic<uint32_t> presetStateEpoch {0};
    std::vector<SharedFrame> sharedFrames;
    std::array<std::vector<VoiceFrame>,maxVoices> voiceFrames;
    juce::WaitableEvent voiceWorkerWake;
    VoiceWorkerThread voiceWorker {*this};
    WorkerJob voiceWorkerJob;
    std::atomic<int> voiceWorkerState {2};
    std::atomic<uint32_t> parallelVoiceJobs {0};
    bool parallelVoicesEnabled=true;
    std::array<Envelope,3> sharedEnvelopes {};
    std::array<float,3> smoothedEnvelopeMerge {};
    std::array<qin::WavePosition,oscillatorCount> smoothedWaves {};
    std::array<float,oscillatorCount> smoothedDuties {};
    std::array<std::array<double,maxUnison>,oscillatorCount> detuneRatios {};
    std::array<juce::SmoothedValue<float>,oscillatorCount> fmAmounts {};
    bool fmWasActive=false;
    static constexpr int coefficientTableSize=8192;
    std::array<float,coefficientTableSize+1> coefficientTable {};
    std::array<HeldNote,128> heldNotes {};
    int heldNoteCount=0;
    qin::DriveCircuitBank driveCircuit;
    qin::DriveInputFilter driveInputFilter;
    qin::DriveInputFilter drivePostFilter;
    bool driveWasActive=false;
    std::array<juce::SmoothedValue<float>,zao::circuitCount> driveWeights;
    int lastDriveMode=1;
    std::array<FilterState,2> masterFilter {};
    float masterCutoff=0, smoothedFilterMerge=0, smoothedModDepth=0;
    std::array<std::atomic<float>,driveScopeCapturePoints> driveScopeInput {}, driveScopeOutput {};
    std::atomic<uint32_t> driveScopeEpoch {0};
    std::atomic<float> liveFilterCutoff {0};
    std::atomic<float> liveFilterResonance {0};
    std::atomic<int> liveModTarget {-1};
    std::atomic<float> liveModNormalized {0};
    std::atomic<int> activeVoiceCount {0};
    uint32_t driveScopeWrite=0, driveScopeCounter=0;
    lan::ReverbEngine reverb;
    ren::TransformerStage transformerStage;
    uint64_t voiceAge=0, onsetCount=0;
    double sampleRate=48000;
    std::atomic<double> hostTempo {120.0};
    std::atomic<bool> hostBypassed {false};
    float glideShapeSmoothing=.002f;
    float waveSmoothing=.002f;
    float onsetTap=0, onsetPhase=0;
    bool sustain=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthProcessor)
};
