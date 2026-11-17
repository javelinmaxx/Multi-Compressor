#pragma once
#include <algorithm>
#include <array>

namespace qin
{
struct WavePosition { float x=0, y=0; };

inline WavePosition clampWavePosition(float x,float y) noexcept
{
    const auto height=std::clamp(y,0.f,1.f);
    return {std::clamp(x,.5f*height,1.f-.5f*height),height};
}

// Triangle corners are Saw (left), Pulse (right), and Triangle (top).
inline std::array<float,3> waveWeights(WavePosition position) noexcept
{
    const auto point=clampWavePosition(position.x,position.y);
    return {1.f-point.x-.5f*point.y,point.x-.5f*point.y,point.y};
}
}
