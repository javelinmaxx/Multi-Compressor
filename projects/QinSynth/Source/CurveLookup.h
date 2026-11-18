#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace qin
{
// Tables are built once at module load; audio processing only interpolates.
struct CurveLookup
{
    static constexpr int phaseSteps = 4096, progressSteps = 1024, curveSteps = 64;
    std::array<float, phaseSteps + 1> sine {};
    std::array<std::array<float, progressSteps + 1>, curveSteps + 1> attack {}, decay {};

    CurveLookup() noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i <= phaseSteps; ++i)
            sine[size_t(i)] = float(std::sin(2*pi*i/phaseSteps));
        for (int row = 0; row <= curveSteps; ++row)
        {
            const auto curve = -1.0 + 2.0*row/curveSteps;
            const auto exponent = std::exp2(curve*2.0);
            for (int i = 0; i <= progressSteps; ++i)
            {
                // Fourth-power spacing preserves the first samples of long
                // attacks even at the most logarithmic curve setting.
                const auto u = double(i)/progressSteps;
                const auto t = u*u*u*u;
                attack[size_t(row)][size_t(i)] = float(std::pow((1-std::exp(-6.9*t))/(1-std::exp(-6.9)),exponent));
                decay[size_t(row)][size_t(i)] = float(std::pow((1-std::exp(-5.0*t))/(1-std::exp(-5.0)),exponent));
            }
        }
    }

    double sineAt(double phase) const noexcept
    {
        const auto position = std::clamp(phase,0.0,1.0)*phaseSteps;
        const auto index = std::min(size_t(position),size_t(phaseSteps-1));
        const auto t = position-double(index);
        return sine[index]+t*(sine[index+1]-sine[index]);
    }

    float envelopeAt(float progress,float curve,bool attackStage) const noexcept
    {
        const auto c = (std::clamp(curve,-1.f,1.f)+1.f)*(curveSteps*.5f);
        const auto row = std::min(size_t(c),size_t(curveSteps-1));
        const auto between = c-float(row);
        const auto p = std::sqrt(std::sqrt(std::clamp(progress,0.f,1.f)))*progressSteps;
        const auto index = std::min(size_t(p),size_t(progressSteps-1));
        const auto fraction = p-float(index);
        const auto& table = attackStage ? attack : decay;
        const auto low = table[row][index]+fraction*(table[row][index+1]-table[row][index]);
        const auto high = table[row+1][index]+fraction*(table[row+1][index+1]-table[row+1][index]);
        return low+between*(high-low);
    }
};
inline const CurveLookup curveLookup {};
}
