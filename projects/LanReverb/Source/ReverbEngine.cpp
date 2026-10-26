#include "ReverbEngine.h"
#include <cmath>
namespace lan
{
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
constexpr std::array<float,8> ratios { .46375f,.54625f,.66625f,.77125f,.89875f,1.04875f,1.21625f,1.42375f };
constexpr std::array<float,8> tapPattern { -.73f,.37f,-.19f,.89f,-.51f,.13f,.61f,-.97f };
float smooth (float t) noexcept { return t*t*t*(t*(t*6-15)+10); }
float finite (float x) noexcept { return std::isfinite(x) ? std::clamp(x,-16.f,16.f) : 0.f; }
float interpolate (const std::vector<float>& buffer, size_t position, float delay) noexcept
{
    const auto offset = std::clamp(delay,0.f,float(buffer.size()-2));
    const auto whole = size_t(offset); const auto fraction = offset-float(whole);
    const auto a = (position+buffer.size()-whole)%buffer.size();
    const auto b = (a+buffer.size()-1)%buffer.size();
    return buffer[a]+fraction*(buffer[b]-buffer[a]);
}
}
void ResonanceAnalyser::reset (bool preserveSource)
{
    ages = {}; held = {}; decayCount = decayPosition = 0; decayTime = 0;
    if (!preserveSource) { sourceHolds = {}; sourceMemory = {}; previousSample = 0; }
}
Analysis ResonanceAnalyser::analyse (const Frame& frame)
{
    input.fill(0); feedback.fill(0);
    for (int channel = 0; channel < 4; ++channel)
    {
        fftData.fill(0);
        for (int i = 0; i < analysisSize; ++i)
            fftData[size_t(i)] = frame.audio[size_t(channel)][size_t(i)]*float(.5-.5*std::cos(2*pi*i/(analysisSize-1)));
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        auto& power = channel < 2 ? input : feedback;
        for (size_t k = 1; k < power.size(); ++k)
            power[k] += .5*std::pow(4.*fftData[k]/analysisSize,2);
    }
    Analysis result; result.generation = frame.generation;
    const auto binHz = frame.rate/analysisSize;
    for (int row = 0; row < spectrumBins; ++row)
    {
        const auto hz = 30*std::pow(std::min(18000.,frame.rate*.43)/30,row/double(spectrumBins-1));
        const auto halfStep = std::sqrt(std::pow(std::min(18000.,frame.rate*.43)/30,1./(spectrumBins-1)));
        const auto firstBin = std::clamp(int(std::floor(hz/halfStep/binHz)),1,analysisSize/2-1);
        const auto lastBin = std::clamp(int(std::ceil(hz*halfStep/binHz)),firstBin,analysisSize/2-1);
        double inputPeak = 0, feedbackPeak = 0;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        { inputPeak = std::max(inputPeak,input[size_t(bin)]); feedbackPeak = std::max(feedbackPeak,feedback[size_t(bin)]); }
        result.input[size_t(row)] = float(10*std::log10(std::max(1e-12,inputPeak)));
        result.feedback[size_t(row)] = float(10*std::log10(std::max(1e-12,feedbackPeak)));
    }
    struct Candidate { Band band; double priority = 0; };
    std::array<Candidate,maxBands> selected {};
    const auto previousAges = ages; ages.fill(0);
    const auto inputMaximum = *std::max_element(input.begin(),input.end());
    // Source identity is soft memory: only the first 250–650 ms is an absolute
    // veto. Later, measured slow decay can override a remembered source note.
    const auto elapsed = previousSample > 0 && frame.sampleEnd > previousSample
        ? float((frame.sampleEnd-previousSample)/frame.rate) : .1f;
    previousSample = frame.sampleEnd;
    for (auto& hold : sourceHolds) hold = std::max(0.f,hold-elapsed);
    for (auto& memory : sourceMemory) memory = std::max(0.f,memory-elapsed);

    // Regress dB power against real stream time over the last eight frames.
    // A gap (including Auto Off) invalidates slopes rather than inventing a
    // decay rate from stale frames. All of this remains on the analysis worker.
    if (elapsed > .3f) decayCount = decayPosition = 0;
    decayTime += elapsed;
    const auto historySlot = size_t(decayPosition);
    decayTimes[historySlot] = decayTime;
    const auto ceiling = std::min(16000.,frame.rate*.4);
    const auto bandFor = [ceiling] (double hz)
    { return std::clamp(int(std::log(std::max(60.,hz)/60)/std::log(ceiling/60)*decayBands),0,decayBands-1); };
    std::array<double,decayBands> bandPower {};
    for (int k = 1; k < analysisSize/2; ++k)
    {
        const auto power = feedback[size_t(k-1)]+feedback[size_t(k)]+feedback[size_t(k+1)];
        decayHistory[historySlot][size_t(k)] = float(10*std::log10(std::max(1e-14,power)));
        if (k*binHz >= 60 && k*binHz <= ceiling)
            bandPower[size_t(bandFor(k*binHz))] += feedback[size_t(k)];
    }
    for (int band = 0; band < decayBands; ++band)
        bandHistory[historySlot][size_t(band)] = float(10*std::log10(std::max(1e-14,bandPower[size_t(band)])));
    decayPosition = (decayPosition+1)%decayFrames; decayCount = std::min(decayFrames,decayCount+1);
    const auto slope = [&] (const auto& history, int bin)
    {
        double meanTime = 0, meanLevel = 0;
        for (int i = 0; i < decayCount; ++i) { meanTime += decayTimes[size_t(i)]; meanLevel += history[size_t(i)][size_t(bin)]; }
        meanTime /= decayCount; meanLevel /= decayCount;
        double numerator = 0, denominator = 0;
        for (int i = 0; i < decayCount; ++i)
        { const auto t = decayTimes[size_t(i)]-meanTime; numerator += t*(history[size_t(i)][size_t(bin)]-meanLevel); denominator += t*t; }
        return denominator > .01 ? numerator/denominator : 0.;
    };
    const auto maximumBandPower = *std::max_element(bandPower.begin(),bandPower.end());
    const auto decayEvidence = [&] (int bin)
    {
        if (decayCount < 6) return 0.; // at least about half a second of evidence
        std::array<double,decayBands> references {}; int count = 0;
        const auto candidateBand = bandFor(bin*binHz);
        for (int band = 0; band < decayBands; ++band)
        {
            if (std::abs(band-candidateBand) <= 1 || bandPower[size_t(band)] < std::max(1e-10,maximumBandPower*1e-5)) continue;
            const auto rate = slope(bandHistory,band);
            // Include stable bands in the median, but reject fresh rising ones.
            if (rate <= 1) references[size_t(count++)] = rate;
        }
        if (count < 3) return 0.;
        std::nth_element(references.begin(),references.begin()+count/2,references.begin()+count);
        const auto reference = references[size_t(count/2)];
        const auto candidate = slope(decayHistory,bin);
        if (reference > -.6 || candidate > 1 || candidate < reference*.65) return 0.;
        const auto advantage = candidate-reference;
        const auto margin = std::max(.6,std::abs(reference)*.2);
        return std::clamp((advantage-margin)/std::max(1.5,std::abs(reference)*.5),0.,1.);
    };
    for (int k = 1; k < analysisSize/2-14; ++k)
    {
        const auto index = size_t(k);
        if (input[index] < std::max(1e-10,inputMaximum*.000001)
            || input[index] <= input[index-1] || input[index] < input[index+1]) continue;
        std::array<double,18> neighbours {}; int at = 0;
        for (int distance = 4; distance <= 12; ++distance)
        { neighbours[size_t(at++)] = input[size_t(k+distance)]; neighbours[size_t(at++)] = input[size_t(std::max(1,k-distance))]; }
        // A loud neighbouring note must not mask a quieter musical partial.
        std::nth_element(neighbours.begin(),neighbours.begin()+9,neighbours.end());
        const auto prominence = 10*std::log10(input[index]/std::max(1e-12,neighbours[9]));
        if (prominence >= 6)
        {
            sourceHolds[index] = std::max(sourceHolds[index],prominence >= 12
                ? std::clamp(frame.sourceHoldSeconds*.04f,.25f,.65f) : .2f);
            sourceMemory[index] = std::max(sourceMemory[index],std::clamp(frame.sourceHoldSeconds,.3f,45.f));
        }
    }
    std::array<bool,analysisSize/2+1> protectedBins {}, rememberedBins {};
    for (int k = 1; k < analysisSize/2; ++k)
        if (sourceMemory[size_t(k)] > 0)
        {
            const auto radius = std::max(2,int(std::ceil(k*.02)));
            for (int bin = std::max(1,k-radius); bin <= std::min(analysisSize/2,k+radius); ++bin)
            {
                rememberedBins[size_t(bin)] = true;
                if (sourceHolds[size_t(k)] > 0) protectedBins[size_t(bin)] = true;
            }
        }
    const auto overlaps = [&] (const auto& mask, double frequency, double bandwidth)
    {
        const auto firstBin = std::clamp(int(std::floor((frequency-bandwidth)/binHz)),1,analysisSize/2);
        const auto lastBin = std::clamp(int(std::ceil((frequency+bandwidth)/binHz)),firstBin,analysisSize/2);
        for (int bin = firstBin; bin <= lastBin; ++bin) if (mask[size_t(bin)]) return true;
        return false;
    };
    for (int k = 2; k < analysisSize/2-14; ++k)
    {
        const auto hz = k*binHz; const auto index = size_t(k);
        if (hz < 60 || hz > std::min(16000.,frame.rate*.4) || feedback[index] < 1e-9
            || feedback[index] <= feedback[index-1] || feedback[index] < feedback[index+1]) continue;
        double local = 0, inputLocal = 0;
        for (int distance = 4; distance <= 12; ++distance)
        {
            local += feedback[size_t(k+distance)]+feedback[size_t(std::max(1,k-distance))];
            inputLocal += input[size_t(k+distance)]+input[size_t(std::max(1,k-distance))];
        }
        local /= 18; inputLocal /= 18;
        const auto prominence = std::clamp(10*std::log10(feedback[index]/std::max(1e-12,local)),0.,60.);
        const auto inputPeak = std::max({input[index-1],input[index],input[index+1]});
        const auto protection = inputPeak > std::max(1e-9,inputMaximum*.0001)
            ? std::clamp(10*std::log10(inputPeak/std::max(1e-12,inputLocal)),0.,60.) : 0.;
        const auto bandwidth = float(std::max(binHz*2.5,hz/14));
        const auto slowDecay = decayEvidence(k);
        const auto excess = prominence-protection;
        if (overlaps(protectedBins,hz,bandwidth) || excess < (slowDecay > 0 ? 6 : 8)
            || (overlaps(rememberedBins,hz,bandwidth) && slowDecay <= 0)) continue;
        ages[index] = std::min(2.f,std::min(elapsed,.15f)+std::max({previousAges[index-1],previousAges[index],previousAges[index+1]}));
        if (ages[index] < (slowDecay > 0 ? .18f : .25f)) continue;
        // Three separated, attenuation-only bands; FFT resolution limits their
        // minimum width. Preserve original input peaks while the source plays.
        Candidate candidate {{float(hz),bandwidth,-float(std::min(18.,(excess-4)*.9+slowDecay*6))},
                             (excess+slowDecay*12)*std::sqrt(feedback[index])};
        bool duplicate = false;
        for (const auto& existing : selected)
            if (existing.priority > 0 && std::abs(existing.band.frequency-hz) < 2*bandwidth) duplicate = true;
        if (duplicate || candidate.priority <= selected.back().priority) continue;
        int at = maxBands-1;
        while (at > 0 && selected[size_t(at-1)].priority < candidate.priority)
        { selected[size_t(at)] = selected[size_t(at-1)]; --at; }
        selected[size_t(at)] = candidate;
    }
    for (const auto& c : selected) if (c.priority > 0) result.bands[size_t(result.count++)] = c.band;
    // A slow release prevents the notch from disappearing as soon as it works.
    for (int i = 0; i < held.count; ++i)
    {
        auto band = held.bands[size_t(i)];
        if (overlaps(protectedBins,band.frequency,band.width)) continue; // revoke stale cuts on a new note
        bool found = false;
        for (int j = 0; j < result.count; ++j)
            if (std::abs(result.bands[size_t(j)].frequency-band.frequency) < band.width)
            { result.bands[size_t(j)].gainDb = std::min(result.bands[size_t(j)].gainDb,band.gainDb*std::exp(-elapsed/1.2f)); found = true; }
        band.gainDb *= std::exp(-elapsed/1.2f);
        if (!found && result.count < maxBands && band.gainDb < -.2f) result.bands[size_t(result.count++)] = band;
    }
    if (result.count > 0) { result.frequency = result.bands[0].frequency; result.strength = std::clamp(-result.bands[0].gainDb/18,0.f,1.f); }
    held = result; return result;
}
AnalysisService::AnalysisService() : Thread("Lan resonance analysis") {}
AnalysisService::~AnalysisService() { release(); }
void AnalysisService::release() { stopThread(-1); }
void AnalysisService::prepare (double rate, bool worker)
{
    release(); ring = {}; job = {}; ring.rate = job.rate = rate;
    position = filled = sinceJob = 0; sampleCounter = 0; interval = std::max(1,juce::roundToInt(rate*.1));
    enabled = false; offline = !worker; analysedGeneration = 0; analyser.reset();
    // Keep publication versions monotonic across host re-prepares so an open
    // editor cannot mistake a new frame for the previous stream's revision.
    generation.fetch_add(1); submitted.store(0); version.fetch_add(1);
    resultGeneration.store(0); jobState.store(0);
    for (auto& v : values) v.store(0);
    version.fetch_add(1);
    if (worker) startThread(juce::Thread::Priority::low);
}
void AnalysisService::setActive (bool active, float sourceHoldSeconds)
{
    ring.sourceHoldSeconds = sourceHoldSeconds;
    if (enabled == active) return;
    enabled = active; generation.fetch_add(1); filled = sinceJob = position = 0;
}
void AnalysisService::push (float left, float right, float wetLeft, float wetRight) noexcept
{
    ++sampleCounter;
    if (!enabled) return;
    const auto p = size_t(position);
    ring.audio[0][p] = left; ring.audio[1][p] = right; ring.audio[2][p] = wetLeft; ring.audio[3][p] = wetRight;
    position = (position+1)%analysisSize; filled = std::min(analysisSize,filled+1); ++sinceJob;
    if (filled < analysisSize || sinceJob < interval) return;
    int expected = 0;
    if (!jobState.compare_exchange_strong(expected,1,std::memory_order_acquire)) return;
    for (size_t ch = 0; ch < 4; ++ch)
        for (int i = 0; i < analysisSize; ++i) job.audio[ch][size_t(i)] = ring.audio[ch][size_t((position+i)%analysisSize)];
    job.generation = generation.load(); job.rate = ring.rate; job.sourceHoldSeconds = ring.sourceHoldSeconds; job.sampleEnd = sampleCounter;
    sinceJob = 0; submitted.fetch_add(1); jobState.store(2,std::memory_order_release);
}
void AnalysisService::compute()
{
    int expected = 2;
    if (!jobState.compare_exchange_strong(expected,3,std::memory_order_acquire)) return;
    if (analysedGeneration != job.generation) { analyser.reset(true); analysedGeneration = job.generation; }
    publish(analyser.analyse(job)); jobState.store(0,std::memory_order_release);
}
void AnalysisService::finishBlock() { if (offline) compute(); }
void AnalysisService::run()
{
    while (!threadShouldExit())
        if (jobState.load(std::memory_order_acquire) == 2) compute(); else wait(10);
}
void AnalysisService::publish (const Analysis& a) noexcept
{
    version.fetch_add(1);
    int at = 0;
    for (float v : a.input) values[size_t(at++)].store(v);
    for (float v : a.feedback) values[size_t(at++)].store(v);
    for (auto b : a.bands) { values[size_t(at++)].store(b.frequency); values[size_t(at++)].store(b.width); values[size_t(at++)].store(b.gainDb); }
    values[size_t(at++)].store(float(a.count)); values[size_t(at++)].store(a.strength); values[size_t(at)].store(a.frequency);
    resultGeneration.store(a.generation); version.fetch_add(1);
}
bool AnalysisService::read (Analysis& a) const noexcept
{
    const auto before = version.load(); if (before == 0 || (before&1u)) return false;
    Analysis copy; int at = 0;
    for (auto& v : copy.input) v = values[size_t(at++)].load();
    for (auto& v : copy.feedback) v = values[size_t(at++)].load();
    for (auto& b : copy.bands) { b.frequency = values[size_t(at++)].load(); b.width = values[size_t(at++)].load(); b.gainDb = values[size_t(at++)].load(); }
    copy.count = int(values[size_t(at++)].load()); copy.strength = values[size_t(at++)].load(); copy.frequency = values[size_t(at)].load();
    copy.generation = resultGeneration.load(); copy.revision = before;
    if (before != version.load() || copy.generation != generation.load()) return false;
    a = copy; return true;
}
void MovingTap::prepare (double rate, float initial)
{ active = next = wanted = initial; progress = 1; step = float(1/(rate*.05)); }
float MovingTap::read (const std::vector<float>& buffer, size_t position) noexcept
{
    if (progress >= 1 && std::abs(wanted-active) > .1f) { next = wanted; progress = 0; }
    const auto a = interpolate(buffer,position,active);
    if (progress >= 1) return a;
    const auto b = interpolate(buffer,position,next); const auto out = a+smooth(progress)*(b-a);
    progress = std::min(1.f,progress+step); if (progress >= 1) active = next;
    return out;
}
void ReverbEngine::Filter::configure (const Band& band, double rate, float amount)
{
    const auto omega = 2*pi*std::clamp(double(band.frequency),30.,rate*.45)/rate;
    const auto gain = std::pow(10.,band.gainDb*amount/40.);
    const auto alpha = std::sin(omega)/(2*std::max(1.,double(band.frequency)/std::max(1.f,band.width)));
    const auto denominator = 1+alpha/gain;
    b0 = (1+alpha*gain)/denominator; b1 = -2*std::cos(omega)/denominator; b2 = (1-alpha*gain)/denominator;
    a1 = b1; a2 = (1-alpha/gain)/denominator; z1 = z2 = 0;
}
float ReverbEngine::Filter::process (float x) noexcept
{
    const auto y = b0*x+z1; z1 = b1*x-a1*y+z2; z2 = b2*x-a2*y; return float(y);
}
float ReverbEngine::Bank::process (float x, size_t line) noexcept
{ for (int i = 0; i < count; ++i) x = filters[line][size_t(i)].process(x); return x; }
float ReverbEngine::Line::phaseProcess (float x) noexcept
{
    const auto state = phaseMemory[phasePosition];
    const auto b = std::sqrt(std::max(0.f,1-phaseCoefficient*phaseCoefficient));
    const auto y = phaseCoefficient*x+b*state;
    // An orthogonal scattering junction preserves x²+state² even while its
    // coefficient changes; ordinary modulated all-pass equations do not.
    phaseMemory[phasePosition] = b*x-phaseCoefficient*state;
    phasePosition = (phasePosition+1)%phaseMemory.size(); return y;
}
float ReverbEngine::phaseCoefficient (float hz, size_t delay, double rate, size_t line)
{
    const auto z = std::polar(1.,-2*pi*hz*delay/rate);
    const auto desired = std::arg(z)+(line%2 == 0 ? 1 : -1)*pi*.65;
    double best = 10; float coefficient = 0;
    for (int i = -7; i <= 7; ++i)
    {
        const auto a = i*.1; const auto h = (a+z)/(1.+a*z);
        const auto error = std::abs(std::remainder(std::arg(h)-desired,2*pi));
        if (error < best) { best = error; coefficient = float(a); }
    }
    return coefficient;
}
void ReverbEngine::prepare (double rate, bool worker)
{
    sampleRate = std::isfinite(rate) && rate >= 8000 ? rate : 48000;
    analysis.prepare(sampleRate,worker); latest = {}; banks = {}; activeBank = 0; pendingBank = 1;
    warm = 0; bankFade = 1; bankStep = float(1/(sampleRate*.08)); appliedRevision = 0; previousTame = -1; appliedAuto = false; first = true;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto& line = lines[i]; line = {};
        // Reserve the longest supported synchronized note, including tap steering,
        // during prepare; tempo automation never reallocates on the callback.
        const auto capacitySeconds = maximumTimeMs*.001*std::max(1.f,ratios[i])*1.4;
        line.delay.assign(size_t(std::ceil(sampleRate*capacitySeconds))+4,0);
        line.phaseMemory.assign(size_t(std::max(1,juce::roundToInt(sampleRate*(.0017+i*.00083)))),0);
        line.tap.prepare(sampleRate,float(sampleRate*.09*ratios[i]));
        line.wetTap.prepare(sampleRate,float(sampleRate*.09*ratios[i]));
        feedbackGain[i].reset(sampleRate,.05); injectionGain[i].reset(sampleRate,.05);
    }
    for (size_t ch = 0; ch < 2; ++ch)
    { predelay[ch].assign(size_t(std::ceil(sampleRate*.25))+4,0); predelayTap[ch].prepare(sampleRate,float(sampleRate*.02)); }
    predelayPosition = 0;
    for (auto* control : {&mix,&outputGain,&width,&diffusion,&phaseMix,&bypass,&scatterMix}) control->reset(sampleRate,.02);
    scatterMix.reset(sampleRate,.05);
    lowCoefficient = float(1-std::exp(-2*pi*std::min(16000.,sampleRate*.4)/sampleRate));
    dcCoefficient = float(1-std::exp(-2*pi*18/sampleRate)); phaseSmoothing = float(1-std::exp(-1/(sampleRate*.1)));
    displayedReduction.store(0); displayedPeak.store(0); lastDiffusion = lastPhaseFrequency = lastScatter = -1;
}
void ReverbEngine::update (const Settings& s)
{
    const auto active = s.automatic && (s.tame > .001f || s.phase*s.scatter > .001f);
    analysis.setActive(active,std::clamp(s.decaySeconds,.3f,40.f)+1.25f);
    Analysis next;
    if (active && analysis.read(next)) latest = next;
    if (!active) latest = {};
    const auto set = [this] (auto& smoother, float value)
    { if (first) smoother.setCurrentAndTargetValue(value); else smoother.setTargetValue(value); };
    set(mix,std::clamp(s.mix,0.f,1.f)); set(outputGain,juce::Decibels::decibelsToGain(std::clamp(s.outputDb,-24.f,12.f)));
    // Width is a mid/side rotation whose normalization keeps maximum singular
    // gain <=1; 200% cannot make the internal feedback loop hotter.
    set(width,std::clamp(s.width,0.f,2.f)); set(diffusion,std::clamp(s.diffusion,0.f,1.f)); set(phaseMix,std::clamp(s.phase,0.f,1.f));
    set(scatterMix,s.scatter);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto& line = lines[i];
        const auto autoSpread = active ? latest.strength*s.phase*.09f*std::sin(latest.frequency*.017f+float(i)*2.399963f) : 0.f;
        const auto wetSeconds = std::clamp(s.timeMs,1.f,maximumTimeMs)*.001f*(1+s.scatter*(ratios[i]-1));
        const auto seconds = wetSeconds
            *(1+std::clamp(s.tap,-1.f,1.f)*.3f*(1+s.scatter*(tapPattern[i]-1))+s.scatter*autoSpread);
        if (first) line.wetTap.prepare(sampleRate,float(sampleRate)*wetSeconds);
        else line.wetTap.target(float(sampleRate)*wetSeconds);
        const auto delaySamples = float(sampleRate)*seconds;
        if (first) line.tap.prepare(sampleRate,delaySamples); else line.tap.target(delaySamples);
        // Conservative per-line RT60 gains, always strictly below unity.
        set(feedbackGain[i],std::min(.9995f,float(std::pow(10.,-3*(seconds+s.scatter*line.phaseMemory.size()/sampleRate)/std::clamp(s.decaySeconds,.3f,40.f)))));
        const auto sourceGain = std::pow(10.,-3*(wetSeconds+s.scatter*line.phaseMemory.size()/sampleRate)/std::clamp(s.decaySeconds,.3f,40.f));
        set(injectionGain[i],.35355339f*float(std::sqrt(std::max(0.,1-sourceGain*sourceGain))));
        if (first || !juce::exactlyEqual(lastPhaseFrequency,latest.frequency))
            line.phaseTarget = latest.frequency > 0 && active
                ? phaseCoefficient(latest.frequency,line.phaseMemory.size(),sampleRate,i) : tapPattern[i]*.45f;
    }
    for (size_t ch = 0; ch < 2; ++ch)
    {
        const auto samples = std::clamp(s.predelayMs,0.f,200.f)*.001f*float(sampleRate);
        if (first) predelayTap[ch].prepare(sampleRate,samples); else predelayTap[ch].target(samples);
    }
    if (bankFade >= 1 && warm == 0 && (latest.revision != appliedRevision || active != appliedAuto || std::abs(s.tame-previousTame) > .005f))
    {
        pendingBank = 1-activeBank; auto& bank = banks[size_t(pendingBank)]; bank = {};
        bank.count = active && s.tame > 0 ? latest.count : 0;
        float reduction = 0;
        for (int k = 0; k < bank.count; ++k)
        {
            for (size_t line = 0; line < 8; ++line) bank.filters[line][size_t(k)].configure(latest.bands[size_t(k)],sampleRate,s.tame);
            reduction = std::max(reduction,-latest.bands[size_t(k)].gainDb*s.tame);
        }
        displayedReduction.store(reduction); warm = juce::roundToInt(sampleRate*.02); bankFade = 0;
        appliedRevision = latest.revision; appliedAuto = active; previousTame = s.tame;
    }
    lastPhaseFrequency = latest.frequency; first = false;
}
void ReverbEngine::process (juce::AudioBuffer<float>& buffer, const Settings& settings, bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;
    if (lines[0].delay.empty() || buffer.getNumChannels() == 0) return;
    auto safeSettings = settings;
    const Settings defaults;
    for (auto pair : {std::pair<float*,float>{&safeSettings.timeMs,defaults.timeMs}, {&safeSettings.decaySeconds,defaults.decaySeconds},
                      {&safeSettings.predelayMs,defaults.predelayMs}, {&safeSettings.diffusion,defaults.diffusion}, {&safeSettings.width,defaults.width},
                      {&safeSettings.tap,defaults.tap}, {&safeSettings.phase,defaults.phase}, {&safeSettings.tame,defaults.tame},
                      {&safeSettings.mix,defaults.mix}, {&safeSettings.outputDb,defaults.outputDb}, {&safeSettings.scatter,defaults.scatter}})
        if (!std::isfinite(*pair.first)) *pair.first = pair.second;
    safeSettings.scatter = std::clamp(safeSettings.scatter,0.f,1.f);
    safeSettings.phase = std::clamp(safeSettings.phase,0.f,1.f);
    safeSettings.tame = std::clamp(safeSettings.tame,0.f,1.f);
    const bool initial = first; update(safeSettings);
    const auto bypassed = safeSettings.bypass || hostBypass;
    if (initial) bypass.setCurrentAndTargetValue(bypassed ? 1.f : 0.f); else bypass.setTargetValue(bypassed ? 1.f : 0.f);
    auto* left = buffer.getWritePointer(0); auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    float blockPeak = 0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto dryLeft = finite(left[sample]), dryRight = right != nullptr ? finite(right[sample]) : dryLeft;
        predelay[0][predelayPosition] = dryLeft; predelay[1][predelayPosition] = dryRight;
        const auto inputLeft = predelayTap[0].read(predelay[0],predelayPosition);
        const auto inputRight = predelayTap[1].read(predelay[1],predelayPosition);
        predelayPosition = (predelayPosition+1)%predelay[0].size();
        std::array<float,8> raw {}, nodes {}, scattered {};
        float wetLeft = 0, wetRight = 0, probeLeft = 0, probeRight = 0;
        const auto scatterAmount = scatterMix.getNextValue();
        if (!juce::exactlyEqual(scatterAmount,lastScatter))
        {
            // Rotate between the input-aligned delay pickups and the reverb
            // pickups. All four vectors are orthogonal, avoiding cancellation
            // when the eight delays coincide at Scatter zero.
            projectionCos = scatterAmount >= 1 ? 0.f : std::cos(scatterAmount*float(pi/2));
            projectionSin = scatterAmount <= 0 ? 0.f : std::sin(scatterAmount*float(pi/2));
            lastScatter = scatterAmount;
        }
        const auto phaseAmount = phaseMix.getNextValue();
        const auto transition = smooth(bankFade);
        for (size_t line = 0; line < 8; ++line)
        {
            auto& memory = lines[line];
            const auto wetSample = memory.wetTap.read(memory.delay,memory.position);
            const auto x = memory.tap.read(memory.delay,memory.position);
            raw[line] = x;
            auto value = banks[size_t(activeBank)].process(x,line);
            if (bankFade < 1 || warm > 0)
            {
                const auto pending = banks[size_t(pendingBank)].process(x,line);
                if (warm == 0) value += transition*(pending-value);
            }
            memory.phaseCoefficient += phaseSmoothing*(phaseAmount*memory.phaseTarget-memory.phaseCoefficient);
            const auto phased = memory.phaseProcess(value);
            value += scatterAmount*(phased-value); // exact phase bypass at the delay endpoint

            // Fixed bass/DC and ultrasonic damping remain below unity; they
            // also keep very long tails away from numerical/DC accumulation.
            memory.dc += dcCoefficient*(value-memory.dc); value -= memory.dc;
            memory.low += lowCoefficient*(value-memory.low); nodes[line] = memory.low;
            const auto signLeft = (line&2u) != 0 ? -1.f : 1.f;
            const auto signRight = (line&4u) != 0 ? -1.f : 1.f;
            const auto pickupLeft = projectionCos+projectionSin*signLeft;
            const auto pickupRight = projectionCos*((line&1u) != 0 ? -1.f : 1.f)+projectionSin*signRight;
            wetLeft += .35355339f*wetSample*pickupLeft;
            wetRight += .35355339f*wetSample*pickupRight;
            probeLeft += .35355339f*raw[line]*pickupLeft; probeRight += .35355339f*raw[line]*pickupRight;
        }
        if (warm > 0) --warm;
        else if (bankFade < 1)
        { bankFade = std::min(1.f,bankFade+bankStep); if (bankFade >= 1) activeBank = pendingBank; }
        scattered = nodes;
        // Three stages of Givens butterflies are orthogonal at every
        // diffusion setting, including while it moves. A blend of identity and
        // a Hadamard matrix would shorten long tails at intermediate settings.
        // A nonzero density floor makes full Scatter a reverb even when the
        // finer Diffusion control is at minimum; zero Scatter is identity.
        const auto diffuse = scatterAmount*(.25f+.75f*diffusion.getNextValue());
        if (!juce::exactlyEqual(diffuse,lastDiffusion))
        { matrixCos = std::cos(diffuse*float(pi/4)); matrixSin = std::sin(diffuse*float(pi/4)); lastDiffusion = diffuse; }
        for (size_t stride = 1; stride < 8; stride *= 2)
            for (size_t base = 0; base < 8; base += 2*stride)
                for (size_t i = 0; i < stride; ++i)
                { const auto a = scattered[base+i], b = scattered[base+i+stride]; scattered[base+i] = matrixCos*a+matrixSin*b; scattered[base+i+stride] = -matrixSin*a+matrixCos*b; }
        for (size_t line = 0; line < 8; ++line)
        {
            const auto feedback = scattered[line];
            const auto gain = feedbackGain[line].getNextValue();
            // The source feed is independent of automatic feedback tap/gain
            // changes; only recirculated audio receives EQ and phase processing.
            const auto injection = injectionGain[line].getNextValue()
                *(inputLeft+((line&1u) != 0 ? -inputRight : inputRight));
            auto value = gain*feedback+injection;
            // Last-resort smooth bounded feedback write, idle at normal levels.
            if (std::abs(value) > 2) value = std::copysign(2+std::tanh(std::abs(value)-2),value);
            auto& memory = lines[line]; memory.delay[memory.position] = std::isfinite(value) ? value : 0;
            memory.position = (memory.position+1)%memory.delay.size();
        }
        analysis.push(inputLeft,inputRight,probeLeft,probeRight);
        const auto stereoWidth = width.getNextValue();
        const auto normalization = 1/std::max(1.f,stereoWidth);
        const auto mid = .5f*(wetLeft+wetRight)*normalization, side = .5f*(wetLeft-wetRight)*stereoWidth*normalization;
        const auto wet = mix.getNextValue(), gain = outputGain.getNextValue(), bypassAmount = bypass.getNextValue();
        const auto processedLeft = ((1-wet)*dryLeft+wet*(mid+side))*gain;
        const auto processedRight = ((1-wet)*dryRight+wet*(mid-side))*gain;
        const auto channelLeft = right != nullptr ? processedLeft : .5f*(processedLeft+processedRight);
        left[sample] = channelLeft+bypassAmount*(dryLeft-channelLeft);
        if (right != nullptr) right[sample] = processedRight+bypassAmount*(dryRight-processedRight);
        blockPeak = std::max({blockPeak,std::abs(left[sample]),right != nullptr ? std::abs(right[sample]) : 0.f});
    }
    analysis.finishBlock(); displayedPeak.store(blockPeak);
}
}
