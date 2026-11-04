#include "PluginEditor.h"
#include "../../EditorScaling.h"
#include "../../EditorTooltips.h"
using namespace stageStyle;
NianSaturatorEditor::NianSaturatorEditor (NianSaturatorProcessor& p) : AudioProcessorEditor (&p)
{
    addAndMakeVisible (titleHint);
    setOpaque (true); setLookAndFeel (&look);
    look.setColour (juce::TooltipWindow::backgroundColourId, panel);
    look.setColour (juce::TooltipWindow::textColourId, foreground);
    look.setColour (juce::TooltipWindow::outlineColourId, grid);
    addAndMakeVisible (bypass);
    bypass.setTooltip ("Bypasses processing while preserving the plugin's latency.");
    bypassAttachment = std::make_unique<State::ButtonAttachment> (p.parameters, "bypass", bypass);
    constexpr std::array<const char*, 4> ids { "crossover", "lowDrive", "highDrive", "output" };
    constexpr std::array<const char*, 4> names { "CROSSOVER", "LOW", "HIGH", "OUTPUT" };
    constexpr std::array<const char*, 4> units { " Hz", " %", " %", " dB" };
    constexpr std::array<const char*, 4> hints {
        "Sets the frequency where the low and high bands meet.",
        "Sets the low-band peak enhancement or drive.",
        "Sets the high-band peak enhancement or drive.",
        "Adjusts the final stereo output level." };
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, foreground);
        slider.setTextValueSuffix (units[i]); slider.setName (names[i]); slider.setTitle (names[i]);
        slider.setDoubleClickReturnValue (true, p.parameters.getParameter (ids[i])->convertFrom0to1 (
            p.parameters.getParameter (ids[i])->getDefaultValue()));
        labels[i].setText (names[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centred);
        labels[i].setFont (juce::FontOptions (10.0f));
        labels[i].setColour (juce::Label::textColourId, secondary);
        addAndMakeVisible (slider); addAndMakeVisible (labels[i]);
        attachments[i] = std::make_unique<State::SliderAttachment> (p.parameters, ids[i], slider);
        editorTooltips::setHint (slider, labels[i], hints[i]);
    }
    mode.setColour (juce::ComboBox::backgroundColourId, panel);
    mode.setColour (juce::ComboBox::textColourId, foreground);
    mode.setColour (juce::ComboBox::outlineColourId, grid);
    mode.setColour (juce::ComboBox::arrowColourId, amber);
    mode.addItem ("F / PEAK",1); mode.addItem ("T / DRIVE",2);
    mode.setTooltip ("Selects automatic narrow peak enhancement or conventional drive.");
    addAndMakeVisible (mode); modeAttachment = std::make_unique<State::ComboBoxAttachment> (p.parameters,"mode",mode);
    editorScaling::enable (*this, designWidth, designHeight);
}
void NianSaturatorEditor::resized()
{
    titleHint.setBounds (16, 30, 310, 36);
    bypass.setBounds (346, 30, 82, 28);
    mode.setBounds (260, 94, 150, 26);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const auto x = 30 + static_cast<int> (i) * 96;
        labels[i].setBounds (x, 140, 96, 16);
        sliders[i].setBounds (x, 156, 96, 76);
    }
    const auto scale = editorScaling::transform (*this, designWidth, designHeight);
    titleHint.setTransform (scale);
    bypass.setTransform (scale);
    mode.setTransform (scale);
    for (auto& slider : sliders) slider.setTransform (scale);
    for (auto& label : labels) label.setTransform (scale);
}
void NianSaturatorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.saveState();
    g.addTransform (editorScaling::transform (*this, designWidth, designHeight));
    g.setGradientFill (juce::ColourGradient (background.brighter (0.035f), 0, 0,
                                            background.darker (0.13f), 0, designHeight, false));
    g.fillRect (0, 0, designWidth, designHeight);
    g.setColour (secondary); g.setFont (juce::FontOptions (10.0f));
    g.drawText ("J-SONIC / ADAPTIVE SATURATION", 16, 12, 310, 16, juce::Justification::centredLeft);
    g.setColour (foreground); g.setFont (juce::FontOptions (25.0f));
    g.drawText ("NIAN SATURATOR", 16, 30, 310, 36, juce::Justification::centredLeft);
    drawPanel (g, { 16, 82, 412, 162 });
    g.setColour (foreground); g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
    g.drawText ("SATURATION", 28, 94, 380, 20, juce::Justification::centredLeft);
    g.setColour (secondary); g.setFont (juce::FontOptions (9.0f));
    g.drawText ("Adaptive peak emphasis or two-band drive", 28, 116, 380, 16, juce::Justification::centredLeft);
    g.restoreState();
}
