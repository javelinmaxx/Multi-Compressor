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
    "Adjusts feedback mixing density and glides each tap's level by up to plus or minus three decibels every half Time.",
    "Sets the stereo spread of the wet signal with a normalized mid and side balance.",
    "Moves the feedback read taps through a 250-millisecond crossfade to scatter existing ringing frequencies.",
    "Sets feedback phase dispersion and correction across sixteen resonance bands within a five-millisecond delay budget.",
    "Sets narrow feedback attenuation across up to sixteen resonance bands within a shared six-decibel limit while protecting incoming notes.",
    "Adds a softly faded octave-up layer to the feedback-loop injection using a twice-oversampled grain shifter; Saves CPU when Off.",
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
    processor.setSpectrumMonitor(true);
    editorScaling::enable(*this,designWidth,designHeight); gpu.attach(*this); startTimerHz(20);
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
    if (!isShowing()) { spectrumDisplay.reset(); lastSpectrumTick=0; return; }
    const auto now=juce::Time::getMillisecondCounterHiRes()*.001;
    const auto elapsed=lastSpectrumTick>0 ? float(now-lastSpectrumTick) : .05f;
    lastSpectrumTick=now;
    lan::Analysis next;
    if (processor.readAnalysis(next)) analysis = next;
    // A worker publication can briefly make a nonblocking read fail. Keep
    // the last valid frame instead of flashing an empty plot on that tick.
    spectrumDisplay.update(analysis,elapsed);
    tapDisplay=processor.readTapDisplay();
    reduction = processor.reductionDb(); outputPeak = processor.peakLevel();
    repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{24,92,692,180}));
    repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{24,538,692,250}));
}
void LanReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll(background); g.saveState(); g.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    g.setGradientFill(juce::ColourGradient(background.brighter(.035f),0,0,background.darker(.13f),0,designHeight,false)); g.fillRect(0,0,designWidth,designHeight);
    g.setColour(secondary); g.setFont(juce::FontOptions(10.f)); g.drawText("J-SONIC / ADAPTIVE REVERB + DELAY",24,12,470,16,juce::Justification::centredLeft);
    g.setColour(foreground); g.setFont(juce::FontOptions(25.f)); g.drawText("LAN REVERB",24,30,420,36,juce::Justification::centredLeft);
    const auto magentaPanel=juce::Colour(0xff38263f);
    drawPanel(g,{24,88,692,190},magentaPanel); drawPanel(g,{24,292,692,232},magentaPanel);
    drawPanel(g,{24,538,692,250},magentaPanel);
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
    if (analysis.revision > 0 && spectrumDisplay.hasFrame())
    {
        for (int band = 0; active && band < analysis.count; ++band)
        {
            const auto& b = analysis.bands[size_t(band)];
            const auto x = plot.getX()+plot.getWidth()*std::log(std::max(lan::shelfMinHz,b.frequency)/lan::shelfMinHz)/std::log(ceiling/lan::shelfMinHz);
            g.setColour(amber.withAlpha(.13f)); g.fillRect(juce::Rectangle<float>(x-4,plot.getY(),8,plot.getHeight()));
        }
        for (int which = 0; which < 2; ++which)
        {
            juce::Path path;
            const auto& values=spectrumDisplay.trace(which);
            std::array<float,lan::spectrumBins> heights {};
            for(size_t i=0;i<values.size();++i)
                heights[i]=plot.getBottom()-plot.getHeight()*std::clamp((values[i]+100)/100,0.f,1.f);
            path.startNewSubPath(plot.getX(),heights[0]);
            const auto step=plot.getWidth()/float(lan::spectrumBins-1);
            for(size_t i=0;i+1<values.size();++i)
            {
                const auto first=heights[i], second=heights[i+1];
                const auto slopeFirst=i==0 ? second-first : (second-heights[i-1])*.5f;
                const auto slopeSecond=i+2<values.size() ? (heights[i+2]-first)*.5f : second-first;
                const auto low=std::min(first,second), high=std::max(first,second);
                const auto x=plot.getX()+step*float(i);
                path.cubicTo(x+step/3.f,std::clamp(first+slopeFirst/3.f,low,high),
                             x+2.f*step/3.f,std::clamp(second-slopeSecond/3.f,low,high),
                             x+step,second);
            }
            g.setColour(which == 0 ? secondary.withAlpha(.65f) : amber); g.strokePath(path,juce::PathStrokeType(which == 0 ? 1.f : 1.5f));
        }
    }
    else { g.setColour(secondary); g.drawText("Listening to the feedback tail",plot,juce::Justification::centred); }
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
    g.setColour(foreground); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText("TAP DISTRIBUTION",40,550,230,18,juce::Justification::centredLeft);
    g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("FIRST 3 SECONDS  /  NOMINAL REPEAT LEVEL",384,552,314,14,juce::Justification::centredRight);
    constexpr float axisLeft=100.f, axisWidth=464.f, rowStart=613.f, rowStep=18.f;
    const auto timeX=[] (float seconds) { return axisLeft+axisWidth*seconds/3.f; };
    const auto cyan=juce::Colour(0xff86c8de);
    g.setColour(inset); g.fillRoundedRectangle(juce::Rectangle<float>(87,576,612,184),4.f);
    for (int second=0;second<=3;++second)
    {
        const auto x=timeX(float(second));
        g.setColour(grid); g.drawVerticalLine(juce::roundToInt(x),600.f,753.f);
        g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
        g.drawText(juce::String(second)+" s",juce::roundToInt(x)-18,582,36,14,juce::Justification::centred);
    }
    g.setColour(secondary); g.drawText("TAP",43,582,38,14,juce::Justification::centredLeft);
    g.drawText("LOOP GAIN",589,582,98,14,juce::Justification::centredRight);
    if (tapDisplay.valid)
    {
        for (size_t i=0;i<lan::tapRatios.size();++i)
        {
            const auto y=rowStart+rowStep*float(i);
            const auto from=tapDisplay.feedbackFromSeconds[i], to=tapDisplay.feedbackToSeconds[i];
            const auto fade=std::clamp(tapDisplay.fade[i],0.f,1.f);
            const auto eased=fade*fade*(3.f-2.f*fade);
            const auto current=from+eased*(to-from);
            const auto wet=tapDisplay.wetSeconds[i];
            const auto gainDb=tapDisplay.feedbackGainDb[i];
            g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
            g.drawText("T"+juce::String(int(i+1)),43,juce::roundToInt(y)-7,38,14,juce::Justification::centredLeft);
            g.setColour(grid.withAlpha(.7f)); g.drawHorizontalLine(juce::roundToInt(y),axisLeft,axisLeft+axisWidth);
            // Repeat stems use the current loop-gain estimate. Mixing, EQ and
            // the limiter can change actual levels after each recirculation.
            if (wet>=0 && wet<=3.f && current>.001f)
                for (int repeat=0;repeat<96;++repeat)
                {
                    const auto seconds=wet+float(repeat)*current;
                    if (seconds>3.f) break;
                    const auto db=tapDisplay.injectionGainDb[i]+float(repeat)*gainDb;
                    const auto level=std::clamp((db+60.f)/60.f,0.f,1.f);
                    if (level<=0) continue;
                    const auto x=timeX(seconds);
                    g.setColour(amber.withAlpha(repeat==0 ? .7f : .2f+.5f*level));
                    g.drawLine(x,y+3.f,x,y+3.f-10.f*level,repeat==0 ? 1.5f : 1.f);
                }
            if (from>=0 && from<=3.f && to>=0 && to<=3.f && std::abs(to-from)>.0002f)
            {
                g.setColour(amber.withAlpha(.45f)); g.drawLine(timeX(from),y-6.f,timeX(to),y-6.f,2.f);
                g.fillEllipse(timeX(to)-2.f,y-8.f,4.f,4.f);
            }
            const auto target=tapDisplay.feedbackTargetSeconds[i];
            if (target>=0 && target<=3.f && std::abs(target-to)>.0002f)
            { g.setColour(amber.withAlpha(.3f)); g.drawEllipse(timeX(target)-2.5f,y-8.5f,5.f,5.f,1.f); }
            if (current>=0 && current<=3.f)
            { g.setColour(amber); g.fillEllipse(timeX(current)-3.f,y-9.f,6.f,6.f); }
            if (wet>=0 && wet<=3.f)
            { g.setColour(cyan); g.fillEllipse(timeX(wet)-2.5f,y-2.5f,5.f,5.f); }
            g.setColour(foreground); g.setFont(juce::FontOptions(9.f));
            g.drawText(juce::String(gainDb>=0 ? "+" : "")+juce::String(gainDb,1)+" dB",588,juce::roundToInt(y)-7,98,14,juce::Justification::centredRight);
        }
    }
    else { g.setColour(secondary); g.drawText("Waiting for audio",100,650,464,18,juce::Justification::centred); }
    g.setColour(cyan); g.fillEllipse(44.f,770.f,5.f,5.f);
    g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("WET PICKUP",54,765,89,15,juce::Justification::centredLeft);
    g.setColour(amber); g.fillEllipse(152.f,768.f,6.f,6.f);
    g.setColour(secondary); g.drawText("MOVING FEEDBACK TAP",163,765,168,15,juce::Justification::centredLeft);
    g.drawText("STEM HEIGHT: -60 TO 0 dB",470,765,216,15,juce::Justification::centredRight);
    g.restoreState();
}
