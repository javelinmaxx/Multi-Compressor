#pragma once
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include <array>

class QinLookAndFeel final : public StageLookAndFeel
{
public:
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,
                          const juce::Slider::SliderStyle,juce::Slider&) override;
};

class QinSynthEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QinSynthEditor(QinSynthProcessor&);
    ~QinSynthEditor() override { stopTimer(); gpu.detach(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    friend struct QinSynthEditorTestAccess;
    static constexpr int designWidth=920, designHeight=765;
    void showOscillator(int oscillator);
    void showEnvelope(int envelope);
    void showFxSection(int section);
    void updateVisibility();
    void timerCallback() override;
    void updateDrivePreview();
    void multiplyFilterTimes();
    QinSynthProcessor& processor;
    int currentOscillator=0, currentEnvelope=0, currentFxSection=0;
    float liveCutoff=0;
    uint32_t lastScopeEpoch=0;
    bool liveScopeValid=false;
    QinSynthProcessor::DriveScope liveScope;
    std::array<float,QinSynthProcessor::driveScopePoints> previewInput {}, previewOutput {};
    QinLookAndFeel look;
    pluginGpu::Renderer gpu;
    juce::TooltipWindow tooltipWindow {this};
    editorTooltips::TitleHint titleHint {JucePlugin_VersionString};
    std::array<juce::TextButton,3> oscillatorTabs, envelopeTabs;
    std::array<juce::TextButton,2> fxTabs;
    juce::TextButton noiseButton {"N"};
    juce::TextButton filterTimesFive {"*5"};
    juce::Slider filterMergeSlider;
    juce::Label filterMergeLabel;
    std::array<juce::Slider,44> sliders;
    std::array<juce::Label,44> labels;
    std::array<juce::Slider,3> curveSliders;
    juce::Label curveLabel;
    std::array<juce::ComboBox,3> waves;
    std::array<juce::Label,3> waveLabels;
    juce::ComboBox filterType, modTarget;
    juce::Label filterTypeLabel, modTargetLabel;
    using State=juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,44> sliderAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> curveAttachments;
    std::array<std::unique_ptr<State::ComboBoxAttachment>,3> waveAttachments;
    std::unique_ptr<State::ComboBoxAttachment> filterTypeAttachment, modTargetAttachment;
    std::unique_ptr<State::ButtonAttachment> noiseAttachment;
    std::unique_ptr<State::SliderAttachment> filterMergeAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthEditor)
};
