#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lan
{
// One sampled random target per tap every Time/2. The next target is reached
// over that same interval, so there are no stepped feedback-gain changes.
class TapGainMotion
{
public:
    void prepare(double rate,uint32_t seed) noexcept
    {
        sampleRate=std::isfinite(rate) && rate>=8000 ? rate : 48000;
        randomState=seed!=0 ? seed : 1u;
        startGain=targetGain=currentGain=1.f;
        position=interval=0;
    }
    float process(float diffusion,float timeMs) noexcept
    {
        const auto depth=std::clamp(std::isfinite(diffusion) ? diffusion : 0.f,0.f,1.f);
        if(depth<=0 && std::abs(currentGain-1.f)<1.e-7f
           && std::abs(targetGain-1.f)<1.e-7f) return 1.f;
        if(position>=interval || (depth<=0 && std::abs(targetGain-1.f)>1.e-7f))
        {
            startGain=currentGain;
            const auto db=depth>0 ? (2.f*uniform()-1.f)*3.f*depth : 0.f;
            targetGain=std::pow(10.f,db/20.f);
            interval=std::max(1,int(std::round(sampleRate*std::clamp(timeMs,1.f,18000.f)*.0005)));
            position=0;
        }
        const auto t=float(++position)/float(interval);
        const auto eased=t*t*(3.f-2.f*t);
        currentGain=startGain+(targetGain-startGain)*eased;
        if(position>=interval) currentGain=targetGain;
        return currentGain;
    }
    float gain() const noexcept { return currentGain; }
    float target() const noexcept { return targetGain; }
    int glideSamples() const noexcept { return interval; }
private:
    float uniform() noexcept
    {
        randomState^=randomState<<13;
        randomState^=randomState>>17;
        randomState^=randomState<<5;
        return float(randomState>>8)*(1.f/16777215.f);
    }
    double sampleRate=48000;
    uint32_t randomState=1;
    float startGain=1, targetGain=1, currentGain=1;
    int position=0, interval=0;
};
}
