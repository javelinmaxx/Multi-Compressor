#pragma once
#include <algorithm>
#include <cmath>

namespace historyDisplay
{
// Signed gain and waveform dBFS share the meter's axis: Yang 0..-18 dB,
// Yin +12..-6 dB. The GR engine reports a positive reduction amount in Yang.
inline float positionForDb (float db, bool yin) noexcept
{ return ((yin ? 12.f : 0.f)-db)/18.f; }
inline float waveformPosition (float minimum, float maximum, bool yin) noexcept
{
    const auto peak = std::max (std::abs (minimum),std::abs (maximum));
    const auto db = 20.f*std::log10 (std::max (peak,1.e-5f));
    return std::clamp (positionForDb (db,yin),0.f,1.f);
}
inline constexpr float inputAlpha = .22f, outputAlpha = .30f;
}
