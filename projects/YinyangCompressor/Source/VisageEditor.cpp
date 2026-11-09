#include "VisageEditor.h"
#include "../../EditorScaling.h"
#include "RatioMapping.h"
#if JUCE_LINUX
#endif
#include <algorithm>
#include <cmath>
#include <embedded/fonts.h>

namespace
{
constexpr int designWidth = 548;
constexpr int designHeight = 726;
constexpr int firstRow = 106;
constexpr int rowHeight = 51;
constexpr int columnWidth = 264;
constexpr int leftMargin = 12;
}

YinyangVisageEditor::YinyangVisageEditor (YinyangCompressorProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    const std::array<const char*, 19> ids { "model", "input", "threshold", "ratio", "attack",
        "release", "knee", "output", "mix", "sidechain", "drive", "transformer", "bypass",
        "upward", "stereoLink", "midSide", "evenHarmonics", "variTransient", "scHighTilt" };
    for (size_t i = 0; i < ids.size(); ++i)
        controls[i] = p.parameters.getParameter (ids[i]);

    view.setWindowDimensions (visage::Dimension::nativePixels (designWidth),
                              visage::Dimension::nativePixels (designHeight));
    view.onDraw() = [this] (visage::Canvas& canvas) { draw (canvas); };
    view.onMouseDown() = [this] (const visage::MouseEvent& e)
    {
        const auto scale = static_cast<float> (view.width()) / designWidth;
        const auto x = e.windowPosition().x / scale;
        const auto y = e.windowPosition().y / scale;
        dragging = controlAt (x, y);
        if (dragging < 0) return;
        auto* parameter = controls[static_cast<size_t> (dragging)];
        parameter->beginChangeGesture();
        setControl (dragging, x);
    };
    view.onMouseDrag() = [this] (const visage::MouseEvent& e)
    {
        if (dragging >= 0 && dynamic_cast<juce::AudioParameterFloat*> (controls[static_cast<size_t> (dragging)]) != nullptr)
            setControl (dragging, e.windowPosition().x * designWidth / view.width());
    };
    view.onMouseUp() = [this] (const visage::MouseEvent&)
    {
        if (dragging >= 0)
            controls[static_cast<size_t> (dragging)]->endChangeGesture();
        dragging = -1;
    };
    editorScaling::enable (*this, designWidth, designHeight);
    startTimerHz (30);
}

YinyangVisageEditor::~YinyangVisageEditor()
{
    stopTimer();
    if (dragging >= 0)
        controls[static_cast<size_t> (dragging)]->endChangeGesture();
    detachWindow();
}

bool YinyangVisageEditor::hasRenderedFrame()
{
    if (! isVisageAttached()) return false;
    const auto& image = view.takeScreenshot();
    return image.width() > 0 && image.height() > 0
        && image.sample (0, 0).toARGB() == 0xff111c27;
}

void YinyangVisageEditor::detachWindow()
{
#if JUCE_LINUX
    if (registeredFd >= 0)
    {
        juce::LinuxEventLoop::unregisterFdCallback (registeredFd);
        registeredFd = -1;
    }
#endif
    if (view.window()) view.close();
    parentHandle = nullptr;
}

void YinyangVisageEditor::attachWindow()
{
    auto* peer = getPeer();
    void* handle = peer != nullptr ? peer->getNativeHandle() : nullptr;
    if (handle == nullptr || ! isShowing()) return;
    if (handle == parentHandle && view.window()) return;
    detachWindow();
    parentHandle = handle;
    view.show (handle);
#if JUCE_LINUX
    if (view.window())
    {
        registeredFd = view.window()->posixFd();
        if (registeredFd >= 0)
            juce::LinuxEventLoop::registerFdCallback (registeredFd, [this] (int)
            {
                if (view.window()) view.window()->processPluginFdEvents();
            });
    }
#endif
    resized();
}

void YinyangVisageEditor::parentHierarchyChanged() { attachWindow(); }
void YinyangVisageEditor::visibilityChanged()
{
    if (isShowing()) attachWindow();
    else detachWindow();
}
void YinyangVisageEditor::resized()
{
    if (view.window()) view.setNativeWindowDimensionsFromHost (getWidth(), getHeight());
}

