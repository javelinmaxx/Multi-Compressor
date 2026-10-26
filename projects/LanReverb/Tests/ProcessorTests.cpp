#include "../Source/PluginEditor.h"
#include "../../StagePluginTests.h"
#include <filesystem>
#include <random>
using namespace stageTests;
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
struct Audio { std::vector<float> left, right; float maxReduction = 0; uint32_t jobs = 0; };
Audio render (lan::Settings settings, double seconds, double rate, int signal, int blockSize = 128, bool automate = false, bool tapSwitch = false)
{
    lan::ReverbEngine engine; engine.prepare(rate,false);
    Audio out; const auto count = juce::roundToInt(seconds*rate); out.left.reserve(size_t(count)); out.right.reserve(size_t(count));
    std::mt19937 rng(72463); std::uniform_real_distribution<float> noise(-.25f,.25f);
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
    check(result.bands[0].gainDb < -1 && result.bands[0].gainDb >= -9,"resonance correction requests bounded attenuation");
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
void tapTests()
{
    std::vector<float> data(4096,1.f); lan::MovingTap tap; tap.prepare(48000,100);
    for (int i = 0; i < 20000; ++i)
    {
        if (i%31 == 0) tap.target(float(1+i%3500));
        check(std::abs(tap.read(data,size_t(i)%data.size())-1) < 1e-6,"rapid queued tap automation is a convex DC-unity fade");
    }
    check(tap.current() > 100,"queued tap changes eventually advance the read head");
    // A 50 ms queued fade must not introduce a one-sample step on a stationary
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
void processorTests()
{
    stateTests<LanReverbProcessor>(13);
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
        const auto out = render(settings,10,48000,1);
        spectra[size_t(mode)] = spectrum(out);
        late[size_t(mode)] = energy(out,3,8,48000); if (mode == 1) correction = out.maxReduction;
        for (int i = 0; i < 100; ++i) resonances << mode << ',' << i*.1 << ',' << std::sqrt(energy(out,i*.1,(i+1)*.1,48000)) << ',' << out.maxReduction << '\n';
        if (mode == 1) writeWav(directory.getChildFile("tamed-tail.wav"),out,48000,8*48000);
    }
    std::ofstream spectral(directory.getChildFile("spectra.csv").getFullPathName().toStdString()); spectral << "frequency_hz,off_power,eq_power,phase_power,combined_power\n";
    for (size_t k = 1; k < spectra[0].size(); ++k) spectral << k*48000./4096 << ',' << spectra[0][k] << ',' << spectra[1][k] << ',' << spectra[2][k] << ',' << spectra[3][k] << '\n';
    std::cout << "Tail control: off=" << late[0] << " EQ=" << late[1] << " phase=" << late[2] << " cut=" << correction << " dB\n";
    check(correction > 1,"actual feedback ringing triggers automatic EQ");
    check(late[1] < late[0]*.9,"automatic EQ reduces actual late ringing energy");
    check(late[3] < late[0]*.9,"combined phase and EQ controls reduce actual late ringing energy");
    lan::Settings shifted; shifted.mix = 1; shifted.decaySeconds = 30; shifted.diffusion = .1f; shifted.phase = 0; shifted.automatic = false;
    const auto shiftedAudio = render(shifted,10,48000,1,128,false,true);
    const auto shiftedSpectrum = spectrum(shiftedAudio);
    double dot = 0, beforeNorm = 0, afterNorm = 0;
    std::ofstream tapReport(directory.getChildFile("tap-shift.csv").getFullPathName().toStdString()); tapReport << "frequency_hz,fixed_power,shifted_power\n";
    for (size_t k = 1; k < shiftedSpectrum.size(); ++k)
    {
        dot += spectra[0][k]*shiftedSpectrum[k]; beforeNorm += spectra[0][k]*spectra[0][k]; afterNorm += shiftedSpectrum[k]*shiftedSpectrum[k];
        tapReport << k*48000./4096 << ',' << spectra[0][k] << ',' << shiftedSpectrum[k] << '\n';
    }
    const auto similarity = dot/std::sqrt(std::max(1e-300,beforeNorm*afterNorm));
    std::cout << "Tap spectral similarity=" << similarity << " combined power=" << late[3] << '\n';
    check(energy(shiftedAudio,3,8,48000) > 1e-15,"tap-shift comparison contains a measurable tail");
    check(similarity < .99,"manual feedback tap shift changes an already ringing spectrum");
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
        else { analyserTests(directory); tapTests(); processorTests(); scatterTests(directory); measurements(directory); }
        std::cout << checks << " Lan Reverb checks passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
