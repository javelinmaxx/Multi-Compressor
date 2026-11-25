#include "../Source/PluginEditor.h"
#include "../../ParameterRetentionTests.h"
#include "../../MonoStereoRoutingTests.h"
#include "../../StagePluginTests.h"
using namespace stageTests;
namespace
{
void referenceTests()
{
    // Captured from commit b5297a76ba before the shared-stage extraction, with
    // Haas center and variability at zero; this fixture is independent of the new code.
    std::ifstream file (juce::File (STAGE_PROJECT_DIR).getChildFile ("Tests/Fixtures/kuo-stereo-velvet.csv").getFullPathName().toStdString());
    check (file.good(), "pre-refactor Kuo Stereo audio fixture exists");
    std::string line; std::getline (file, line);
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        KuoVelvetProcessor p;
        set (p, "width", 85); set (p, "transientProtect", 100); set (p, "guardAttack", 0.7f);
        p.prepareToPlay (rate, 127);
        check (p.getLatencySamples() == juce::roundToInt (0.0015 * rate), "Kuo Velvet retains 1.5 ms lookahead");
        uint32_t state = 12345; juce::MidiBuffer midi; double error = 0;
        for (int base = 0; base < 8192; base += 127)
        {
            const int n = std::min (127, 8192 - base); juce::AudioBuffer<float> b (2, n);
            if (base > 4096) set (p, "guardAttack", 5);
            for (int i = 0; i < n; ++i)
            {
                state = state * 1664525u + 1013904223u;
                const auto noise = (static_cast<float> (state >> 8) / 8388608.0f - 1.0f) * 0.075f;
                const auto t = static_cast<double> (base + i) / rate;
                const auto x = noise + static_cast<float> (0.12 * std::sin (2 * 3.141592653589793 * 220 * t));
                b.setSample (0, i, x + ((base+i) % 2000 == 0 ? 0.9f : 0.0f)); b.setSample (1, i, x * 0.6f);
            }
            p.processBlock (b, midi);
            for (int i = 0; i < n; ++i)
            {
                if (! std::getline (file, line)) throw std::runtime_error ("incomplete reference fixture");
                std::replace (line.begin(), line.end(), ',', ' '); std::istringstream row (line);
                double storedRate = 0, left = 0, right = 0; int index = 0; row >> storedRate >> index >> left >> right;
                if (! row || std::abs (storedRate - rate) > 0.01 || index != base + i) throw std::runtime_error ("invalid reference fixture");
                error = std::max ({ error, std::abs (b.getSample (0, i) - left), std::abs (b.getSample (1, i) - right) });
            }
        }
        check (error < 1.0e-6, "Velvet matches original Kuo Stereo at unity band levels with transient protection and automation");
        std::cout << "Pre-refactor parity at " << rate << " Hz: " << error << " maximum error\n";
    }
}
void bandStateTests()
{
    KuoVelvetProcessor baseline, restored;
    set (baseline, "width", 70); set (baseline, "velvetLow", 0);
    set (baseline, "velvetMid", 175); set (baseline, "velvetHigh", 25);
    set (baseline, "side", 65);
    juce::MemoryBlock data; baseline.getStateInformation (data);
    restored.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    for (const auto* id : { "width", "velvetLow", "velvetMid", "velvetHigh", "side" })
        check (std::abs (baseline.parameters.getRawParameterValue (id)->load()
               - restored.parameters.getRawParameterValue (id)->load()) < 1.0e-5f,
               "band and Side settings survive state restoration");
    baseline.prepareToPlay (48000, 128); restored.prepareToPlay (48000, 128);
    juce::MidiBuffer midi; uint32_t random = 2468; double error = 0;
    for (int base = 0; base < 8192; base += 128)
    {
        juce::AudioBuffer<float> a (2, 128), b;
        for (int i = 0; i < 128; ++i)
        {
            random = random * 1664525u + 1013904223u;
            const auto input = (static_cast<float> (random >> 8) / 8388608.0f - 1.0f) * 0.2f;
            a.setSample (0, i, input); a.setSample (1, i, input);
        }
        b.makeCopyOf (a); baseline.processBlock (a, midi); restored.processBlock (b, midi);
        for (int i = 0; i < 128; ++i)
        {
            for (int c = 0; c < 2; ++c)
                error = std::max (error, static_cast<double> (std::abs (a.getSample (c, i) - b.getSample (c, i))));
        }
    }
    check (error < 1.0e-6, "restored band and Side settings render identical audio");
}
double bandSideEnergy (float toneHz, std::array<float, 3> bands)
{
    KuoVelvetProcessor p;
    set (p, "width", 100); set (p, "transientProtect", 0);
    set (p, "velvetLow", bands[0]); set (p, "velvetMid", bands[1]); set (p, "velvetHigh", bands[2]);
    p.prepareToPlay (48000, 128);
    juce::MidiBuffer midi; double energy = 0;
    for (int base = 0; base < 16384; base += 128)
    {
        juce::AudioBuffer<float> b (2, 128);
        for (int i = 0; i < 128; ++i)
        {
            const auto x = 0.2f * std::sin (juce::MathConstants<float>::twoPi * toneHz * (base+i) / 48000.0f);
            b.setSample (0, i, x); b.setSample (1, i, x);
        }
        p.processBlock (b, midi);
        if (base >= 8192)
            for (int i = 0; i < 128; ++i)
            {
                const auto side = 0.5f * (b.getSample (0, i) - b.getSample (1, i));
                energy += side * side;
            }
    }
    return energy;
}
void bandAndSideTests()
{
    for (const auto tone : { 60.0f, 500.0f, 6000.0f })
    {
        const auto muted = bandSideEnergy (tone, { 0, 0, 0 });
        const auto active = bandSideEnergy (tone, { 100, 100, 100 });
        check (muted < 1.0e-10 && active > 0.01, "band sliders control generated side without changing dry stereo");
    }
    const auto loAt60 = bandSideEnergy (60, { 100, 0, 0 });
    const auto hiAt60 = bandSideEnergy (60, { 0, 0, 100 });
    const auto midAt500 = bandSideEnergy (500, { 0, 100, 0 });
    const auto loAt500 = bandSideEnergy (500, { 100, 0, 0 });
    const auto hiAt6000 = bandSideEnergy (6000, { 0, 0, 100 });
    const auto loAt6000 = bandSideEnergy (6000, { 100, 0, 0 });
    check (loAt60 > hiAt60 * 10 && midAt500 > loAt500 * 2 && hiAt6000 > loAt6000 * 10,
           "200 Hz and 1 kHz crossovers route low, mid and high tones to their sliders");
    for (float width : { 0.0f, 100.0f })
    {
        KuoVelvetProcessor p;
        set (p, "width", width); set (p, "side", 0); set (p, "transientProtect", 0);
        p.prepareToPlay (48000, 128);
        juce::MidiBuffer midi; double error = 0;
        for (int base = 0; base < 8192; base += 128)
        {
            juce::AudioBuffer<float> b (2, 128);
            for (int i = 0; i < 128; ++i)
            {
                b.setSample (0, i, 0.15f * std::sin ((base+i) * 0.09f));
                b.setSample (1, i, 0.12f * std::cos ((base+i) * 0.11f));
            }
            p.processBlock (b, midi);
            for (int i = 0; i < 128; ++i)
                error = std::max (error, static_cast<double> (std::abs (b.getSample (0, i) - b.getSample (1, i))));
        }
        check (error < 1.0e-7, "zero Side removes both original and generated stereo side");
    }
    KuoVelvetProcessor p; set (p, "width", 0); set (p, "side", 100);
    p.prepareToPlay (48000, 128);
    juce::AudioBuffer<float> b (2, 256); b.clear(); b.setSample (0, 0, 1); b.setSample (1, 0, -0.5f);
    juce::MidiBuffer midi; p.processBlock (b, midi);
    check (std::abs (b.getSample (0, 72) - 1) < 1.0e-7f && std::abs (b.getSample (1, 72) + 0.5f) < 1.0e-7f,
           "full Side retains the original stereo field at zero Width");
}
float impulseEnergy (float protect, float attack)
{
    KuoVelvetProcessor p; set (p, "width", 100); set (p, "transientProtect", protect); set (p, "guardAttack", attack);
    p.prepareToPlay (48000, 128); juce::MidiBuffer midi; float energy = 0;
    for (int base = 0; base < 8192; base += 128)
    {
        juce::AudioBuffer<float> b (2, 128); b.clear();
        if (base <= 4000 && base + 128 > 4000) { b.setSample (0, 4000-base, 1); b.setSample (1, 4000-base, 1); }
        p.processBlock (b, midi);
        for (int i = 0; i < 128; ++i)
        {
            const auto side = 0.5f * (b.getSample (0, i) - b.getSample (1, i)); energy += side * side;
        }
    }
    return energy;
}
void velvetTests()
{
    for (float rate : { 44100.f, 48000.f, 96000.f })
    {
        kuo::VelvetStage stage; stage.prepare (rate, { 1, 1, 1 }, false);
        stage.setParameters (0, 1, 0, { 1, 1, 1 });
        bool onset = false;
        for (int i = 0; i < 4096; ++i) onset |= stage.process (i >= 4000 ? 1.f : 0.f).onset;
        check (! stage.isConvolving() && onset, "zero Width skips convolution while keeping the shared Haas onset detector hot");
        stage.setParameters (1, 0, 0, { 1, 1, 1 });
        double energy = 0;
        for (int i = 0; i < 8192; ++i)
        {
            const auto side = stage.process (.2f*std::sin (i*.17f)).side;
            energy += side*side;
        }
        check (stage.isConvolving() && energy > .01, "raising Width immediately reactivates widening from live history");
        stage.setParameters (0, 0, 0, { 1, 1, 1 });
        for (int i = 0; i < 16000; ++i) stage.process (.2f*std::sin (i*.17f));
        check (! stage.isConvolving() && stage.process (.1f).side == 0.f, "Width fade reaches exact silence and suspends tap reads");
    }
    for (int mode = 0; mode < 2; ++mode)
    {
        KuoVelvetProcessor p; set (p, "transientProtect", 0); set (p, "width", mode == 1 ? 0.0f : 100.0f);
        p.prepareToPlay (48000, 128);
        juce::MidiBuffer midi; std::vector<float> input; double error = 0, sideEnergy = 0;
        uint32_t random = 98765;
        for (int base = 0; base < 16384; base += 128)
        {
            juce::AudioBuffer<float> b (2, 128);
            for (int i = 0; i < 128; ++i)
            {
                random = random * 1664525u + 1013904223u;
                const auto x = (static_cast<float> (random >> 8) / 8388608.0f - 1.0f) * 0.2f;
                input.push_back (x); b.setSample (0, i, x); b.setSample (1, i, x);
            }
            p.processBlock (b, midi);
            for (int i = 0; i < 128; ++i)
            {
                const auto index = base+i-72;
                const auto expected = index >= 0 ? input[static_cast<size_t> (index)] : 0.0f;
                const auto left = b.getSample (0, i), right = b.getSample (1, i);
                error = std::max (error, static_cast<double> (std::abs (0.5f * (left+right) - expected)));
                const auto side = 0.5f * (left-right); sideEnergy += side * side;
            }
        }
        check (error < 1.0e-6, "mono input's mid signal remains unchanged by Velvet");
        if (mode == 0) check (sideEnergy > 1.0, "full-band Velvet widens mono audio");
        else check (sideEnergy < 1.0e-10, "zero Width produces no additional side energy");
    }
    const auto dryEnergy = impulseEnergy (0, 0), protectedEnergy = impulseEnergy (100, 0), slowEnergy = impulseEnergy (100, 5);
    check (dryEnergy > 0.1f && protectedEnergy < dryEnergy * 0.01f, "full transient protection removes the isolated impulse's convolution tail");
    check (slowEnergy > protectedEnergy && slowEnergy < dryEnergy, "Attack controls the amount of convolution admitted at an onset");
    // Full protection must still widen a settled noise bed between real onsets.
    KuoVelvetProcessor noiseProcessor;
    set (noiseProcessor, "width", 100); set (noiseProcessor, "transientProtect", 100);
    noiseProcessor.prepareToPlay (48000, 128);
    juce::MidiBuffer noiseMidi; uint32_t noiseState = 13579; double noiseSideEnergy = 0;
    for (int base = 0; base < 16384; base += 128)
    {
        juce::AudioBuffer<float> b (2, 128);
        for (int i = 0; i < 128; ++i)
        {
            noiseState = noiseState * 1664525u + 1013904223u;
            const auto x = (static_cast<float> (noiseState >> 8) / 8388608.0f - 1.0f) * 0.1f;
            b.setSample (0, i, x); b.setSample (1, i, x);
        }
        noiseProcessor.processBlock (b, noiseMidi);
        if (base >= 8192)
            for (int i = 0; i < 128; ++i)
            {
                const auto side = 0.5f * (b.getSample (0, i) - b.getSample (1, i));
                noiseSideEnergy += side * side;
            }
    }
    check (noiseSideEnergy > 0.1, "Transient at 100 percent still widens a settled noise bed");
    for (bool hostBypass : { false, true })
    {
        KuoVelvetProcessor p; set (p, "output", 12); set (p, "bypass", hostBypass ? 0.0f : 1.0f); p.prepareToPlay (48000, 256);
        juce::AudioBuffer<float> b (2, 256); b.clear(); b.setSample (0, 0, 1); b.setSample (1, 0, -0.5f);
        juce::MidiBuffer midi;
        if (hostBypass) p.processBlockBypassed (b, midi); else p.processBlock (b, midi);
        check (std::abs (b.getSample (0, 72) - 1) < 1.0e-7f && std::abs (b.getSample (1, 72) + 0.5f) < 1.0e-7f,
               "host and parameter bypass preserve input level and lookahead latency");
    }
}
}
int main (int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        verifyParameterRetention<KuoVelvetProcessor>();
        monoStereoTests::run<KuoVelvetProcessor> (nullptr,true);
        stateTests<KuoVelvetProcessor> (9); referenceTests(); velvetTests(); bandStateTests(); bandAndSideTests();
        blockSizeTests<KuoVelvetProcessor>();
        if (argc > 2 && juce::String (argv[1]) == "--editor")
        {
            const juce::File clean (argc > 3 ? argv[3] : "");
            editorTests<KuoVelvetProcessor> (juce::File (argv[2]), 382, 8, 444,
                                             argc > 3 ? &clean : nullptr);
        }
        std::cout << checks << " Kuo Velvet checks passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
