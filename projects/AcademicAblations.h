#pragma once

// Only dedicated research executables define this macro. Plugin targets never
// include these controls or their branches; there are no hidden host parameters.
#if defined(J_SONIC_ABLATION_TESTS)
#include <array>
namespace academic
{
struct HaasOptions
{
    bool spectrum = true, side = true, averaging = true, margin = true, exhaustive = false;
    float targetRatio = .20f, sidePenalty = .35f, averagingSeconds = .25f, switchMargin = .98f;
};
struct HaasMetric { float spectrum = 0, sideRatio = 0, rawScore = 0; };
inline thread_local HaasOptions haas;
inline thread_local std::array<HaasMetric, 121> haasMetrics {};
struct SmartOptions
{
    bool protection = true, wetThreshold = true, dissonance = true;
    double bandwidthScale = 1;
};
inline thread_local SmartOptions smart;
inline thread_local void (*smartTap) (const std::array<float, 2>&, const std::array<float, 2>&) = nullptr;
struct PhaseOptions
{
    bool residualAnchor = true, slope = true, unwrap = true, smooth = true;
    int peaks = 24;
};
inline thread_local PhaseOptions phase;
}
#endif