void YinyangVisageEditor::timerCallback()
{
    attachWindow();
    view.redraw();
}

int YinyangVisageEditor::controlAt (float x, float y) const
{
    const int column = static_cast<int> ((x - leftMargin) / columnWidth);
    const int row = static_cast<int> ((y - firstRow) / rowHeight);
    if (x < leftMargin || y < firstRow || column < 0 || column > 1 || row < 0 || row >= 10
        || column * 10 + row >= static_cast<int> (controls.size()))
        return -1;
    return column * 10 + row;
}

void YinyangVisageEditor::setControl (int index, float x)
{
    auto* parameter = controls[static_cast<size_t> (index)];
    const int column = index / 10;
    const float start = leftMargin + column * columnWidth;
    float normalized = std::clamp ((x - start - 10.0f) / (columnWidth - 35.0f), 0.0f, 1.0f);
    if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        normalized = parameter->getValue() < 0.5f ? 1.0f : 0.0f;
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        normalized = std::round (normalized * (choice->choices.size() - 1)) / (choice->choices.size() - 1);
    parameter->setValueNotifyingHost (normalized);
    view.redraw();
}

void YinyangVisageEditor::draw (visage::Canvas& canvas)
{
    const float scale = static_cast<float> (view.width()) / designWidth;
    canvas.setColor (0xff111c27);
    canvas.fill (0, 0, view.width(), view.height());
    const visage::Font title (23.0f * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font font (13.0f * scale, visage::fonts::Lato_Regular_ttf);
    canvas.setColor (0xffecd3a5);
    canvas.text ("YINYANG COMPRESSOR  /  VISAGE", title, visage::Font::kLeft,
                 20 * scale, 18 * scale, 520 * scale, 35 * scale);
    canvas.setColor (0xff8bb5c2);
    canvas.text ("Drag a bar to automate a parameter", font, visage::Font::kLeft,
                 20 * scale, 58 * scale, 500 * scale, 20 * scale);

    for (int i = 0; i < static_cast<int> (controls.size()); ++i)
    {
        auto* parameter = controls[static_cast<size_t> (i)];
        if (parameter == nullptr) continue;
        const float x = (leftMargin + (i / 10) * columnWidth) * scale;
        const float y = (firstRow + (i % 10) * rowHeight) * scale;
        const float value = parameter->getValue();
        canvas.setColor (0xffd8e3e8);
        canvas.text (parameter->getName (32).toStdString(), font, visage::Font::kLeft,
                     x + 7 * scale, y, 165 * scale, 20 * scale);
        auto valueText = parameter->getCurrentValueAsText();
        if (i == 3 && processor.parameters.getRawParameterValue ("upward")->load() >= 0.5f)
            valueText = juce::String (character::hostToYinRatio (
                processor.parameters.getRawParameterValue ("ratio")->load()), 2) + ":1";
        canvas.text (valueText.toStdString(), font, visage::Font::kRight,
                     x + 163 * scale, y, 83 * scale, 20 * scale);
        canvas.setColor (0xff2a3c4a);
        canvas.fill (x + 8 * scale, y + 26 * scale, 230 * scale, 10 * scale);
        canvas.setColor (0xff31b1c4);
        canvas.fill (x + 8 * scale, y + 26 * scale, value * 230 * scale, 10 * scale);
    }
    canvas.setColor (0xffecd3a5);
    canvas.text ("Gain reduction  " + juce::String (processor.reduction.load(), 1).toStdString() + " dB",
                 font, visage::Font::kLeft, 20 * scale, 634 * scale, 250 * scale, 24 * scale);
    canvas.text ("Input  " + juce::String (processor.inputPeak.load(), 1).toStdString() + " dBFS",
                 font, visage::Font::kLeft, 20 * scale, 662 * scale, 250 * scale, 24 * scale);
    canvas.text ("Output  " + juce::String (processor.outputPeak.load(), 1).toStdString() + " dBFS",
                 font, visage::Font::kLeft, 280 * scale, 662 * scale, 250 * scale, 24 * scale);
}
