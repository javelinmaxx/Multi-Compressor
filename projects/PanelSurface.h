#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace panelSurface
{
inline void drawBeveledPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                             float radius, juce::Colour material, juce::Colour edge)
{
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillRoundedRectangle (bounds.translated (0, 2), radius + 1);
    g.setGradientFill (juce::ColourGradient (material.brighter (0.025f), bounds.getTopLeft(),
                                            material.darker (0.03f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (edge.withAlpha (0.72f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
    const auto bevelX = bounds.getX() + radius;
    const auto bevelRight = bounds.getRight() - radius;
    const auto bevelY = bounds.getY() + radius;
    const auto bevelBottom = bounds.getBottom() - radius;
    const auto lightSurface = material.getPerceivedBrightness() > 0.45f;
    g.setColour (juce::Colours::white.withAlpha (lightSurface ? 0.19f : 0.11f));
    g.drawHorizontalLine (juce::roundToInt (bounds.getY() + 1), bevelX, bevelRight);
    g.drawVerticalLine (juce::roundToInt (bounds.getX() + 1), bevelY, bevelBottom);
    g.setColour (juce::Colours::black.withAlpha (lightSurface ? 0.23f : 0.17f));
    g.drawHorizontalLine (juce::roundToInt (bounds.getBottom() - 1), bevelX, bevelRight);
    g.drawVerticalLine (juce::roundToInt (bounds.getRight() - 1), bevelY, bevelBottom);
}

// A single grain image is rotated with the dial, as a physical face would be.
inline const juce::Image& knobNoise()
{
    static const auto noise = []
    {
        juce::Image image (juce::Image::ARGB, 128, 128, true);
        auto state = 0x873b5e29u;
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const auto value = state & 255u;
            const auto colour = (value & 1u) == 0u ? juce::Colours::white : juce::Colour (0xff8c8c8c);
            image.setPixelAt (x, y, colour.withAlpha (0.1f));
        }
        return image;
    }();
    return noise;
}

inline void drawMetalKnobFace (juce::Graphics& g, juce::Rectangle<float> dial,
                              juce::Colour top, juce::Colour bottom, juce::Colour rim, float angle)
{
    const auto material = top.withAlpha (1.f).interpolatedWith (bottom.withAlpha (1.f), 0.5f);
    g.setGradientFill (juce::ColourGradient (material.interpolatedWith (top, 0.12f), dial.getTopLeft(),
                                            material.interpolatedWith (bottom, 0.12f), dial.getBottomLeft(), false));
    g.fillEllipse (dial);
    juce::Path clip;
    clip.addEllipse (dial);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (angle, dial.getCentreX(), dial.getCentreY()));
    g.reduceClipRegion (clip);
    g.drawImageWithin (knobNoise(), juce::roundToInt (dial.getX()), juce::roundToInt (dial.getY()),
                       juce::roundToInt (dial.getWidth()), juce::roundToInt (dial.getHeight()),
                       juce::RectanglePlacement::stretchToFit);
    g.restoreState();
    g.setColour (rim.withAlpha (0.82f));
    g.drawEllipse (dial, 1.25f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawEllipse (dial.reduced (1.8f), 0.8f);
}
}
