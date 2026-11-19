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
    void setSuffix(juce::String value) { suffix=std::move(value); repaint(); }
private:
    juce::String suffix {"VOICES"};
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

class QinAnalogSwitch final : public juce::ToggleButton
{
public:
    explicit QinAnalogSwitch(const juce::String& caption="RETRIG");
    void advance(float seconds);
    void snapToState() noexcept { position=getToggleState() ? 1.f : 0.f; }
    void paintButton(juce::Graphics&,bool,bool) override;
private:
    float position=0;
};

class QinTuningPanel final : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
};

class QinTuningKey final : public juce::Slider
{
public:
    QinTuningKey();
    void setIdentity(juce::String note,bool black,bool root);
    const juce::String& getNoteName() const noexcept { return noteName; }
    bool isRootKey() const noexcept { return rootKey; }
    bool isBlackKey() const noexcept { return blackKey; }
    void paint(juce::Graphics&) override;
private:
    juce::String noteName;
    bool blackKey=false, rootKey=false;
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
    static constexpr int designWidth=920, designHeight=640;
    void showOscillator(int oscillator);
    void showEnvelope(int envelope);
    void showFxSection(int section);
    void updateVisibility();
    void timerCallback() override;
    void updateDrivePreview();
    void updateEnvelopeTimePresentation();
    void updatePlayAvailability();
    void updateTuningLabels();
    QinSynthProcessor& processor;
    int currentOscillator=0, currentEnvelope=0, currentFxSection=0;
    bool lastTimesFive=false, lastPolyLegato=false, lastVerbSync=false;
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
    QinAnalogSwitch transformerButton {"TRANSFORMER"};
    juce::Slider driveMode, driveMix;
    juce::Label driveModeLabel, driveMixLabel;
    QinVoiceSlider voiceCount;
    std::array<QinVoiceSlider,3> octaveDisplays;
    juce::TextButton tuneButton {"TUNE"}, syncButton {"SYNC"};
    QinTuningPanel tuningPanel;
    std::array<QinTuningKey,12> tuningSliders;
    juce::Slider tuningRoot, tuningA4, verbNoteSlider;
    juce::Label tuningRootLabel, tuningA4Label;
    juce::Label activeVoicesLabel;
    QinAnalogSwitch polyLegatoButton {"POLY LEGATO"};
    juce::Slider legatoRule;
    juce::Label legatoRuleLabel, slideTimeLabel, slideShapeLabel;
    juce::Slider slideTime, slideShape;
    std::array<juce::Slider,4> noiseSliders;
    std::array<juce::Label,4> noiseLabels;
    std::array<juce::Slider,3> detuneShapeSliders;
    std::array<juce::Label,3> detuneShapeLabels;
    std::array<juce::Slider,3> fmAmountSliders;
    std::array<juce::Label,3> fmAmountLabels;
    std::array<juce::Slider,3> fmSources;
    std::array<juce::Label,3> fmSourceLabels;
    juce::ComboBox noiseStyle;
    juce::Label noiseStyleLabel;
    QinAnalogSwitch timesFiveButton {"*5"};
    juce::Slider filterMergeSlider;
    juce::Label filterMergeLabel;
    std::array<juce::Slider,44> sliders;
    std::array<juce::Label,44> labels;
    std::array<juce::Slider,3> curveSliders;
    juce::Label curveLabel;
    std::array<QinWavePad,3> waves;
    std::array<juce::RangedAudioParameter*,3> waveXParameters {}, waveYParameters {};
    std::array<QinAnalogSwitch,3> retrig;
    std::array<juce::Label,3> waveLabels;
    juce::ComboBox filterType, modTarget;
    juce::Label modTargetLabel;
    using State=juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,44> sliderAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> curveAttachments;
    std::array<std::unique_ptr<State::ButtonAttachment>,3> retrigAttachments;
    std::unique_ptr<State::SliderAttachment> voiceCountAttachment;
    std::array<std::unique_ptr<State::SliderAttachment>,3> octaveAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,12> tuningAttachments;
    std::unique_ptr<State::SliderAttachment> tuningRootAttachment, tuningA4Attachment, verbNoteAttachment;
    std::unique_ptr<State::ButtonAttachment> syncAttachment;
    std::unique_ptr<State::ButtonAttachment> polyLegatoAttachment;
    std::unique_ptr<State::SliderAttachment> legatoRuleAttachment;
    std::unique_ptr<State::SliderAttachment> slideTimeAttachment, slideShapeAttachment;
    std::unique_ptr<State::ComboBoxAttachment> filterTypeAttachment, modTargetAttachment;
    std::unique_ptr<State::SliderAttachment> driveModeAttachment, driveMixAttachment;
    std::unique_ptr<State::ButtonAttachment> transformerAttachment;
    std::unique_ptr<State::ButtonAttachment> noiseAttachment, timesFiveAttachment;
    std::array<std::unique_ptr<State::SliderAttachment>,4> noiseSliderAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> detuneShapeAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> fmAmountAttachments;
    std::array<std::unique_ptr<State::SliderAttachment>,3> fmSourceAttachments;
    std::unique_ptr<State::ComboBoxAttachment> noiseStyleAttachment;
    std::unique_ptr<State::SliderAttachment> filterMergeAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QinSynthEditor)
};
