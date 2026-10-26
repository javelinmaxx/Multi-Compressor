#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace lan
{
struct Settings
{
    float timeMs = 90, decaySeconds = 10, predelayMs = 20, diffusion = .85f, width = 1.25f;
    float tap = 0, phase = .6f, tame = .75f, mix = .8f, outputDb = 0, scatter = 1;
    bool automatic = true, bypass = false;
};
inline constexpr int spectrumBins = 64, maxBands = 3, analysisSize = 4096;
struct Band { float frequency = 0, width = 0, gainDb = 0; };
struct Analysis
{
    std::array<Band, maxBands> bands {};
    std::array<float, spectrumBins> input {}, feedback {};
    float strength = 0, frequency = 0;
    int count = 0;
    uint32_t revision = 0, generation = 0;
};
struct Frame
{
    std::array<std::array<float, analysisSize>, 4> audio {}; // input L/R, feedback L/R
    double rate = 48000;
    float sourceHoldSeconds = 12;
    uint32_t generation = 0;
    uint64_t sampleEnd = 0;
};
// Input-aware peak prominence and persistence detect narrow, sustained loop
// coloration. Only attenuation is requested; this is not a feedback canceller.
class ResonanceAnalyser
{
public:
    Analysis analyse (const Frame&);
    void reset (bool preserveSource = false);
private:
    juce::dsp::FFT fft { 12 };
    std::array<float, 2*analysisSize> fftData {};
    std::array<double, analysisSize/2+1> input {}, feedback {};
    std::array<float, analysisSize/2+1> ages {}, sourceHolds {};
    Analysis held;
    uint64_t previousSample = 0;
};
class AnalysisService final : private juce::Thread
{
public:
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free, "Audio publication requires lock-free scalars");
    AnalysisService();
    ~AnalysisService() override;
    void prepare (double rate, bool worker);
    void release();
    void setActive (bool, float sourceHoldSeconds = 12);
    void push (float left, float right, float wetLeft, float wetRight) noexcept;
    void finishBlock(); // synchronous only in explicit offline test rendering
    bool read (Analysis&) const noexcept;
    uint32_t jobs() const noexcept { return submitted.load(); }
    uint32_t currentGeneration() const noexcept { return generation.load(); }
private:
    void run() override;
    void compute();
    void publish (const Analysis&) noexcept;
    ResonanceAnalyser analyser;
    Frame ring, job;
    std::atomic<int> jobState { 0 }; // free, writing, ready, processing
    std::atomic<uint32_t> generation { 1 }, submitted { 0 }, version { 0 }, resultGeneration { 0 };
    static constexpr int valuesSize = 2*spectrumBins+3*maxBands+3;
    std::array<std::atomic<float>, valuesSize> values {};
    int position = 0, filled = 0, sinceJob = 0, interval = 4800;
    bool enabled = false, offline = false;
    uint32_t analysedGeneration = 0;
    uint64_t sampleCounter = 0;
};

// A convex dual-tap fade avoids read-head jumps. Targets arriving mid-fade
// are queued until its endpoint, so automation cannot restart the ramp.
class MovingTap
{
public:
    void prepare (double rate, float initial);
    void target (float samples) noexcept { wanted = samples; }
    float read (const std::vector<float>&, size_t writePosition) noexcept;
    float current() const noexcept { return active; }
private:
    float active = 1, next = 1, wanted = 1, progress = 1, step = 1;
};
class ReverbEngine
{
public:
    void prepare (double rate, bool worker = true);
    void release() { analysis.release(); }
    void process (juce::AudioBuffer<float>&, const Settings&, bool hostBypass = false);
    bool readAnalysis (Analysis& a) const noexcept { return analysis.read (a); }
    float reductionDb() const noexcept { return displayedReduction.load(); }
    uint32_t analysisJobs() const noexcept { return analysis.jobs(); }
    float peakLevel() const noexcept { return displayedPeak.load(); }
private:
    friend struct ReverbEngineTestAccess;
    struct Filter
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void configure (const Band&, double rate, float amount);
        float process (float) noexcept;
    };
    struct Bank
    {
        std::array<std::array<Filter, maxBands>, 8> filters;
        int count = 0;
        float process (float x, size_t line) noexcept;
    };
    struct Line
    {
        std::vector<float> delay, phaseMemory;
        size_t position = 0, phasePosition = 0;
        MovingTap tap, wetTap;
        float phaseCoefficient = 0, phaseTarget = 0, low = 0, dc = 0;
        float phaseProcess (float) noexcept;
    };
    void update (const Settings&);
    static float phaseCoefficient (float frequency, size_t delay, double rate, size_t line);
    std::array<Line, 8> lines;
    std::array<std::vector<float>, 2> predelay;
    std::array<MovingTap, 2> predelayTap;
    size_t predelayPosition = 0;
    AnalysisService analysis;
    Analysis latest;
    std::array<Bank, 2> banks;
    int activeBank = 0, pendingBank = 1, warm = 0;
    float bankFade = 1, bankStep = 1, previousTame = -1;
    uint32_t appliedRevision = 0;
    bool appliedAuto = false, first = true;
    double sampleRate = 48000;
    float lowCoefficient = 0, dcCoefficient = 0, phaseSmoothing = 0;
    float matrixCos = 1, matrixSin = 0, lastDiffusion = -1, lastPhaseFrequency = -1;
    float lastScatter = -1, projectionCos = 0, projectionSin = 1;
    std::array<juce::SmoothedValue<float>, 8> feedbackGain, injectionGain;
    juce::SmoothedValue<float> mix, outputGain, width, diffusion, phaseMix, bypass, scatterMix;
    std::atomic<float> displayedReduction { 0 }, displayedPeak { 0 };
};
}
