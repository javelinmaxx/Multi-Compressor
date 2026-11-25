#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "ToggleShine.h"

namespace analogToggleHardware
{
// The same steel hex nut, recessed flange, and polished lever are used by every analog toggle.
inline juce::Rectangle<float> draw (juce::Graphics& g, float cx, float pivotY, float tipY,
                                    float nutRadius, float flangeRadius, float capRadius,
                                    juce::Colour accent, juce::Colour idle, juce::Colour recess,
                                    bool active, bool highlighted = false, bool down = false)
{
    juce::Path nut;
    for (int corner = 0; corner < 6; ++corner)
    {
        const auto angle = juce::MathConstants<float>::twoPi * float (corner) / 6.0f;
        const juce::Point<float> point (cx + std::cos (angle) * nutRadius,
                                        pivotY + std::sin (angle) * nutRadius);
        if (corner == 0) nut.startNewSubPath (point); else nut.lineTo (point);
    }
    nut.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.fillPath (nut, juce::AffineTransform::translation (0, 1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb8bec2), cx - nutRadius, pivotY - nutRadius,
                                            juce::Colour (0xff596169), cx + nutRadius, pivotY + nutRadius, false));
    g.fillPath (nut);
    g.setColour (recess.darker (0.25f).withAlpha (0.75f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));

    const auto flange = juce::Rectangle<float> (cx - flangeRadius, pivotY - flangeRadius,
                                                  2.0f * flangeRadius, 2.0f * flangeRadius);
    g.setColour (recess.darker (0.25f));
    g.fillEllipse (flange.expanded (1.0f));
    g.setColour (active ? accent : idle.withAlpha (0.75f));
    g.drawEllipse (flange.expanded (2.0f), active ? 1.8f : 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc8cdd0), flange.getTopLeft(),
                                            juce::Colour (0xff717980), flange.getBottomRight(), false));
    g.fillEllipse (flange);
    g.setColour (recess);
    g.fillEllipse (flange.reduced (3.0f));

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawLine (cx + 1.0f, pivotY + 1.0f, cx + 1.0f, tipY + 1.0f, 5.6f);
    g.setColour (down ? juce::Colours::white : juce::Colour (0xffc6cbd0));
    g.drawLine (cx, pivotY, cx, tipY, 4.8f);
    const auto cap = juce::Rectangle<float> (cx - capRadius, tipY - capRadius,
                                              2.0f * capRadius, 2.0f * capRadius);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0e3e5), cap.getTopLeft(),
                                            juce::Colour (0xff777f86), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (highlighted ? accent : juce::Colour (0xff515a61));
    g.drawEllipse (cap, 0.8f);
    toggleShine::draw (g, cx, pivotY, tipY, flangeRadius, capRadius);
    return flange;
}
}
