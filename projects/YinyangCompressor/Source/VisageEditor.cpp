#include "VisageEditor.h"
#include "../../EditorScaling.h"
#include "RatioMapping.h"
#include "HistoryDisplayScale.h"
#if JUCE_LINUX
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <embedded/fonts.h>
#include <embedded/yinyang_knob.h>

namespace
{
constexpr int designWidth = 548;
constexpr int designHeight = 726;
}

YinyangVisageEditor::YinyangVisageEditor (YinyangCompressorProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    for (size_t i = 0; i < yinyangVisage::controls.size(); ++i)
        controls[i] = p.parameters.getParameter (yinyangVisage::controls[i].id);

    view.setWindowDimensions (visage::Dimension::nativePixels (designWidth),
                              visage::Dimension::nativePixels (designHeight));
    view.onDraw() = [this] (visage::Canvas& canvas) { draw (canvas); };
    view.setAcceptsKeystrokes (true);
    view.onKeyPress() = [this] (const visage::KeyEvent& event) { return handleKey (event); };
    view.onMouseMove() = [this] (const visage::MouseEvent& event)
    {
        const float scale = static_cast<float> (view.width()) / designWidth;
        if (scale <= 0.0f) return;
        hoverX = event.windowPosition().x / scale;
        hoverY = event.windowPosition().y / scale;
        int target = controlAt (hoverX, hoverY);
        if (target < 0 && hoverX >= 406 && hoverX < 512 && hoverY >= 142 && hoverY < 482) target = -2;
        if (target < 0 && target != -2 && hoverX >= 24 && hoverX < 420 && hoverY >= 18 && hoverY < 64) target = -3;
        if (target != hoveredControl)
        {
            hoveredControl = target;
            hoverStarted = juce::Time::getMillisecondCounterHiRes() * 0.001;
            view.redraw();
        }
    };
    view.onMouseExit() = [this] (const visage::MouseEvent&) { hoveredControl = -1; view.redraw(); };
    valueEditor = std::make_unique<visage::TextEditor> ("Parameter value");
    valueEditor->setNumberEntry();
    valueEditor->setMargin (3.0f, 0.0f);
    valueEditor->setMaxCharacters (32);
    valueEditor->onEnterKey() += [this] { finishEdit (true); };
    valueEditor->onEscapeKey() += [this] { finishEdit (false); };
    valueEditor->onFocusChange() += [this] (bool focused, bool)
    {
        if (! focused && editingControl >= 0) finishEdit (true);
    };
    view.addChild (*valueEditor, false);
    createAccessibleControls();
    view.onMouseDown() = [this] (const visage::MouseEvent& e)
    {
        const auto scale = static_cast<float> (view.width()) / designWidth;
        const auto x = e.windowPosition().x / scale;
        const auto y = e.windowPosition().y / scale;
        if (modelMenuOpen)
        {
            if (x >= 24 && x < 324 && y >= 100 && y < 212)
            {
                const int selected = std::clamp (static_cast<int> ((y - 100) / 28), 0, 3);
                controls[0]->beginChangeGesture();
                setControl (0, static_cast<float> (selected) / 3.0f);
                controls[0]->endChangeGesture();
            }
            modelMenuOpen = false;
            view.redraw();
            return;
        }
        dragging = controlAt (x, y);
        if (dragging < 0)
        {
            if (x >= 406 && x < 512 && y >= 142 && y < 482)
            {
                focusedControl = static_cast<int> (controls.size());
                view.requestKeyboardFocus();
                const bool enabled = processor.waveformDisplay.load (std::memory_order_relaxed);
                processor.waveformDisplay.store (! enabled, std::memory_order_relaxed);
                view.redraw();
            }
            return;
        }
        focusedControl = dragging;
        view.requestKeyboardFocus();
        auto* parameter = controls[static_cast<size_t> (dragging)];
        const auto& spec = yinyangVisage::controls[static_cast<size_t> (dragging)];
        const auto kind = spec.kind;
        if (kind == yinyangVisage::ControlKind::model)
        {
            dragging = -1;
            showModelMenu();
            return;
        }
        if (kind == yinyangVisage::ControlKind::knob && y >= spec.y + spec.height - 22.0f)
        {
            const int index = dragging;
            dragging = -1;
            beginEdit (index);
            return;
        }
        parameter->beginChangeGesture();
        dragStartY = y;
        dragStartValue = parameter->getValue();
        if (kind == yinyangVisage::ControlKind::toggle)
            setControl (dragging, dragStartValue < 0.5f ? 1.0f : 0.0f);
        else if (e.repeatClickCount() >= 2)
            setControl (dragging, parameter->getDefaultValue());
    };
    view.onMouseDrag() = [this] (const visage::MouseEvent& e)
    {
        if (dragging >= 0 && yinyangVisage::controls[static_cast<size_t> (dragging)].kind == yinyangVisage::ControlKind::knob)
        {
            const auto scale = static_cast<float> (view.width()) / designWidth;
            const auto distance = dragStartY - e.windowPosition().y / scale;
            setControl (dragging, dragStartValue + distance / (e.isShiftDown() ? 800.0f : 160.0f));
        }
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
    detachWindow();
}

bool YinyangVisageEditor::hasRenderedFrame()
{
    if (! isVisageAttached()) return false;
    const auto& image = view.takeScreenshot();
    const int model = std::clamp (static_cast<int> (std::round (controls[0]->getValue() * 3.0f)), 0, 3);
    const bool yin = controls[13]->getValue() >= 0.5f;
    return image.width() > 0 && image.height() > 0
        && image.width() == view.nativeWidth() && image.height() == view.nativeHeight()
        && image.sample (0, 0).toARGB() == yinyangVisage::themeFor (model, yin).background;
}

void YinyangVisageEditor::savePreview (const std::string& path)
{
    if (isVisageAttached()) view.takeScreenshot().save (path);
}

void YinyangVisageEditor::detachWindow()
{
    if (dragging >= 0)
    {
        controls[static_cast<size_t> (dragging)]->endChangeGesture();
        dragging = -1;
    }
    modelMenuOpen = false;
#if JUCE_LINUX
    if (registeredFd >= 0)
    {
        juce::LinuxEventLoop::unregisterFdCallback (registeredFd);
        registeredFd = -1;
    }
#endif
    if (editingControl >= 0) finishEdit (false);
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
    const auto scale = std::min (static_cast<float> (getWidth()) / designWidth,
                                 static_cast<float> (getHeight()) / designHeight);
    for (size_t i = 0; i < accessibleControls.size(); ++i)
    {
        const auto& c = yinyangVisage::controls[i];
        if (accessibleControls[i]) accessibleControls[i]->setBounds (
            juce::roundToInt (c.x * scale), juce::roundToInt (c.y * scale),
            juce::roundToInt (c.width * scale), juce::roundToInt (c.height * scale));
    }
    if (waveformControl) waveformControl->setBounds (juce::roundToInt (406 * scale), juce::roundToInt (142 * scale),
                                                      juce::roundToInt (106 * scale), juce::roundToInt (340 * scale));
    layoutEditField();
}

void YinyangVisageEditor::timerCallback()
{
    attachWindow();
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto model = static_cast<int> (std::round (controls[0]->getValue() * 3.0f));
    const bool yin = controls[13]->getValue() >= 0.5f;
    if (model != lastModel || yin != lastYin)
    {
        history.clear();
        displayedReduction = 0.0f;
        lastModel = model;
        lastYin = yin;
        if (auto* ratio = dynamic_cast<juce::Slider*> (accessibleControls[3].get()))
        {
            ratio->updateText();
            if (auto* handler = ratio->getAccessibilityHandler())
                handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        }
    }
    if (waveformControl)
        waveformControl->setToggleState (processor.waveformDisplay.load (std::memory_order_relaxed), juce::dontSendNotification);
    const auto raw = processor.reduction.load (std::memory_order_relaxed);
    const auto reduction = std::isfinite (raw) ? std::clamp (raw, 0.0f, yin ? 12.0f : 18.0f) : 0.0f;
    const auto elapsed = lastTick > 0.0 ? std::max (0.0, now - lastTick) : 0.0;
    displayedReduction = std::max (reduction, displayedReduction * std::exp (-static_cast<float> (elapsed) / 0.25f));
    lastTick = now;
    history.push (now, reduction);
    view.redraw();
}

int YinyangVisageEditor::controlAt (float x, float y) const
{
    for (int i = static_cast<int> (yinyangVisage::controls.size()) - 1; i >= 0; --i)
    {
        const auto& control = yinyangVisage::controls[static_cast<size_t> (i)];
        if (x >= control.x && x < control.x + control.width
            && y >= control.y && y < control.y + control.height)
            return i;
    }
    return -1;
}

void YinyangVisageEditor::setControl (int index, float normalized)
{
    auto* parameter = controls[static_cast<size_t> (index)];
    const auto value = std::clamp (normalized, 0.0f, 1.0f);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (parameter->convertFrom0to1 (value)));
    view.redraw();
}

