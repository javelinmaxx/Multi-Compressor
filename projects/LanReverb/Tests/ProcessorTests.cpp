#include "../Source/PluginEditor.h"
#include "../../StagePluginTests.h"
#include <filesystem>
#include <random>
using namespace stageTests;
namespace lan
{
struct ReverbEngineTestAccess
{
    static void forceCorrection (ReverbEngine& engine)
    {
        engine.latest = {}; engine.latest.count = maxBands; engine.latest.frequency = 1100; engine.latest.strength = 1; engine.latest.revision = 2;
        engine.latest.bands = {{{1100,80,-9},{3300,240,-12},{7200,520,-18}}};
        for (auto& bank : engine.banks)
        {
            bank.count = maxBands;
            for (auto& line : bank.filters) for (int i = 0; i < maxBands; ++i) line[size_t(i)].configure(engine.latest.bands[size_t(i)],engine.sampleRate,1);
        }
        engine.appliedRevision = 2; engine.appliedAuto = true; engine.previousTame = 1;
        engine.warm = 0; engine.bankFade = 1;
    }
    static std::complex<double> response (const ReverbEngine::BandPhase& section, double hz, double rate, bool target = true)
    {
        const auto k1 = target ? section.target1 : section.k1, k2 = target ? section.target2 : section.k2;
        const auto z = std::polar(1.,-2*juce::MathConstants<double>::pi*hz/rate);
        const auto a1 = k1*(1+k2);
        return (k2+a1*z+z*z)/(1.+a1*z+k2*z*z);
    }
    static void multiPhaseTests (const juce::File& directory)
    {
        constexpr auto pi = juce::MathConstants<double>::pi;
        const std::array<Band,maxBands> bands {{{300,300.f/14,-18},{1400,100,-12},{6500,6500.f/14,-9}}};
        std::ofstream report(directory.getChildFile("multiband-phase.csv").getFullPathName().toStdString());
        report << "rate,line,frequency_hz,phase_0_deg,phase_1_deg,phase_2_deg,total_phase_deg,magnitude_db,total_delay_bound_ms\n";
        for (double rate : {44100.,48000.,96000.})
        {
            auto frame = std::make_unique<Frame>(); frame->rate = rate;
            for (int i = 0; i < analysisSize; ++i)
            {
                double x = 0;
                for (size_t band = 0; band < bands.size(); ++band)
                    x += .3*std::pow(.3,double(band))*std::sin(2*pi*bands[band].frequency*i/rate);
                frame->audio[2][size_t(i)] = float(x); frame->audio[3][size_t(i)] = -float(x);
            }
            ResonanceAnalyser analyser; Analysis detected;
            for (int i = 0; i < 6; ++i) detected = analyser.analyse(*frame);
            check(detected.count == 3,"production analyser detects three simultaneous feedback resonances");
            ReverbEngine engine; engine.prepare(rate,false); engine.latest = detected; engine.updatePhaseBands(true,1);
            for (const auto& section : engine.lines[0].phaseBands)
                check(section.target2 > 0,"all three detected feedback resonances engage automatic phase sections");
            engine.latest.count = maxBands;
            engine.latest.bands = bands; engine.latest.frequency = bands[0].frequency; engine.latest.revision = 1;
            engine.updatePhaseBands(true,1);
            for (size_t line = 0; line < 2; ++line)
            {
                auto& sections = engine.lines[line].phaseBands;
                for (size_t i = 0; i < bands.size(); ++i)
                {
                    const auto h = response(sections[i],bands[i].frequency,rate);
                    const auto baseline = std::polar(1.,-4*pi*bands[i].frequency/rate);
                    check(std::abs(h-baseline) > .005,
                          "all three tracked frequencies receive distinct phase correction beyond the baseline");
                    check(std::abs(std::abs(h)-1) < 1e-9,"each fixed phase section has unity gain at its target frequency");
                }
                // Render the actual three-section cascade after settling to its
                // targets, and compare its impulse Fourier response with H(z).
                for (int i = 0; i < juce::roundToInt(rate*transitionSeconds); ++i)
                    for (auto& section : sections) section.process(0);
                for (const auto& section : sections)
                    check(section.remaining == 0,"phase changes finish at the end of the 250 ms fade");
                std::array<std::complex<double>,3> measured {}; double outputEnergy = 0;
                for (int i = 0; i < 32768; ++i)
                {
                    float x = i == 0 ? 1.f : 0.f;
                    for (auto& section : sections) x = section.process(x);
                    outputEnergy += x*x;
                    for (size_t band = 0; band < bands.size(); ++band)
                        measured[band] += double(x)*std::polar(1.,-2*pi*bands[band].frequency*i/rate);
                }
                check(std::abs(outputEnergy-1) < 1e-5,"fixed three-band lattice cascade preserves impulse energy");
                for (size_t band = 0; band < bands.size(); ++band)
                {
                    std::complex<double> expected {1,0};
                    for (const auto& section : sections) expected *= response(section,bands[band].frequency,rate);
                    check(std::abs(expected-measured[band]) < 2e-5,"rendered multiband phase matches its analytic all-pass response");
                }
                for (int point = 0; point < 180; ++point)
                {
                    const auto hz = 60*std::pow(16000./60,point/179.); std::complex<double> h {1,0};
                    report << rate << ',' << line << ',' << hz;
                    for (const auto& section : sections)
                    { const auto band = response(section,hz,rate); h *= band; report << ',' << std::arg(band)*180/pi; }
                    check(std::abs(std::abs(h)-1) < 1e-8,"multiband phase does not add static EQ gain or loss");
                    constexpr double delta = .01;
                    std::complex<double> plus {1,0}, minus {1,0};
                    for (const auto& section : sections)
                    { plus *= response(section,hz+delta,rate); minus *= response(section,hz-delta,rate); }
                    const auto bandDelay = -std::arg(plus/minus)/(4*pi*delta);
                    const auto manualDelay = engine.lines[line].phaseMemory.size()/rate
                        *(1+std::abs(engine.lines[line].phaseTarget))/(1-std::abs(engine.lines[line].phaseTarget));
                    check(bandDelay+manualDelay+autoTapBudgetSeconds <= phaseDelayLimitSeconds+1e-7,
                          "combined phase branch plus automatic tap steering stays within five milliseconds");
                    report << ',' << std::arg(h)*180/pi << ',' << 20*std::log10(std::abs(h)) << ',' << (bandDelay+manualDelay+autoTapBudgetSeconds)*1000 << '\n';
                }
            }
            const auto before = engine.lines[0].phaseBands;
            std::swap(engine.latest.bands[0],engine.latest.bands[2]); ++engine.latest.revision; engine.first = false;
            engine.updatePhaseBands(true,1);
            for (size_t i = 0; i < bands.size(); ++i)
                check(std::abs(engine.lines[0].phaseBands[i].target1-before[i].target1) < 1e-12
                      && std::abs(engine.lines[0].phaseBands[i].target2-before[i].target2) < 1e-12,
                      "priority reordering preserves phase section identity");
            engine.latest.bands[0].frequency = 7200; ++engine.latest.revision; engine.updatePhaseBands(true,1);
            check(std::abs(engine.lines[0].phaseBands[2].target1-before[2].target1) > 1e-4,
                  "a secondary resonance updates phase even when the primary frequency is unchanged");
            engine.updatePhaseBands(false,1);
            for (const auto& line : engine.lines) for (const auto& band : line.phaseBands)
                check(std::abs(band.target2) < 1e-12 && std::abs(band.target1) < 1e-12,"Auto Off releases every automatic phase section toward the pure-delay baseline");
            engine.updatePhaseBands(true,0);
            for (const auto& line : engine.lines) for (const auto& band : line.phaseBands)
                check(std::abs(band.target2) < 1e-12 && std::abs(band.target1) < 1e-12,"zero Phase releases all automatic bands to their pure-delay baseline");
            for (float time : {20.f,18000.f})
            {
                Settings settings; settings.timeMs = time; settings.phase = 1; settings.tame = 0;
                engine.first = true; engine.latest.strength = 1; engine.update(settings);
                for (const auto& line : engine.lines)
                    check(std::abs(line.tap.current()-line.wetTap.current()) <= autoTapBudgetSeconds*rate+.2,
                          "automatic tap offsets remain below one millisecond even with long synced notes");
            }
            // Sweep targets on populated lattice states. Orthogonal junctions
            // must not create energy even during coefficient changes or removal.
            std::array<ReverbEngine::BandPhase,maxBands> moving;
            double inputEnergy = 0, outputEnergy = 0;
            for (int i = 0; i < 48000; ++i)
            {
                if (i%137 == 0)
                    for (size_t band = 0; band < moving.size(); ++band)
                    {
                        auto b = bands[band]; b.frequency *= float(.6+(i%11)*.08);
                        moving[band].target(b,rate,(i%7)*.16,band%2 == 0 ? 1 : -1);
                    }
                const auto input = i < 12000 ? float(.15*std::sin(2*pi*997*i/rate)) : 0.f;
                auto x = input; for (auto& section : moving) x = section.process(x);
                check(std::isfinite(x),"moving multiband phase remains finite on a populated tail");
                for (const auto& section : moving)
                {
                    const auto r = std::sqrt(section.k2);
                    const auto delayBound = 2*(1+r)/(1-r)/rate;
                    check(delayBound <= bandPhaseBudgetSeconds+1e-9,"pole-delay bound stays below the per-band budget throughout coefficient fades");
                }
                inputEnergy += input*input; outputEnergy += x*x;
                check(outputEnergy <= inputEnergy+1e-4,"modulated normalized lattices never generate feedback energy");
            }
        }
    }
    static void checkTapCapacity (const ReverbEngine& engine)
    {
        for (const auto& line : engine.lines)
            check(line.tap.current() <= float(line.delay.size()-2) && line.wetTap.current() <= float(line.delay.size()-2),
                  "long synchronized wet and steered feedback taps fit their prepared buffers");
    }
    static void seedFeedback (ReverbEngine& engine)
    {
        // A controlled in-loop disturbance, absent from the source spectrum.
        for (size_t line = 0; line < engine.lines.size(); ++line)
        {
            auto& memory = engine.lines[line];
            const auto sign = (line&2u) != 0 ? -1.f : 1.f;
            for (size_t i = 0; i < memory.delay.size(); ++i)
            {
                const auto ago = (memory.position+memory.delay.size()-i)%memory.delay.size();
                memory.delay[i] += sign*float(.08*std::sin(-2*juce::MathConstants<double>::pi*1800*ago/engine.sampleRate));
            }
        }
    }
};
}
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
struct Audio { std::vector<float> left, right; float maxReduction = 0; uint32_t jobs = 0; };
Audio render (lan::Settings settings, double seconds, double rate, int signal, int blockSize = 128, bool automate = false, bool tapSwitch = false, bool rogueFeedback = false)
{
    lan::ReverbEngine engine; engine.prepare(rate,false);
    Audio out; const auto count = juce::roundToInt(seconds*rate); out.left.reserve(size_t(count)); out.right.reserve(size_t(count));
    std::mt19937 rng(72463); std::uniform_real_distribution<float> noise(-.25f,.25f);
    bool seeded = false;
    for (int at = 0; at < count; at += blockSize)
    {
        const auto n = std::min(blockSize,count-at); juce::AudioBuffer<float> b(2,n);
        for (int i = 0; i < n; ++i)
        {
            const auto t = (at+i)/rate; float x = 0;
            if (signal == 0 && at+i == 0) x = .8f;
            if (signal == 1 && t < 1) x = float(.14*std::sin(2*pi*220*t)+.1*std::sin(2*pi*997*t));
            if (signal == 2 && t < 2) x = noise(rng);
            b.setSample(0,i,x); b.setSample(1,i,x);
        }
        if (tapSwitch && at >= 2*rate) settings.tap = .7f;
        if (automate)
        {
            const auto step = (at/blockSize)/17;
            settings.tap = step%2 == 0 ? -1.f : 1.f; settings.timeMs = step%3 == 0 ? 20.f : 500.f;
            settings.scatter = float(step%5)*.25f;
            settings.phase = step%2 == 0 ? 0.f : 1.f; settings.diffusion = float(step%5)*.25f;
            settings.mix = 1; settings.decaySeconds = 40;
        }
        if (rogueFeedback && !seeded && at >= rate) { lan::ReverbEngineTestAccess::seedFeedback(engine); seeded = true; }
        engine.process(b,settings);
        out.maxReduction = std::max(out.maxReduction,engine.reductionDb());
        for (int i = 0; i < n; ++i)
        {
            check(std::isfinite(b.getSample(0,i)) && std::isfinite(b.getSample(1,i)),"feedback output remains finite");
            check(std::abs(b.getSample(0,i)) < 10 && std::abs(b.getSample(1,i)) < 10,"feedback writes prevent runaway output");
            out.left.push_back(b.getSample(0,i)); out.right.push_back(b.getSample(1,i));
        }
    }
    out.jobs = engine.analysisJobs(); return out;
}
double energy (const Audio& a, double first, double last, double rate)
{
    double sum = 0; const auto lo = size_t(first*rate), hi = std::min(a.left.size(),size_t(last*rate));
    for (auto i = lo; i < hi; ++i) sum += .5*(a.left[i]*a.left[i]+a.right[i]*a.right[i]);
    return sum/std::max(size_t(1),hi-lo);
}
double ratio (const Audio& a, double first, double last, double rate)
{
    double mid = 0, side = 0;
    for (size_t i = size_t(first*rate); i < std::min(a.left.size(),size_t(last*rate)); ++i)
    { mid += std::pow(a.left[i]+a.right[i],2); side += std::pow(a.left[i]-a.right[i],2); }
    return std::sqrt(side/std::max(mid,1e-30));
}
void writeWav (const juce::File& path, const Audio& audio, double rate, int maxSamples)
{
    path.getParentDirectory().createDirectory(); path.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = path.createOutputStream(); check(stream != nullptr,"audio report stream opens");
    auto writer = format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(16));
    check(writer != nullptr,"audio report writer opens");
    const auto count = std::min(int(audio.left.size()),maxSamples);
    const float* channels[] {audio.left.data(),audio.right.data()}; check(writer->writeFromFloatArrays(channels,2,count),"audio report is written");
}
void analyserTests (const juce::File& directory)
{
    auto frame = std::make_unique<lan::Frame>(); frame->rate = 48000;
    const auto frequency = 85.*48000/lan::analysisSize;
    for (int i = 0; i < lan::analysisSize; ++i)
    {
        const auto sample = float(.2*std::sin(2*pi*frequency*i/48000));
        frame->audio[2][size_t(i)] = sample; frame->audio[3][size_t(i)] = -sample;
    }
    lan::ResonanceAnalyser analyser; lan::Analysis result;
    for (int i = 0; i < 5; ++i) result = analyser.analyse(*frame);
    check(result.count > 0 && std::abs(result.frequency-frequency) < 15,"persistent anti-phase feedback peak is detected without mid cancellation");
    check(result.bands[0].gainDb < -1 && result.bands[0].gainDb >= -18,"resonance correction requests bounded attenuation");
    std::ofstream response(directory.getChildFile("detector.csv").getFullPathName().toStdString());
    response << "time_s,cut_db,frequency_hz\n";
    for (int i = 0; i < 4; ++i) frame->audio[size_t(i)].fill(0);
    for (int i = 0; i < 100; ++i)
    { result = analyser.analyse(*frame); response << i*.1 << ',' << (result.count > 0 ? result.bands[0].gainDb : 0) << ',' << result.frequency << '\n'; }
    check(result.count == 0,"resonance bands release during silence");
    analyser.reset();
    for (int i = 0; i < lan::analysisSize; ++i)
    {
        const auto sample = float(.2*std::sin(2*pi*frequency*i/48000));
        frame->audio[0][size_t(i)] = frame->audio[1][size_t(i)] = sample;
        frame->audio[2][size_t(i)] = sample; frame->audio[3][size_t(i)] = -sample;
    }
    for (int i = 0; i < 8; ++i) result = analyser.analyse(*frame);
    check(result.count == 0,"matching source partial is protected during sustained input");
    analyser.reset(); std::mt19937 rng(4823); std::uniform_real_distribution<float> noise(-.1f,.1f);
    for (int f = 0; f < 20; ++f)
    {
        for (int i = 0; i < lan::analysisSize; ++i)
        { frame->audio[0][size_t(i)] = frame->audio[1][size_t(i)] = 0; frame->audio[2][size_t(i)] = noise(rng); frame->audio[3][size_t(i)] = noise(rng); }
        result = analyser.analyse(*frame);
        check(result.count == 0,"broadband noise does not create persistent spectral cuts");
    }
}
void inputProtectionTests (const juce::File& directory)
{
    std::ofstream results(directory.getChildFile("input-protection.csv").getFullPathName().toStdString());
    results << "rate,stage,time_s,protected_targets,other_targets\n";
    for (double rate : {44100.,48000.,96000.})
    {
        auto frame = std::make_unique<lan::Frame>(); frame->rate = rate; frame->sourceHoldSeconds = 3;
        lan::ResonanceAnalyser analyser;
        std::mt19937 rng(6302); std::uniform_real_distribution<float> noise(-.001f,.001f);
        const auto fill = [&] (bool source, bool rogue)
        {
            for (int i = 0; i < lan::analysisSize; ++i)
            {
                const auto t = i/rate;
                const auto input = float(.2*std::sin(2*pi*220*t)+.003*std::sin(2*pi*997.3*t)+.04*std::sin(2*pi*329.7*t))+noise(rng);
                frame->audio[0][size_t(i)] = source ? input : 0;
                frame->audio[1][size_t(i)] = source ? -input : 0;
                const auto feedback = float(.25*std::sin(2*pi*(997.3+rate/lan::analysisSize*.6)*t)+.1*std::sin(2*pi*329.7*t)+.05*std::sin(2*pi*220*t)
                    +(rogue ? .12*std::sin(2*pi*1800*t) : 0));
                frame->audio[2][size_t(i)] = feedback; frame->audio[3][size_t(i)] = -feedback;
            }
        };
        for (int step = 0; step < 12; ++step)
        {
            fill(true,false); const auto a = analyser.analyse(*frame);
            check(a.count == 0,"a weak detuned source partial over a chord is protected even when feedback is much more tonal");
        }
        for (int step = 0; step < 20; ++step)
        {
            fill(false,true); const auto a = analyser.analyse(*frame); int protectedTargets = 0, other = 0;
            for (int band = 0; band < a.count; ++band)
                if (std::abs(a.bands[size_t(band)].frequency-997.3) < 160 || std::abs(a.bands[size_t(band)].frequency-329.7) < 80 || std::abs(a.bands[size_t(band)].frequency-220) < 80) ++protectedTargets; else ++other;
            check(protectedTargets == 0,"source-note memory protects decaying input peaks after the source stops");
            if (step > 5) check(other > 0 && std::abs(a.frequency-1800) < 120,"unrelated persistent feedback peaks still guide EQ and automatic phase");
            results << rate << ",source_tail," << step*.1 << ',' << protectedTargets << ',' << other << '\n';
        }
        // An existing cut must be revoked when that frequency becomes a source note.
        analyser.reset(); fill(false,true);
        for (int step = 0; step < 5; ++step) analyser.analyse(*frame);
        for (int i = 0; i < lan::analysisSize; ++i)
            frame->audio[0][size_t(i)] = frame->audio[1][size_t(i)] = frame->audio[2][size_t(i)];
        check(analyser.analyse(*frame).count == 0,"new source notes revoke overlapping held EQ and phase targets");
    }
    lan::AnalysisService service; service.prepare(48000,false); uint64_t sample = 0;
    const auto advance = [&] (int count, bool source, bool additional)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto tone = float(.2*std::sin(2*pi*(85.*48000/lan::analysisSize)*sample/48000));
            const auto other = additional ? float(.12*std::sin(2*pi*1800*sample/48000)) : 0.f;
            service.push(source ? tone : 0,source ? -tone : 0,tone+other,-tone-other); ++sample;
        }
        service.finishBlock();
    };
    service.setActive(true,3); advance(4800,true,false);
    service.setActive(false,3); advance(12000,false,false);
    service.setActive(true,3);
    for (int i = 0; i < 5; ++i) advance(4800,false,true);
    lan::Analysis remembered; check(service.read(remembered),"resumed worker publishes a valid protected frame");
    for (int i = 0; i < remembered.count; ++i)
        check(std::abs(remembered.bands[size_t(i)].frequency-996.09375) > 120,"observed source-note protection survives an Auto Off/On cycle");
    service.setActive(false,3); advance(4*48000,false,false); service.setActive(true,3);
    for (int i = 0; i < 5; ++i) advance(4800,false,true);
    bool expired = false; lan::Analysis after; check(service.read(after),"worker resumes after a long inactive interval");
    for (int i = 0; i < after.count; ++i) expired |= std::abs(after.bands[size_t(i)].frequency-996.09375) < 120;
    check(expired,"source protection expires by stream time even while Auto is off");
}
void relativeDecayTests (const juce::File& directory)
{
    std::ofstream report(directory.getChildFile("relative-decay.csv").getFullPathName().toStdString());
    report << "rate,scenario,time_s,target_slope_db_s,reference_slope_db_s,cut_db,target_hz\n";
    // Every tone is first observed in the input. The loudest tone is not the
    // slow-decaying target: absolute spectral level alone cannot pass this test.
    for (double rate : {44100.,48000.,96000.})
        for (int scenario = 0; scenario < 7; ++scenario)
        {
            auto frame = std::make_unique<lan::Frame>(); frame->rate = rate; frame->sourceHoldSeconds = 30;
            lan::ResonanceAnalyser analyser;
            constexpr std::array<double,7> requested {187.5,375,750,1500,3000,6000,937.5};
            std::array<double,7> frequencies {};
            for (size_t j = 0; j < frequencies.size(); ++j)
                frequencies[j] = std::round(requested[j]*lan::analysisSize/rate)*rate/lan::analysisSize;
            const auto target = frequencies.back();
            const double referenceSlope = scenario == 5 ? -1.5 : -12;
            const double targetSlope = scenario == 0 ? -2 : scenario == 3 ? 8 : scenario == 5 ? -.2 : scenario == 6 ? -18 : -12;
            const auto fill = [&] (double t, bool source)
            {
                for (int i = 0; i < lan::analysisSize; ++i)
                {
                    double x = 0;
                    for (size_t j = 0; j < frequencies.size(); ++j)
                    {
                        if (scenario == 4 && j != frequencies.size()-1) continue;
                        const auto slope = j == frequencies.size()-1 ? targetSlope : referenceSlope;
                        const auto amplitude = (j == 0 ? .3 : .03)*std::pow(10.,slope*t/20);
                        x += amplitude*std::sin(2*pi*frequencies[j]*(i/rate+t));
                    }
                    frame->audio[0][size_t(i)] = source ? float(x) : 0;
                    frame->audio[1][size_t(i)] = source ? -float(x) : 0;
                    frame->audio[2][size_t(i)] = float(x); frame->audio[3][size_t(i)] = -float(x);
                }
                frame->sampleEnd += uint64_t(std::round(rate*.1));
            };
            for (int step = 0; step < 8; ++step) { fill(0,true); check(analyser.analyse(*frame).count == 0,"source notes are protected before decay comparison"); }
            bool detected = false; double deepest = 0;
            for (int step = 0; step < 25; ++step)
            {
                const auto t = step*.1; fill(t,scenario == 2);
                const auto a = analyser.analyse(*frame); double cut = 0;
                for (int band = 0; band < a.count; ++band)
                {
                    const auto& b = a.bands[size_t(band)];
                    if (std::abs(b.frequency-target) < 2*rate/lan::analysisSize)
                    { cut = std::max(cut,-double(b.gainDb)); detected = true; }
                    check(b.gainDb >= -18 && b.gainDb <= 0,"aggressive decay correction stays within its attenuation-only limit");
                    if (scenario == 0 || scenario == 5)
                        check(std::abs(b.frequency-target) < 2*rate/lan::analysisSize,"decay comparison selects the slow tail rather than louder normally decaying notes");
                }
                deepest = std::max(deepest,cut);
                if (t < .5) check(cut < 1e-7,"short input guard prevents immediate tail cuts");
                report << rate << ',' << scenario << ',' << t << ',' << targetSlope << ',' << referenceSlope << ',' << cut << ',' << target << '\n';
            }
            if (scenario == 0 || scenario == 5)
                check(detected && deepest > 9,"relative decay overrides long source memory and permits stronger than 9 dB correction");
            else
                check(!detected,"equal decay, active source, rising energy and single-tone tails do not masquerade as relative decay resonance");
        }
}
void feedbackRoutingTests (const juce::File& directory)
{
    std::ofstream report(directory.getChildFile("feedback-routing.csv").getFullPathName().toStdString());
    report << "rate,sample,reference,corrected\n";
    for (double rate : {44100.,48000.,96000.})
    {
        lan::Settings normal; normal.automatic = false; normal.predelayMs = 0; normal.timeMs = 80; normal.mix = 1; normal.width = 1; normal.phase = 0;
        auto corrected = normal; corrected.automatic = true; corrected.phase = 1; corrected.tame = 1;
        lan::ReverbEngine reference, processing; reference.prepare(rate,false); processing.prepare(rate,false);
        lan::ReverbEngineTestAccess::forceCorrection(processing);
        const auto count = juce::roundToInt(rate*.5); double earlyError = 0, laterDifference = 0; float earlyPeak = 0;
        for (int at = 0; at < count; at += 128)
        {
            const auto n = std::min(128,count-at); juce::AudioBuffer<float> a(2,n), b(2,n); a.clear();
            if (at == 0) { a.setSample(0,0,.8f); a.setSample(1,0,.8f); }
            b.makeCopyOf(a);
            reference.process(a,normal); processing.process(b,corrected);
            for (int i = 0; i < n; ++i)
            {
                const auto x = a.getSample(0,i), y = b.getSample(0,i); const auto t = (at+i)/rate;
                if (t < .065) { earlyError = std::max(earlyError,double(std::abs(x-y))); earlyPeak = std::max(earlyPeak,std::abs(x)); }
                if (t > .1) laterDifference += (x-y)*(x-y);
                if (at+i < rate*.12) report << rate << ',' << at+i << ',' << x << ',' << y << '\n';
            }
        }
        check(earlyPeak > .01,"first-arrival routing comparison contains actual wet signal");
        check(earlyError < 2e-7,"active EQ, phase and automatic tap steering do not alter the first wet arrivals");
        check(laterDifference > 1e-5,"the same correction affects later recirculated feedback instead of being bypassed");
    }
}
void tapTests()
{
    for (double rate : {44100.,48000.,96000.})
    {
        std::vector<float> constant(4096,1); lan::MovingTap transition; transition.prepare(rate,100); transition.target(200);
        const auto samples = juce::roundToInt(rate*.25);
        for (int i = 0; i < samples-3; ++i) transition.read(constant,0);
        check(std::abs(transition.current()-100) < .01,"tap crossfade does not finish before 250 ms");
        for (int i = 0; i < 12; ++i) transition.read(constant,0);
        check(std::abs(transition.current()-200) < .01,"tap crossfade completes at approximately 250 ms");
    }
    std::vector<float> data(4096,1.f); lan::MovingTap tap; tap.prepare(48000,100);
    for (int i = 0; i < 20000; ++i)
    {
        if (i%31 == 0) tap.target(float(1+i%3500));
        check(std::abs(tap.read(data,size_t(i)%data.size())-1) < 1e-6,"rapid queued tap automation is a convex DC-unity fade");
    }
    check(std::abs(tap.current()-100) > 1,"queued tap changes eventually advance the read head");
    tap.target(700);
    for (int i = 0; i < 36010; ++i) tap.read(data,0);
    check(std::abs(tap.current()-700) < .01,"latest queued tap request is reached after the longer fades complete");
    // A 250 ms queued fade must not introduce a one-sample step on a stationary
    // tone, even when the requested delays alternate every 31 samples.
    for (size_t i = 0; i < data.size(); ++i) data[i] = float(.2*std::sin(2*pi*i/128));
    tap.prepare(48000,100); float previous = tap.read(data,0); double jump = 0;
    for (int i = 1; i < 20000; ++i)
    {
        if (i%31 == 0) tap.target(float(1+i%3500));
        const auto value = tap.read(data,size_t(i)%data.size()); jump = std::max(jump,double(std::abs(value-previous))); previous = value;
    }
    check(jump < .012,"tap automation adds no abrupt tone discontinuity");
}
class TempoPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<double> bpm {120};
    bool available = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        if (!available) return {};
        PositionInfo position; position.setBpm(bpm); position.setIsPlaying(false); return position;
    }
};
void tempoSyncTests (const juce::File& directory)
{
    std::ofstream report(directory.getChildFile("tempo-sync.csv").getFullPathName().toStdString());
    report << "rate,note_index,bpm,expected_ms,first_return_ms\n";
    const auto findNote = [] (const char* label)
    {
        for (size_t i = 0; i < lan::noteLengths.size(); ++i)
            if (juce::String(lan::noteLengths[i].label) == label) return int(i);
        throw std::runtime_error("missing synchronized note length");
    };
    TempoPlayHead host; LanReverbProcessor processor; processor.setPlayHead(&host); processor.prepareToPlay(48000,128);
    juce::MidiBuffer midi; juce::AudioBuffer<float> silence(2,128);
    const auto tick = [&] { silence.clear(); processor.processBlock(silence,midi); };
    set(processor,"sync",1); set(processor,"note",float(findNote("1/4"))); tick();
    check(std::abs(processor.settings().timeMs-500) < .001,"quarter note uses a stopped host's 120 BPM tempo");
    host.bpm = 90; tick(); check(std::abs(processor.settings().timeMs-60000.f/90) < .001,"tempo changes are applied at the next callback");
    host.bpm = {}; tick(); check(std::abs(processor.tempoBpm()-90) < 1e-9,"missing BPM retains the last valid tempo");
    host.available = false; tick(); check(std::abs(processor.tempoBpm()-90) < 1e-9,"missing transport retains the last valid tempo"); host.available = true;
    for (auto invalid : {0.,-50.,10.,500.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    { host.bpm = invalid; tick(); check(std::abs(processor.tempoBpm()-90) < 1e-9,"invalid or unsupported host tempo cannot disturb the delay"); }
    processor.setPlayHead(nullptr); tick(); check(std::abs(processor.tempoBpm()-90) < 1e-9,"detaching the playhead retains its last tempo");
    set(processor,"time",137); set(processor,"sync",0); tick();
    check(std::abs(processor.settings().timeMs-137) < .001,"free time retains its own value when Sync is disabled");
    set(processor,"sync",1); tick(); check(std::abs(processor.settings().timeMs-60000.f/90) < .001,"re-enabling Sync retains the note selection");
    LanReverbProcessor noHost; set(noHost,"sync",1); set(noHost,"note",float(findNote("1/8")));
    check(std::abs(noHost.settings().timeMs-250) < .001,"a plugin without host tempo starts at 120 BPM");
    for (size_t i = 1; i < lan::noteLengths.size(); ++i)
        check(lan::noteLengths[i].beats > lan::noteLengths[i-1].beats,"note lengths increase monotonically around the Time knob");

    struct Example { const char* note; double bpm, milliseconds; };
    constexpr std::array<Example,4> examples {{{"1/2",120,1000},{"1/4 D",120,750},{"1/4 T",120,1000./3},{"1/64 T",400,6.25}}};
    for (double rate : {44100.,48000.,96000.})
        for (const auto& example : examples)
        {
            TempoPlayHead clock; clock.bpm = example.bpm;
            LanReverbProcessor p; p.setPlayHead(&clock);
            set(p,"sync",1); set(p,"note",float(findNote(example.note))); set(p,"automatic",0);
            set(p,"scatter",0); set(p,"predelay",0); set(p,"width",100); set(p,"mix",100);
            p.prepareToPlay(rate,128); int firstReturn = -1;
            const auto count = juce::roundToInt(rate*(example.milliseconds*.001+.05));
            for (int at = 0; at < count && firstReturn < 0; at += 128)
            {
                juce::AudioBuffer<float> block(2,128); block.clear();
                if (at == 0) { block.setSample(0,0,.8f); block.setSample(1,0,.8f); }
                p.processBlock(block,midi);
                for (int i = 0; i < 128; ++i)
                    if (std::abs(block.getSample(0,i)) > 1e-5) { firstReturn = at+i; break; }
            }
            check(firstReturn >= 0 && std::abs(firstReturn-example.milliseconds*.001*rate) <= 1.1,
                  "real synced delay first arrival matches straight, dotted and triplet timing within interpolation tolerance");
            report << rate << ',' << findNote(example.note) << ',' << example.bpm << ',' << example.milliseconds << ',' << firstReturn*1000/rate << '\n';
        }
    // The longest note must not clamp to the former 500 ms range or exhaust
    // memory at the most extreme Time/Tap/Scatter combinations.
    for (float scatter : {0.f,.5f,1.f})
        for (float tap : {-1.f,1.f})
        {
            lan::Settings s; s.timeMs = lan::noteTimeMs(findNote("1/1 D"),20); s.scatter = scatter; s.tap = tap;
            lan::ReverbEngine engine; engine.prepare(48000,false); lan::ReverbEngineTestAccess::forceCorrection(engine);
            juce::AudioBuffer<float> block(2,1); block.clear(); engine.process(block,s);
            check(std::abs(s.timeMs-18000) < .001,"dotted whole note at minimum tempo retains eighteen seconds");
            lan::ReverbEngineTestAccess::checkTapCapacity(engine);
        }
}
void processorTests()
{
    stateTests<LanReverbProcessor>(15);
    LanReverbProcessor p; set(p,"mix",0); set(p,"automatic",0); p.prepareToPlay(48000,128);
    check(p.getLatencySamples() == 0,"reverb introduces no dry-path latency");
    juce::MidiBuffer midi; juce::AudioBuffer<float> b(2,128);
    for (int i = 0; i < 128; ++i) { b.setSample(0,i,float(i)*.001f); b.setSample(1,i,-float(i)*.002f); }
    p.processBlock(b,midi);
    for (int i = 0; i < 128; ++i) check(std::abs(b.getSample(0,i)-i*.001f) < 1e-7 && std::abs(b.getSample(1,i)+i*.002f) < 1e-7,"zero wet is immediate unity dry");
    LanReverbProcessor bypassed; bypassed.prepareToPlay(48000,128);
    for (int i = 0; i < 128; ++i) b.setSample(0,i,.2f), b.setSample(1,i,-.1f);
    bypassed.processBlockBypassed(b,midi);
    check(std::abs(b.getSample(0,0)-.2f) < 1e-7 && std::abs(b.getSample(1,0)+.1f) < 1e-7,"initial host bypass is exact dry");
    auto buses = p.getBusesLayout(); buses.inputBuses.set(0,juce::AudioChannelSet::mono()); buses.outputBuses.set(0,juce::AudioChannelSet::mono());
    check(p.setBusesLayout(buses),"matched mono layout is supported");
    p.prepareToPlay(48000,127); juce::AudioBuffer<float> mono(1,127); mono.clear(); mono.setSample(0,0,.5f); p.processBlock(mono,midi);
    check(std::abs(mono.getSample(0,0)-.5f) < 1e-7,"mono dry path is preserved");
    std::vector<float> reference;
    for (int size : {1,127,512})
    {
        lan::Settings settings; settings.automatic = false;
        const auto output = render(settings,.35,48000,0,size);
        if (reference.empty()) reference = output.left;
        else
        {
            double maximum = 0;
            for (size_t i = 0; i < reference.size(); ++i) maximum = std::max(maximum,double(std::abs(reference[i]-output.left[i])));
            check(maximum < 1e-6,"static feedback processing is host-block invariant");
        }
    }
    lan::ReverbEngine engine; engine.prepare(48000,false); lan::Settings settings; settings.automatic = false;
    juce::AudioBuffer<float> block(2,4800); block.clear(); engine.process(block,settings); check(engine.analysisJobs() == 0,"Auto Off suspends FFT job submission");
    settings.automatic = true; engine.process(block,settings); check(engine.analysisJobs() > 0,"Auto On resumes FFT analysis");
    const auto before = engine.analysisJobs(); settings.bypass = true; engine.process(block,settings);
    check(engine.analysisJobs() > before,"analysis and feedback remain hot during bypass");
    settings.automatic = false; block.clear(); block.setSample(0,0,std::numeric_limits<float>::quiet_NaN()); block.setSample(1,1,std::numeric_limits<float>::infinity()); settings.timeMs = std::numeric_limits<float>::quiet_NaN();
    engine.process(block,settings); check(std::isfinite(block.getMagnitude(0,block.getNumSamples())),"nonfinite host input and parameters are contained");
}
std::vector<double> spectrum (const Audio& audio)
{
    juce::dsp::FFT fft(12); std::array<float,8192> data {}; std::vector<double> power(2049,0); int frames = 0;
    for (size_t at = 3*48000; at+4096 < 8*48000; at += 2048)
        for (int channel = 0; channel < 2; ++channel)
        {
            data.fill(0); const auto& source = channel == 0 ? audio.left : audio.right;
            for (size_t i = 0; i < 4096; ++i) data[i] = source[at+i]*float(.5-.5*std::cos(2*pi*i/4095));
            fft.performFrequencyOnlyForwardTransform(data.data());
            for (size_t k = 0; k < power.size(); ++k) power[k] += std::pow(4.*data[k]/4096,2);
            ++frames;
        }
    for (auto& p : power) p /= frames;
    return power;
}
void scatterTests (const juce::File& directory)
{
    std::ofstream summary(directory.getChildFile("scatter-summary.csv").getFullPathName().toStdString());
    summary << "rate,scatter,first_return_ms,echo_window_share,active_fraction,side_mid_rms\n";
    std::ofstream waveform(directory.getChildFile("scatter.csv").getFullPathName().toStdString()); waveform << "scatter,time_s,left,right\n";
    for (double rate : {44100.,48000.,96000.})
    {
        double delayDensity = 0, reverbDensity = 0;
        for (float amount : {0.f,.5f,1.f})
        {
            lan::Settings s; s.scatter = amount; s.automatic = false; s.mix = 1; s.width = 1; s.predelayMs = 0;
            s.timeMs = 80; s.decaySeconds = 3; s.phase = 1; s.diffusion = 1;
            const auto audio = render(s,1,rate,0);
            double total = 0, focused = 0, maximum = 0; int first = -1, active = 0;
            for (size_t i = 0; i < audio.left.size(); ++i)
            {
                maximum = std::max(maximum,double(std::abs(audio.left[i])));
                if (first < 0 && std::abs(audio.left[i]) > 1e-7) first = int(i);
                const auto t = i/rate; const auto e = .5*(audio.left[i]*audio.left[i]+audio.right[i]*audio.right[i]);
                total += e;
                if (std::fmod(t+.5/rate,.08) < .012) focused += e;
                if (juce::exactlyEqual(float(rate),48000.f) && i%8 == 0) waveform << amount << ',' << t << ',' << audio.left[i] << ',' << audio.right[i] << '\n';
            }
            for (size_t i = size_t(.2*rate); i < size_t(.8*rate); ++i) if (std::abs(audio.left[i]) > maximum*.001) ++active;
            const auto density = active/(.6*rate), share = focused/std::max(1e-30,total);
            summary << rate << ',' << amount << ',' << first*1000/rate << ',' << share << ',' << density << ',' << ratio(audio,.2,.8,rate) << '\n';
            if (amount < .01f)
            {
                delayDensity = density;
                check(std::abs(first/rate-.08) <= 1.1/rate,"Scatter zero returns its first repeat at Time with no hidden phase delay");
                check(share > .98,"Scatter zero concentrates impulse energy in regular delay repeats");
                check(ratio(audio,.2,.8,rate) < 1e-6,"Scatter zero does not create artificial stereo spread from mono");
            }
            if (amount > .99f) { reverbDensity = density; check(share < .8,"full Scatter spreads reflections beyond the delay-repeat windows"); }
        }
        check(reverbDensity > delayDensity*2,"full Scatter produces a substantially denser reverb tail");
        lan::Settings sparse; sparse.scatter = 1; sparse.phase = 0; sparse.diffusion = 0;
        sparse.timeMs = 80; sparse.decaySeconds = 3; sparse.predelayMs = 0; sparse.mix = 1; sparse.width = 1; sparse.automatic = false;
        const auto minimum = render(sparse,1,rate,0);
        double minimumPeak = 0; int minimumActive = 0;
        for (auto x : minimum.left) minimumPeak = std::max(minimumPeak,double(std::abs(x)));
        for (size_t i = size_t(.2*rate); i < size_t(.8*rate); ++i) if (std::abs(minimum.left[i]) > minimumPeak*.001) ++minimumActive;
        check(minimumActive/(.6*rate) > delayDensity*2,"full Scatter remains a reverb with Diffusion and Phase at minimum");
    }
    lan::ReverbEngine delay; delay.prepare(48000,false); lan::Settings s; s.scatter = 0; s.tame = 0; s.phase = 1; s.mix = 1; s.width = 1; s.predelayMs = 0;
    juce::AudioBuffer<float> impulse(2,24000); impulse.clear(); impulse.setSample(0,0,.8f); delay.process(impulse,s);
    check(impulse.getMagnitude(1,0,24000) < 1e-7,"zero Scatter preserves independent stereo delay channels");
    check(delay.analysisJobs() == 0,"phase-only automatic analysis sleeps when Scatter zero bypasses phase spreading");
    lan::ReverbEngine moving; moving.prepare(48000,false); s.automatic = false; s.decaySeconds = 3;
    double jump = 0; float previous = 0;
    for (int at = 0; at < 96000; at += 128)
    {
        juce::AudioBuffer<float> block(2,128);
        for (int i = 0; i < 128; ++i)
        { const auto x = float(.05*std::cos(2*pi*330*(at+i)/48000)); block.setSample(0,i,x); block.setSample(1,i,x); }
        s.scatter = at >= 33600 && at < 67200 ? 1.f : 0.f; moving.process(block,s);
        for (int i = 0; i < 128; ++i) { const auto value = block.getSample(0,i); jump = std::max(jump,double(std::abs(value-previous))); previous = value; }
    }
    check(jump < .05,"Scatter endpoint automation avoids abrupt output steps on a low-level sustained tone");
}
void measurements (const juce::File& directory)
{
    std::ofstream decay(directory.getChildFile("decay.csv").getFullPathName().toStdString()); decay << "decay_s,time_s,rms\n";
    double shortLate = 0, longLate = 0;
    for (float seconds : {2.f,10.f,30.f})
    {
        lan::Settings settings; settings.automatic = false; settings.mix = 1; settings.decaySeconds = seconds;
        const auto out = render(settings,12,48000,0);
        check(out.jobs == 0,"manual reverb runs without spectral jobs");
        for (int window = 0; window < 120; ++window) decay << seconds << ',' << window*.1 << ',' << std::sqrt(energy(out,window*.1,(window+1)*.1,48000)) << '\n';
        if (seconds < 3) shortLate = energy(out,3,4,48000); else if (seconds > 20) longLate = energy(out,3,4,48000);
        if (seconds > 9 && seconds < 11) writeWav(directory.getChildFile("wet-impulse.wav"),out,48000,8*48000);
    }
    check(longLate > shortLate*10,"long decay retains substantially more late energy than short decay");
    std::ofstream stereo(directory.getChildFile("stereo.csv").getFullPathName().toStdString()); stereo << "width,side_mid_rms\n";
    for (float width : {0.f,1.f,2.f})
    {
        lan::Settings settings; settings.automatic = false; settings.mix = 1; settings.width = width;
        const auto out = render(settings,3,48000,2);
        const auto side = ratio(out,.5,2.5,48000); stereo << width << ',' << side << '\n';
        if (width < .1f) check(side < 1e-6,"zero width is mono"); else check(side > .1,"mono excitation produces substantial wet side energy");
        if (width > .9f && width < 1.1f)
        {
            std::ofstream scatter(directory.getChildFile("stereo-scatter.csv").getFullPathName().toStdString()); scatter << "left,right\n";
            for (size_t i = 24000; i < 30000; i += 3) scatter << out.left[i] << ',' << out.right[i] << '\n';
        }
    }
    std::ofstream resonances(directory.getChildFile("resonance.csv").getFullPathName().toStdString()); resonances << "mode,time_s,rms,max_cut_db\n";
    std::array<double,4> late {}; std::array<std::vector<double>,4> spectra; float correction = 0;
    for (int mode = 0; mode < 4; ++mode)
    {
        lan::Settings settings; settings.mix = 1; settings.decaySeconds = 30; settings.diffusion = .1f; settings.phase = mode == 2 ? 1.f : mode == 3 ? .6f : 0.f;
        settings.tame = mode == 1 ? 1.f : mode == 3 ? .75f : 0.f; settings.automatic = mode != 0;
        const auto out = render(settings,10,48000,1,128,false,false,true);
        spectra[size_t(mode)] = spectrum(out);
        late[size_t(mode)] = energy(out,3,8,48000); if (mode == 1) correction = out.maxReduction;
        for (int i = 0; i < 100; ++i) resonances << mode << ',' << i*.1 << ',' << std::sqrt(energy(out,i*.1,(i+1)*.1,48000)) << ',' << out.maxReduction << '\n';
        if (mode == 1) writeWav(directory.getChildFile("tamed-tail.wav"),out,48000,8*48000);
    }
    std::ofstream spectral(directory.getChildFile("spectra.csv").getFullPathName().toStdString()); spectral << "frequency_hz,off_power,eq_power,phase_power,combined_power\n";
    for (size_t k = 1; k < spectra[0].size(); ++k) spectral << k*48000./4096 << ',' << spectra[0][k] << ',' << spectra[1][k] << ',' << spectra[2][k] << ',' << spectra[3][k] << '\n';
    std::cout << "Tail control: off=" << late[0] << " EQ=" << late[1] << " phase=" << late[2] << " cut=" << correction << " dB\n";
    check(correction > 1,"actual feedback ringing triggers automatic EQ");
    double rogueBefore = 0, rogueAfter = 0;
    for (size_t k = 1; k < spectra[0].size(); ++k)
        if (k*48000./4096 >= 1600 && k*48000./4096 <= 2050)
        { rogueBefore += spectra[0][k]; rogueAfter += spectra[1][k]; }
    std::cout << "Unrelated feedback-band power ratio=" << rogueAfter/std::max(1e-30,rogueBefore) << '\n';
    check(rogueAfter < rogueBefore*.9,"feedback EQ reduces the added ringing band without requiring source-tail attenuation");
    lan::Settings shifted; shifted.mix = 1; shifted.decaySeconds = 30; shifted.diffusion = .1f; shifted.phase = 0; shifted.automatic = false;
    const auto shiftedAudio = render(shifted,10,48000,1,128,false,true,true);
    const auto shiftedSpectrum = spectrum(shiftedAudio);
    double dot = 0, beforeNorm = 0, afterNorm = 0, differenceNorm = 0;
    std::ofstream tapReport(directory.getChildFile("tap-shift.csv").getFullPathName().toStdString()); tapReport << "frequency_hz,fixed_power,shifted_power\n";
    for (size_t k = 1; k < shiftedSpectrum.size(); ++k)
    {
        dot += spectra[0][k]*shiftedSpectrum[k]; beforeNorm += spectra[0][k]*spectra[0][k]; afterNorm += shiftedSpectrum[k]*shiftedSpectrum[k];
        differenceNorm += std::pow(spectra[0][k]-shiftedSpectrum[k],2);
        tapReport << k*48000./4096 << ',' << spectra[0][k] << ',' << shiftedSpectrum[k] << '\n';
    }
    const auto similarity = dot/std::sqrt(std::max(1e-300,beforeNorm*afterNorm));
    std::cout << "Tap spectral similarity=" << similarity << " combined power=" << late[3] << '\n';
    check(energy(shiftedAudio,3,8,48000) > 1e-15,"tap-shift comparison contains a measurable tail");
    check(differenceNorm/std::max(1e-300,beforeNorm) > .01,"manual feedback tap shift materially changes the absolute ringing spectrum");
    std::ofstream tapSummary(directory.getChildFile("tap-shift-summary.csv").getFullPathName().toStdString());
    tapSummary << "shape_similarity,relative_spectral_l2,late_power_ratio\n" << similarity << ','
        << std::sqrt(differenceNorm/std::max(1e-300,beforeNorm)) << ',' << energy(shiftedAudio,3,8,48000)/late[0] << '\n';
    std::ofstream stress(directory.getChildFile("automation.csv").getFullPathName().toStdString()); stress << "rate,time_s,rms\n";
    for (double rate : {8000.,44100.,48000.,96000.,192000.})
    {
        lan::Settings settings; settings.mix = 1; settings.decaySeconds = 40;
        const auto out = render(settings,4,rate,2,128,true);
        check(energy(out,3,4,rate) < .25,"extreme automated tails remain bounded after excitation stops");
        for (int i = 0; i < 40; ++i) stress << rate << ',' << i*.1 << ',' << std::sqrt(energy(out,i*.1,(i+1)*.1,rate)) << '\n';
    }
}
void editorTests (const juce::File& destination)
{
    LanReverbProcessor p; p.prepareToPlay(48000,128);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor()); editor->addToDesktop(0); editor->setVisible(true);
    check(editor->getWidth() == 740 && editor->getHeight() == 540,"editor uses intended dimensions");
    int sliders = 0;
    constexpr std::array<const char*,11> ids {"time","decay","predelay","scatter","diffusion","width","tap","phase","tame","mix","output"};
    for (auto* child : editor->getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
        {
            ++sliders; slider->setValue(slider->getMaximum()*.7,juce::sendNotificationSync);
            check(slider->getTooltip().isNotEmpty(),"every knob has a hint");
            check(std::abs(p.parameters.getRawParameterValue(ids[size_t(sliders-1)])->load()-slider->getValue()) < .01,"knob edits reach their host parameter");
        }
        if (dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::Button*>(child) != nullptr || dynamic_cast<juce::Label*>(child) != nullptr)
        {
            auto* client = dynamic_cast<juce::TooltipClient*>(child); const auto text = client != nullptr ? client->getTooltip() : juce::String();
            check(text.isNotEmpty() && text.endsWithChar('.') && text.dropLastCharacters(1).indexOfChar('.') < 0,"every control and caption has a one-sentence hint");
        }
        if (auto* hint = dynamic_cast<editorTooltips::TitleHint*>(child))
            check(hint->getTooltip().contains("J-Sonic Team") && hint->getTooltip().contains(JucePlugin_VersionString),"title hover shows release information");
    }
    check(sliders == 11,"all eleven controls are present");
    juce::Slider* timeControl = nullptr; juce::Button* syncControl = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(child)) if (slider->getName() == "TIME") timeControl = slider;
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) if (button->getButtonText() == "SYNC") syncControl = button;
    }
    check(timeControl != nullptr && syncControl != nullptr,"Time knob has a Sync button");
    const auto pump = [] { for (int i = 0; i < 4; ++i) { juce::Thread::sleep(20); juce::Timer::callPendingTimersSynchronously(); } };
    const auto freeTime = p.parameters.getRawParameterValue("time")->load();
    set(p,"sync",1); pump();
    timeControl->setValue(15,juce::sendNotificationSync);
    check(std::abs(p.parameters.getRawParameterValue("note")->load()-15) < .01,"synced Time knob edits the host note parameter");
    check(timeControl->getTextFromValue(timeControl->getValue()) == "1/4 D","synced Time shows a note label rather than milliseconds");
    check(std::abs(p.parameters.getRawParameterValue("time")->load()-freeTime) < .01,"note editing leaves the stored millisecond value intact");
    set(p,"note",11); pump(); check(timeControl->getTextFromValue(timeControl->getValue()) == "1/4 T","host note automation updates the visible Time knob");
    syncControl->setToggleState(false,juce::sendNotificationSync); pump();
    check(p.parameters.getRawParameterValue("sync")->load() < .5f && std::abs(timeControl->getValue()-freeTime) < .01,"clicking Sync off restores the free Time knob and value");
    for (auto* parameter : p.getParameters()) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    juce::MidiBuffer midi;
    for (int at = 0; at < 28800; at += 128)
    {
        juce::AudioBuffer<float> audio(2,128);
        for (int i = 0; i < 128; ++i)
        { const auto x = at < 9600 ? float(.2*std::sin(2*pi*330*(at+i)/48000)+.1*std::sin(2*pi*2200*(at+i)/48000)) : 0.f; audio.setSample(0,i,x); audio.setSample(1,i,x); }
        p.processBlock(audio,midi); juce::Thread::sleep(2);
    }
    for (int tick = 0; tick < 3; ++tick) { juce::Thread::sleep(10); juce::Timer::callPendingTimersSynchronously(); }
    lan::Analysis live; check(p.readAnalysis(live) && live.revision > 0,"real background worker publishes feedback data for the native editor");
    auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds()); juce::MemoryOutputStream png;
    destination.getParentDirectory().createDirectory(); check(juce::PNGImageFormat().writeImageToStream(snapshot,png) && destination.replaceWithData(png.getData(),png.getDataSize()),"native preview is saved");
    for (float factor : {.5f,1.2f})
    {
        editor->setSize(juce::roundToInt(740*factor),juce::roundToInt(540*factor));
        for (auto* child : editor->getChildren())
            if (dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::Button*>(child) != nullptr)
            {
                const auto bounds = editor->getLocalArea(child,child->getLocalBounds());
                auto* hit = editor->getComponentAt(bounds.getCentre());
                check(editor->getLocalBounds().contains(bounds) && (hit == child || child->isParentOf(hit)),"controls stay visible and clickable in the scalable editor");
            }
    }
}
}
int main (int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto directory = argc > 2 && juce::String(argv[1]) == "--report"
            ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[2])
            : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("lan-reverb-report");
        directory.createDirectory();
        if (argc > 2 && juce::String(argv[1]) == "--editor") editorTests(juce::File(argv[2]));
        else { analyserTests(directory); inputProtectionTests(directory); relativeDecayTests(directory); feedbackRoutingTests(directory); lan::ReverbEngineTestAccess::multiPhaseTests(directory); tapTests(); tempoSyncTests(directory); processorTests(); scatterTests(directory); measurements(directory); }
        std::cout << checks << " Lan Reverb checks passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
