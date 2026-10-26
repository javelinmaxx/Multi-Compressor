#pragma once
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
#include "../../EditorTooltips.h"
class LanReverbEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LanReverbEditor (LanReverbProcessor&);
    ~LanReverbEditor() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    static constexpr int designWidth = 740, designHeight = 540;
    LanReverbProcessor& processor;
    StageLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    juce::TextButton automatic { "AUTO" };
    std::array<juce::Slider,10> sliders;
    std::array<juce::Label,10> labels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,10> attachments;
    std::unique_ptr<State::ButtonAttachment> autoAttachment, bypassAttachment;
    lan::Analysis analysis;
    float reduction = 0, outputPeak = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LanReverbEditor)
};
