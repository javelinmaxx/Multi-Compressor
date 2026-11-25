#pragma once
#include "../../EditorDimming.h"
#include "../../PluginGpuRenderer.h"
#include "PluginProcessor.h"
#include "SpectrumShelfHandle.h"
#include "SpectrumDisplaySmoothing.h"
#include "../../StagePluginStyle.h"
#include "../../EditorTooltips.h"
class LanReverbEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LanReverbEditor (LanReverbProcessor&);
    ~LanReverbEditor() override { stopTimer(); processor.setSpectrumMonitor(false); gpu.detach(); setLookAndFeel(nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    pluginGpu::Renderer gpu;
    void timerCallback() override;
    void updateTimeMode();
    void updateShelves();
    void moveShelf (int, juce::Point<float>);
    static constexpr int designWidth = 740, designHeight = 808;
    LanReverbProcessor& processor;
    StageLookAndFeel look;
    juce::TooltipWindow tooltipWindow { this };
    editorTooltips::TitleHint titleHint { JucePlugin_VersionString };
    StageBypassButton bypass;
    juce::TextButton automatic { "AUTO" }, sync { "SYNC" };
    bool timeSynced = false;
    std::array<juce::Slider,13> sliders;
    std::array<juce::Label,13> labels;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>,13> attachments;
    std::unique_ptr<State::ButtonAttachment> autoAttachment, bypassAttachment, syncAttachment;
    std::array<SpectrumShelfHandle,4> shelfHandles {
        SpectrumShelfHandle(false),SpectrumShelfHandle(true),
        SpectrumShelfHandle(false,true),SpectrumShelfHandle(true,true)};
    lan::Analysis analysis;
    lan::TapDisplay tapDisplay;
    lan::SpectrumDisplaySmoother spectrumDisplay;
    double lastSpectrumTick=0;
    float reduction = 0, outputPeak = 0;
    editorDimming::Overlay dimmer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LanReverbEditor)
};
