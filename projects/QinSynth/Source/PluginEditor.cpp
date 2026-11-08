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
    for(size_t i=0;i<fxTabs.size();++i)
    {
        auto& tab=fxTabs[i];
        tab.setButtonText(i==0 ? "VOICE + DRIVE" : "DELAY-VERB");
        tab.setName(i==0 ? "Voice and drive tab" : "Delay-verb tab");
        tab.setTooltip(i==0 ? "Shows voice shaping, key panning, Zao Drive, and output."
                            : "Shows Lan's delay-reverb and MIDI onset motion controls.");
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(3);
        tab.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
        tab.setColour(juce::TextButton::buttonOnColourId,accent);
        tab.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
        tab.setColour(juce::TextButton::textColourOnId,stageStyle::background);
        tab.onClick=[this,i]{showFxSection(int(i));};
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
        if(i<12 && i%4==2) slider.onValueChange=[this]{repaint();};
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
    fxTabs[0].setToggleState(true,juce::dontSendNotification);
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
void QinSynthEditor::showFxSection(int section)
{
    currentFxSection=juce::jlimit(0,1,section);
    for(size_t i=0;i<fxTabs.size();++i)
        fxTabs[i].setToggleState(int(i)==currentFxSection,juce::dontSendNotification);
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
    for(auto& tab:fxTabs) tab.setVisible(currentPage==1);
    for(size_t index=12;index<sliders.size();++index)
    {
        const auto visible=currentPage==1 && ((index<17)==(currentFxSection==0));
        sliders[index].setVisible(visible);
        labels[index].setVisible(visible);
    }
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,30,360,38);
    pageTabs[0].setBounds(18,82,115,32);
    pageTabs[1].setBounds(139,82,115,32);
    for(int osc=0;osc<3;++osc) oscillatorTabs[size_t(osc)].setBounds(39+osc*112,158,106,35);
    fxTabs[0].setBounds(39,158,154,35);
    fxTabs[1].setBounds(199,158,154,35);
    for(int osc=0;osc<3;++osc)
    {
        waveLabels[size_t(osc)].setBounds(43,422,120,16);
        waves[size_t(osc)].setBounds(45,458,132,29);
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(osc*4+control);
            constexpr std::array<int,4> positions {615,205,708,801};
            const auto x=positions[size_t(control)];
            labels[index].setBounds(x,422,90,16);
            sliders[index].setBounds(x,438,90,98);
        }
    }
    for(int index=12;index<23;++index)
    {
        const auto effect=index>=17;
        const auto column=effect ? index-17 : index-12;
        const auto x=effect ? 48+column*145 : 78+column*150;
        labels[size_t(index)].setBounds(x,276,112,17);
        sliders[size_t(index)].setBounds(x,296,112,108);
    }
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
    for(auto& tab:pageTabs) tab.setTransform(scale);
    for(auto& tab:oscillatorTabs) tab.setTransform(scale);
    for(auto& tab:fxTabs) tab.setTransform(scale);
    for(auto& slider:sliders) slider.setTransform(scale);
    for(auto& label:labels) label.setTransform(scale);
    for(auto& wave:waves) wave.setTransform(scale);
    for(auto& label:waveLabels) label.setTransform(scale);
}
void QinSynthEditor::paint(juce::Graphics& graphics)
{
    const auto cyan=juce::Colour(0xff55aec7);
    graphics.fillAll(stageStyle::background);
    graphics.saveState();
    graphics.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    graphics.setGradientFill(juce::ColourGradient(juce::Colour(0xff222228),0,0,
        stageStyle::background,0,designHeight,false));
    graphics.fillRect(0,0,designWidth,designHeight);
    graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
    graphics.drawText("J-SONIC / VIRTUAL ANALOG INSTRUMENT",18,12,500,16,juce::Justification::centredLeft);
    graphics.setColour(stageStyle::foreground); graphics.setFont(juce::FontOptions(25.f));
    graphics.drawText("QIN SYNTH",18,30,360,38,juce::Justification::centredLeft);
    graphics.setColour(stageStyle::inset);
    graphics.fillRoundedRectangle(juce::Rectangle<float>(650.f,30.f,252.f,35.f),5.f);
    graphics.setColour(cyan); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
    graphics.drawText("16 VOICES   /   3 OSC   /   8x UNI",661,36,231,23,juce::Justification::centred);
    if(currentPage==0)
    {
        stageStyle::drawPanel(graphics,{16.f,122.f,568.f,456.f},juce::Colour(0xff27272c));
        stageStyle::drawPanel(graphics,{592.f,122.f,312.f,456.f},juce::Colour(0xff27272c));
        graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText("OSCILLATOR "+juce::String(currentOscillator+1),37,132,260,20,juce::Justification::centredLeft);
        graphics.drawText("UNISON / MIX",608,134,260,20,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
        graphics.drawText("WAVEFORM",39,199,132,16,juce::Justification::centredLeft);
        graphics.drawText("STACK SPREAD",610,199,176,16,juce::Justification::centredLeft);
        graphics.drawText("OSCILLATOR SHAPE",39,401,245,17,juce::Justification::centredLeft);
        graphics.drawText("Each tab retains its own waveform and unison settings.",39,539,500,19,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::grid);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(34.f,218.f,532.f,180.f),8.f);
        graphics.setColour(stageStyle::inset);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(38.f,222.f,524.f,172.f),6.f);
        graphics.setColour(cyan.withAlpha(.18f));
        for(int line=1;line<4;++line) graphics.drawHorizontalLine(222+line*43,38.f,562.f);
        for(int line=1;line<8;++line) graphics.drawVerticalLine(38+line*65,222.f,394.f);
        const auto wave=waves[size_t(currentOscillator)].getSelectedId();
        juce::Path waveform;
        for(int point=0;point<=520;++point)
        {
            const auto cycle=std::fmod(point/145.f,1.f);
            const auto shape=wave==2 ? (cycle<.5f ? 1.f : -1.f)
                            : wave==3 ? (1.f-4.f*std::abs(cycle-.5f)) : (2.f*cycle-1.f);
            const auto x=38.f+point, y=308.f-63.f*shape;
            if(point==0) waveform.startNewSubPath(x,y); else waveform.lineTo(x,y);
        }
        graphics.setColour(cyan.withAlpha(.23f)); graphics.strokePath(waveform,juce::PathStrokeType(5.f));
        graphics.setColour(cyan); graphics.strokePath(waveform,juce::PathStrokeType(1.7f));
        graphics.setColour(stageStyle::inset);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(608.f,220.f,280.f,168.f),7.f);
        const auto copies=juce::jlimit(1,8,juce::roundToInt(sliders[size_t(currentOscillator*4+2)].getValue()));
        for(int copy=0;copy<8;++copy)
        {
            const auto active=copy<copies;
            const auto x=627.f+copy*34.f;
            const auto height=active ? 48.f+15.f*float(copy%3) : 22.f;
            graphics.setColour(active ? cyan.withAlpha(.18f) : stageStyle::grid);
            graphics.fillRoundedRectangle(juce::Rectangle<float>(x,352.f-height,21.f,height),3.f);
            graphics.setColour(active ? cyan : stageStyle::secondary.withAlpha(.3f));
            graphics.drawHorizontalLine(int(351.f-height),x,x+21.f);
        }
        graphics.setColour(cyan); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText(juce::String(copies)+" VOICE"+(copies==1 ? "" : "S"),623,233,245,22,juce::Justification::centred);
        graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
        graphics.drawText("Per-oscillator level, count, and detune",608,539,277,18,juce::Justification::centredLeft);
    }
    else
    {
        stageStyle::drawPanel(graphics,{16.f,122.f,888.f,456.f},juce::Colour(0xff27272c));
        stageStyle::drawPanel(graphics,{31.f,209.f,858.f,350.f},juce::Colour(0xff211f25));
        graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText(currentFxSection==0 ? "VOICE / ZAO DRIVE" : "LAN DELAY-VERB / MIDI ONSET MOTION",
            43,220,560,22,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::secondary); graphics.setFont(juce::FontOptions(10.f));
        graphics.drawText(currentFxSection==0 ? "Pitch-based pan and analog output coloration"
                                           : "MIDI note-ons steer the tap and phase; resonance tame stays off.",
            43,244,680,18,juce::Justification::centredLeft);
        graphics.setColour(stageStyle::inset);
        graphics.fillRoundedRectangle(juce::Rectangle<float>(48.f,447.f,825.f,88.f),6.f);
        if(currentFxSection==0)
        {
            graphics.setColour(cyan.withAlpha(.65f));
            graphics.drawLine(96.f,490.f,825.f,490.f,2.f);
            constexpr std::array<const char*,4> stages {"MIDI NOTE","VOICE AMP","ZAO CIRCUIT","OUTPUT"};
            for(size_t stage=0;stage<stages.size();++stage)
            {
                const auto x=105.f+stage*236.f;
                graphics.setColour(accent); graphics.fillEllipse(x-4.f,486.f,8.f,8.f);
                graphics.setColour(cyan); graphics.setFont(juce::FontOptions(11.f));
                graphics.drawText(stages[stage],x-64.f,500,128,18,juce::Justification::centred);
            }
        }
        else
        {
            graphics.setColour(cyan.withAlpha(.65f));
            graphics.drawLine(90.f,490.f,829.f,490.f,1.5f);
            for(int tap=0;tap<8;++tap)
            {
                const auto x=116.f+tap*100.f;
                const auto height=17.f+float((tap*5)%4)*10.f;
                graphics.setColour(cyan.withAlpha(.3f));
                graphics.fillRect(x,490.f-height,4.f,height);
                graphics.setColour(accent);
                graphics.fillEllipse(x-4.f,486.f,12.f,12.f);
            }
            graphics.setColour(cyan); graphics.setFont(juce::FontOptions(10.f));
            graphics.drawText("EIGHT MOVING FEEDBACK TAPS",600,505,245,17,juce::Justification::centredRight);
        }
    }
    graphics.restoreState();
}
