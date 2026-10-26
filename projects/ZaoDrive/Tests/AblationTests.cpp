#include "../Source/DriveEngine.h"
#include "../../AcademicAblations.h"
#include "../../AcademicTestUtils.h"
#include <random>
namespace
{
using namespace academic;
using Analyser = zao::SmartAnalyser;
Analyser::Frame captured;
size_t captureIndex = 0;
void tap (const std::array<float,2>& input, const std::array<float,2>& wet)
{
    const auto i = captureIndex++ % Analyser::fftSize;
    captured.left[i] = input[0]; captured.right[i] = input[1];
    captured.wetLeft[i] = wet[0]; captured.wetRight[i] = wet[1];
}
struct Variant { const char* name; SmartOptions options; bool off = false; };
const std::array<Variant,7> variants {{
    {"off", {}, true}, {"current", {}}, {"no_protection", {false,true,true,1}},
    {"no_wet_threshold", {true,false,true,1}}, {"half_bandwidth", {true,true,true,.5}},
    {"double_bandwidth", {true,true,true,2}}, {"energy_only", {true,true,false,1}}
}};
std::vector<float> filter (const Analyser::Result& bands, double rate, const Analyser::Frame& frame)
{
    zao::SmartHarmonics filters; filters.prepare (rate,false); filters.setPreview (bands);
    std::vector<float> out (Analyser::fftSize);
    // Repeated stationary frame warms IIR state; measure only the final cycle.
    // Circuit spectra are bin-centred and periodic. Noise is explicitly periodic.
    for (int pass = 0; pass < 4; ++pass)
        for (size_t i = 0; i < out.size(); ++i)
        {
            std::array<float,2> wet {frame.wetLeft[i],frame.wetRight[i]};
            filters.process ({frame.left[i],frame.right[i]},wet); out[i] = wet[0];
        }
    return out;
}
std::vector<double> powers (const std::vector<float>& samples)
{
    juce::dsp::FFT fft (12); std::array<float,8192> data {};
    for (size_t i = 0; i < samples.size(); ++i) data[i] = samples[i]*float (.5-.5*std::cos (2*zao::pi*i/(samples.size()-1)));
    fft.performFrequencyOnlyForwardTransform (data.data());
    std::vector<double> p (2049); for (size_t i = 0; i < p.size(); ++i) p[i] = data[i]*data[i]; return p;
}
}
int main (int argc, char** argv)
{
    try
    {
        const std::filesystem::path directory = argc > 1 ? argv[1] : "academic-results";
        manifest (directory,"ZaoDrive");
        auto audioSettings = csv(directory,"audio-settings.csv", "rate,model,corpus,clear,auto_compensate,transformer,bypass,input_db,output_db,mix,capture_smart");
        auto out = csv (directory,"smart.csv","rate,model,corpus,drive_db,variant,confidence,targets,weak_wet_targets,protected_targets,wet_rms_db,total_rms_db,estimated_cost_before,estimated_cost_after,measured_target_power_ratio");
        auto partials = csv (directory,"partials.csv","rate,model,corpus,variant,frequency_hz,wet_change_db,total_change_db");
        auto targets = csv (directory,"targets.csv","rate,model,corpus,variant,frequency_hz,bandwidth_hz,gain_db,order,wet_power_before,wet_power_after");
        constexpr std::array<const char*,5> names {"single_note","C_chord","harmonic_collision","inharmonic","periodic_noise"};
        int protectionWitness = 0, thresholdWitness = 0, energyWitness = 0;
        for (double rate : {44100.,48000.,96000.})
            for (int model = 0; model < 4; ++model)
                for (int corpus = 0; corpus < 5; ++corpus)
                {
                    std::vector<int> bins = corpus == 0 ? std::vector<int> {23} : corpus == 1 ? std::vector<int> {22,28,33}
                                         : corpus == 2 ? std::vector<int> {23,46,37,69} : std::vector<int> {23,37,61};
                    std::vector<float> source (Analyser::fftSize);
                    std::mt19937 random (83291); std::uniform_real_distribution<float> noise (-.18f,.18f);
                    for (size_t i = 0; i < source.size(); ++i)
                        if (corpus == 4) source[i] = noise (random);
                        else for (int bin : bins) source[i] += float (.25/bins.size()*std::sin (2*zao::pi*bin*i/source.size()));
                    zao::Settings settings; settings.model = model; settings.driveDb = 24; settings.outputDb = 0;
                    settings.clear = settings.transformer = settings.smart = settings.autoCompensate = settings.bypass = false;
                    settings.inputDb = settings.outputDb = 0; settings.mix = 1; settings.toneHz = 16000;
                    check(!settings.clear && !settings.autoCompensate && !settings.transformer && !settings.bypass && !settings.smart,
                          "academic Zao capture disables all auxiliary processing switches");
                    audioSettings << rate << ',' << model << ',' << corpus << ",0,0,0,0,0,0,1,0\n";
                    zao::DriveEngine engine; engine.prepare (rate,128,false);
                    captureIndex = 0; captured = {}; captured.sampleRate = rate; captured.settings = settings;
                    smartTap = tap;
                    juce::AudioBuffer<float> block (2,128);
                    for (int at = 0; at < 6*Analyser::fftSize; at += 128)
                    {
                        for (int i = 0; i < 128; ++i) block.setSample (0,i,source[size_t (at+i)%source.size()]), block.setSample (1,i,source[size_t (at+i)%source.size()]);
                        engine.process (block,settings);
                    }
                    smartTap = nullptr;
                    const auto frame = captured;
                    const std::vector<float> before (frame.wetLeft.begin(),frame.wetLeft.end());
                    const auto beforePower = powers (before);
                    const auto maximumPower = *std::max_element (beforePower.begin(),beforePower.end());
                    std::vector<float> totalBefore (before.size());
                    for (size_t i = 0; i < before.size(); ++i) totalBefore[i] = frame.left[i]+before[i];
                    std::array<Analyser::Result,7> results;
                    for (size_t v = 0; v < variants.size(); ++v)
                    {
                        smart = variants[v].options; Analyser analyser;
                        auto bands = variants[v].off ? Analyser::Result {} : analyser.analyse (frame);
                        results[v] = bands;
                        // Fresh analyser determinism protects against stale spectral state.
                        Analyser repeat; const auto again = variants[v].off ? Analyser::Result {} : repeat.analyse (frame);
                        check (bands.count == again.count && juce::exactlyEqual(bands.baselineCost,again.baselineCost),"repeated frame gives identical Smart selection");
                        check (bands.count >= 0 && bands.count <= Analyser::maxBands,"Smart band count bounded");
                        const auto after = filter (bands,rate,frame); const auto afterPower = powers (after);
                        if (rate == 48000 && model == 1 && (corpus == 1 || corpus == 2) && v == 1)
                        {
                            // Continue the actual production bank over a periodic,
                            // post-circuit wet capture; no predicted gain is rendered.
                            zao::SmartHarmonics bank; bank.prepare(rate,false); bank.setPreview(bands);
                            auto audio = csv(directory,(std::string("audio-")+names[size_t(corpus)]+".csv").c_str(),
                                             "sample,rate,input,wet_off,wet_on,total_off,total_on");
                            for (int pass = 0; pass < 20; ++pass)
                                for (size_t i = 0; i < before.size(); ++i)
                                {
                                    std::array<float,2> wet {frame.wetLeft[i],frame.wetRight[i]};
                                    bank.process({frame.left[i],frame.right[i]},wet);
                                    check(std::isfinite(wet[0]),"Smart spectrogram render remains finite");
                                    if (pass >= 4) audio << (pass-4)*before.size()+i << ',' << rate << ','
                                        << frame.left[i] << ',' << before[i] << ',' << wet[0] << ','
                                        << frame.left[i]+before[i] << ',' << frame.left[i]+wet[0] << '\n';
                                }
                            check(bands.count > 0,"spectrogram fixture exercises Smart cuts");
                            auto weights = csv(directory,(std::string("clash-")+names[size_t(corpus)]+".csv").c_str(),
                                               "frequency_hz,weight,target");
                            for (int k = 0; k <= 2048; ++k)
                            {
                                const auto hz = k*rate/4096; double weight = 0; bool target = false;
                                for (int bin : bins) weight += Analyser::intervalDissonance(hz,bin*rate/4096)/bins.size();
                                for (int b = 0; b < bands.count; ++b)
                                    target |= std::abs(hz-bands.bands[size_t(b)].frequency) <= bands.bands[size_t(b)].bandwidth;
                                weights << hz << ',' << weight << ',' << target << '\n';
                            }
                        }
                        std::vector<float> totalAfter (before.size());
                        for (size_t i = 0; i < before.size(); ++i) totalAfter[i] = frame.left[i]+after[i];
                        double selectedBefore = 0, selectedAfter = 0; int weak = 0, protectedCount = 0;
                        for (int b = 0; b < bands.count; ++b)
                        {
                            const auto& band = bands.bands[size_t (b)];
                            check (std::isfinite (band.gainDb) && band.gainDb >= -6 && band.gainDb <= 0,"Smart cuts finite and bounded");
                            const auto bin = size_t (std::clamp (juce::roundToInt (band.frequency*Analyser::fftSize/rate),1,2047));
                            const auto p = beforePower[bin-1]+beforePower[bin]+beforePower[bin+1];
                            const auto q = afterPower[bin-1]+afterPower[bin]+afterPower[bin+1];
                            weak += p < maximumPower*.0001;
                            bool nearInput = false;
                            if (corpus != 4) for (int input : bins) nearInput |= std::abs (band.frequency-input*rate/Analyser::fftSize) < 4*band.bandwidth;
                            protectedCount += nearInput;
                            selectedBefore += p; selectedAfter += q;
                            targets << rate << ',' << model << ',' << names[size_t(corpus)] << ',' << variants[v].name << ',' << band.frequency << ',' << band.bandwidth << ',' << band.gainDb << ',' << band.order << ',' << p << ',' << q << '\n';
                        }
                        check (std::isfinite (rms (after)),"filtered wet signal remains finite");
                        if (variants[v].off) check (before == after,"Smart Off is an identity on the wet residual");
                        if (corpus == 4) check (bands.count == 0,"unpitched noise remains neutral in all variants");
                        if (v == 2 && protectedCount > 0) ++protectionWitness;
                        if (v == 3 && bands.count != results[1].count) ++thresholdWitness;
                        if (v == 6 && ! juce::exactlyEqual(bands.baselineCost,results[1].baselineCost)) ++energyWitness;
                        out << rate << ',' << model << ',' << names[size_t(corpus)] << ",24," << variants[v].name << ',' << bands.confidence << ',' << bands.count << ',' << weak << ',' << protectedCount << ',' << db(rms(after)/std::max(1e-15,rms(before))) << ',' << db(rms(totalAfter)/std::max(1e-15,rms(totalBefore))) << ',' << bands.baselineCost << ',' << bands.selectedCost << ',';
                        if (selectedBefore > 1e-20) out << selectedAfter/selectedBefore;
                        out << '\n';
                        if (corpus != 4) for (int bin : bins)
                        {
                            const auto hz = bin*rate/Analyser::fftSize;
                            partials << rate << ',' << model << ',' << names[size_t(corpus)] << ',' << variants[v].name << ',' << hz << ',' << db(amplitude(after,hz,rate)/std::max(1e-15,amplitude(before,hz,rate))) << ',' << db(amplitude(totalAfter,hz,rate)/std::max(1e-15,amplitude(totalBefore,hz,rate))) << '\n';
                        }
                    }
                }
        check (protectionWitness > 0,"corpus exposes input protection's effect on selected bands");
        check (thresholdWitness > 0,"corpus exposes the wet threshold's effect on selection");
        check (energyWitness > 0,"energy-only baseline differs from interval scoring");
        std::cout << checks << " Smart ablation checks passed\n";
    }
    catch (const std::exception& e) { smartTap = nullptr; std::cerr << e.what() << '\n'; return 1; }
}
