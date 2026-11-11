#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace stageStyle
{
inline const auto background = juce::Colour (0xff100e12), panel = juce::Colour (0xff1c1920),
    inset = juce::Colour (0xff09080c), foreground = juce::Colour (0xffeee9e2),
    secondary = juce::Colour (0xffaaa0a0), amber = juce::Colour (0xffe3a45b), grid = juce::Colour (0xff30262c);
void drawPanel (juce::Graphics&, juce::Rectangle<float>, juce::Colour base = panel);
}
class StageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accentColour = stageStyle::amber;
    juce::Colour markColour = stageStyle::secondary;
    juce::Colour trackColour = stageStyle::grid;
    juce::Colour dialTopColour = juce::Colour (0xff65504a);
    juce::Colour dialBottomColour = juce::Colour (0xff30252a);
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           const juce::Slider::SliderStyle, juce::Slider&) override;
};
class StageBypassButton final : public juce::ToggleButton
{
public:
    StageBypassButton() : ToggleButton ("BYPASS") {}
    juce::Colour accentColour = stageStyle::amber;
    juce::Colour panelColour = stageStyle::panel;
    void paintButton (juce::Graphics&, bool, bool) override;
};
class StageTransformerSwitch final : public juce::ToggleButton
{
public:
    static constexpr int pivotOffsetY = 48;
    StageTransformerSwitch() : ToggleButton ("CORE") {}
    void paintButton (juce::Graphics&, bool, bool) override;
};
