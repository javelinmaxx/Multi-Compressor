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

void captureNoveltyHandoff(const std::filesystem::path& directory)
{
    constexpr int rate = 48000, n = 2048, hop = 128, count = 48000;
    const std::array<int,2> starts {12000,36000};
    std::vector<float> input(count), background(count), attack(count), frame(n), previous;
    auto audio = csv(directory,"novelty-audio.csv","sample,rate,input,background,attack,expected_onset");
    for (int i = 0; i < count; ++i)
    {
        const auto t = i/double(rate);
        background[size_t(i)] = float(.25*std::sin(2*pi*220*t)+.10*std::sin(2*pi*330*t));
        for (int start : starts)
        {
            const auto age = (i-start)/double(rate);
            if (age >= 0 && age <= .12)
                attack[size_t(i)] += float(.035*(1-std::exp(-age/.0015))*std::exp(-age/.022)
                    *(std::cos(2*pi*2437.5*age)+.4*std::cos(2*pi*6750*age)));
        }
        input[size_t(i)] = background[size_t(i)]+attack[size_t(i)];
        audio << i << ',' << rate << ',' << input[size_t(i)] << ',' << background[size_t(i)] << ','
              << attack[size_t(i)] << ',' << (i == starts[0] || i == starts[1]) << '\n';
    }
    auto frames = csv(directory,"novelty-frames.csv","frame_sample,rate,frame_size,hop,novelty,threshold,emerging_score,active_peaks,crest,flux,onset,estimating,targets,anchor_sample,input_peak_sample,noise_floor,largest_magnitude");
    auto spectra = csv(directory,"novelty-spectrum.csv","frame_sample,bin,frequency_hz,input_magnitude,previous_neighbour_max,novelty_magnitude,event_novelty_magnitude");
    frames.precision(17); spectra.precision(17);
    PhaseDetectionTrace trace, selectedTrace; xuan::Analysis selected;
    struct Reset { ~Reset() { phaseDetectionTrace = nullptr; } } reset;
    phase = {}; phaseDetectionTrace = &trace;
    xuan::OnsetState detector; double energy = 0;
    std::vector<int> events; int firstEvent = -1, selectedFrame = -1;
    for (int at = hop; at <= count; at += hop)
    {
        for (int i = 0; i < n; ++i) frame[size_t(i)] = at-n+i >= 0 ? input[size_t(at-n+i)] : 0.f;
        const auto analysis = xuan::PhaseEngine::analyse(frame,rate,previous,energy,nullptr,&detector,at);
        if (at < 9600) continue; // Display only after the 200 ms baseline warmup.
        frames << at << ',' << rate << ',' << n << ',' << hop << ',' << analysis.novelty << ','
               << trace.threshold << ',' << trace.emergingScore << ',' << trace.activePeaks << ','
               << analysis.crest << ',' << analysis.flux << ',' << analysis.onset << ',' << trace.estimating << ','
               << analysis.count << ',' << at-n+trace.anchor << ',' << at-n+trace.inputPeak << ','
               << trace.floor << ',' << trace.largest << '\n';
        for (int k = 1; k < n/2; ++k)
            spectra << at << ',' << k << ',' << k*rate/double(n) << ',' << trace.magnitude[size_t(k)] << ','
                    << trace.reference[size_t(k)] << ',' << trace.novel[size_t(k)] << ',' << trace.collected[size_t(k)] << '\n';
        if (analysis.onset)
        {
            events.push_back(at);
            check(analysis.novelty > trace.threshold,"observed new event crosses the actual adaptive novelty threshold");
            check(trace.residualUsed && analysis.count > 0,"observed detector event supplies residual-anchored phase targets");
            if (firstEvent < 0)
            {
                firstEvent = at; selectedTrace = trace; selected = analysis; selectedFrame = at;
            }
        }
        if (!trace.estimating) check(analysis.count == 0,"no phase targets are returned outside detected event/context frames");
    }
    check(events.size() == starts.size(),"masked two-hit fixture has exactly two post-warmup detector events");
    for (size_t i = 0; i < starts.size(); ++i)
        check(events[i] >= starts[i] && events[i]-starts[i] < rate*.03,"each masked attack is detected within 30 ms");
    check(selectedFrame == firstEvent && selectedTrace.residualUsed,"figure captures the first actual detector-to-estimator handoff");
    check(selectedTrace.anchor == int(std::max_element(selectedTrace.residual.begin(),selectedTrace.residual.end(),
          [](float a, float b) { return std::abs(a) < std::abs(b); })-selectedTrace.residual.begin()),
          "phase anchor is the actual positive-spectral-residual absolute peak");
    check(selectedFrame-n+selectedTrace.anchor >= starts[0] && selectedFrame-n+selectedTrace.anchor < starts[0]+rate*.03,
          "selected phase anchor belongs to the newly emerging masked transient");
    auto context = csv(directory,"novelty-context.csv","frame_sample,sample,input,windowed_input,residual,is_anchor,is_input_peak");
    context.precision(17);
    for (int i = 0; i < n; ++i)
        context << selectedFrame << ',' << selectedFrame-n+i << ',' << input[size_t(selectedFrame-n+i)] << ','
                << selectedTrace.windowed[size_t(i)] << ',' << selectedTrace.residual[size_t(i)] << ','
                << (i == selectedTrace.anchor) << ',' << (i == selectedTrace.inputPeak) << '\n';
    auto peaks = csv(directory,"novelty-peaks.csv","frequency_hz,bin,anchor_index,anchor_value,left_real,left_imag,real,imag,right_real,right_imag,target_angle_rad,relative_delay_s,interpolated_deg,requested_deg");
    auto curve = csv(directory,"novelty-target-curve.csv","frequency_hz,interpolated_deg,requested_deg");
    peaks.precision(17); curve.precision(17);
    xuan::Settings settings; settings.automatic = true; settings.depth = 100;
    for (int i = 0; i < selected.count; ++i)
    {
        const auto& target = selected.targets[size_t(i)];
        const auto k = size_t(juce::roundToInt(target.frequency*n/rate));
        peaks << target.frequency << ',' << k << ',' << selectedTrace.anchor << ',' << selectedTrace.anchorValue << ','
              << selectedTrace.spectrum[k-1].real() << ',' << selectedTrace.spectrum[k-1].imag() << ','
              << selectedTrace.spectrum[k].real() << ',' << selectedTrace.spectrum[k].imag() << ','
              << selectedTrace.spectrum[k+1].real() << ',' << selectedTrace.spectrum[k+1].imag() << ','
              << target.angle << ',' << target.delay << ',' << xuan::PhaseEngine::bandValue(target.frequency,selected,false)*(180/pi) << ','
              << xuan::PhaseEngine::phaseAt(target.frequency,settings,selected)*(180/pi) << '\n';
    }
    for (int i = 0; i <= 1024; ++i)
    {
        const auto hz = 60*std::pow(20000./60,i/1024.);
        curve << hz << ',' << xuan::PhaseEngine::bandValue(hz,selected,false)*(180/pi) << ','
              << xuan::PhaseEngine::phaseAt(hz,settings,selected)*(180/pi) << '\n';
    }
}

}
int main (int argc, char** argv)
{
    try
    {
        const std::filesystem::path directory = argc > 1 ? argv[1] : "academic-results";
        manifest(directory,"APRiCOT");
        auto out = csv(directory,"phase.csv","rate,corpus,variant,targets,onset,anchor_ms,peak,rms,crest_db,arrival_dispersion_ms,mean_phase_error_deg,magnitude_ripple_db,pre_window_energy,tail_energy");
        auto curves = csv(directory,"response.csv","rate,corpus,variant,frequency_hz,requested_deg,measured_deg,gain_db");
        auto kernels = csv(directory,"fir-kernels.csv","rate,corpus,sample,causal_ms,relative_ms,coefficient");
        auto ideal = csv(directory,"fir-response.csv","rate,corpus,frequency_hz,interpolated_deg,requested_deg,real,imag,gain_db,phase_error_deg");
        auto peaks = csv(directory,"fir-peaks.csv","rate,corpus,frequency_hz,raw_angle_deg,interpolated_deg,requested_deg");
        auto targetFile = csv(directory,"phase-targets.csv","rate,corpus,variant,frequency_hz,angle_deg,delay_ms");
        kernels.precision(17); ideal.precision(17); peaks.precision(17);
        constexpr std::array<const char*,3> names {"dispersed_packets","broadband_attack","attack_over_sustain"};
        int anchorWitness = 0, slopeWitness = 0, peakWitness = 0, uncappedWitness = 0;
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
                    if (v == 2)
                    {
                        const auto centre = juce::roundToInt(rate*.025);
                        check(ir.size() == size_t(2*centre+1),"actual FIR has finite plus/minus 25 ms support");
                        check(std::abs(ir.front()) < 1e-12 && std::abs(ir.back()) < 1e-12,
                              "actual FIR taper reaches zero at both support boundaries");
                        // Independent audio-path check: the FIR sits after a
                        // separate 25 ms lookahead, not a 50 ms kernel shift.
                        const auto pulse = render(std::vector<float>{1.f},rate,settings,analysis);
                        const auto lookahead = juce::roundToInt(rate*.05)-centre;
                        for (size_t i = 0; i < ir.size(); ++i)
                        {
                            check(std::isfinite(ir[i]),"exported FIR coefficients are finite");
                            check(std::abs(pulse[size_t(lookahead)+i]-ir[i]) < 2e-5,
                                  "partitioned convolution of a unit pulse reproduces the exported FIR");
                            kernels << rate << ',' << names[size_t(corpus)] << ',' << i << ','
                                    << 1000*i/rate << ',' << 1000*(double(i)-centre)/rate << ',' << ir[i] << '\n';
                        }
                        xuan::Settings neutral;
                        const auto identity = xuan::PhaseEngine::impulse(rate,neutral);
                        check(identity.size() == ir.size() && identity[size_t(centre)] == 1.f,
                              "neutral FIR is an exact unit impulse at the declared centre");
                        const auto referenceResponse = response(identity,1234.5,rate);
                        check(std::abs(referenceResponse-std::complex<double>(1,0)) < 1e-10,
                              "centre-delay removal leaves the neutral response at unity and zero phase");
                        std::vector<double> frequencies;
                        const auto upper = std::min(20000.,rate*.49);
                        for (int i = 0; i <= 2048; ++i) frequencies.push_back(20*std::pow(upper/20,i/2048.));
                        for (int i = 0; i < analysis.count; ++i)
                        {
                            const auto hz = double(analysis.targets[size_t(i)].frequency);
                            check(hz >= 20 && hz <= upper,"selected peak lies within the plotted frequency range");
                            frequencies.push_back(hz);
                            peaks << rate << ',' << names[size_t(corpus)] << ',' << hz << ','
                                  << analysis.targets[size_t(i)].angle*(180/pi) << ','
                                  << xuan::PhaseEngine::bandValue(hz,analysis,false)*(180/pi) << ','
                                  << xuan::PhaseEngine::phaseAt(hz,settings,analysis)*(180/pi) << '\n';
                        }
                        std::sort(frequencies.begin(),frequencies.end());
                        frequencies.erase(std::unique(frequencies.begin(),frequencies.end()),frequencies.end());
                        for (const auto hz : frequencies)
                        {
                            const auto h = response(ir,hz,rate);
                            const auto requested = xuan::PhaseEngine::phaseAt(hz,settings,analysis);
                            const auto gain = db(std::abs(h));
                            const auto residual = std::remainder(std::arg(h)-requested,2*pi)*(180/pi);
                            check(std::isfinite(gain) && std::isfinite(residual),"dense realized FIR response is finite");
                            ideal << rate << ',' << names[size_t(corpus)] << ',' << hz << ','
                                  << xuan::PhaseEngine::bandValue(hz,analysis,false)*(180/pi) << ','
                                  << requested*(180/pi) << ',' << h.real() << ',' << h.imag() << ','
                                  << gain << ',' << residual << '\n';
                        }
                    }
                    double error = 0;
                    for (int k = 0; k < reference.count; ++k)
                    {
                        const auto& target = reference.targets[size_t(k)];
                        error += std::abs(std::remainder(std::arg(response(ir,target.frequency,rate))-target.angle,2*pi))*(180/pi);
                    }
                    double minGain = 1e9, maxGain = -1e9;
                    for (int point = 0; point <= 96; ++point)
                    {
                        const auto hz = 40*std::pow(std::min(18000.,rate*.4)/40,point/96.);
                        const auto h = response(ir,hz,rate); const auto gain = db(std::abs(h));
                        minGain = std::min(minGain,gain); maxGain = std::max(maxGain,gain);
                        const auto requested = xuan::PhaseEngine::phaseAt(hz,settings,analysis);
                        check (std::isfinite(gain) && std::isfinite(requested),"rendered response and uncapped phase request remain finite");
                        if (std::abs(requested) > 150*pi/180+1e-4) ++uncappedWitness;
                        curves << rate << ',' << names[size_t(corpus)] << ',' << variant.name << ',' << hz << ',' << requested*(180/pi) << ',' << std::arg(h)*(180/pi) << ',' << gain << '\n';
                    }
                    for (int k = 0; k < analysis.count; ++k)
                    {
                        const auto& t = analysis.targets[size_t(k)];
                        targetFile << rate << ',' << names[size_t(corpus)] << ',' << variant.name << ',' << t.frequency << ',' << t.angle*(180/pi) << ',' << t.delay*1000 << '\n';
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
        check(uncappedWitness > 0,"academic fixtures exercise requests beyond the former phase limit");
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
        captureNoveltyHandoff(directory);
        std::cout << checks << " phase ablation checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
