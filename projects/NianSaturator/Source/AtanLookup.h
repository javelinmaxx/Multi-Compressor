#pragma once
#include <array>
#include <cmath>

namespace nian
{
struct AtanLookup
{
    static constexpr int steps = 4096;
    static constexpr float limit = 12.f;
    std::array<float,steps+1> values {};
    AtanLookup() noexcept
    {
        for(int i=0;i<=steps;++i)
            values[size_t(i)] = std::atan(limit*float(i)/steps);
    }
    float evaluate(float x) const noexcept
    {
        const auto magnitude=std::abs(x);
        if(!(magnitude<limit)) return std::atan(x);
        const auto position=magnitude*(steps/limit);
        const auto index=size_t(position);
        const auto fraction=position-float(index);
        const auto result=values[index]+fraction*(values[index+1]-values[index]);
        return std::copysign(result,x);
    }
};
inline const AtanLookup atanLookup {};
}
