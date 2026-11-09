#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <visage/app.h>
#include <array>

class YinyangVisageEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YinyangVisageEditor (YinyangCompressorProcessor&);
    ~YinyangVisageEditor() override;
    bool isVisageAttached() const noexcept { return view.window() != nullptr && view.isShowing(); }
    bool hasRenderedFrame();
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void attachWindow();
    void detachWindow();
    void timerCallback() override;
    void draw (visage::Canvas&);
    int controlAt (float x, float y) const;
    void setControl (int index, float x);

    YinyangCompressorProcessor& processor;
    visage::ApplicationWindow view;
    std::array<juce::RangedAudioParameter*, 19> controls {};
    int dragging = -1;
    void* parentHandle = nullptr;
    int registeredFd = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YinyangVisageEditor)
};
