#include "PluginEditor.h"
#include "../../EditorScaling.h"
#include "../../EditorTooltips.h"
using namespace stageStyle;
void KuoVelvetLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const auto centre = static_cast<float> (y + height / 2);
    const auto left = static_cast<float> (x + 5);
    const auto right = static_cast<float> (x + width - 5);
    g.setColour (trackColour.brighter (0.3f));
    g.fillRoundedRectangle (left, centre - 2.0f, right - left, 4.0f, 2.0f);
    g.setColour (accentColour.withAlpha (0.72f));
    g.fillRoundedRectangle (left, centre - 2.0f, std::max (0.0f, sliderPos - left), 4.0f, 2.0f);
    g.setColour (accentColour);
    g.fillRoundedRectangle (sliderPos - 4.0f, centre - 7.0f, 8.0f, 14.0f, 3.0f);
}
KuoVelvetEditor::KuoVelvetEditor (KuoVelvetProcessor& p) : AudioProcessorEditor (&p)
{
    look.accentColour = juce::Colour (0xff22536e);
    look.markColour = juce::Colour (0xff234962);
    look.trackColour = juce::Colour (0xff668da3);
    look.dialTopColour = juce::Colour (0xffd6eef7);
    look.dialBottomColour = juce::Colour (0xffa9d0e5);
    bypass.accentColour = juce::Colour (0xff9ddbf2);
    bypass.panelColour = juce::Colour (0xff244b63);
    addAndMakeVisible (titleHint);
    setOpaque (true); setLookAndFeel (&look);
    look.setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff244b63));
    look.setColour (juce::TooltipWindow::textColourId, foreground);
    look.setColour (juce::TooltipWindow::outlineColourId, grid);
    addAndMakeVisible (bypass);
    bypass.setTooltip ("Bypasses processing while preserving the plugin's latency.");
    bypassAttachment = std::make_unique<State::ButtonAttachment> (p.parameters, "bypass", bypass);
    constexpr std::array<const char*, 5> ids { "width", "transientProtect", "guardAttack", "side", "output" };
    constexpr std::array<const char*, 5> names { "WIDTH", "TRANSIENT", "ATTACK", "SIDE", "OUTPUT" };
    constexpr std::array<const char*, 5> units { " %", " %", " ms", " %", " dB" };
    constexpr std::array<const char*, 5> hints {
        "Sets the amount of sparse convolution added to the stereo side signal.",
        "Reduces convolution input during clear spectral onsets to keep transients crisp.",
        "Sets how quickly transient protection attenuates the convolution input.",
        "Scales the combined original and Velvet side energy in the final stage.",
        "Adjusts the final stereo output level." };
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff183447));
        slider.setTextValueSuffix (units[i]); slider.setName (names[i]); slider.setTitle (names[i]);
        slider.setDoubleClickReturnValue (true, p.parameters.getParameter (ids[i])->convertFrom0to1 (
            p.parameters.getParameter (ids[i])->getDefaultValue()));
        labels[i].setText (names[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centred);
        labels[i].setFont (juce::FontOptions (10.0f));
        labels[i].setColour (juce::Label::textColourId, juce::Colour (0xff183447));
        addAndMakeVisible (slider); addAndMakeVisible (labels[i]);
        attachments[i] = std::make_unique<State::SliderAttachment> (p.parameters, ids[i], slider);
        editorTooltips::setHint (slider, labels[i], hints[i]);
    }
    constexpr std::array<const char*, 3> bandIds { "velvetLow", "velvetMid", "velvetHigh" };
    constexpr std::array<const char*, 3> bandNames { "LO  <200 Hz", "MID  200-1k", "HI  1k-20k" };
    constexpr std::array<const char*, 3> bandHints {
        "Sets the Velvet width added below 200 Hz.",
        "Sets the Velvet width added between 200 Hz and 1 kHz.",
        "Sets the Velvet width added from 1 kHz through the audible high band." };
    for (size_t i = 0; i < bandSliders.size(); ++i)
    {
        auto& slider = bandSliders[i];
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff183447));
        slider.setTextValueSuffix (" %"); slider.setName (bandNames[i]); slider.setTitle (bandNames[i]);
        slider.setDoubleClickReturnValue (true, 100.0);
        bandLabels[i].setText (bandNames[i], juce::dontSendNotification);
        bandLabels[i].setJustificationType (juce::Justification::centredLeft);
        bandLabels[i].setFont (juce::FontOptions (10.0f));
        bandLabels[i].setColour (juce::Label::textColourId, juce::Colour (0xff183447));
        addAndMakeVisible (slider); addAndMakeVisible (bandLabels[i]);
        bandAttachments[i] = std::make_unique<State::SliderAttachment> (p.parameters, bandIds[i], slider);
        editorTooltips::setHint (slider, bandLabels[i], bandHints[i]);
    }
    editorScaling::enable (*this, designWidth, designHeight);
    gpu.attach (*this);
}
void KuoVelvetEditor::resized()
{
    titleHint.setBounds (16, 30, 310, 36);
    bypass.setBounds (346, 30, 82, 28);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const auto x = 25 + static_cast<int> (i) * 82;
        labels[i].setBounds (x, 132, 82, 16);
        sliders[i].setBounds (x, 148, 82, 76);
    }
    for (size_t i = 0; i < bandSliders.size(); ++i)
    {
        const auto y = 262 + static_cast<int> (i) * 32;
        bandLabels[i].setBounds (28, y, 102, 24);
        bandSliders[i].setBounds (130, y, 278, 24);
    }
    const auto scale = editorScaling::transform (*this, designWidth, designHeight);
    titleHint.setTransform (scale);
    bypass.setTransform (scale);
    for (auto& slider : sliders) slider.setTransform (scale);
    for (auto& label : labels) label.setTransform (scale);
    for (auto& slider : bandSliders) slider.setTransform (scale);
    for (auto& label : bandLabels) label.setTransform (scale);
}
void KuoVelvetEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.saveState();
    g.addTransform (editorScaling::transform (*this, designWidth, designHeight));
    g.setGradientFill (juce::ColourGradient (background.brighter (0.015f), 0, 0,
                                            background.darker (0.06f), 0, designHeight, false));
    g.fillRect (0, 0, designWidth, designHeight);
    g.setColour (juce::Colour (0xffc8dfeb)); g.setFont (juce::FontOptions (10.0f));
    g.drawText ("J-SONIC / VELVET STEREO", 16, 12, 310, 16, juce::Justification::centredLeft);
    g.setColour (foreground); g.setFont (juce::FontOptions (25.0f));
    g.drawText ("KUO VELVET", 16, 30, 310, 36, juce::Justification::centredLeft);
    drawPanel (g, { 16, 82, 412, 153 }, juce::Colour (0xff8cb9d3));
    drawPanel (g, { 16, 244, 412, 122 }, juce::Colour (0xff8cb9d3));
    g.setColour (juce::Colour (0xff183447)); g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
    g.drawText ("VELVET WIDTH", 28, 94, 380, 20, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff183447)); g.setFont (juce::FontOptions (9.0f));
    g.drawText ("Sparse stereo texture with transient protection", 28, 112, 380, 16, juce::Justification::centredLeft);
    g.restoreState();
}
