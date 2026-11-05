#pragma once
#include "../../PluginGpuRenderer.h"
#include "../../EditorTooltips.h"
#include "PluginProcessor.h"
#include "../../StagePluginStyle.h"
class NianSaturatorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NianSaturatorEditor (NianSaturatorProcessor&);
    ~NianSaturatorEditor() override { gpu.detach(); setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    pluginGpu::Renderer gpu;
    class SignalDisplay final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit SignalDisplay (NianSaturatorProcessor&);
        void paint (juce::Graphics&) override;
        void refresh();
    private:
        void timerCallback() override { refresh(); }
        NianSaturatorProcessor& processor;
        juce::dsp::FFT fft { 11 };
        std::array<NianSaturatorProcessor::DisplaySample, 2048> history {};
        std::array<std::array<float, 1025>, 3> magnitudes {};
        std::array<float, 2048> window {};
        int position = 0, received = 0;
        float lastCrossover = 0;
    };
    static constexpr int designWidth = 444, designHeight = 432;
    SignalDisplay signalDisplay;
    StageLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    juce::ComboBox mode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::array<juce::Slider, 5> sliders;
    std::array<juce::Label, 5> labels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>, 5> attachments;
    std::unique_ptr<State::ButtonAttachment> bypassAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NianSaturatorEditor)
};
