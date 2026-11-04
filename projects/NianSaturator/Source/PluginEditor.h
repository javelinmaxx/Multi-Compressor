#pragma once
#include "../../EditorTooltips.h"
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
class NianSaturatorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NianSaturatorEditor (NianSaturatorProcessor&);
    ~NianSaturatorEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    class SignalDisplay final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit SignalDisplay (NianSaturatorProcessor& p) : processor (p) { startTimerHz (30); }
        void paint (juce::Graphics&) override;
        void refresh();
    private:
        void timerCallback() override { refresh(); }
        NianSaturatorProcessor& processor;
        std::array<NianSaturatorProcessor::DisplaySample, 256> history {};
        int position = 0;
    };
    static constexpr int designWidth = 444, designHeight = 432;
    SignalDisplay signalDisplay;
    StageLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    juce::ComboBox mode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::array<juce::Slider, 4> sliders;
    std::array<juce::Label, 4> labels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>, 4> attachments;
    std::unique_ptr<State::ButtonAttachment> bypassAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NianSaturatorEditor)
};
