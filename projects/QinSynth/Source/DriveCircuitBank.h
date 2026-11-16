#pragma once
#include "../../ZaoDrive/Source/Circuits.h"
#include <array>
#include <algorithm>

namespace qin
{
// Qin uses the same four circuit solvers as Zao, with Zao's default bias and
// diode-driver knee. Only the active circuit and a fading outgoing circuit run.
class DriveCircuitBank
{
public:
    void prepare(double sampleRate) noexcept
    {
        for(size_t channel=0;channel<2;++channel)
        {
            bjt[channel].prepare(sampleRate);
            opamp[channel].prepare(sampleRate);
            shunt[channel].prepare(sampleRate);
            crossover[channel].prepare(sampleRate);
        }
    }
    void reset(int mode) noexcept
    {
        for(size_t channel=0;channel<2;++channel)
            switch(mode)
            {
                case 0: bjt[channel].reset(); break;
                case 1: opamp[channel].reset(); break;
                case 2: shunt[channel].reset(); break;
                default: crossover[channel].reset(); break;
            }
    }
    double process(int mode,int channel,double input) noexcept
    {
        const auto c=size_t(std::clamp(channel,0,1));
        switch(mode)
        {
            case 0: return bjt[c].process(input);
            case 1: return opamp[c].process(input);
            case 2: return shunt[c].process(input*4.0);
            default: return crossover[c].process(input*4.0);
        }
    }
private:
    std::array<zao::BjtCommonEmitter,2> bjt;
    std::array<zao::OpAmpFeedbackClipper,2> opamp;
    std::array<zao::DiodeShuntClipper,2> shunt;
    std::array<zao::SeriesDiodeCrossover,2> crossover;
};
}
