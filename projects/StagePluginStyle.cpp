#include "StagePluginStyle.h"
#include "PanelSurface.h"
#include "AnalogToggleHardware.h"
#include <cmath>

namespace stageStyle
{
void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base)
{
    panelSurface::drawBeveledPanel (g, bounds, 10.0f, base, grid);
}
}

using namespace stageStyle;

void StageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float value, float start, float end, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height)).reduced (8);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = start + value * (end - start);
    for (int tick = 0; tick <= 10; ++tick)
    {
        const auto a = start + (end - start) * static_cast<float> (tick) / 10.0f;
        const auto inner = radius - (tick % 5 == 0 ? 4.0f : 2.0f);
        g.setColour (markColour.withAlpha (tick % 5 == 0 ? 0.5f : 0.25f));
        g.drawLine (centre.x + std::sin (a) * inner, centre.y - std::cos (a) * inner,
                    centre.x + std::sin (a) * radius, centre.y - std::cos (a) * radius, 1.0f);
    }
    juce::Path track, active;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0, start, end, true);
    active.addCentredArc (centre.x, centre.y, radius, radius, 0, start, angle, true);
    g.setColour (trackColour.brighter (0.25f));
    g.strokePath (track, juce::PathStrokeType (2.0f));
    g.setColour (accentColour);
    g.strokePath (active, juce::PathStrokeType (2.2f));
    auto dial = juce::Rectangle<float> (radius * 2 - 10, radius * 2 - 10).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (dial.translated (0, 2));
    panelSurface::drawMetalKnobFace (g, dial, dialTopColour, dialBottomColour, markColour, angle);
    g.setColour (accentColour);
    const auto inner = dial.getWidth() * 0.23f, outer = dial.getWidth() * 0.39f;
    g.drawLine (centre.x + std::sin (angle) * inner, centre.y - std::cos (angle) * inner,
                centre.x + std::sin (angle) * outer, centre.y - std::cos (angle) * outer, 1.8f);
}

juce::Label* StageLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (10.0f));
    return label;
}

void StageBypassButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1);
    const auto active = getToggleState();
    const auto face = active ? panelColour.interpolatedWith (accentColour.darker (0.5f), 0.6f) : panelColour;
    g.setColour (down ? face.darker (0.1f) : highlighted ? face.brighter (0.06f) : face);
    g.fillRoundedRectangle (bounds, 6);
    g.setColour (active ? accentColour : secondary.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 6, 1.0f);
    g.setColour (active ? foreground : secondary);
    g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
    g.drawText (getButtonText(), bounds.toNearestInt(), juce::Justification::centred);
}

void StageTransformerSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto cx = static_cast<float> (getWidth()) * 0.5f;
    constexpr auto pivotY = static_cast<float> (StageTransformerSwitch::pivotOffsetY);
    g.setColour (secondary);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("CORE", 0, 0, getWidth(), 15, juce::Justification::centred);
    const auto tipY = pivotY + (getToggleState() ? -9.0f : 9.0f);
    const auto flange = analogToggleHardware::draw (g, cx, pivotY, tipY, 15.0f, 11.0f, 4.4f,
                                                    amber, secondary, panel.darker (0.5f),
                                                    getToggleState(), highlighted, down);
    g.setColour (getToggleState() ? amber : secondary);
    g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
    g.drawText (getToggleState() ? "ON" : "OFF", 0, 76, getWidth(), 18, juce::Justification::centred);
    if (hasKeyboardFocus (true)) { g.setColour (amber); g.drawEllipse (flange.expanded (3.0f), 1.0f); }
}

void StageLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float, float, const juce::Slider::SliderStyle, juce::Slider&)
{
    const auto centre = static_cast<float> (x + width / 2);
    const auto top = static_cast<float> (y + 5);
    const auto bottom = static_cast<float> (y + height - 5);
    g.setColour (trackColour.brighter (0.3f));
    g.fillRoundedRectangle (centre - 2.0f, top, 4.0f, bottom - top, 2.0f);
    g.setColour (accentColour.withAlpha (0.72f));
    g.fillRoundedRectangle (centre - 1.0f, sliderPos, 2.0f,
                            std::max (0.0f, bottom - sliderPos), 1.0f);
    g.setColour (accentColour);
    g.fillRoundedRectangle (centre - 6.0f, sliderPos - 3.0f, 12.0f, 6.0f, 2.0f);
}
