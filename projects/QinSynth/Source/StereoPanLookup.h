#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace qin
{
struct StereoPanGains { float left=0, right=0; };

// Equal-power gains for moving key pan and spread. Stationary gains are cached
// once per render segment; this table keeps glides free of per-subvoice sqrt.
struct StereoPanLookup
{
    static constexpr int steps=4096;
    static constexpr float limit=.98f;
    std::array<StereoPanGains,steps+1> values {};

    StereoPanLookup() noexcept
    {
        for(int i=0;i<=steps;++i)
        {
            const auto pan=-limit+2.f*limit*float(i)/steps;
            values[size_t(i)]={std::sqrt((1.f-pan)*.5f),std::sqrt((1.f+pan)*.5f)};
        }
    }
    StereoPanGains evaluate(float pan) const noexcept
    {
        const auto position=(std::clamp(pan,-limit,limit)+limit)*(steps/(2.f*limit));
        const auto index=std::min(size_t(position),size_t(steps-1));
        const auto fraction=position-float(index);
        const auto& a=values[index];
        const auto& b=values[index+1];
        return {a.left+fraction*(b.left-a.left),a.right+fraction*(b.right-a.right)};
    }
};
inline const StereoPanLookup stereoPanLookup {};
}
