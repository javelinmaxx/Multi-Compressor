#include "PluginEditor.h"
#include "../../EditorScaling.h"
using namespace stageStyle;
namespace
{
constexpr std::array<const char*,10> ids {"time","decay","predelay","diffusion","width","tap","phase","tame","mix","output"};
constexpr std::array<const char*,10> names {"TIME","DECAY","PRE-DELAY","DIFFUSION","WIDTH","TAP SHIFT","PHASE","TAME","DRY / WET","OUTPUT"};
constexpr std::array<const char*,10> units {" ms"," s"," ms"," %"," %"," %"," %"," %"," %"," dB"};
constexpr std::array<const char*,10> hints {
    "Sets the spacing of the eight feedback delays for compact echoes or a larger reverb space.",
    "Sets the nominal time for the feedback tail to fall by sixty decibels before damping and resonance control.",
    "Delays the signal entering the reverb while leaving the dry signal immediate.",
    "Blends discrete delay recirculation into a dense orthogonal feedback network.",
    "Sets the stereo spread of the wet signal with a normalized mid and side balance.",
    "Moves the feedback read taps through a smooth crossfade to scatter existing ringing frequencies.",
    "Sets phase dispersion and the strength of automatic phase and tap scattering around persistent peaks.",
    "Sets the depth of automatic attenuation for persistent feedback peaks that stand out beyond the input spectrum.",
    "Blends the immediate dry signal with the reverb and reaches a fully wet output at one hundred percent.",
    "Adjusts the final output level after the dry and wet signals are mixed." };
}
LanReverbEditor::LanReverbEditor (LanReverbProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setOpaque(true); setLookAndFeel(&look);
    look.setColour(juce::TooltipWindow::backgroundColourId,panel);
    look.setColour(juce::TooltipWindow::textColourId,foreground); look.setColour(juce::TooltipWindow::outlineColourId,grid);
    addAndMakeVisible(titleHint); addAndMakeVisible(bypass); addAndMakeVisible(automatic);
    automatic.setClickingTogglesState(true);
    automatic.setColour(juce::TextButton::buttonColourId,inset); automatic.setColour(juce::TextButton::buttonOnColourId,amber.darker(.6f));
    automatic.setTooltip("Uses spectral feedback analysis to guide resonance EQ and phase scattering; Saves CPU when Off.");
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
    editorScaling::enable(*this,designWidth,designHeight); startTimerHz(20);
}
void LanReverbEditor::resized()
{
    titleHint.setBounds(24,30,420,36); automatic.setBounds(526,36,78,26); bypass.setBounds(620,36,96,26);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const auto x = 42+int(i%5)*134, y = i < 5 ? 310 : 422;
        labels[i].setBounds(x,y,120,16); sliders[i].setBounds(x,y+20,120,76);
    }
    const auto transform = editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(transform); automatic.setTransform(transform); bypass.setTransform(transform);
    for (auto& slider : sliders) slider.setTransform(transform);
    for (auto& label : labels) label.setTransform(transform);
}
void LanReverbEditor::timerCallback()
{
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
    const auto active = processor.settings().automatic;
    g.setColour(amber); g.setFont(juce::FontOptions(10.f));
    const auto status = active ? "TAMING "+juce::String(reduction,1)+" dB" : juce::String("AUTO OFF");
    g.drawText(status,478,102,150,16,juce::Justification::centredRight);
    const juce::Rectangle<float> plot(42,132,656,112);
    g.setColour(inset); g.fillRoundedRectangle(plot,5);
    const auto rate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.;
    const auto ceiling = float(std::min(18000.,rate*.43));
    for (float hz : {100.f,1000.f,10000.f})
    {
        if (hz > ceiling) continue;
        const auto x = plot.getX()+plot.getWidth()*std::log(hz/30.f)/std::log(ceiling/30.f);
        g.setColour(grid); g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());
        g.setColour(secondary); g.drawText(hz < 1000 ? "100 Hz" : hz < 10000 ? "1 kHz" : "10 kHz",int(x)-24,246,48,16,juce::Justification::centred);
    }
    for (int line = 1; line < 4; ++line) { g.setColour(grid); g.drawHorizontalLine(int(plot.getY()+line*plot.getHeight()/4),plot.getX(),plot.getRight()); }
    if (active && analysis.revision > 0)
    {
        for (int band = 0; band < analysis.count; ++band)
        {
            const auto& b = analysis.bands[size_t(band)];
            const auto x = plot.getX()+plot.getWidth()*std::log(std::max(30.f,b.frequency)/30.f)/std::log(ceiling/30.f);
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
    g.setColour(secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("INPUT / GOLD: FEEDBACK",40,258,230,16,juce::Justification::centredLeft);
    g.drawText("OUT "+juce::String(juce::Decibels::gainToDecibels(outputPeak,-100.f),1)+" dBFS",500,258,198,16,juce::Justification::centredRight);
    g.restoreState();
}
