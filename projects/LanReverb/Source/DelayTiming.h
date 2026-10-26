#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace lan
{
struct NoteLength { const char* label; double beats; };
// Quarter-note beats, ordered by duration so the Time knob remains monotonic.
inline constexpr std::array<NoteLength,21> noteLengths {{
    {"1/64 T",1./24}, {"1/64",1./16}, {"1/32 T",1./12}, {"1/64 D",3./32},
    {"1/32",1./8}, {"1/16 T",1./6}, {"1/32 D",3./16}, {"1/16",1./4},
    {"1/8 T",1./3}, {"1/16 D",3./8}, {"1/8",1./2}, {"1/4 T",2./3},
    {"1/8 D",3./4}, {"1/4",1}, {"1/2 T",4./3}, {"1/4 D",1.5},
    {"1/2",2}, {"1/1 T",8./3}, {"1/2 D",3}, {"1/1",4}, {"1/1 D",6}
}};
inline constexpr int defaultNote = 10;
inline constexpr double minimumTempo = 20, maximumTempo = 400;
inline constexpr float maximumTimeMs = 18000; // dotted whole note at 20 BPM
inline float noteTimeMs (int index, double bpm) noexcept
{
    if (!std::isfinite(bpm)) bpm = 120;
    bpm = std::clamp(bpm,minimumTempo,maximumTempo);
    return float(noteLengths[size_t(std::clamp(index,0,int(noteLengths.size())-1))].beats*60000/bpm);
}
}
