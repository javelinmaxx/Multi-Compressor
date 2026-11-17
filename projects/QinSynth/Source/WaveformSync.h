#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace qin
{
struct WaveformWindow
{
    float start=0, length=0;
    bool synchronized=false;
};

// Find a repeating period in the recent input, then anchor the display at a
// rising crossing. Input and driven output must use this same window.
template<size_t Size>
WaveformWindow waveformWindow(const std::array<float,Size>& samples) noexcept
{
    constexpr int count=int(Size);
    static_assert(Size>=32);
    float mean=0, peak=0;
    for(const auto sample:samples) mean+=sample;
    mean/=float(Size);
    for(const auto sample:samples) peak=std::max(peak,std::abs(sample-mean));
    const auto fallback=WaveformWindow{float(count-std::min(count,128)),float(std::min(count,128)-1),false};
    if(peak<.003f) return fallback;

    const auto correlation=[&](int lag)
    {
        const auto pairs=std::min(count-lag,std::max(128,lag*2));
        float dot=0, leftEnergy=0, rightEnergy=0;
        for(int index=count-pairs-lag;index<count-lag;++index)
        {
            const auto a=samples[size_t(index)]-mean;
            const auto b=samples[size_t(index+lag)]-mean;
            dot+=a*b; leftEnergy+=a*a; rightEnergy+=b*b;
        }
        return dot/std::sqrt(std::max(1.e-12f,leftEnergy*rightEnergy));
    };
    float previous=correlation(5), current=correlation(6);
    float bestScore=.68f;
    float bestPeriod=0;
    for(int lag=6;lag<count/2;++lag)
    {
        const auto next=correlation(lag+1);
        if(current>previous && current>=next && current>bestScore)
        {
            const auto curvature=previous-2.f*current+next;
            const auto offset=curvature< -1.e-6f
                ? std::clamp(.5f*(previous-next)/curvature,-.5f,.5f) : 0.f;
            bestPeriod=float(lag)+offset;
            if(current>.88f) break;
            bestScore=current;
        }
        previous=current; current=next;
    }
    if(bestPeriod<1.f) return fallback;

    for(int index=count-1;index>bestPeriod;--index)
    {
        const auto before=samples[size_t(index-1)]-mean;
        const auto after=samples[size_t(index)]-mean;
        if(before<=0 && after>0)
        {
            const auto crossing=float(index-1)+(-before)/(after-before);
            const auto periods=crossing>=2.f*bestPeriod ? 2 : 1;
            const auto length=float(periods)*bestPeriod;
            if(crossing>=length) return {crossing-length,length,true};
        }
    }
    return fallback;
}

template<size_t Size>
float waveformSample(const std::array<float,Size>& samples,float position) noexcept
{
    const auto clamped=std::clamp(position,0.f,float(Size-1));
    const auto index=size_t(clamped);
    const auto next=std::min(index+1,Size-1);
    return samples[index]+(samples[next]-samples[index])*(clamped-float(index));
}
}
