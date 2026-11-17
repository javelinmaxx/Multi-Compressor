#pragma once

#include <algorithm>
#include <cmath>

namespace qin
{
// Shape the pitch spacing without moving the two outermost unison voices.
// -100 clusters voices near center, 0 is linear, and +100 clusters at the edges.
inline double detunePosition(int voice,int count,float shape) noexcept
{
    if(count<=1) return 0.0;
    const auto linear=2.0*voice/(count-1)-1.0;
    if(std::abs(shape)<1.e-6f || voice==0 || voice==count-1) return linear;
    const auto exponent=std::pow(3.0,-std::clamp(double(shape),-1.0,1.0));
    return std::copysign(std::pow(std::abs(linear),exponent),linear);
}
}
