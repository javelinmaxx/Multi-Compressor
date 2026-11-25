#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace panelSurface
{
// A fixed tile keeps the brushed aluminium texture stable between repaints.
inline const juce::Image& aluminiumGrain()
{
    static const auto grain = []
    {
        juce::Image image (juce::Image::ARGB, 96, 96, true);
        auto state = 0x6d2b79f5u;
        for (int i = 0; i < 1500; ++i)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const auto x = static_cast<int> (state % 96u);
            state = state * 1664525u + 1013904223u;
            const auto y = static_cast<int> ((state >> 8) % 96u);
            const auto alpha = static_cast<juce::uint8> (3 + ((state >> 20) % 10u));
            image.setPixelAt (x, y, juce::Colour::fromRGBA (220, 221, 223, alpha));
        }
        juce::Graphics texture (image);
        for (int streak = 0; streak < 36; ++streak)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const auto y = static_cast<int> (state % 96u);
            const auto x = static_cast<int> ((state >> 8) % 96u);
            const auto length = 12 + static_cast<int> ((state >> 17) % 67u);
            texture.setColour ((streak & 1 ? juce::Colours::black : juce::Colours::white)
                                   .withAlpha (0.008f + float (state % 9u) * 0.001f));
            texture.drawHorizontalLine (y, float (x), float (std::min (96, x + length)));
        }
        return image;
    }();
    return grain;
}

inline void drawBeveledPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                             float radius, juce::Colour material, juce::Colour edge)
{
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillRoundedRectangle (bounds.translated (0, 2), radius + 1);
    g.setGradientFill (juce::ColourGradient (material.brighter (0.025f), bounds.getTopLeft(),
                                            material.darker (0.03f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle (bounds, radius);

    juce::Path clip;
    clip.addRoundedRectangle (bounds, radius);
    g.saveState();
    g.reduceClipRegion (clip);
    const auto& grain = aluminiumGrain();
    for (auto y = juce::roundToInt (bounds.getY()); y < juce::roundToInt (bounds.getBottom()); y += grain.getHeight())
        for (auto x = juce::roundToInt (bounds.getX()); x < juce::roundToInt (bounds.getRight()); x += grain.getWidth())
            g.drawImageAt (grain, x, y);
    g.restoreState();

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
        for (int y = 0; y < 128; y += 2)
            for (int x = 0; x < 128; x += 2)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const auto value = state & 255u;
            const auto colour = (value & 1u) == 0u ? juce::Colours::white : juce::Colours::black;
            const auto opacity = static_cast<juce::uint8> (10u + (value >> 3) % 20u);
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    image.setPixelAt (x + dx, y + dy, colour.withAlpha (opacity));
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
