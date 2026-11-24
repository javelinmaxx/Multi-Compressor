#pragma once
#include <algorithm>
#include <array>
#include <cmath>

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

// At 50% these are the original shapes. Moving either way narrows the
// triangle into two tents with silence between; saw gains a flat end.
struct DutyShape
{
    double sawStart=0, sawEnd=1, sawSlope=2, pulseWidth=.5, triangleInverseWidth=4;
};

inline DutyShape makeDutyShape(double duty) noexcept
{
    DutyShape shape;
    const auto displacement=2.0*(std::clamp(duty,0.0,1.0)-.5);
    shape.sawStart=displacement<0.0 ? -.45*displacement : 0.0;
    shape.sawEnd=displacement>0.0 ? 1.0-.45*displacement : 1.0;
    shape.sawSlope=2.0/(shape.sawEnd-shape.sawStart);
    shape.pulseWidth=.02+.96*std::clamp(duty,0.0,1.0);
    shape.triangleInverseWidth=4.0/(1.0-.9*std::abs(displacement));
    return shape;
}

inline double dutySaw(double phase,const DutyShape& shape) noexcept
{
    return phase<shape.sawStart ? -1.0 : phase>=shape.sawEnd ? 1.0
        : (phase-shape.sawStart)*shape.sawSlope-1.0;
}

inline double dutyPulse(double phase,const DutyShape& shape) noexcept
{
    return phase<shape.pulseWidth ? 1.0 : -1.0;
}

inline double dutyTriangle(double phase,const DutyShape& shape) noexcept
{
    const auto positive=std::max(0.0,1.0-std::abs(phase-.5)*shape.triangleInverseWidth);
    const auto negative=std::max(0.0,1.0-std::min(phase,1.0-phase)*shape.triangleInverseWidth);
    return positive-negative;
}

inline double dutySaw(double phase,double duty) noexcept { return dutySaw(phase,makeDutyShape(duty)); }
inline double dutyPulse(double phase,double duty) noexcept { return dutyPulse(phase,makeDutyShape(duty)); }
inline double dutyTriangle(double phase,double duty) noexcept { return dutyTriangle(phase,makeDutyShape(duty)); }
}
