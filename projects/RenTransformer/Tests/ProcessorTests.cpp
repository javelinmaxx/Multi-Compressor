#include "../Source/PluginEditor.h"
#include "../../ParameterRetentionTests.h"
#include "../../StagePluginTests.h"
using namespace stageTests;
namespace
{
void transformerTests()
{
    for (int i=0;i<=16000;++i)
    {
        const auto x=-16.f+32.f*i/16000.f;
        check(std::abs(character::transformerTanh.evaluate(x)-std::tanh(x))<8.e-7f,
              "shared transformer lookup matches analytic tanh through clipping and overload");
    }
    character::TransformerCore driveOnly;
    driveOnly.prepare (192000);
    double driveError = 0;
    for (int i = 0; i < 2048; ++i)
    {
        const auto input = static_cast<float> (0.31 * std::sin (2 * juce::MathConstants<double>::pi * 233 * i / 192000.0));
        const auto expected = std::tanh (2.0f * input) / 2.0f;
        driveError = std::max (driveError, static_cast<double> (std::abs (driveOnly.process (input, 2.0f, 0.7f, 0.0f) - expected)));
    }
    check (driveError < 1.0e-6, "zero Response leaves only drive saturation in the transformer core");
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (float coreDb : { 0.0f, 6.0f, 18.0f })
        {
            RenTransformerProcessor p; set (p, "drive", coreDb); set (p, "evenHarmonics", 35); set (p, "output", 3);
            p.prepareToPlay (rate, 127);
            check (p.getLatencySamples() == 64, "Ren reports Yinyang's 64 sample transformer latency");
            character::OversampledTransformer reference; reference.prepare (rate);
            juce::MidiBuffer midi;
            double error = 0;
            for (int base = 0; base < 4096; base += 127)
            {
                juce::AudioBuffer<float> b (2, 127);
                std::array<float, 127> expected {};
                for (int i = 0; i < 127; ++i)
                {
                    const auto x = static_cast<float> (0.23 * std::sin (2 * juce::MathConstants<double>::pi * 137 * (base+i) / rate));
                    b.setSample (0, i, x); b.setSample (1, i, x);
                    expected[static_cast<size_t> (i)] = reference.process (x * juce::Decibels::decibelsToGain (3.0f),
                        juce::Decibels::decibelsToGain (coreDb), 0.35f);
                }
                p.processBlock (b, midi);
                for (int i = 0; i < 127; ++i)
                    error = std::max (error, static_cast<double> (std::abs (b.getSample (0, i) - expected[static_cast<size_t> (i)])));
            }
            check (error < 1.0e-6, "Ren matches the original Yinyang transformer at three rates and drive levels");
        }
    {
        RenTransformerProcessor p; set (p, "response", 0); set (p, "evenHarmonics", 0);
        p.prepareToPlay (48000, 127);
        character::OversampledTransformer reference; reference.prepare (48000);
        juce::MidiBuffer midi;
        double error = 0, fullDifference = 0;
        character::OversampledTransformer full; full.prepare (48000);
        for (int base = 0; base < 4096; base += 127)
        {
            juce::AudioBuffer<float> b (2, 127);
            std::array<float, 127> expected {};
            for (int i = 0; i < 127; ++i)
            {
                const auto input = static_cast<float> (0.3 * std::sin (2 * juce::MathConstants<double>::pi * 137 * (base + i) / 48000.0));
                b.setSample (0, i, input); b.setSample (1, i, input);
                const auto drive = juce::Decibels::decibelsToGain (6.0f);
                expected[static_cast<size_t> (i)] = reference.process (input, drive, 0.0f, 0.0f);
                const auto complete = full.process (input, drive);
                if (base >= 1024) fullDifference += std::abs (expected[static_cast<size_t> (i)] - complete);
            }
            p.processBlock (b, midi);
            for (int i = 0; i < 127; ++i)
                error = std::max (error, static_cast<double> (std::abs (b.getSample (0, i) - expected[static_cast<size_t> (i)])));
        }
        check (fullDifference > 0.01, "Response endpoints produce distinct transformer sounds");
        check (error < 1.0e-6, "Ren zero Response follows the drive-only transformer path");
    }
    for (int mode = 0; mode < 4; ++mode)
    {
        RenTransformerProcessor p;
        set (p, "output", 6);
        if (mode == 0) set (p, "transformer", 0);
        if (mode == 1) set (p, "mix", 0);
        if (mode == 2) set (p, "bypass", 1);
        p.prepareToPlay (48000, 256);
        juce::AudioBuffer<float> b (2, 256); b.clear();
        b.setSample (0, 0, 0.5f); b.setSample (1, 0, -0.25f);
        juce::MidiBuffer midi;
        if (mode == 3) p.processBlockBypassed (b, midi); else p.processBlock (b, midi);
        const auto gain = mode < 2 ? juce::Decibels::decibelsToGain (6.0f) : 1.0f;
        bool match = true;
        for (int i = 0; i < 256; ++i)
            match &= std::abs (b.getSample (0, i) - (i == 64 ? gain * 0.5f : 0)) < 1.0e-6f
                  && std::abs (b.getSample (1, i) - (i == 64 ? gain * -0.25f : 0)) < 1.0e-6f;
        check (match, "Off, zero Mix, parameter bypass and host bypass preserve latency and correct gain");
    }
    ren::TransformerStage stage; stage.prepare (48000);
    juce::AudioBuffer<float> b (2, 2048); b.clear();
    stage.process (b, 6, 0, 0, 1, true, false);
    check (stage.isRunning(), "transformer runs when enabled");
    stage.process (b, 6, 0, 0, 1, false, false);
    check (! stage.isRunning(), "transformer DSP stops after the Off fade");
    stage.process (b, 6, 0, 0, 1, true, false);
    check (stage.isRunning(), "transformer restarts after warming its latency buffer");
    stage.process (b, 6, 0, 0, 1, true, true);
    check (stage.isRunning(), "selected transformer stays hot during bypass");
    stage.process (b, 6, 0, 0, 0, true, false);
    check (! stage.isRunning(), "zero Mix suspends the inaudible transformer after its fade");
    stage.process (b, 6, 0, 0, 1, true, false);
    check (stage.isRunning(), "raising Mix warms and reactivates the transformer");
    RenTransformerProcessor mono;
    auto buses = mono.getBusesLayout(); buses.inputBuses.set (0, juce::AudioChannelSet::mono());
    buses.outputBuses.set (0, juce::AudioChannelSet::mono());
    check (mono.setBusesLayout (buses), "mono processing is supported");
    mono.prepareToPlay (48000, 128); juce::AudioBuffer<float> m (1, 128); m.clear();
    juce::MidiBuffer midi; mono.processBlock (m, midi);
    check (std::isfinite (m.getMagnitude (0, 128)), "mono transformer output is finite");
}
}
int main (int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        verifyParameterRetention<RenTransformerProcessor>();
        stateTests<RenTransformerProcessor> (7); transformerTests();
        blockSizeTests<RenTransformerProcessor>();
        if (argc > 2 && juce::String (argv[1]) == "--editor")
            editorTests<RenTransformerProcessor> (juce::File (argv[2]), 260, 5, 444);
        std::cout << checks << " Ren Transformer checks passed\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
