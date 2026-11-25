#pragma once
#include "../../EditorDimming.h"
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
class KuoVelvetLookAndFeel final : public StageLookAndFeel
{
public:
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};
class KuoVelvetEditor final : public juce::AudioProcessorEditor
{
public:
    explicit KuoVelvetEditor (KuoVelvetProcessor&);
    ~KuoVelvetEditor() override { gpu.detach(); setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    pluginGpu::Renderer gpu;
    static constexpr int designWidth = 444, designHeight = 382;
    KuoVelvetLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    std::array<juce::Slider, 5> sliders;
    std::array<juce::Label, 5> labels;
    std::array<juce::Slider, 3> bandSliders;
    std::array<juce::Label, 3> bandLabels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>, 5> attachments;
    std::array<std::unique_ptr<State::SliderAttachment>, 3> bandAttachments;
    std::unique_ptr<State::ButtonAttachment> bypassAttachment;
    editorDimming::Overlay dimmer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KuoVelvetEditor)
};
