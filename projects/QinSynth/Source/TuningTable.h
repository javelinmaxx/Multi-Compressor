#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace qin
{
// Logic's User tuning table is twelve cent offsets relative to a root key.
// Interpolation keeps mono/polyphonic pitch slides continuous between notes.
struct TuningTable
{
    std::array<float,12> cents {};
    int root=0;
    float referenceA4=440.f;
    double referenceSemitones=0;
    bool equalTempered=true;

    void setReferenceA4(float hz) noexcept
    {
        referenceA4=std::clamp(hz,400.f,480.f);
        referenceSemitones=12.0*std::log2(double(referenceA4)/440.0);
    }

    double tunedPitch(double midiPitch) const noexcept
    {
        if(equalTempered) return midiPitch;
        const auto lower=int(std::floor(midiPitch));
        const auto fraction=std::clamp(midiPitch-lower,0.0,1.0);
        const auto degree=[this](int note)
        {
            const auto index=((note-root)%12+12)%12;
            return double(cents[size_t(index)]);
        };
        const auto offset=degree(lower)+(degree(lower+1)-degree(lower))*fraction;
        return midiPitch+offset*.01+referenceSemitones;
    }
    double frequency(double midiPitch) const noexcept
    { return 440.0*std::exp2((tunedPitch(midiPitch)-69.0)/12.0); }
};
}
