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
    }
    editorScaling::enable(*this,designWidth,designHeight);
    gpu.attach(*this);
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,30,360,38);
    for(int osc=0;osc<3;++osc)
    {
        const auto y=84+osc*112;
        waveLabels[size_t(osc)].setBounds(96,y+13,105,16);
        waves[size_t(osc)].setBounds(100,y+46,100,25);
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(osc*4+control);
            const auto x=213+control*128;
            labels[index].setBounds(x,y+13,116,16);
            sliders[index].setBounds(x,y+29,116,75);
        }
    }
    for(int index=12;index<23;++index)
    {
        const auto effect=index>=17;
        const auto column=effect ? index-17 : index-12;
        const auto x=28+column*120, y=effect ? 580 : 456;
        labels[size_t(index)].setBounds(x,y,112,16);
        sliders[size_t(index)].setBounds(x,y+16,112,effect ? 75 : 67);
    }
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
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
    for(int osc=0;osc<3;++osc)
    {
        const auto y=84+osc*112;
        stageStyle::drawPanel(graphics,{16.f,float(y),728.f,106.f},juce::Colour(0xff27262b));
        graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
        graphics.drawText("OSC "+juce::String(osc+1),28,y+13,70,18,juce::Justification::centredLeft);
    }
    stageStyle::drawPanel(graphics,{16.f,420.f,728.f,122.f},juce::Colour(0xff27262b));
    stageStyle::drawPanel(graphics,{16.f,548.f,728.f,150.f},juce::Colour(0xff27262b));
    graphics.setColour(accent); graphics.setFont(juce::FontOptions(12.f).withStyle("bold"));
    graphics.drawText("VOICE / ZAO DRIVE",28,431,260,20,juce::Justification::centredLeft);
    graphics.drawText("LAN DELAY-VERB / MIDI ONSET MOTION",28,559,460,20,juce::Justification::centredLeft);
    graphics.restoreState();
}
