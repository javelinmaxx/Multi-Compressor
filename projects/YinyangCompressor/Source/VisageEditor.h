#pragma once

#include "PluginProcessor.h"
#include "GainReductionHistory.h"
#include "VisageLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <visage/app.h>
#include <visage/widgets.h>
#include <array>

class YinyangVisageEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YinyangVisageEditor (YinyangCompressorProcessor&);
    ~YinyangVisageEditor() override;
    bool isVisageAttached() const noexcept { return view.window() != nullptr && view.isShowing(); }
    bool hasRenderedFrame();
    void savePreview (const std::string& path);
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void attachWindow();
    void detachWindow();
    void timerCallback() override;
    void draw (visage::Canvas&);
    int controlAt (float x, float y) const;
    void setControl (int index, float normalized);
    void drawControls (visage::Canvas&, float scale);
    void drawMeter (visage::Canvas&, float scale, const yinyangVisage::Theme&, bool yin);
    void drawTooltip (visage::Canvas&, float scale);
    void drawModelMenu (visage::Canvas&, float scale);
    void showModelMenu();
    void beginEdit (int index);
    void finishEdit (bool commit);
    void layoutEditField();
    bool handleKey (const visage::KeyEvent&);
    void createAccessibleControls();
    double lastTick = 0.0;
    float displayedReduction = 0.0f;
    int lastModel = -1;
    bool lastYin = false;
    float dragStartY = 0.0f, dragStartValue = 0.0f;

    YinyangCompressorProcessor& processor;
    visage::ApplicationWindow view;
    std::array<juce::RangedAudioParameter*, 19> controls {};
    std::array<std::unique_ptr<juce::Component>, 19> accessibleControls;
    using State = juce::AudioProcessorValueTreeState;
    std::array<std::unique_ptr<State::SliderAttachment>, 19> sliderAttachments;
    std::array<std::unique_ptr<State::ButtonAttachment>, 19> buttonAttachments;
    std::unique_ptr<State::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::ToggleButton> waveformControl;
    std::unique_ptr<visage::TextEditor> valueEditor;
    int focusedControl = 0, editingControl = -1, hoveredControl = -1;
    bool modelMenuOpen = false;
    int modelMenuSelection = 0;
    float hoverX = 0.0f, hoverY = 0.0f;
    double hoverStarted = 0.0;
    GainReductionHistory history;
    int dragging = -1;
    void* parentHandle = nullptr;
    int registeredFd = -1;
    friend struct YinyangVisageEditorTestAccess;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YinyangVisageEditor)
};
