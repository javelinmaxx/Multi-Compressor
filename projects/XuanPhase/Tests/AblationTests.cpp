#include "../Source/PhaseEngine.h"
#include "../../AcademicAblations.h"
#include "../../AcademicTestUtils.h"
namespace
{
using namespace academic;
constexpr double pi = juce::MathConstants<double>::pi;
struct Variant { const char* name; PhaseOptions options; int baseline = 0; };
const std::array<Variant,8> variants {{
    {"bypass",{},1}, {"manual_fixed",{},2}, {"current",{}},
    {"no_residual_anchor",{false,true,true,true,24}}, {"no_slope",{true,false,true,true,24}},
    {"six_peaks",{true,true,true,true,6}}, {"no_unwrap",{true,true,false,true,24}},
    {"linear_interpolation",{true,true,true,false,24}}
}};
int orderFor (int n) { int order = 0; while ((1<<order) < n) ++order; return order; }
std::vector<float> render (const std::vector<float>& input, double rate, const xuan::Settings& s, const xuan::Analysis& a)
{
    xuan::PhaseEngine engine; engine.prepare (rate,[s] { return s; },false); engine.setOffline (s,a);
    const auto count = 1<<orderFor (int (input.size())+engine.latency()+int (rate*.05));
    std::vector<float> out (size_t(count),0);
    for (int at = 0; at < count; at += 128)
    {
        juce::AudioBuffer<float> block (2,std::min(128,count-at)); block.clear();
        for (int i = 0; i < block.getNumSamples(); ++i)
            if (at+i < int (input.size())) block.setSample (0,i,input[size_t(at+i)]), block.setSample (1,i,input[size_t(at+i)]);
        engine.process (block,s);
        for (int i = 0; i < block.getNumSamples(); ++i) out[size_t(at+i)] = block.getSample(0,i);
    }
    return out;
}
std::complex<double> response (const std::vector<float>& ir, double hz, double rate)
{
    std::complex<double> sum {};
    for (size_t i = 0; i < ir.size(); ++i) sum += double(ir[i])*std::polar(1.,-2*pi*hz*i/rate);
    return sum*std::polar(1.,2*pi*hz*juce::roundToInt(rate*.025)/rate);
}
double dispersion (const std::vector<float>& audio, double rate, const xuan::Analysis& reference)
{
    using C = std::complex<float>;
    juce::dsp::FFT fft (orderFor(int(audio.size())));
    std::vector<C> time(audio.size()), spectrum(audio.size());
    for (size_t i = 0; i < audio.size(); ++i) time[i] = audio[i];
    fft.perform(time.data(),spectrum.data(),false);
    double sum = 0, squared = 0; int count = 0;
    for (int band = 0; band < reference.count; ++band)
    {
        const auto hz = reference.targets[size_t(band)].frequency;
        std::vector<C> selected(audio.size()), envelope(audio.size());
        const auto width = std::max(70.,hz*.06);
        // Positive-frequency analytic band; the squared modulus gives envelope
        // energy without carrier-phase-dependent zero crossings.
        for (size_t k = 1; k < audio.size()/2; ++k)
            selected[k] = spectrum[k]*float(std::exp(-.5*std::pow((k*rate/audio.size()-hz)/width,2)));
        fft.perform(selected.data(),envelope.data(),true);
        double weighted = 0, energy = 0;
        for (size_t i = 0; i < envelope.size(); ++i) { const auto e = std::norm(envelope[i]); weighted += e*i/rate; energy += e; }
        if (energy < 1e-15) continue;
        const auto arrival = weighted/energy; sum += arrival; squared += arrival*arrival; ++count;
    }
    if (count < 2) return 0;
    return 1000*std::sqrt(std::max(0.,squared/count-std::pow(sum/count,2)));
}
}
int main (int argc, char** argv)
{
    try
    {
        const std::filesystem::path directory = argc > 1 ? argv[1] : "academic-results";
        manifest(directory,"XuanPhase");
        auto out = csv(directory,"phase.csv","rate,corpus,variant,targets,onset,anchor_ms,peak,rms,crest_db,arrival_dispersion_ms,mean_phase_error_deg,magnitude_ripple_db,pre_window_energy,tail_energy");
        auto curves = csv(directory,"response.csv","rate,corpus,variant,frequency_hz,requested_deg,measured_deg,gain_db");
        auto targetFile = csv(directory,"phase-targets.csv","rate,corpus,variant,frequency_hz,angle_deg,delay_ms");
        constexpr std::array<const char*,3> names {"dispersed_packets","broadband_attack","attack_over_sustain"};
        int anchorWitness = 0, slopeWitness = 0, peakWitness = 0;
        for (double rate : {44100.,48000.,96000.})
            for (int corpus = 0; corpus < 3; ++corpus)
            {
                const auto n = 1<<orderFor(juce::roundToInt(rate*.04));
                std::vector<float> input(size_t(n),0), background(size_t(n),0);
                for (int i = 0; i < n; ++i)
                {
                    const auto t = i/rate;
                    if (corpus == 1)
                    {
                        for (int bin = 4; bin*rate/n < std::min(19500.,rate*.43); bin += 13)
                        {
                            const auto centre = .022+(bin%5)*.0003;
                            input[size_t(i)] += float (.012*std::exp(-.5*std::pow((t-centre)/.002,2))*std::cos(2*pi*bin*rate/n*(t-centre)+.3*(bin%7)));
                        }
                    }
                    else
                        for (int band = 0; band < 4; ++band)
                        {
                            const auto hz = std::array<double,4> {281.25,843.75,2437.5,6750}[size_t(band)];
                            const auto centre = corpus == 2 ? n/rate-.008+band*.0006 : .021875+band*55./48000;
                            input[size_t(i)] += float (.15*std::exp(-.5*std::pow((t-centre)/(70./48000),2))*std::cos(2*pi*hz*(t-centre)));
                        }
                    if (corpus == 2)
                    {
                        background[size_t(i)] = float (.25*std::sin(2*pi*220*t+.3));
                        input[size_t(i)] += background[size_t(i)];
                    }
                }
                const auto analyse = [&] (PhaseOptions options)
                {
                    phase = options; std::vector<float> previous; double energy = 0; xuan::OnsetState state;
                    // Paired snapshots isolate residual anchoring while keeping
                    // the exact same background/detector state for all variants.
                    for (int warm = 0; warm < 80; ++warm) xuan::PhaseEngine::analyse(background,rate,previous,energy,nullptr,&state,n+warm*128);
                    return xuan::PhaseEngine::analyse(input,rate,previous,energy,nullptr,&state,n+80*128);
                };
                const auto reference = analyse({});
                if (reference.count == 0) std::cerr << "No targets: " << rate << " " << names[size_t(corpus)] << " novelty=" << reference.novelty << " flux=" << reference.flux << " crest=" << reference.crest << "\n";
                check(reference.count > 0,"synthetic attack yields phase targets");
                std::array<xuan::Analysis,8> analyses;
                for (size_t v = 0; v < variants.size(); ++v)
                {
                    const auto& variant = variants[v]; auto analysis = analyse(variant.options); analyses[v] = analysis;
                    xuan::Settings settings; settings.automatic = variant.baseline == 0; settings.depth = 100;
                    if (variant.baseline == 2) { settings.low = 45; settings.high = -45; }
                    if (variant.baseline == 1) analysis = {};
                    phase = variant.options;
                    const auto audio = render(input,rate,settings,analysis);
                    if (rate == 48000 && (v == 0 || v == 2))
                    {
                        const auto latency = 2*juce::roundToInt(rate*.025);
                        if (v == 0) for (size_t i = 0; i < input.size(); ++i)
                            check(std::abs(audio[i+size_t(latency)]-input[i]) < 1e-5,
                                  "phase reference matches input after declared latency removal");
                        audioCapture(directory,std::string("audio-")+names[size_t(corpus)]+"-"+variant.name+".csv",
                                     input,input,audio,audio,rate,latency);
                    }
                    const auto ir = xuan::PhaseEngine::impulse(rate,settings,analysis);
                    double error = 0;
                    for (int k = 0; k < reference.count; ++k)
                    {
                        const auto& target = reference.targets[size_t(k)];
                        error += std::abs(std::remainder(std::arg(response(ir,target.frequency,rate))-target.angle,2*pi))*180/pi;
                    }
                    double minGain = 1e9, maxGain = -1e9;
                    for (int point = 0; point <= 96; ++point)
                    {
                        const auto hz = 40*std::pow(std::min(18000.,rate*.4)/40,point/96.);
                        const auto h = response(ir,hz,rate); const auto gain = db(std::abs(h));
                        minGain = std::min(minGain,gain); maxGain = std::max(maxGain,gain);
                        const auto requested = xuan::PhaseEngine::phaseAt(hz,settings,analysis);
                        check (std::isfinite(gain) && std::abs(requested) <= 150*pi/180+1e-6,"rendered response finite and requested correction capped");
                        curves << rate << ',' << names[size_t(corpus)] << ',' << variant.name << ',' << hz << ',' << requested*180/pi << ',' << std::arg(h)*180/pi << ',' << gain << '\n';
                    }
                    for (int k = 0; k < analysis.count; ++k)
                    {
                        const auto& t = analysis.targets[size_t(k)];
                        targetFile << rate << ',' << names[size_t(corpus)] << ',' << variant.name << ',' << t.frequency << ',' << t.angle*180/pi << ',' << t.delay*1000 << '\n';
                        check (std::abs(t.delay) <= .025001,"estimated delay bounded");
                    }
                    if (v == 3 && std::abs(analysis.anchor-reference.anchor) > 1/rate) ++anchorWitness;
                    if (v == 4) for (int k = 0; k < reference.count; ++k) if (std::abs(reference.targets[size_t(k)].delay) > 1e-6) ++slopeWitness;
                    if (v == 5 && analysis.count < reference.count) ++peakWitness;
                    double pre = 0, tail = 0;
                    for (size_t i = 0; i < audio.size(); ++i)
                    {
                        check (std::isfinite(audio[i]),"output samples finite");
                        const auto time = i/rate-.05;
                        if (time < .015) pre += audio[i]*audio[i];
                        if (time > .033) tail += audio[i]*audio[i];
                    }
                    out << rate << ',' << names[size_t(corpus)] << ',' << variant.name << ',' << analysis.count << ',' << analysis.onset << ',' << analysis.anchor*1000 << ',' << peak(audio) << ',' << rms(audio) << ',' << db(peak(audio)/std::max(1e-15,rms(audio))) << ',' << dispersion(audio,rate,reference) << ',' << error/reference.count << ',' << maxGain-minGain << ',' << pre << ',' << tail << '\n';
                }
            }
        check(anchorWitness > 0,"corpus exercises residual anchoring");
        check(slopeWitness > 0,"corpus exercises phase-slope timing");
        check(peakWitness > 0,"broadband corpus exercises reduced peak budget");
        // An explicit seam probe isolates interpolation independent of detection.
        xuan::Analysis seam; seam.count = 2;
        seam.targets[0] = {1000,100, float(179*pi/180),0}; seam.targets[1] = {1500,100,float(-179*pi/180),0};
        phase = {}; const auto unwrapped = xuan::PhaseEngine::bandValue(1250,seam,false);
        phase.unwrap = false; const auto raw = xuan::PhaseEngine::bandValue(1250,seam,false);
        check(std::abs(unwrapped-pi) < 1e-5 && std::abs(raw) < 1e-5,"unwrapping ablation crosses zero at the seam");
        seam.targets[0].angle = .2f; seam.targets[1].angle = 1.2f;
        phase = {}; const auto smooth = xuan::PhaseEngine::bandValue(1100,seam,false);
        phase.smooth = false; const auto linear = xuan::PhaseEngine::bandValue(1100,seam,false);
        check(std::abs(smooth-linear) > .05,"interpolation ablation changes off-center response");
        auto probes = csv(directory,"interpolation-probes.csv","probe,current,ablated,units");
        probes << "phase_seam," << unwrapped << ',' << raw << ",radians\nquarter_interval," << smooth << ',' << linear << ",radians\n";
        std::cout << checks << " phase ablation checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
