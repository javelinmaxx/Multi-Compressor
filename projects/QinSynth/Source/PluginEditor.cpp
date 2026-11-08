#include "PluginEditor.h"
#include "../../EditorScaling.h"

namespace
{
const auto accent=juce::Colour(0xffc6b47b);
constexpr std::array<const char*,23> ids {
    "osc1Level","osc1Octave","osc1Unison","osc1Detune",
    "osc2Level","osc2Octave","osc2Unison","osc2Detune",
    "osc3Level","osc3Octave","osc3Unison","osc3Detune",
    "attack","release","keyPan","drive","output",
    "time","decay","scatter","verbMix","tapMotion","phaseMotion" };
constexpr std::array<const char*,23> names {
    "LEVEL","OCTAVE","UNISON","DETUNE",
    "LEVEL","OCTAVE","UNISON","DETUNE",
    "LEVEL","OCTAVE","UNISON","DETUNE",
    "ATTACK","RELEASE","KEY PAN","ZAO DRIVE","OUTPUT",
    "TIME","DECAY","SCATTER","MIX","TAP MOTION","PHASE MOTION" };
constexpr std::array<const char*,23> hints {
    "Sets the first oscillator's level.","Moves the first oscillator by whole octaves.",
    "Sets up to eight unison copies for the first oscillator.","Spreads the first oscillator's unison copies in cents.",
    "Sets the second oscillator's level.","Moves the second oscillator by whole octaves.",
    "Sets up to eight unison copies for the second oscillator.","Spreads the second oscillator's unison copies in cents.",
    "Sets the third oscillator's level.","Moves the third oscillator by whole octaves.",
    "Sets up to eight unison copies for the third oscillator.","Spreads the third oscillator's unison copies in cents.",
    "Sets the time for a new note to reach full level.","Sets how long a note fades after release.",
    "Places lower MIDI notes left and higher notes right, like a piano recording.",
    "Blends in Zao's op-amp feedback clipping circuit.","Adjusts the instrument's output gain.",
    "Sets the base delay time of Lan's feedback network.","Sets how long Lan's delay-reverb tail decays.",
    "Moves Lan's feedback from delay toward diffuse reverb.","Blends the delay-reverb with the synth signal.",
    "Sets how far each MIDI note-on shifts Lan's feedback taps.",
    "Sets how strongly each MIDI note-on changes Lan's phase scatter." };
constexpr std::array<const char*,3> waveHints {
    "Chooses the first oscillator's band-limited virtual-analog waveform.",
    "Chooses the second oscillator's band-limited virtual-analog waveform.",
    "Chooses the third oscillator's band-limited virtual-analog waveform." };
}
QinSynthEditor::QinSynthEditor(QinSynthProcessor& synth) : AudioProcessorEditor(&synth)
{
    setOpaque(true);
    look.accentColour=accent;
    look.dialTopColour=juce::Colour(0xff645a45);
    look.dialBottomColour=juce::Colour(0xff2a2925);
    setLookAndFeel(&look);
    addAndMakeVisible(titleHint);
    for(size_t i=0;i<pageTabs.size();++i)
    {
        auto& tab=pageTabs[i];
        tab.setButtonText(i==0 ? "SOUND" : "FX");
        tab.setName(i==0 ? "Sound page" : "FX page");
        tab.setTooltip(i==0 ? "Shows the three oscillator tabs and their sound controls."
                             : "Shows the voice, drive, and delay-reverb controls.");
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(1);
        tab.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
        tab.setColour(juce::TextButton::buttonOnColourId,accent);
        tab.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
        tab.setColour(juce::TextButton::textColourOnId,stageStyle::background);
        tab.onClick=[this,i]{showPage(int(i));};
        addAndMakeVisible(tab);
    }
    for(size_t i=0;i<oscillatorTabs.size();++i)
    {
        auto& tab=oscillatorTabs[i];
        tab.setButtonText("OSC "+juce::String(int(i)+1));
        tab.setName("Oscillator "+juce::String(int(i)+1)+" tab");
        tab.setTooltip("Opens oscillator "+juce::String(int(i)+1)+" in the Sound editor.");
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(2);
        tab.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
        tab.setColour(juce::TextButton::buttonOnColourId,accent);
        tab.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
        tab.setColour(juce::TextButton::textColourOnId,stageStyle::background);
        tab.onClick=[this,i]{showOscillator(int(i));};
        addAndMakeVisible(tab);
    }
    for(size_t i=0;i<sliders.size();++i)
    {
        auto& slider=sliders[i]; auto& label=labels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,18);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        slider.setName(names[i]); slider.setTitle(names[i]);
        label.setText(names[i],juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.f));
        label.setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        sliderAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,ids[i],slider);
        editorTooltips::setHint(slider,label,hints[i]);
    }
    for(size_t i=0;i<waves.size();++i)
    {
        auto& wave=waves[i]; auto& label=waveLabels[i];
        wave.addItem("Saw",1); wave.addItem("Pulse",2); wave.addItem("Triangle",3);
        wave.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a));
        wave.setColour(juce::ComboBox::textColourId,stageStyle::foreground);
        wave.setTooltip(waveHints[i]);
        label.setText("WAVE",juce::dontSendNotification);
        label.setFont(juce::FontOptions(10.f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,stageStyle::secondary);
        label.setTooltip(waveHints[i]);
        addAndMakeVisible(wave); addAndMakeVisible(label);
        waveAttachments[i]=std::make_unique<State::ComboBoxAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"Wave",wave);
        wave.onChange=[this]{repaint();};
    }
    pageTabs[0].setToggleState(true,juce::dontSendNotification);
    oscillatorTabs[0].setToggleState(true,juce::dontSendNotification);
    updateVisibility();
    editorScaling::enable(*this,designWidth,designHeight);
    gpu.attach(*this);
}
void QinSynthEditor::showPage(int page)
{
    currentPage=juce::jlimit(0,1,page);
    for(size_t i=0;i<pageTabs.size();++i)
        pageTabs[i].setToggleState(int(i)==currentPage,juce::dontSendNotification);
    updateVisibility();
    repaint();
}
void QinSynthEditor::showOscillator(int oscillator)
{
    currentOscillator=juce::jlimit(0,2,oscillator);
    for(size_t i=0;i<oscillatorTabs.size();++i)
        oscillatorTabs[i].setToggleState(int(i)==currentOscillator,juce::dontSendNotification);
    updateVisibility();
    repaint();
}
void QinSynthEditor::updateVisibility()
{
    for(int osc=0;osc<3;++osc)
    {
        const bool visible=currentPage==0 && currentOscillator==osc;
        oscillatorTabs[size_t(osc)].setVisible(currentPage==0);
        waves[size_t(osc)].setVisible(visible);
        waveLabels[size_t(osc)].setVisible(visible);
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(osc*4+control);
            sliders[index].setVisible(visible);
            labels[index].setVisible(visible);
        }
    }
    for(size_t index=12;index<sliders.size();++index)
    {
        sliders[index].setVisible(currentPage==1);
        labels[index].setVisible(currentPage==1);
    }
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,30,360,38);
    pageTabs[0].setBounds(18,82,110,32);
    pageTabs[1].setBounds(134,82,110,32);
    for(int osc=0;osc<3;++osc) oscillatorTabs[size_t(osc)].setBounds(30,194+osc*54,124,42);
    for(int osc=0;osc<3;++osc)
    {
        waveLabels[size_t(osc)].setBounds(189,389,110,16);
        waves[size_t(osc)].setBounds(194,416,102,27);
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(osc*4+control);
            const auto x=303+control*103;
            labels[index].setBounds(x,389,99,16);
            sliders[index].setBounds(x,405,99,88);
        }
    }
    for(int index=12;index<23;++index)
    {
        const auto effect=index>=17;
        const auto column=effect ? index-17 : index-12;
        const auto x=33+column*(effect ? 116 : 138);
        const auto y=effect ? 402 : 218;
        labels[size_t(index)].setBounds(x,y,108,16);
        sliders[size_t(index)].setBounds(x,y+17,108,91);
    }
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
    for(auto& tab:pageTabs) tab.setTransform(scale);
    for(auto& tab:oscillatorTabs) tab.setTransform(scale);
    for(auto& slider:sliders) slider.setTransform(scale);
    for(auto& label:labels) label.setTransform(scale);
    for(auto& wave:waves) wave.setTransform(scale);
    for(auto& label:waveLabels) label.setTransform(scale);
}
void QinSynthEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(stageStyle::background);
    graphics.saveState();
    graphics.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    graphics.setGradientFill(juce::ColourGradient(juce::Colour(0xff17171c),0,0,
        stageStyle::background,0,designHeight,false));
    graphics.fillRect(0,0,designWidth,designHeight);
    graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
    graphics.drawText("J-SONIC / VIRTUAL ANALOG INSTRUMENT",18,12,500,16,juce::Justification::centredLeft);
    graphics.setColour(stageStyle::foreground); graphics.setFont(juce::FontOptions(25.f));
    graphics.drawText("QIN SYNTH",18,30,360,38,juce::Justification::centredLeft);
    graphics.setColour(accent); graphics.setFont(juce::FontOptions(11.f));
    graphics.drawText("16 voices  /  3 oscillators  /  8x unison",440,38,298,20,juce::Justification::centredRight);
    if(currentPage==0)
    {
        stageStyle::drawPanel(graphics,{16.f,122.f,728.f,456.f},juce::Colour(0xff27262b));
        stageStyle::drawPanel(graphics,{28.f,150.f,140.f,402.f},juce::Colour(0xff201f24));
        stageStyle::drawPanel(graphics,{180.f,150.f,550.f,402.f},juce::Colour(0xff222126));
        graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText("OSCILLATORS",41,160,110,20,juce::Justification::centredLeft);
        graphics.drawText("OSCILLATOR "+juce::String(currentOscillator+1),194,163,260,20,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
        graphics.drawText("VIRTUAL ANALOG / BAND-LIMITED",194,183,300,17,juce::Justification::centredLeft);
        graphics.drawText("Select an oscillator at left to edit its waveform and unison voices.",194,514,510,20,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::grid);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(193.f,211.f,524.f,157.f),9.f);
        graphics.setColour(stageStyle::inset);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(197.f,215.f,516.f,149.f),7.f);
        graphics.setColour(accent.withAlpha(.18f));
        for(int line=1;line<4;++line)
            graphics.drawHorizontalLine(215+line*37,197.f,713.f);
        const auto wave=waves[size_t(currentOscillator)].getSelectedId();
        juce::Path waveform;
        for(int point=0;point<=520;++point)
        {
            const auto cycle=std::fmod(point/145.f,1.f);
            const auto shape=wave==2 ? (cycle<.5f ? 1.f : -1.f)
                            : wave==3 ? (1.f-4.f*std::abs(cycle-.5f)) : (2.f*cycle-1.f);
            const auto x=197.f+point, y=290.f-55.f*shape;
            if(point==0) waveform.startNewSubPath(x,y); else waveform.lineTo(x,y);
        }
        graphics.setColour(accent.withAlpha(.2f)); graphics.strokePath(waveform,juce::PathStrokeType(5.f));
        graphics.setColour(accent); graphics.strokePath(waveform,juce::PathStrokeType(1.5f));
    }
    else
    {
        stageStyle::drawPanel(graphics,{16.f,122.f,728.f,204.f},juce::Colour(0xff27262b));
        stageStyle::drawPanel(graphics,{16.f,338.f,728.f,240.f},juce::Colour(0xff27262b));
        graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText("VOICE / ZAO DRIVE",31,143,320,20,juce::Justification::centredLeft);
        graphics.drawText("LAN DELAY-VERB / MIDI ONSET MOTION",31,359,470,20,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
        graphics.drawText("Pitch-based pan and analog output coloration",31,165,400,16,juce::Justification::centredLeft);
        graphics.drawText("Tap and phase targets change at MIDI note-on; resonance tame stays off.",31,381,540,16,juce::Justification::centredLeft);
    }
    graphics.restoreState();
}
