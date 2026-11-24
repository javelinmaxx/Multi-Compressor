#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace qin
{
// Two gentle cuts on the circuit input only. Coefficients and bypass amounts
// glide independently so host automation does not switch filter states abruptly.
class DriveInputFilter
{
public:
    void prepare(double rate,float lowHz,float highHz) noexcept
    {
        sampleRate=std::isfinite(rate) && rate>=8000.0 ? float(rate) : 48000.f;
        for(auto* value:{&lowCoefficient,&highCoefficient,&lowAmount,&highAmount})
            value->reset(sampleRate,.015);
        lowCoefficient.setCurrentAndTargetValue(coefficient(std::max(20.f,lowHz)));
        highCoefficient.setCurrentAndTargetValue(coefficient(highHz));
        lowAmount.setCurrentAndTargetValue(lowHz>0.f ? 1.f : 0.f);
        highAmount.setCurrentAndTargetValue(highHz<20000.f ? 1.f : 0.f);
        lastLow=lowHz; lastHigh=highHz;
        resetStates();
    }
    void resetStates() noexcept { lowState={}; highState={}; }
    bool isActive() const noexcept
    {
        return lowAmount.getCurrentValue()>0.f || highAmount.getCurrentValue()>0.f
            || lowAmount.isSmoothing() || highAmount.isSmoothing();
    }
    void setCutoffs(float lowHz,float highHz) noexcept
    {
        lowHz=std::clamp(lowHz,0.f,1000.f);
        highHz=std::clamp(highHz,1000.f,20000.f);
        if(std::abs(lowHz-lastLow)>.01f)
        {
            lowCoefficient.setTargetValue(coefficient(std::max(20.f,lowHz)));
            lowAmount.setTargetValue(lowHz>0.f ? 1.f : 0.f);
            lastLow=lowHz;
        }
        if(std::abs(highHz-lastHigh)>.01f)
        {
            highCoefficient.setTargetValue(coefficient(highHz));
            highAmount.setTargetValue(highHz<20000.f ? 1.f : 0.f);
            lastHigh=highHz;
        }
    }
    void beginSample() noexcept
    {
        currentLowCoefficient=lowCoefficient.getNextValue();
        currentHighCoefficient=highCoefficient.getNextValue();
        currentLowAmount=lowAmount.getNextValue();
        currentHighAmount=highAmount.getNextValue();
    }
    float process(int channel,float input) noexcept
    {
        const auto index=static_cast<size_t>(std::clamp(channel,0,1));
        auto& low=lowState[index];
        auto& high=highState[index];
        low+=currentLowCoefficient*(input-low);
        const auto afterLow=input-currentLowAmount*low;
        high+=currentHighCoefficient*(afterLow-high);
        return afterLow+currentHighAmount*(high-afterLow);
    }
private:
    float coefficient(float hz) const noexcept
    { return 1.f-std::exp(-juce::MathConstants<float>::twoPi*hz/sampleRate); }
    float sampleRate=48000.f,lastLow=0,lastHigh=20000;
    float currentLowCoefficient=0,currentHighCoefficient=0,currentLowAmount=0,currentHighAmount=0;
    juce::SmoothedValue<float> lowCoefficient,highCoefficient,lowAmount,highAmount;
    std::array<float,2> lowState {},highState {};
};
}
