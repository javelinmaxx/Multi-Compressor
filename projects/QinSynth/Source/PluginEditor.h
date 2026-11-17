#pragma once
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include <array>
#include <functional>

class QinLookAndFeel final : public StageLookAndFeel
{
public:
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,
                          const juce::Slider::SliderStyle,juce::Slider&) override;
};

class QinVoiceSlider final : public juce::Slider
{
public:
    QinVoiceSlider();
    void paint(juce::Graphics&) override;
};

class QinWavePad final : public juce::Component, public juce::SettableTooltipClient
{
public:
    QinWavePad();
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool setPosition(qin::WavePosition);
    qin::WavePosition getPosition() const noexcept { return position; }
    std::function<void()> onDragStart, onDragEnd;
    std::function<void(qin::WavePosition)> onPositionChange;
private:
    qin::WavePosition position;
    bool dragging=false;
};

class QinRetrigSwitch final : public juce::ToggleButton
{
public:
    explicit QinRetrigSwitch(const juce::String& caption="RETRIG");
    void paintButton(juce::Graphics&,bool,bool) override;
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
    static constexpr int designWidth=920, designHeight=719;
    void showOscillator(int oscillator);
    void showEnvelope(int envelope);
    void showFxSection(int section);
    void updateVisibility();
    void timerCallback() override;
    void updateDrivePreview();
    void updateEnvelopeTimePresentation();
    void updatePlayAvailability();
    QinSynthProcessor& processor;
    int currentOscillator=0, currentEnvelope=0, currentFxSection=0;
    bool lastTimesFive=false, lastPolyLegato=false;
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
    QinRetrigSwitch transformerButton {"TRANSFORMER"};
    juce::ComboBox driveMode;
    juce::Label driveModeLabel;
    QinVoiceSlider voiceCount;
    juce::Label activeVoicesLabel;
    QinRetrigSwitch polyLegatoButton {"POLY LEGATO"};
    juce::ComboBox legatoRule;
    juce::Label legatoRuleLabel, slideTimeLabel, slideShapeLabel;
    juce::Slider slideTime, slideShape;
    std::array<juce::Slider,4> noiseSliders;
    std::array<juce::Label,4> noiseLabels;
    juce::ComboBox noiseStyle;
    juce::Label noiseStyleLabel;
    juce::TextButton timesFiveButton {"*5"};
    juce::Slider filterMergeSlider;
    juce::Label filterMergeLabel;
    std::array<juce::Slider,44> sliders;
    std::array<juce::Label,44> labels;
    std::array<juce::Slider,3> curveSliders;
    juce::Label curveLabel;
    std::array<QinWavePad,3> waves;
    std::array<juce::RangedAudioParameter*,3> waveXParameters {}, waveYParameters {};
    std::array<QinRetrigSwitch,3> retrig;
    std::array<juce::Label,3> waveLabels;
    juce::ComboBox filterType, modTarget;
    juce::Label filterTypeLabel, modTargetLabel;
    using State=juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,44> sliderAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> curveAttachments;
    std::array<std::unique_ptr<State::ButtonAttachment>,3> retrigAttachments;
    std::unique_ptr<State::SliderAttachment> voiceCountAttachment;
    std::unique_ptr<State::ButtonAttachment> polyLegatoAttachment;
    std::unique_ptr<State::ComboBoxAttachment> legatoRuleAttachment;
    std::unique_ptr<State::SliderAttachment> slideTimeAttachment, slideShapeAttachment;
    std::unique_ptr<State::ComboBoxAttachment> filterTypeAttachment, modTargetAttachment;
    std::unique_ptr<State::ComboBoxAttachment> driveModeAttachment;
    std::unique_ptr<State::ButtonAttachment> transformerAttachment;
    std::unique_ptr<State::ButtonAttachment> noiseAttachment, timesFiveAttachment;
    std::array<std::unique_ptr<State::SliderAttachment>,4> noiseSliderAttachments;
    std::unique_ptr<State::ComboBoxAttachment> noiseStyleAttachment;
    std::unique_ptr<State::SliderAttachment> filterMergeAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthEditor)
};
