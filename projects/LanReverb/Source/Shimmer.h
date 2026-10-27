#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

namespace lan
{
// Re-arm between phrases, then soften the actual delayed arrival rather than
// spending the attack while the grain buffers are still silent.
class ShimmerOnsetFade
{
public:
    void prepare (double rate) noexcept
    {
        step = 1/(rate*attackSeconds);
        quietHold = std::max(1,juce::roundToInt(rate*quietSeconds)); reset();
    }
    void reset() noexcept { progress = 1; quiet = quietHold; armed = true; }
    float process (float left, float right) noexcept
    {
        if (std::max(std::abs(left),std::abs(right)) > onsetThreshold)
        {
            if (armed) { progress = 0; armed = false; }
            quiet = 0;
        }
        else if (quiet < quietHold && ++quiet == quietHold) armed = true;
        progress = std::min(1.,progress+step);
        return float(progress*progress*(3-2*progress));
    }
    static constexpr double attackSeconds = .1, quietSeconds = .05;
    static constexpr float onsetThreshold = .0001f; // -80 dBFS; ignore the FIR noise floor
private:
    double progress = 1, step = 1;
    int quiet = 0, quietHold = 1;
    bool armed = true;
};
// Four crossfaded delay grains read at twice the write speed for an octave up.
// The shared grain clock preserves stereo relationships; only the added wet
// layer acquires the grain/converter delay, leaving the dry path immediate.
class Shimmer
{
public:
    void prepare (double rate)
    {
        const auto hostRate = std::isfinite(rate) && rate >= 8000 ? rate : 48000.;
        onsetFade.prepare(hostRate);
        // 200 ms grains, staggered by 50 ms, at the doubled processing rate.
        // A multiple of four makes the quarter-cycle offsets exact at any Fs.
        grainSize = std::max(32,4*juce::roundToInt(hostRate*2*grainSeconds/4));
        for (auto& channel : delay) channel.assign(size_t(grainSize+4),0);
        window.resize(size_t(grainSize));
        for (int i = 0; i < grainSize; ++i)
        {
            auto taper = .5*(1-std::cos(juce::MathConstants<double>::twoPi*i/grainSize));
            taper *= taper; taper *= taper; taper *= taper;
            window[size_t(i)] = float(taper);
        }
        // Broad four-way Hann overlaps cancel some sustained notes completely.
        // Concentrate the windows (Hann^8), then normalize each shared phase:
        // smooth head resets and DC unity without those deep spectral holes.
        for (int i = 0; i < grainSize/4; ++i)
        {
            double sum = 0;
            for (int grain = 0; grain < 4; ++grain) sum += window[size_t(i+grain*(grainSize/4))];
            for (int grain = 0; grain < 4; ++grain) window[size_t(i+grain*(grainSize/4))] /= float(sum);
        }
        oversampling.initProcessing(1); oversampling.reset();
        clearGrains(); running = false; highSamples = 0;
    }
    std::array<float,2> process (float left, float right, bool active) noexcept
    {
        if (!active) { running = false; return {}; }
        if (!running)
        {
            // Reuse prepared storage; do not replay a previous tail on enable.
            // The network send has its own 250 ms fade.
            oversampling.reset(); clearGrains(); onsetFade.reset(); running = true;
        }
        float l = left, r = right; float* channels[] {&l,&r};
        juce::dsp::AudioBlock<float> block(channels,2,1);
        auto high = oversampling.processSamplesUp(block);
        for (size_t sample = 0; sample < high.getNumSamples(); ++sample)
        {
            for (size_t channel = 0; channel < delay.size(); ++channel)
                delay[channel][size_t(write)] = high.getSample(int(channel),int(sample));
            std::array<float,2> output {};
            for (int grain = 0; grain < 4; ++grain)
            {
                auto age = phase+grain*(grainSize/4);
                if (age >= grainSize) age -= grainSize;
                // Delay decreases one sample per sample, so the read head
                // advances by two. Reset its position only at zero window gain.
                auto read = write-(grainSize-age+2);
                if (read < 0) read += grainSize+4;
                const auto gain = window[size_t(age)];
                for (size_t channel = 0; channel < delay.size(); ++channel)
                    output[channel] += gain*delay[channel][size_t(read)];
            }
            for (size_t channel = 0; channel < delay.size(); ++channel)
                high.setSample(int(channel),int(sample),output[channel]);
            if (++write == grainSize+4) write = 0;
            if (++phase == grainSize) phase = 0;
            ++highSamples;
        }
        oversampling.processSamplesDown(block);
        const auto fade = onsetFade.process(l,r);
        return {l*fade,r*fade};
    }
    uint64_t processedHighSamples() const noexcept { return highSamples; }
    static constexpr double grainSeconds = .2;
private:
    void clearGrains() noexcept
    {
        for (auto& channel : delay) std::fill(channel.begin(),channel.end(),0.f);
        write = phase = 0;
    }
    juce::dsp::Oversampling<float> oversampling {2,1,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,false};
    std::array<std::vector<float>,2> delay;
    std::vector<float> window;
    ShimmerOnsetFade onsetFade;
    int grainSize = 0, write = 0, phase = 0;
    uint64_t highSamples = 0;
    bool running = false;
};
}