void YinyangVisageEditor::draw (visage::Canvas& canvas)
{
    const float scale = static_cast<float> (view.width()) / designWidth;
    drawControls (canvas, scale);
    drawModelMenu (canvas, scale);
    drawTooltip (canvas, scale);
}

void YinyangVisageEditor::drawControls (visage::Canvas& canvas, float scale)
{
    const auto yin = controls[13]->getValue() >= 0.5f;
    const auto model = std::clamp (static_cast<int> (std::round (controls[0]->getValue() * 3.0f)), 0, 3);
    const auto& theme = yinyangVisage::themeFor (model, yin);
    const visage::Font heading (24 * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font label (10 * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font small (9 * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font valueFont (12 * scale, visage::fonts::Lato_Regular_ttf);
    const auto panel = [&] (float x, float y, float w, float h, unsigned int colour)
    {
        canvas.setColor (0x50000000);
        canvas.roundedRectangle (x * scale, (y + 3) * scale, w * scale, h * scale, 10 * scale);
        canvas.setColor (colour);
        canvas.roundedRectangle (x * scale, y * scale, w * scale, h * scale, 10 * scale);
        canvas.setColor (theme.grid);
        canvas.roundedRectangleBorder (x * scale, y * scale, w * scale, h * scale, 10 * scale, scale);
    };
    const auto text = [&] (const std::string& caption, const visage::Font& font, unsigned int colour,
                           float x, float y, float w, float h, visage::Font::Justification justify = visage::Font::kCenter)
    {
        canvas.setColor (colour);
        canvas.text (caption, font, justify, x * scale, y * scale, w * scale, h * scale);
    };

    canvas.setColor (theme.background);
    canvas.fill (0, 0, view.width(), view.height());
    const auto titleAccent = juce::Colour (theme.background).getPerceivedBrightness() > 0.7f ? theme.text : theme.control;
    text ("J-SONIC  /  DYNAMICS", label, titleAccent, 24, 18, 340, 15, visage::Font::kLeft);
    text ("YINYANG COMPRESSOR", heading, theme.text, 24, 32, 420, 32, visage::Font::kLeft);
    panel (24, 110, 300, 342, theme.panel);
    panel (24, 464, 300, 116, theme.panel);
    panel (24, 592, 300, 116, theme.panel);
    panel (344, 110, 180, 470, theme.meterPanel);
    panel (344, 592, 180, 116, theme.panel);
    drawMeter (canvas, scale, theme, yin);
    if (focusedControl == static_cast<int> (controls.size()) && view.hasKeyboardFocus())
    {
        canvas.setColor (theme.control);
        canvas.roundedRectangleBorder (406 * scale, 142 * scale, 106 * scale, 340 * scale, 4 * scale, scale);
    }
    text ("SIDECHAIN", label, theme.text, 38, 472, 240, 18, visage::Font::kLeft);
    text ("TRANSFORMER STAGE", label, theme.text, 38, 600, 240, 18, visage::Font::kLeft);
    text ("OUTPUT", label, theme.text, 358, 600, 150, 18, visage::Font::kLeft);

    static constexpr std::array<const char*, 4> modelNames { "FET / Feed-forward", "VCA / Feedback", "Variable-mu", "Optical" };
    for (size_t i = 0; i < yinyangVisage::controls.size(); ++i)
    {
        const auto& c = yinyangVisage::controls[i];
        auto* parameter = controls[i];
        if (! parameter) continue;
        const float v = parameter->getValue();
        if (static_cast<int> (i) == focusedControl && view.hasKeyboardFocus())
        {
            canvas.setColor (theme.control);
            canvas.roundedRectangleBorder (c.x * scale, c.y * scale, c.width * scale, c.height * scale, 4 * scale, scale);
        }
        if (c.kind == yinyangVisage::ControlKind::model)
        {
            canvas.setColor (theme.panel);
            canvas.roundedRectangle (c.x * scale, c.y * scale, c.width * scale, c.height * scale, 5 * scale);
            canvas.setColor (theme.grid);
            canvas.roundedRectangleBorder (c.x * scale, c.y * scale, c.width * scale, c.height * scale, 5 * scale, scale);
            text (modelNames[static_cast<size_t> (model)], valueFont, theme.text, c.x + 10, c.y, c.width - 30, c.height, visage::Font::kLeft);
            text ("▾", valueFont, theme.secondary, c.x + c.width - 24, c.y, 20, c.height);
            continue;
        }
        if (c.kind == yinyangVisage::ControlKind::toggle)
        {
            if (i == 12 || i == 13)
            {
                canvas.image (yinyang::art::button_png, c.x * scale, c.y * scale,
                              c.width * scale, c.height * scale);
                if (v >= 0.5f)
                {
                    canvas.setColor (theme.control);
                    canvas.roundedRectangleBorder ((c.x + c.width * 0.1f) * scale, (c.y + c.height * 0.15f) * scale,
                        c.width * 0.8f * scale, c.height * 0.70f * scale, 5 * scale, 1.5f * scale);
                }
                const auto caption = i == 13 ? (yin ? "YIN" : "YANG") : "BYPASS";
                text (caption, small, 0xfff0f4f4, c.x, c.y, c.width, c.height);
            }
            else
            {
                const float glyph = c.height * 0.70f;
                const float centerX = c.x + c.width * 0.5f;
                const float centerY = c.y + c.height * 0.5f;
                const auto& art = v >= 0.5f ? yinyang::art::switch_on_png : yinyang::art::switch_off_png;
                canvas.image (art, (centerX - glyph) * scale, (centerY - glyph) * scale,
                              glyph * 2 * scale, glyph * 2 * scale);
                const auto caption = i == 15 ? "MID / SIDE" : c.caption;
                text (caption, small, theme.text, c.x - 4, c.y - 12, c.width + 8, 14);
            }
            continue;
        }
        const float diameter = std::min (c.width - 12.0f, c.height - 24.0f);
        const float cx = c.x + c.width * 0.5f;
        const float cy = c.y + diameter * 0.5f;
        const float radius = diameter * 0.5f;
        text (c.caption, small, theme.text, c.x - 4, c.y - 15, c.width + 8, 14);
        for (int tick = 0; tick <= 10; ++tick)
        {
            const float angle = -2.35f + tick * 4.70f / 10.0f;
            const float dx = std::sin (angle), dy = -std::cos (angle);
            canvas.setColor (theme.secondary);
            canvas.segment ((cx + dx * (radius + 1)) * scale, (cy + dy * (radius + 1)) * scale,
                            (cx + dx * (radius + 4)) * scale, (cy + dy * (radius + 4)) * scale,
                            std::max (1.0f, scale), false);
        }
        canvas.image (yinyang::art::knob_png, (cx - radius) * scale, (cy - radius) * scale,
                      diameter * scale, diameter * scale);
        const float angle = -2.35f + std::clamp (v, 0.0f, 1.0f) * 4.70f;
        const float markerX = cx + std::sin (angle) * radius * 0.64f;
        const float markerY = cy - std::cos (angle) * radius * 0.64f;
        const float markerSize = std::max (3.0f, diameter * 0.07f);
        canvas.setColor (0xff182026);
        canvas.circle ((markerX - markerSize * 0.5f) * scale, (markerY - markerSize * 0.5f) * scale, markerSize * scale);
        auto valueText = parameter->getCurrentValueAsText();
        if (i == 3 && yin)
            valueText = juce::String (character::hostToYinRatio (processor.parameters.getRawParameterValue ("ratio")->load()), 2);
        const auto unit = parameter->getLabel();
        if (unit.isNotEmpty()) valueText += (unit == ":1" ? "" : " ") + unit;
        text (valueText.toStdString(), valueFont, theme.text, c.x - 2, c.y + c.height - 20, c.width + 4, 18);
    }
}

void YinyangVisageEditor::drawMeter (visage::Canvas& canvas, float scale,
                                     const yinyangVisage::Theme& theme, bool yin)
{
    const bool waveform = processor.waveformDisplay.load (std::memory_order_relaxed);
    const visage::Font small (9 * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font label (11 * scale, visage::fonts::Lato_Regular_ttf);
    const visage::Font large (24 * scale, visage::fonts::Lato_Regular_ttf);
    constexpr float plotX = 410.0f, plotY = 172.0f, plotW = 98.0f, plotH = 310.0f;
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto meterY = [=] (float value)
    {
        return plotY + plotH * std::clamp (historyDisplay::positionForDb (yin ? value : -value, yin, waveform), 0.0f, 1.0f);
    };
    const auto drawText = [&] (const std::string& value, const visage::Font& font, unsigned int colour,
                               float x, float y, float w, float h, visage::Font::Justification justify)
    {
        canvas.setColor (colour);
        canvas.text (value, font, justify, x * scale, y * scale, w * scale, h * scale);
    };
    drawText (yin ? "YIN GAIN" : "YANG GAIN REDUCTION", label, 0xffb9c8c4, 356, 128, 156, 20, visage::Font::kCenter);
    canvas.setColor (theme.gaugeTrack);
    canvas.roundedRectangle (360 * scale, plotY * scale, 14 * scale, plotH * scale, 4 * scale);
    const int tickStart = yin ? (waveform ? -48 : -6) : 0;
    const int tickEnd = yin ? 12 : waveform ? 48 : 18;
    for (int db = tickStart; db <= tickEnd; db += 6)
    {
        const float y = meterY (static_cast<float> (db));
        canvas.setColor (theme.grid);
        canvas.segment (plotX * scale, y * scale, (plotX + plotW) * scale, y * scale, scale, false);
        const auto caption = yin && db > 0 ? "+" + std::to_string (db)
            : ! yin && waveform && db > 0 ? "-" + std::to_string (db) : std::to_string (db);
        drawText (caption, small, 0xffb9c8c4, 378, y - 7, 26, 14, visage::Font::kCenter);
    }
    for (int second = 0; second <= 3; ++second)
    {
        const float x = plotX + plotW * static_cast<float> (second) / 3.0f;
        canvas.setColor (theme.grid);
        canvas.segment (x * scale, plotY * scale, x * scale, (plotY + plotH) * scale, scale, false);
    }
    const float maximum = yin ? 12.0f : 18.0f;
    const float base = yin ? meterY (0.0f) : plotY + plotH;
    const float fill = std::clamp (displayedReduction / maximum, 0.0f, 1.0f) * (base - plotY);
    if (fill > 0)
    {
        canvas.setColor (theme.meterColor);
        canvas.roundedRectangle (360 * scale, (base - fill) * scale, 14 * scale, fill * scale, 3 * scale);
    }
    if (waveform)
    {
        drawText ("IN", small, 0xffb9c8c4, 410, 153, 25, 15, visage::Font::kLeft);
        drawText ("OUT", small, 0xffe7ac58, 440, 153, 32, 15, visage::Font::kCenter);
        std::array<WaveformHistory::Frame, 98> columns {};
        std::array<bool, 98> valid {};
        processor.waveform.visit (now, [&] (float position, const auto& frame)
        {
            const auto i = static_cast<size_t> (std::clamp (static_cast<int> (position * plotW), 0, 97));
            auto& column = columns[i];
            if (! valid[i]) { column = frame; valid[i] = true; }
            else
            {
                column.inputMin = std::min (column.inputMin, frame.inputMin);
                column.inputMax = std::max (column.inputMax, frame.inputMax);
                column.outputMin = std::min (column.outputMin, frame.outputMin);
                column.outputMax = std::max (column.outputMax, frame.outputMax);
            }
        });
        for (int trace = 0; trace < 2; ++trace)
        {
            canvas.setColor (trace == 0 ? 0x38b9c8c4 : 0x4de7ac58);
            for (size_t i = 0; i < columns.size(); ++i)
            {
                if (! valid[i]) continue;
                const auto& frame = columns[i];
                const auto position = historyDisplay::waveformPosition (trace == 0 ? frame.inputMin : frame.outputMin,
                                                                         trace == 0 ? frame.inputMax : frame.outputMax, yin);
                const auto y = plotY + position * plotH;
                if (y < plotY + plotH)
                    canvas.fill ((plotX + static_cast<float> (i)) * scale, y * scale, scale, (plotY + plotH - y) * scale);
            }
        }
    }
    bool started = false;
    float previousX = 0.0f, previousY = 0.0f;
    double previousTime = 0.0;
    canvas.setColor (theme.meterColor);
    history.visit (now, [&] (float position, float reduction, double time)
    {
        const float x = plotX + plotW * position;
        const float y = meterY (yin ? std::min (reduction, 12.0f) : reduction);
        if (started && time - previousTime <= 0.25)
            canvas.segment (previousX * scale, previousY * scale, x * scale, y * scale, 1.7f * scale, true);
        previousX = x;
        previousY = y;
        previousTime = time;
        started = true;
    });
    drawText ("-3 s", small, 0xffb9c8c4, 408, 486, 44, 18, visage::Font::kLeft);
    drawText ("NOW", small, 0xffb9c8c4, 466, 486, 44, 18, visage::Font::kRight);
    const auto value = (yin ? "+" : "") + juce::String (displayedReduction, 1).toStdString() + " dB";
    drawText (value, large, theme.meterColor, 356, 512, 156, 32, visage::Font::kCenter);
    drawText ("Click graph for waveform", small, 0xffb9c8c4, 356, 548, 156, 16, visage::Font::kCenter);
}

namespace
{
class AccessibleSlider final : public juce::Slider
{
public:
    void paint (juce::Graphics&) override {}
};
class AccessibleToggle final : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool, bool) override {}
};
class AccessibleChoice final : public juce::ComboBox
{
public:
    void paint (juce::Graphics&) override {}
};
}

void YinyangVisageEditor::createAccessibleControls()
{
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    for (size_t i = 0; i < yinyangVisage::controls.size(); ++i)
    {
        const auto& spec = yinyangVisage::controls[i];
        if (spec.kind == yinyangVisage::ControlKind::knob)
        {
            auto slider = std::make_unique<AccessibleSlider>();
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sliderAttachments[i] = std::make_unique<State::SliderAttachment> (processor.parameters, spec.id, *slider);
            slider->setTooltip (spec.hint);
            if (i == 3)
            {
                slider->textFromValueFunction = [this] (double value)
                {
                    const auto ratio = controls[13]->getValue() >= 0.5f
                        ? character::hostToYinRatio (static_cast<float> (value)) : static_cast<float> (value);
                    return juce::String (ratio, 2) + ":1";
                };
                slider->valueFromTextFunction = [this] (const juce::String& text)
                {
                    const auto value = text.getFloatValue();
                    return controls[13]->getValue() >= 0.5f ? character::yinToHostRatio (value) : value;
                };
            }
            accessibleControls[i] = std::move (slider);
        }
        else if (spec.kind == yinyangVisage::ControlKind::toggle)
        {
            auto button = std::make_unique<AccessibleToggle>();
            button->setButtonText (spec.caption);
            button->setTooltip (spec.hint);
            buttonAttachments[i] = std::make_unique<State::ButtonAttachment> (processor.parameters, spec.id, *button);
            accessibleControls[i] = std::move (button);
        }
        else
        {
            auto choice = std::make_unique<AccessibleChoice>();
            choice->addItemList ({ "FET / Feed-forward", "VCA / Feedback", "Variable-mu", "Optical" }, 1);
            choice->setTooltip (spec.hint);
            modelAttachment = std::make_unique<State::ComboBoxAttachment> (processor.parameters, spec.id, *choice);
            accessibleControls[i] = std::move (choice);
        }
        auto& control = *accessibleControls[i];
        control.setName (spec.caption);
        control.setTitle (spec.caption);
        control.setDescription (spec.hint);
        control.setHelpText (spec.hint);
        control.setAccessible (true);
        control.setWantsKeyboardFocus (true);
        control.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (control);
    }
    auto overlay = std::make_unique<AccessibleToggle>();
    overlay->setButtonText ("Waveform overlay");
    overlay->setName ("Waveform overlay");
    overlay->setTitle ("Waveform overlay");
    overlay->setDescription ("Show or hide the input and output waveform behind the gain history.");
    overlay->setTooltip ("Show or hide the input and output waveform behind the gain history.");
    overlay->setClickingTogglesState (true);
    overlay->setToggleState (processor.waveformDisplay.load(), juce::dontSendNotification);
    overlay->setAccessible (true);
    overlay->setWantsKeyboardFocus (true);
    overlay->setInterceptsMouseClicks (false, false);
    overlay->onClick = [this, target = overlay.get()]
    {
        processor.waveformDisplay.store (target->getToggleState(), std::memory_order_relaxed);
        view.redraw();
    };
    addAndMakeVisible (*overlay);
    waveformControl = std::move (overlay);
}

void YinyangVisageEditor::showModelMenu()
{
    modelMenuSelection = std::clamp (static_cast<int> (std::round (controls[0]->getValue() * 3.0f)), 0, 3);
    modelMenuOpen = true;
    focusedControl = 0;
    view.requestKeyboardFocus();
    view.redraw();
}

void YinyangVisageEditor::drawModelMenu (visage::Canvas& canvas, float scale)
{
    if (! modelMenuOpen) return;
    static constexpr std::array<const char*, 4> names { "FET / Feed-forward", "VCA / Feedback", "Variable-mu", "Optical" };
    const auto& theme = yinyangVisage::themeFor (
        std::clamp (static_cast<int> (std::round (controls[0]->getValue() * 3.0f)), 0, 3), controls[13]->getValue() >= 0.5f);
    canvas.setColor (theme.panel);
    canvas.roundedRectangle (24 * scale, 100 * scale, 300 * scale, 112 * scale, 5 * scale);
    canvas.setColor (theme.grid);
    canvas.roundedRectangleBorder (24 * scale, 100 * scale, 300 * scale, 112 * scale, 5 * scale, scale);
    const visage::Font font (12 * scale, visage::fonts::Lato_Regular_ttf);
    for (int i = 0; i < 4; ++i)
    {
        if (i == modelMenuSelection)
        {
            canvas.setColor (theme.control);
            canvas.roundedRectangle (27 * scale, (102 + i * 28) * scale, 294 * scale, 24 * scale, 4 * scale);
        }
        canvas.setColor (i == modelMenuSelection ? (juce::Colour (theme.control).getPerceivedBrightness() > 0.55f ? 0xff172028 : 0xfff6f8f9) : theme.text);
        canvas.text (names[static_cast<size_t> (i)], font, visage::Font::kLeft,
                     34 * scale, (100 + i * 28) * scale, 280 * scale, 28 * scale);
    }
}

void YinyangVisageEditor::layoutEditField()
{
    if (editingControl < 0 || valueEditor == nullptr) return;
    const auto& c = yinyangVisage::controls[static_cast<size_t> (editingControl)];
    const float scale = static_cast<float> (view.width()) / designWidth;
    valueEditor->setBounds (c.x * scale, (c.y + c.height - 23) * scale, c.width * scale, 23 * scale);
    valueEditor->setFont (visage::Font (12 * scale, visage::fonts::Lato_Regular_ttf));
}

void YinyangVisageEditor::beginEdit (int index)
{
    if (index < 0 || index >= static_cast<int> (controls.size())
        || yinyangVisage::controls[static_cast<size_t> (index)].kind != yinyangVisage::ControlKind::knob)
        return;
    if (editingControl >= 0) finishEdit (true);
    focusedControl = editingControl = index;
    const auto raw = controls[static_cast<size_t> (index)]->convertFrom0to1 (controls[static_cast<size_t> (index)]->getValue());
    const auto shown = index == 3 && controls[13]->getValue() >= 0.5f ? character::hostToYinRatio (raw) : raw;
    valueEditor->setText (juce::String (shown, 2).toStdString());
    layoutEditField();
    valueEditor->setVisible (true);
    valueEditor->requestKeyboardFocus();
    valueEditor->selectAll();
    view.redraw();
}

void YinyangVisageEditor::finishEdit (bool commit)
{
    const int index = editingControl;
    if (index < 0) return;
    if (commit)
    {
        const auto text = valueEditor->text().toUtf8();
        const char* begin = text.c_str();
        char* end = nullptr;
        errno = 0;
        const float entered = std::strtof (begin, &end);
        while (end && std::isspace (static_cast<unsigned char> (*end))) ++end;
        if (end != begin && end && *end == '\0' && errno != ERANGE && std::isfinite (entered))
        {
            auto* parameter = controls[static_cast<size_t> (index)];
            const auto value = index == 3 && controls[13]->getValue() >= 0.5f
                ? character::yinToHostRatio (std::clamp (entered, 1.0f, 2.0f)) : entered;
            parameter->beginChangeGesture();
            setControl (index, parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }
    editingControl = -1;
    valueEditor->setVisible (false);
    view.requestKeyboardFocus();
    view.redraw();
}

bool YinyangVisageEditor::handleKey (const visage::KeyEvent& event)
{
    using visage::KeyCode;
    const auto key = event.keyCode();
    if (modelMenuOpen)
    {
        if (key == KeyCode::Escape || key == KeyCode::Tab)
        {
            modelMenuOpen = false;
            view.redraw();
            return true;
        }
        if (key == KeyCode::Up || key == KeyCode::Down)
        {
            modelMenuSelection = (modelMenuSelection + (key == KeyCode::Up ? 3 : 1)) % 4;
            view.redraw();
            return true;
        }
        if (key == KeyCode::Return || key == KeyCode::KPEnter || key == KeyCode::Space)
        {
            controls[0]->beginChangeGesture();
            setControl (0, static_cast<float> (modelMenuSelection) / 3.0f);
            controls[0]->endChangeGesture();
            modelMenuOpen = false;
            view.redraw();
            return true;
        }
        return false;
    }
    if (key == KeyCode::Tab)
    {
        const int count = static_cast<int> (controls.size()) + 1;
        focusedControl = (focusedControl + (event.isShiftDown() ? count - 1 : 1)) % count;
        view.redraw();
        return true;
    }
    if (focusedControl == static_cast<int> (controls.size()))
    {
        if (key == KeyCode::Return || key == KeyCode::KPEnter || key == KeyCode::Space)
        {
            const bool enabled = processor.waveformDisplay.load (std::memory_order_relaxed);
            processor.waveformDisplay.store (! enabled, std::memory_order_relaxed);
            view.redraw();
            return true;
        }
        return false;
    }
    auto* parameter = controls[static_cast<size_t> (focusedControl)];
    const auto kind = yinyangVisage::controls[static_cast<size_t> (focusedControl)].kind;
    if (key == KeyCode::Return || key == KeyCode::KPEnter)
    {
        if (kind == yinyangVisage::ControlKind::knob) beginEdit (focusedControl);
        else if (kind == yinyangVisage::ControlKind::model) showModelMenu();
        else
        {
            parameter->beginChangeGesture();
            setControl (focusedControl, parameter->getValue() < 0.5f ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
        return true;
    }
    if (key == KeyCode::Space && kind == yinyangVisage::ControlKind::toggle)
    {
        parameter->beginChangeGesture();
        setControl (focusedControl, parameter->getValue() < 0.5f ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        return true;
    }
    if (key == KeyCode::Left || key == KeyCode::Down || key == KeyCode::Right || key == KeyCode::Up)
    {
        const float direction = key == KeyCode::Left || key == KeyCode::Down ? -1.0f : 1.0f;
        const float amount = kind == yinyangVisage::ControlKind::model ? 1.0f / 3.0f
            : kind == yinyangVisage::ControlKind::toggle ? 1.0f : event.isShiftDown() ? 0.001f : 0.01f;
        parameter->beginChangeGesture();
        setControl (focusedControl, parameter->getValue() + direction * amount);
        parameter->endChangeGesture();
        return true;
    }
    return false;
}

void YinyangVisageEditor::drawTooltip (visage::Canvas& canvas, float scale)
{
    if (hoveredControl == -1 || dragging >= 0 || editingControl >= 0 || modelMenuOpen
        || juce::Time::getMillisecondCounterHiRes() * 0.001 - hoverStarted < 0.6)
        return;
    const juce::String hint = hoveredControl == -2
        ? "Click to show or hide the input and output waveform behind the gain history."
        : hoveredControl == -3 ? "Developer: J-Sonic Team. Year: 2026. Version: " + juce::String (JucePlugin_VersionString)
        : yinyangVisage::controls[static_cast<size_t> (hoveredControl)].hint;
    juce::StringArray words;
    words.addTokens (hint, " ", "");
    std::array<juce::String, 3> lines;
    int line = 0;
    for (const auto& word : words)
    {
        if (lines[static_cast<size_t> (line)].length() + word.length() > 42 && line < 2) ++line;
        auto& target = lines[static_cast<size_t> (line)];
        if (target.isNotEmpty()) target += " ";
        target += word;
    }
    const float width = 260.0f, height = 12.0f + (line + 1) * 15.0f;
    const float x = std::clamp (hoverX + 12.0f, 4.0f, designWidth - width - 4.0f);
    const float y = hoverY + height + 18.0f < designHeight ? hoverY + 16.0f : hoverY - height - 8.0f;
    canvas.setColor (0xf20d1820);
    canvas.roundedRectangle (x * scale, y * scale, width * scale, height * scale, 5 * scale);
    canvas.setColor (0xffb5c7ce);
    canvas.roundedRectangleBorder (x * scale, y * scale, width * scale, height * scale, 5 * scale, scale);
    const visage::Font font (10.5f * scale, visage::fonts::Lato_Regular_ttf);
    canvas.setColor (0xfff1f4f4);
    for (int i = 0; i <= line; ++i)
        canvas.text (lines[static_cast<size_t> (i)].toStdString(), font, visage::Font::kLeft,
                     (x + 7) * scale, (y + 6 + i * 15) * scale, (width - 14) * scale, 14 * scale);
}
