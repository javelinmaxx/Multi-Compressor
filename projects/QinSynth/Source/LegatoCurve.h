#pragma once
#include <algorithm>
#include <cmath>

namespace qin
{
inline float legatoCurve(float progress,float shape) noexcept
{
    const auto t=std::clamp(progress,0.f,1.f);
    const auto position=std::clamp(shape,0.f,1.f);
    const auto logarithmic=std::log1p(9.f*t)/std::log(10.f);
    const auto sigmoid=t*t*(3.f-2.f*t);
    return position<=.5f ? logarithmic+(t-logarithmic)*(position*2.f)
                        : t+(sigmoid-t)*((position-.5f)*2.f);
}
}
