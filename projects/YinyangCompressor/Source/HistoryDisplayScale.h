#pragma once
#include <algorithm>
#include <cmath>

namespace historyDisplay
{
// The waveform overlay expands the shared gain/level axis down to -48 dB.
// Yin keeps +12 dB of headroom for its positive gain trace.
inline float positionForDb (float db, bool yin, bool waveform = false) noexcept
{
    const auto top = yin ? 12.f : 0.f;
    return (top-db)/(waveform ? top+48.f : 18.f);
}
inline float waveformPosition (float minimum, float maximum, bool yin) noexcept
{
    const auto peak = std::max (std::abs (minimum),std::abs (maximum));
    const auto db = 20.f*std::log10 (std::max (peak,1.e-5f));
    return std::clamp (positionForDb (db,yin,true),0.f,1.f);
}
inline constexpr float inputAlpha = .22f, outputAlpha = .30f;
}
