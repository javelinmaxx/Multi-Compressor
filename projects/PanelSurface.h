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

// Neutral radial brushing adds a metal face without replacing each plugin's dial colour.
inline const juce::Image& radialKnobGrain()
{
    static const auto grain = []
    {
        juce::Image image (juce::Image::ARGB, 128, 128, true);
        juce::Graphics texture (image);
        auto state = 0x873b5e29u;
        for (int ray = 0; ray < 96; ++ray)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const auto angle = juce::MathConstants<float>::twoPi * float (ray) / 96.f;
            const auto inner = 4.f + float (state % 12u);
            const auto outer = 62.f;
            texture.setColour ((ray & 1 ? juce::Colours::black : juce::Colours::white)
                                   .withAlpha (0.05f + float (state % 8u) * 0.007f));
            texture.drawLine (64.f + std::cos (angle) * inner, 64.f + std::sin (angle) * inner,
                              64.f + std::cos (angle) * outer, 64.f + std::sin (angle) * outer, 1.1f);
        }
        return image;
    }();
    return grain;
}

inline void drawMetalKnobFace (juce::Graphics& g, juce::Rectangle<float> dial,
                              juce::Colour top, juce::Colour bottom, juce::Colour rim)
{
    const auto material = top.withAlpha (1.f).interpolatedWith (bottom.withAlpha (1.f), 0.5f);
    g.setGradientFill (juce::ColourGradient (material.interpolatedWith (top, 0.12f), dial.getTopLeft(),
                                            material.interpolatedWith (bottom, 0.12f), dial.getBottomLeft(), false));
    g.fillEllipse (dial);
    juce::Path clip;
    clip.addEllipse (dial);
    g.saveState();
    g.reduceClipRegion (clip);
    g.drawImageWithin (radialKnobGrain(), juce::roundToInt (dial.getX()), juce::roundToInt (dial.getY()),
                       juce::roundToInt (dial.getWidth()), juce::roundToInt (dial.getHeight()),
                       juce::RectanglePlacement::stretchToFit);
    g.restoreState();
    g.setColour (rim.withAlpha (0.82f));
    g.drawEllipse (dial, 1.25f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawEllipse (dial.reduced (1.8f), 0.8f);
}
}
