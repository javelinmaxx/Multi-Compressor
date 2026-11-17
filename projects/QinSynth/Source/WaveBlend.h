#pragma once
#include <algorithm>
#include <array>

namespace qin
{
struct WavePosition { float x=0, y=0; };

inline WavePosition clampWavePosition(float x,float y) noexcept
{
    return {std::clamp(x,0.f,1.f),std::clamp(y,0.f,1.f)};
}

// Bottom: Saw/Pulse. Top: Triangle/Sine. Bilinear weights sum to one.
inline std::array<float,4> waveWeights(WavePosition position) noexcept
{
    const auto point=clampWavePosition(position.x,position.y);
    return {(1.f-point.x)*(1.f-point.y),point.x*(1.f-point.y),
            (1.f-point.x)*point.y,point.x*point.y};
}
}
