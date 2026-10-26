#include "PluginEditor.h"
#include "../../EditorScaling.h"
using namespace stageStyle;
namespace
{
constexpr std::array<const char*,12> ids {"time","decay","predelay","scatter","diffusion","width","tap","phase","tame","shimmer","mix","output"};
constexpr std::array<const char*,12> names {"TIME","DECAY","PRE-DELAY","SCATTER","DIFFUSION","WIDTH","TAP SHIFT","PHASE","TAME","SHIMMER","DRY / WET","OUTPUT"};
constexpr std::array<const char*,12> units {" ms"," s"," ms"," %"," %"," %"," %"," %"," %"," %"," %"," dB"};
constexpr std::array<const char*,12> hints {
    "Sets the repeat time or room size in milliseconds, or selects a musical note length when Sync is on.",
    "Sets the nominal feedback decay time before filtering, while shelf boost can sustain an indefinite tail.",
    "Delays the signal entering the reverb while leaving the dry signal immediate.",
    "Morphs aligned delay repeats at zero into a dense stereo reverb at full.",
    "Adjusts the mixing density of the feedback network as Scatter increases.",
    "Sets the stereo spread of the wet signal with a normalized mid and side balance.",
    "Moves the feedback read taps through a 250-millisecond crossfade to scatter existing ringing frequencies.",
    "Sets feedback phase dispersion and correction across sixteen resonance bands within a five-millisecond delay budget.",
    "Sets narrow feedback attenuation across up to sixteen resonance bands within a shared six-decibel limit while protecting incoming notes.",
    "Adds an octave-up input layer to the feedback-loop injection using a twice-oversampled pitch shifter; Saves CPU when Off.",
    "Blends the immediate dry signal with the reverb and reaches a fully wet output at one hundred percent.",
    "Adjusts the final output level after the dry and wet signals are mixed." };
}
LanReverbEditor::LanReverbEditor (LanReverbProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setOpaque(true); setLookAndFeel(&look);
    look.setColour(juce::TooltipWindow::backgroundColourId,panel);
    look.setColour(juce::TooltipWindow::textColourId,foreground); look.setColour(juce::TooltipWindow::outlineColourId,grid);
    addAndMakeVisible(titleHint); addAndMakeVisible(bypass); addAndMakeVisible(automatic); addAndMakeVisible(sync);
    sync.setClickingTogglesState(true);
    sync.setColour(juce::TextButton::buttonColourId,inset); sync.setColour(juce::TextButton::buttonOnColourId,amber.darker(.6f));
    sync.setTooltip("Synchronizes Time to the DAW tempo using straight, dotted or triplet note lengths.");
    syncAttachment = std::make_unique<State::ButtonAttachment>(p.parameters,"sync",sync);
    sync.onClick = [this] { updateTimeMode(); };
    automatic.setClickingTogglesState(true);
    automatic.setColour(juce::TextButton::buttonColourId,inset); automatic.setColour(juce::TextButton::buttonOnColourId,amber.darker(.6f));
    automatic.setTooltip("Compares spectral peaks and frequency decay rates to guide input-protected feedback correction; Saves CPU when Off.");
    bypass.setTooltip("Bypasses the output while keeping the reverb and resonance analysis running for a smooth return.");
    autoAttachment = std::make_unique<State::ButtonAttachment>(p.parameters,"automatic",automatic);
    bypassAttachment = std::make_unique<State::ButtonAttachment>(p.parameters,"bypass",bypass);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i]; slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,18);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,foreground);
        slider.setTextValueSuffix(units[i]); slider.setName(names[i]); slider.setTitle(names[i]);
        auto* param = p.parameters.getParameter(ids[i]);
        slider.setDoubleClickReturnValue(true,param->convertFrom0to1(param->getDefaultValue()));
        labels[i].setText(names[i],juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setFont(juce::FontOptions(10.f)); labels[i].setColour(juce::Label::textColourId,secondary);
        addAndMakeVisible(slider); addAndMakeVisible(labels[i]);
        attachments[i] = std::make_unique<State::SliderAttachment>(p.parameters,ids[i],slider);
        editorTooltips::setHint(slider,labels[i],hints[i]);
    }
    for (int i = 0; i < 2; ++i)
    {
        auto& handle = shelfHandles[size_t(i)]; addAndMakeVisible(handle);
        const auto frequencyId = i == 0 ? "lowShelfHz" : "highShelfHz";
        const auto gainId = i == 0 ? "lowShelfDb" : "highShelfDb";
        handle.begin = [this,frequencyId,gainId]
        { processor.parameters.getParameter(frequencyId)->beginChangeGesture(); processor.parameters.getParameter(gainId)->beginChangeGesture(); };
        handle.end = [this,frequencyId,gainId]
        { processor.parameters.getParameter(frequencyId)->endChangeGesture(); processor.parameters.getParameter(gainId)->endChangeGesture(); };
        handle.drag = [this,i] (const juce::MouseEvent& e)
        { moveShelf(i,e.getEventRelativeTo(this).position.transformedBy(editorScaling::transform(*this,designWidth,designHeight).inverted())); };
        handle.reset = [this,frequencyId,gainId]
        {
            for (const auto* id : {frequencyId,gainId})
            { auto* parameter = processor.parameters.getParameter(id); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->getDefaultValue()); parameter->endChangeGesture(); }
            updateShelves(); repaint();
        };
    }
    sliders[9].textFromValueFunction = [] (double value) { return value < .05 ? juce::String("OFF") : juce::String(value,1)+" %"; };
    sliders[9].setTextValueSuffix("");
    updateTimeMode();
    editorScaling::enable(*this,designWidth,designHeight); startTimerHz(20);
}
void LanReverbEditor::updateTimeMode()
{
    const auto enabled = processor.parameters.getRawParameterValue("sync")->load() >= .5f;
    if (enabled == timeSynced) return;
    // Rebind only on the message thread; the free time and note choice each
    // retain their own host-automatable value when switching modes.
    attachments[0].reset(); timeSynced = enabled;
    sliders[0].setTextValueSuffix(enabled ? "" : " ms");
    attachments[0] = std::make_unique<State::SliderAttachment>(processor.parameters,enabled ? "note" : "time",sliders[0]);
}
void LanReverbEditor::moveShelf (int which, juce::Point<float> position)
{
    const auto rate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.;
    const auto ceiling = lan::shelfFrequencyCeiling(rate);
    const auto frequency = lan::shelfMinHz*std::pow(ceiling/lan::shelfMinHz,std::clamp((position.x-42)/656.f,0.f,1.f));
    const auto gain = std::clamp((160-position.y)/3.5f,-12.f,lan::shelfMaxBoostDb);
    auto* frequencyParameter = processor.parameters.getParameter(which == 0 ? "lowShelfHz" : "highShelfHz");
    auto* gainParameter = processor.parameters.getParameter(which == 0 ? "lowShelfDb" : "highShelfDb");
    frequencyParameter->setValueNotifyingHost(frequencyParameter->convertTo0to1(float(frequency)));
    gainParameter->setValueNotifyingHost(gainParameter->convertTo0to1(gain));
    updateShelves(); repaint();
}
void LanReverbEditor::updateShelves()
{
    const auto s = processor.settings(); const auto rate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.;
    const auto ceiling = lan::shelfFrequencyCeiling(rate);
    for (int i = 0; i < 2; ++i)
    {
        const auto hz = std::clamp(double(i == 0 ? s.lowShelfHz : s.highShelfHz),double(lan::shelfMinHz),ceiling);
        const auto gain = i == 0 ? s.lowShelfDb : s.highShelfDb;
        const auto x = 42+656*std::log(hz/lan::shelfMinHz)/std::log(ceiling/lan::shelfMinHz); const auto y = 160-gain*3.5f;
        shelfHandles[size_t(i)].setBounds(juce::roundToInt(x)-8,juce::roundToInt(y)-8,16,16);
    }
}
void LanReverbEditor::resized()
{
    titleHint.setBounds(24,30,420,36); automatic.setBounds(526,36,78,26); bypass.setBounds(620,36,96,26);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const auto top = i < 6;
        const auto x = 45+int(top ? i : i-6)*110;
        const auto y = top ? 310 : 422, controlWidth = 100;
        labels[i].setBounds(x,y,controlWidth,16); sliders[i].setBounds(x,y+20,controlWidth,76);
    }
    updateShelves();
    labels[0].setBounds(45,310,46,16); sync.setBounds(98,307,47,20);
    const auto transform = editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(transform); automatic.setTransform(transform); bypass.setTransform(transform); sync.setTransform(transform);
    for (auto& slider : sliders) slider.setTransform(transform);
    for (auto& label : labels) label.setTransform(transform);
    for (auto& handle : shelfHandles) handle.setTransform(transform);
}
void LanReverbEditor::timerCallback()
{
    updateTimeMode(); updateShelves();
    if (!isShowing()) return;
    if (!processor.readAnalysis(analysis)) analysis = {};
    reduction = processor.reductionDb(); outputPeak = processor.peakLevel();
    repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{24,92,692,180}));
}
void LanReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll(background); g.saveState(); g.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    g.setGradientFill(juce::ColourGradient(background.brighter(.035f),0,0,background.darker(.13f),0,designHeight,false)); g.fillRect(0,0,designWidth,designHeight);
    g.setColour(secondary); g.setFont(juce::FontOptions(10.f)); g.drawText("J-SONIC / ADAPTIVE REVERB + DELAY",24,12,470,16,juce::Justification::centredLeft);
    g.setColour(foreground); g.setFont(juce::FontOptions(25.f)); g.drawText("LAN REVERB",24,30,420,36,juce::Justification::centredLeft);
    drawPanel(g,{24,88,692,190}); drawPanel(g,{24,292,692,232});
    g.setColour(foreground); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText("FEEDBACK FIELD",40,100,230,20,juce::Justification::centredLeft);
    const auto settings = processor.settings(); const auto active = settings.automatic;
    g.setColour(amber); g.setFont(juce::FontOptions(10.f));
    const auto status = active ? "TAMING "+juce::String(reduction,1)+" dB" : juce::String("AUTO OFF");
    g.drawText(status,478,102,150,16,juce::Justification::centredRight);
    const juce::Rectangle<float> plot(42,132,656,112);
    g.setColour(inset); g.fillRoundedRectangle(plot,5);
    const auto rate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.;
    const auto ceiling = float(lan::shelfFrequencyCeiling(rate));
    for (float hz : {100.f,1000.f,10000.f})
    {
        if (hz > ceiling) continue;
        const auto x = plot.getX()+plot.getWidth()*std::log(hz/lan::shelfMinHz)/std::log(ceiling/lan::shelfMinHz);
        g.setColour(grid); g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());
        g.setColour(secondary); g.drawText(hz < 1000 ? "100 Hz" : hz < 10000 ? "1 kHz" : "10 kHz",int(x)-24,246,48,16,juce::Justification::centred);
    }
    for (int line = 1; line < 4; ++line) { g.setColour(grid); g.drawHorizontalLine(int(plot.getY()+line*plot.getHeight()/4),plot.getX(),plot.getRight()); }
    if (active && analysis.revision > 0)
    {
        for (int band = 0; band < analysis.count; ++band)
        {
            const auto& b = analysis.bands[size_t(band)];
            const auto x = plot.getX()+plot.getWidth()*std::log(std::max(lan::shelfMinHz,b.frequency)/lan::shelfMinHz)/std::log(ceiling/lan::shelfMinHz);
            g.setColour(amber.withAlpha(.13f)); g.fillRect(juce::Rectangle<float>(x-4,plot.getY(),8,plot.getHeight()));
        }
        for (int which = 0; which < 2; ++which)
        {
            juce::Path path;
            const auto& values = which == 0 ? analysis.input : analysis.feedback;
            for (size_t i = 0; i < values.size(); ++i)
            {
                const auto x = plot.getX()+plot.getWidth()*float(i)/(lan::spectrumBins-1);
                const auto y = plot.getBottom()-plot.getHeight()*std::clamp((values[i]+100)/100,0.f,1.f);
                if (i == 0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            g.setColour(which == 0 ? secondary.withAlpha(.65f) : amber); g.strokePath(path,juce::PathStrokeType(which == 0 ? 1.f : 1.5f));
        }
    }
    else { g.setColour(secondary); g.drawText(active ? "Listening to the feedback tail" : "Resonance analysis is off",plot,juce::Justification::centred); }
    // The target curve uses the same coefficients and gain budget as the
    // feedback banks; the live bank follows it through its 250 ms crossfade.
    juce::Path eq;
    for (int i = 0; i < 768; ++i)
    {
        const auto hz = lan::shelfMinHz*std::pow(ceiling/lan::shelfMinHz,i/767.);
        const auto db = lan::ReverbEngine::responseDb(settings,analysis,hz,rate);
        const auto x = plot.getX()+plot.getWidth()*i/767.f;
        const auto y = std::clamp(160-db*3.5f,plot.getY(),plot.getBottom());
        if (i == 0) eq.startNewSubPath(x,y); else eq.lineTo(x,y);
    }
    g.setColour(foreground.withAlpha(.8f)); g.strokePath(eq,juce::PathStrokeType(1.2f));
    g.setColour(secondary); g.setFont(juce::FontOptions(8.f));
    g.drawText("TARGET EQ / 0 dB",48,149,110,10,juce::Justification::centredLeft);
    const auto shelfText = [] (const char* name, float hz, float db)
    { return juce::String(name)+" "+(hz >= 1000 ? juce::String(hz/1000,1)+"k" : juce::String(hz,0))+" Hz / "+juce::String(db,1)+" dB"; };
    g.drawText(shelfText("LO",settings.lowShelfHz,settings.lowShelfDb),42,119,230,11,juce::Justification::centredLeft);
    g.drawText(shelfText("HI",settings.highShelfHz,settings.highShelfDb),468,119,230,11,juce::Justification::centredRight);
    g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("INPUT / GOLD: FEEDBACK",40,258,230,16,juce::Justification::centredLeft);
    g.drawText("OUT "+juce::String(juce::Decibels::gainToDecibels(outputPeak,-100.f),1)+" dBFS",500,258,198,16,juce::Justification::centredRight);
    g.restoreState();
}
