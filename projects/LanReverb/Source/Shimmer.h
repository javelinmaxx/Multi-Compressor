#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace lan
{
// An octave-up phase vocoder runs between 2x FIR up/down converters. Its
// deliberate STFT delay affects only the added wet layer, never the dry path.
class Shimmer
{
public:
    void prepare (double rate)
    {
        juce::ignoreUnused(rate);
        for (size_t i = 0; i < window.size(); ++i)
            window[i] = float(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/size));
        oversampling.initProcessing(1); oversampling.reset();
        clearVoices(); position = latency; running = false; highSamples = 0;
    }
    std::array<float,2> process (float left, float right, bool active) noexcept
    {
        if (!active) { running = false; return {}; }
        if (!running)
        {
            // Fixed-size clears, no allocations; never replay a previous tail
            // after an inactive interval. The send itself fades over 250 ms.
            oversampling.reset(); clearVoices(); position = latency; running = true;
        }
        float l = left, r = right; float* channels[] {&l,&r};
        juce::dsp::AudioBlock<float> block(channels,2,1);
        auto high = oversampling.processSamplesUp(block);
        for (size_t i = 0; i < high.getNumSamples(); ++i)
        {
            for (size_t channel = 0; channel < voices.size(); ++channel)
            {
                auto& voice = voices[channel];
                voice.input[size_t(position)] = high.getSample(int(channel),int(i));
                high.setSample(int(channel),int(i),voice.output[size_t(position-latency)]);
            }
            ++highSamples;
            if (++position == size)
            {
                for (auto& voice : voices) transform(voice);
                position = latency;
            }
        }
        oversampling.processSamplesDown(block); return {l,r};
    }
    uint64_t processedHighSamples() const noexcept { return highSamples; }
private:
    static constexpr int size = 2048, hop = size/4, latency = size-hop;
    struct Voice
    {
        std::array<float,size> input {}, overlap {};
        std::array<float,hop> output {};
        std::array<float,2*size> spectrum {};
        std::array<double,size/2+1> previousPhase {}, synthesisPhase {}, magnitude {}, frequency {};
    };
    void clearVoices() noexcept
    {
        for (auto& voice : voices)
        {
            voice.input.fill(0); voice.overlap.fill(0); voice.output.fill(0); voice.spectrum.fill(0);
            voice.previousPhase.fill(0); voice.synthesisPhase.fill(0); voice.magnitude.fill(0); voice.frequency.fill(0);
        }
    }
    void transform (Voice& voice) noexcept
    {
        constexpr auto twoPi = juce::MathConstants<double>::twoPi;
        voice.spectrum.fill(0);
        for (int i = 0; i < size; ++i) voice.spectrum[size_t(i)] = voice.input[size_t(i)]*window[size_t(i)];
        fft.performRealOnlyForwardTransform(voice.spectrum.data(),true);
        voice.magnitude.fill(0); voice.frequency.fill(0);
        for (int bin = 0; bin <= size/2; ++bin)
        {
            const auto real = voice.spectrum[size_t(2*bin)], imaginary = voice.spectrum[size_t(2*bin+1)];
            const auto phase = std::atan2(imaginary,real);
            const auto residual = std::remainder(phase-voice.previousPhase[size_t(bin)]-twoPi*bin*hop/size,twoPi);
            voice.previousPhase[size_t(bin)] = phase;
            const auto destination = 2*bin;
            if (destination > size/2) continue; // outside the oversampled Nyquist
            voice.magnitude[size_t(destination)] = std::hypot(real,imaginary);
            // Use measured inter-frame phase advance rather than bin centers:
            // off-bin input notes retain an accurate +12-semitone ratio.
            voice.frequency[size_t(destination)] = 2*(twoPi*bin/size+residual/hop);
        }
        voice.spectrum.fill(0);
        for (int bin = 0; bin <= size/2; ++bin)
        {
            voice.synthesisPhase[size_t(bin)] = std::remainder(voice.synthesisPhase[size_t(bin)]+voice.frequency[size_t(bin)]*hop,twoPi);
            const auto phase = voice.synthesisPhase[size_t(bin)], magnitude = voice.magnitude[size_t(bin)];
            voice.spectrum[size_t(2*bin)] = float(magnitude*std::cos(phase));
            voice.spectrum[size_t(2*bin+1)] = float(magnitude*std::sin(phase));
        }
        fft.performRealOnlyInverseTransform(voice.spectrum.data());
        // Four periodic Hann windows have sum-of-squares 3/2.
        for (int i = 0; i < size; ++i) voice.overlap[size_t(i)] += voice.spectrum[size_t(i)]*window[size_t(i)]*(2.f/3);
        for (int i = 0; i < hop; ++i) voice.output[size_t(i)] = voice.overlap[size_t(i)];
        for (int i = 0; i < size-hop; ++i)
        { voice.overlap[size_t(i)] = voice.overlap[size_t(i+hop)]; voice.input[size_t(i)] = voice.input[size_t(i+hop)]; }
        std::fill(voice.overlap.begin()+latency,voice.overlap.end(),0.f);
    }
    juce::dsp::FFT fft {11};
    juce::dsp::Oversampling<float> oversampling {2,1,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,false};
    std::array<Voice,2> voices;
    std::array<float,size> window {};
    int position = latency;
    uint64_t highSamples = 0;
    bool running = false;
};
}
