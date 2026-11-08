#pragma once
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include <array>

class QinSynthEditor final : public juce::AudioProcessorEditor
{
public:
    explicit QinSynthEditor(QinSynthProcessor&);
    ~QinSynthEditor() override { gpu.detach(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    friend struct QinSynthEditorTestAccess;
    static constexpr int designWidth=760, designHeight=600;
    void showPage(int page);
    void showOscillator(int oscillator);
    void updateVisibility();
    int currentPage=0, currentOscillator=0;
    StageLookAndFeel look;
    pluginGpu::Renderer gpu;
    juce::TooltipWindow tooltipWindow {this};
    editorTooltips::TitleHint titleHint {JucePlugin_VersionString};
    std::array<juce::TextButton,2> pageTabs;
    std::array<juce::TextButton,3> oscillatorTabs;
    std::array<juce::Slider,23> sliders;
    std::array<juce::Label,23> labels;
    std::array<juce::ComboBox,3> waves;
    std::array<juce::Label,3> waveLabels;
    using State=juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,23> sliderAttachments;
    std::array<std::unique_ptr<State::ComboBoxAttachment>,3> waveAttachments;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthEditor)
};
