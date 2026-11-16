#pragma once
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
class RenTransformerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit RenTransformerEditor (RenTransformerProcessor&);
    ~RenTransformerEditor() override { gpu.detach(); setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    pluginGpu::Renderer gpu;
    static constexpr int designWidth = 444, designHeight = 260;
    StageLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    std::array<juce::Slider, 5> sliders;
    std::array<juce::Label, 5> labels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>, 5> attachments;
    std::unique_ptr<State::ButtonAttachment> bypassAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenTransformerEditor)
};
