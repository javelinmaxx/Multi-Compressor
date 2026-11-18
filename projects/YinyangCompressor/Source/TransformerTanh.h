#pragma once
#include <array>
#include <cmath>

namespace character
{
// Shared by every plugin using the transformer core. The table is initialized
// once at module load, with no allocation or transcendental call on audio.
struct TransformerTanh
{
    static constexpr int steps = 8192;
    static constexpr float limit = 8.0f;
    std::array<float,steps+1> values {};

    TransformerTanh() noexcept
    {
        for (int i=0;i<=steps;++i)
            values[size_t(i)] = std::tanh(-limit+2.0f*limit*i/steps);
    }
    float evaluate(float x) const noexcept
    {
        if (x<=-limit) return values.front();
        if (x>=limit) return values.back();
        if (std::isnan(x)) return x;
        const auto position=(x+limit)*(steps/(2.0f*limit));
        const auto index=size_t(position);
        const auto fraction=position-float(index);
        return values[index]+fraction*(values[index+1]-values[index]);
    }
};
inline const TransformerTanh transformerTanh {};
}
