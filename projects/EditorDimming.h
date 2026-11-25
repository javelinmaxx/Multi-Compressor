#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace editorDimming
{
// A mouse-transparent top layer dims controls without changing their parameters or hit targets.
class Overlay final : public juce::Component, private juce::Timer
{
public:
    void attach (juce::Component& editor, juce::AudioProcessorValueTreeState& state,
                 juce::Component* bypassControl, juce::Component* deltaDisplay = nullptr,
                 const char* deltaParameter = nullptr,
                 juce::Component* secondDeltaDisplay = nullptr, float deltaTopExpansion = 0.0f)
    {
        bypass = state.getRawParameterValue ("bypass");
        delta = deltaParameter != nullptr ? state.getRawParameterValue (deltaParameter) : nullptr;
        bypassButton = bypassControl;
        protectedDisplay = deltaDisplay;
        protectedSecondDisplay = secondDeltaDisplay;
        protectedTopExpansion = deltaTopExpansion;
        start (editor);
    }

    void attachHostBypass (juce::Component& editor, const std::atomic<bool>& hostBypassState)
    {
        hostBypass = &hostBypassState;
        start (editor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bypassed = active (bypass) || active (hostBypass);
        const auto deltaMonitoring = ! bypassed && active (delta);
        if (! bypassed && ! deltaMonitoring) return;

        if (bypassed && bypassButton != nullptr)
            g.excludeClipRegion (getLocalArea (bypassButton, bypassButton->getLocalBounds()));
        if (deltaMonitoring && protectedDisplay != nullptr)
        {
            auto area = getLocalArea (protectedDisplay, protectedDisplay->getLocalBounds());
            area.setTop (area.getY() - juce::roundToInt (area.getHeight() * protectedTopExpansion));
            g.excludeClipRegion (area);
        }
        if (deltaMonitoring && protectedSecondDisplay != nullptr)
            g.excludeClipRegion (getLocalArea (protectedSecondDisplay,
                                                protectedSecondDisplay->getLocalBounds()));

        g.setColour (juce::Colour (0xff34373b).withAlpha (0.68f));
        g.fillRect (getLocalBounds());
    }

    void parentSizeChanged() override
    {
        if (auto* parent = getParentComponent()) setBounds (parent->getLocalBounds());
    }

private:
    static bool active (const std::atomic<float>* value)
    { return value != nullptr && value->load (std::memory_order_relaxed) > 0.5f; }
    static bool active (const std::atomic<bool>* value)
    { return value != nullptr && value->load (std::memory_order_relaxed); }

    void start (juce::Component& editor)
    {
        setInterceptsMouseClicks (false, false);
        editor.addAndMakeVisible (*this);
        setBounds (editor.getLocalBounds());
        toFront (false);
        startTimerHz (15);
    }

    void timerCallback() override
    {
        const auto dimBypass = active (bypass) || active (hostBypass);
        const auto dimDelta = ! dimBypass && active (delta);
        if (dimBypass != lastBypass || dimDelta != lastDelta)
        {
            lastBypass = dimBypass;
            lastDelta = dimDelta;
            repaint();
        }
    }

    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* delta = nullptr;
    const std::atomic<bool>* hostBypass = nullptr;
    juce::Component* bypassButton = nullptr;
    juce::Component* protectedDisplay = nullptr;
    juce::Component* protectedSecondDisplay = nullptr;
    float protectedTopExpansion = 0.0f;
    bool lastBypass = false, lastDelta = false;
};
}
