#pragma once

#include <juce_graphics/juce_graphics.h>

namespace toggleShine
{
// Shared specular details keep the suite's lever switches in the same metal family.
inline void draw (juce::Graphics& g, float cx, float pivotY, float tipY,
                  float flangeRadius, float capRadius)
{
    juce::Path flangeGlint;
    flangeGlint.addCentredArc (cx, pivotY, flangeRadius, flangeRadius, 0,
                               -2.15f, -0.55f, true);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.strokePath (flangeGlint, juce::PathStrokeType (1.2f));

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawLine (cx - 1.35f, pivotY - 0.5f, cx - 1.35f, tipY - 0.5f, 0.95f);

    const auto cap = juce::Rectangle<float> (cx - capRadius, tipY - capRadius,
                                              2.0f * capRadius, 2.0f * capRadius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.58f),
                                            cap.getTopLeft(),
                                            juce::Colours::transparentWhite, cap.getBottomRight(), false));
    g.fillEllipse (cap.reduced (1.0f));
}
}
