#pragma once
#include "ReverbEngine.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lan
{
// Editor-only smoothing. The analyser, resonance decisions, and feedback EQ
// continue to use their original unsmoothed spectral measurements.
class SpectrumDisplaySmoother
{
public:
    void reset() noexcept { ready=false; }
    bool hasFrame() const noexcept { return ready; }
    const std::array<float,spectrumBins>& trace(int channel) const noexcept
    { return display[size_t(std::clamp(channel,0,1))]; }
    void update(const Analysis& frame,float seconds) noexcept
    {
        if(frame.revision==0) { reset(); return; }
        const auto elapsed=std::clamp(std::isfinite(seconds) ? seconds : .05f,.001f,.5f);
        for(int channel=0;channel<2;++channel)
        {
            const auto& input=channel==0 ? frame.input : frame.feedback;
            auto& output=display[size_t(channel)];
            for(int bin=0;bin<spectrumBins;++bin)
            {
                // Smooth a narrow neighborhood in linear amplitude so narrow
                // peaks remain visible instead of being averaged away in dB.
                float amplitude=0;
                constexpr std::array<float,3> weights {.2f,.6f,.2f};
                for(int offset=-1;offset<=1;++offset)
                {
                    const auto index=size_t(std::clamp(bin+offset,0,spectrumBins-1));
                    const auto db=std::clamp(std::isfinite(input[index]) ? input[index] : -100.f,-120.f,24.f);
                    amplitude+=weights[size_t(offset+1)]*std::pow(10.f,db/20.f);
                }
                const auto target=20.f*std::log10(std::max(amplitude,1.e-6f));
                if(!ready) output[size_t(bin)]=target;
                else
                {
                    const auto time=target>output[size_t(bin)] ? .12f : .22f;
                    const auto alpha=1.f-std::exp(-elapsed/time);
                    output[size_t(bin)]+=alpha*(target-output[size_t(bin)]);
                }
            }
        }
        ready=true;
    }
private:
    std::array<std::array<float,spectrumBins>,2> display {};
    bool ready=false;
};
}
