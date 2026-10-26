#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumShelfHandle final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SpectrumShelfHandle (bool high) : isHigh(high)
    {
        setName(high ? "High shelf" : "Low shelf");
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        setTooltip("Drag horizontally for shelf frequency and vertically for gain, or double-click to reset the shelf.");
    }
    ~SpectrumShelfHandle() override { finishGesture(); }
    void paint (juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff171619)); g.fillEllipse(1,1,14,14);
        g.setColour(juce::Colour(0xffc49a5c)); g.drawEllipse(1,1,14,14,1.2f);
        g.setFont(juce::FontOptions(8.f)); g.drawText(isHigh ? "H" : "L",getLocalBounds(),juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override { if (!gesturing) { gesturing = true; if (begin) begin(); } if (drag) drag(e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (drag) drag(e); }
    void mouseUp (const juce::MouseEvent&) override { finishGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { finishGesture(); if (reset) reset(); }
    std::function<void()> begin, end, reset;
    std::function<void(const juce::MouseEvent&)> drag;
private:
    void finishGesture() { if (gesturing) { gesturing = false; if (end) end(); } }
    bool isHigh, gesturing = false;
};
