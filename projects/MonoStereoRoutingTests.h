#pragma once
#include <cmath>
#include <stdexcept>

namespace monoStereoTests
{
template <typename Processor> void run (const char* disableParameter, bool testBypass)
{
    for (bool bypass : {false,true})
    {
        if (bypass && ! testBypass) continue;
        Processor mono, stereo;
        for (auto* p : {&mono,&stereo})
            if (disableParameter != nullptr)
                if (auto* parameter = p->parameters.getParameter (disableParameter))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (0));
        auto layout = mono.getBusesLayout();
        layout.inputBuses.set (0,juce::AudioChannelSet::mono());
        layout.outputBuses.set (0,juce::AudioChannelSet::stereo());
        if (! mono.isBusesLayoutSupported (layout) || ! mono.setBusesLayout (layout))
            throw std::runtime_error ("mono-to-stereo layout rejected");
        mono.prepareToPlay (48000,127); stereo.prepareToPlay (48000,127);
        juce::MidiBuffer midi; juce::AudioBuffer<float> a (2,127), b (2,127);
        for (int block = 0; block < 256; ++block)
        {
            for (int i = 0; i < 127; ++i)
            {
                const auto n = block*127+i;
                const auto x = float(.2*std::sin (juce::MathConstants<double>::twoPi*431*n/48000)) + (n == 0 ? .4f : 0.f);
                a.setSample (0,i,x); a.setSample (1,i,.73f); // host output-only channel contains garbage
                b.setSample (0,i,x); b.setSample (1,i,x);
            }
            if (bypass) { mono.processBlockBypassed (a,midi); stereo.processBlockBypassed (b,midi); }
            else { mono.processBlock (a,midi); stereo.processBlock (b,midi); }
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 127; ++i)
                if (! std::isfinite (a.getSample(ch,i)) || std::abs(a.getSample(ch,i)-b.getSample(ch,i)) > 1e-6f)
                    throw std::runtime_error ("mono-to-stereo differs from duplicated stereo input");
        }
    }
}
}
