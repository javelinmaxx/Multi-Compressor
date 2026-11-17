#include "PluginEditor.h"
#include "LegatoCurve.h"
#include "WaveformSync.h"
#include "../../EditorScaling.h"
#include <cmath>
#include <complex>

namespace
{
const auto accent=juce::Colour(0xffc6b47b);
const auto cyan=juce::Colour(0xff55aec7);
struct Control { const char* id; const char* name; const char* hint; };
constexpr std::array<Control,44> controls {{
    {"osc1Level","LEVEL","Sets oscillator one's contribution to the mix."},
    {"osc1Octave","OCTAVE","Transposes oscillator one by whole octaves."},
    {"osc1Unison","UNISON","Sets the number of oscillator one copies."},
    {"osc1Detune","DETUNE","Offsets oscillator one's unison copies in cents."},
    {"osc1Spread","SPREAD","Sets how far oscillator one's detuned copies spread across stereo."},
    {"osc1Pan","PAN","Positions oscillator one as a whole in stereo."},
    {"osc2Level","LEVEL","Sets oscillator two's contribution to the mix."},
    {"osc2Octave","OCTAVE","Transposes oscillator two by whole octaves."},
    {"osc2Unison","UNISON","Sets the number of oscillator two copies."},
    {"osc2Detune","DETUNE","Offsets oscillator two's unison copies in cents."},
    {"osc2Spread","SPREAD","Sets how far oscillator two's detuned copies spread across stereo."},
    {"osc2Pan","PAN","Positions oscillator two as a whole in stereo."},
    {"osc3Level","LEVEL","Sets oscillator three's contribution to the mix."},
    {"osc3Octave","OCTAVE","Transposes oscillator three by whole octaves."},
    {"osc3Unison","UNISON","Sets the number of oscillator three copies."},
    {"osc3Detune","DETUNE","Offsets oscillator three's unison copies in cents."},
    {"osc3Spread","SPREAD","Sets how far oscillator three's detuned copies spread across stereo."},
    {"osc3Pan","PAN","Positions oscillator three as a whole in stereo."},
    {"attack","ATTACK","Sets the amplitude envelope rise from 1 to 5000 ms on a logarithmic scale."},
    {"ampDecay","DECAY","Sets the amplitude envelope decay from 1 to 5000 ms on a logarithmic scale."},
    {"ampSustain","SUSTAIN","Sets amplitude while the key remains held."},
    {"release","RELEASE","Sets the amplitude envelope release from 1 to 5000 ms on a logarithmic scale."},
    {"fltAttack","ATTACK","Sets the filter envelope rise from 1 to 5000 ms with logarithmic control."},
    {"fltDecay","DECAY","Sets the filter envelope decay from 1 to 5000 ms with logarithmic control."},
    {"fltSustain","SUSTAIN","Sets the held filter-envelope level with finer control near zero."},
    {"fltRelease","RELEASE","Sets the filter envelope release from 1 to 5000 ms with logarithmic control."},
    {"modAttack","ATTACK","Sets the modulation envelope rise from 1 to 5000 ms on a logarithmic scale."},
    {"modDecay","DECAY","Sets the modulation envelope decay from 1 to 5000 ms on a logarithmic scale."},
    {"modSustain","SUSTAIN","Sets the held level of effect modulation."},
    {"modRelease","RELEASE","Sets the modulation envelope release from 1 to 5000 ms on a logarithmic scale."},
    {"modDepth","MOD DEPTH","Sets how strongly the modulation envelope affects its destination."},
    {"filterCutoff","FREQ","Sets the filter's base cutoff on a logarithmic frequency scale."},
    {"filterResonance","RESONANCE","Emphasizes the cutoff with finer control at high resonance."},
    {"filterKeyFollow","KEY FOLLOW","Raises the cutoff with MIDI note pitch."},
    {"filterEnvAmount","ENV AMOUNT","Sets how far the filter envelope moves the cutoff."},
    {"keyPan","KEY PAN","Sets pitch panning from high notes left at -100% to high notes right at +100%."},
    {"output","OUTPUT","Sets the final synth level."},
    {"drive","DRIVE","Blends in the selected Zao circuit."},
    {"time","TIME","Sets Lan's delay and reverb time."},
    {"decay","DECAY","Sets how long Lan's feedback tail lasts."},
    {"scatter","SCATTER","Morphs Lan's delay into a diffuse reverb."},
    {"verbMix","MIX","Blends the Lan effect with the synth."},
    {"tapMotion","TAP MOTION","Sets how far note onsets move Lan's feedback taps."},
    {"phaseMotion","PHASE MOTION","Sets how strongly note onsets change Lan's phase scatter."}
}};
constexpr std::array<Control,4> noiseControls {{
    {"noiseLevel","LEVEL","Sets the noise oscillator level independently of the three tonal oscillators."},
    {"noiseTune","TUNE","Shifts the note-tracked noise resonance by up to two octaves in semitones."},
    {"noiseHpf","HPF","Removes noise below the selected high-pass frequency."},
    {"noisePan","PAN","Positions the noise oscillator independently in stereo."}
}};
void styleTab(juce::TextButton& button,const juce::String& title,int group)
{
    button.setButtonText(title); button.setClickingTogglesState(true); button.setRadioGroupId(group);
    button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
    button.setColour(juce::TextButton::buttonOnColourId,accent);
    button.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
    button.setColour(juce::TextButton::textColourOnId,stageStyle::background);
}
float filterMagnitude(int type,float frequency,float cutoff,float resonance)
{
    if(type==0) return 1.f;
    const auto ratio=frequency/std::max(30.f,cutoff);
    if(type==1)
    {
        const std::complex<float> pole=1.f/(std::complex<float>(1.f,ratio));
        const auto fourth=pole*pole*pole*pole;
        return std::abs(fourth*(1.f+3.5f*resonance)/(1.f+3.5f*resonance*fourth));
    }
    const auto k=std::clamp(1.4f-1.1f*resonance,.3f,1.4f);
    const auto second=1.f/std::sqrt(std::pow(1.f-ratio*ratio,2.f)+k*k*ratio*ratio);
    if(type==2) return second/std::sqrt(1.f+ratio*ratio);
    if(type==4)
    {
        const auto secondDamping=1.4f-.55f*resonance;
        const auto final=1.f/std::sqrt(std::pow(1.f-ratio*ratio,2.f)
                                       +secondDamping*secondDamping*ratio*ratio);
        return second*final;
    }
    return second;
}
}
QinVoiceSlider::QinVoiceSlider()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setRange(1,16,1);
    setMouseDragSensitivity(140);
    setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    setTooltip("Drag up or down to set the maximum number of simultaneous notes from one to sixteen.");
}
void QinVoiceSlider::paint(juce::Graphics& g)
{
    const auto area=getLocalBounds().toFloat().reduced(1);
    g.setColour(stageStyle::grid);
    g.drawRoundedRectangle(area,4,1);
    g.setColour(cyan); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText(juce::String(juce::roundToInt(getValue()))+" VOICES",getLocalBounds(),juce::Justification::centred);
}
QinWavePad::QinWavePad()
{
    setName("WAVE PAD"); setTitle("Waveform blend");
}
bool QinWavePad::setPosition(qin::WavePosition next)
{
    next=qin::clampWavePosition(next.x,next.y);
    if(std::abs(next.x-position.x)<.0005f && std::abs(next.y-position.y)<.0005f) return false;
    position=next; repaint(); return true;
}
void QinWavePad::mouseDown(const juce::MouseEvent& event)
{
    if(!event.mods.isLeftButtonDown()) return;
    dragging=true;
    if(onDragStart) onDragStart();
    mouseDrag(event);
}
void QinWavePad::mouseDrag(const juce::MouseEvent& event)
{
    if(!dragging) return;
    constexpr float inset=16.f, top=16.f;
    const auto right=float(getWidth())-inset, bottom=float(getHeight())-16.f;
    if(bottom<=top || right<=inset) return;
    const auto next=qin::clampWavePosition((event.position.x-inset)/(right-inset),
                                          (bottom-event.position.y)/(bottom-top));
    if(setPosition(next) && onPositionChange) onPositionChange(position);
}
void QinWavePad::mouseUp(const juce::MouseEvent&)
{
    if(!dragging) return;
    dragging=false;
    if(onDragEnd) onDragEnd();
}
void QinWavePad::paint(juce::Graphics& g)
{
    constexpr float inset=16.f, top=16.f;
    const auto right=float(getWidth())-inset, bottom=float(getHeight())-16.f;
    const auto square=juce::Rectangle<float>(inset,top,right-inset,bottom-top);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(square,3.f);
    g.setColour(stageStyle::grid); g.drawRoundedRectangle(square,3.f,1.3f);
    g.setColour(stageStyle::grid.withAlpha(.7f));
    g.drawLine(square.getCentreX(),top,square.getCentreX(),bottom,.7f);
    g.drawLine(inset,square.getCentreY(),right,square.getCentreY(),.7f);
    const auto x=inset+(right-inset)*position.x;
    const auto y=bottom-(bottom-top)*position.y;
    g.setColour(cyan.withAlpha(.35f)); g.drawLine(square.getCentreX(),square.getCentreY(),x,y,1.f);
    g.setColour(accent.withAlpha(.3f)); g.fillEllipse(x-9.f,y-9.f,18.f,18.f);
    g.setColour(juce::Colour(0xfff4f0e6)); g.fillEllipse(x-5.f,y-5.f,10.f,10.f);
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("TRI",0,0,37,13,juce::Justification::centredLeft);
    g.drawText("SINE",getWidth()-39,0,39,13,juce::Justification::centredRight);
    g.drawText("SAW",0,juce::roundToInt(bottom)+2,37,13,juce::Justification::centredLeft);
    g.drawText("PULSE",getWidth()-47,juce::roundToInt(bottom)+2,47,13,juce::Justification::centredRight);
}
QinRetrigSwitch::QinRetrigSwitch(const juce::String& caption) : ToggleButton(caption)
{
    setTooltip(caption=="RETRIG"
        ? "Restarts this oscillator's waveform phase at each note onset when enabled."
        : caption=="TRANSFORMER" ? "Adds Ren's transformer to the output and saves CPU when off."
        : "Enables voice matching and pitch slides when polyphony exceeds one voice.");
}
void QinRetrigSwitch::paintButton(juce::Graphics& g,bool highlighted,bool down)
{
    g.setColour(highlighted ? stageStyle::foreground : stageStyle::secondary);
    g.setFont(juce::FontOptions(10.f).withStyle("bold"));
    g.drawText(getButtonText(),0,0,getWidth()-37,getHeight(),juce::Justification::centredRight);
    const auto track=juce::Rectangle<float>(float(getWidth()-34),float(getHeight())*.5f-9,32,18);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(track,9);
    g.setColour(accent.withAlpha(.6f)); g.drawRoundedRectangle(track,9,1);
    const auto thumb=juce::Rectangle<float>(track.getX()+2+(getToggleState()?13.f:0.f),track.getY()+2,14,14);
    g.setColour(juce::Colours::black.withAlpha(.34f)); g.fillEllipse(thumb.translated(0,1));
    g.setGradientFill(juce::ColourGradient(down ? juce::Colours::white : juce::Colour(0xfff4f0e6),
        thumb.getTopLeft(),juce::Colour(0xff8a8c88),thumb.getBottomLeft(),false)); g.fillEllipse(thumb);
    g.setColour(getToggleState()?accent:stageStyle::secondary.withAlpha(.7f)); g.drawEllipse(thumb,.8f);
}
void QinLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,
    float sliderPos,float minSliderPos,float maxSliderPos,const juce::Slider::SliderStyle style,juce::Slider& slider)
{
    if(style!=juce::Slider::LinearHorizontal)
    { StageLookAndFeel::drawLinearSlider(g,x,y,width,height,sliderPos,minSliderPos,maxSliderPos,style,slider); return; }
    const auto left=float(x+6), right=float(x+width-6), center=float(y)+height*.5f;
    const auto position=std::clamp(sliderPos,left,right);
    g.setColour(trackColour.brighter(.3f));
    g.fillRoundedRectangle(left,center-2.f,right-left,4.f,2.f);
    g.setColour(accentColour.withAlpha(.72f));
    g.fillRoundedRectangle(left,center-1.f,std::max(0.f,position-left),2.f,1.f);
    g.setColour(accentColour); g.fillRoundedRectangle(position-3.f,center-6.f,6.f,12.f,2.f);
}
QinSynthEditor::QinSynthEditor(QinSynthProcessor& synth)
    : AudioProcessorEditor(&synth), processor(synth)
{
    setOpaque(true); look.accentColour=accent; look.dialTopColour=juce::Colour(0xff645a45);
    look.dialBottomColour=juce::Colour(0xff2a2925); setLookAndFeel(&look);
    addAndMakeVisible(titleHint);
    addAndMakeVisible(voiceCount);
    voiceCountAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"voiceCount",voiceCount);
    activeVoicesLabel.setText("0 ACTIVE",juce::dontSendNotification);
    activeVoicesLabel.setFont(juce::FontOptions(11.f).withStyle("bold"));
    activeVoicesLabel.setJustificationType(juce::Justification::centred);
    activeVoicesLabel.setColour(juce::Label::textColourId,cyan);
    activeVoicesLabel.setTooltip("Shows the number of allocated voices, including release tails.");
    addAndMakeVisible(activeVoicesLabel);
    addAndMakeVisible(polyLegatoButton);
    polyLegatoAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"polyLegato",polyLegatoButton);
    polyLegatoButton.onClick=[this] { updatePlayAvailability(); };
    for(const auto* name:{"Closest","Farthest","Upwards","Downwards"})
        legatoRule.addItem(name,legatoRule.getNumItems()+1);
    legatoRule.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a));
    legatoRule.setColour(juce::ComboBox::textColourId,stageStyle::foreground);
    legatoRule.setTooltip("Closest and Farthest use pitch distance, while Upwards and Downwards prefer voices below or above the new note.");
    legatoRuleLabel.setText("VOICE MATCH",juce::dontSendNotification);
    legatoRuleLabel.setTooltip(legatoRule.getTooltip());
    for(auto* label:{&legatoRuleLabel,&slideTimeLabel,&slideShapeLabel})
    {
        label->setFont(juce::FontOptions(9.f));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(*label);
    }
    addAndMakeVisible(legatoRule);
    legatoRuleAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"legatoMode",legatoRule);
    slideTimeLabel.setText("SLIDE",juce::dontSendNotification);
    slideShapeLabel.setText("SHAPE",juce::dontSendNotification);
    for(auto* slider:{&slideTime,&slideShape})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,17);
        slider->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider->setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        addAndMakeVisible(*slider);
    }
    slideTime.setTooltip("Sets mono and polyphonic legato slide time from instant to two seconds.");
    slideTimeLabel.setTooltip(slideTime.getTooltip());
    slideShape.setTooltip("Morphs the pitch glide from logarithmic through linear to sigmoid.");
    slideShapeLabel.setTooltip(slideShape.getTooltip());
    slideTimeAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"slideTime",slideTime);
    slideShapeAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"slideShape",slideShape);
    slideTime.textFromValueFunction=[](double value)
    { return value>=1000 ? juce::String(value*.001,2)+" s" : juce::String(juce::roundToInt(value))+" ms"; };
    slideShape.textFromValueFunction=[](double value)
    {
        if(value<.1) return juce::String("LOG");
        if(value>99.9) return juce::String("SIGMOID");
        if(std::abs(value-50.)<.1) return juce::String("LINEAR");
        return value<50 ? juce::String(juce::roundToInt(value*2))+"% LIN"
                        : juce::String(juce::roundToInt((value-50)*2))+"% SIG";
    };
    slideTime.updateText(); slideShape.updateText();
    slideShape.onValueChange=[this]
    { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{341,651,78,83})); };
    updatePlayAvailability();
    for(size_t i=0;i<oscillatorTabs.size();++i)
    {
        auto& tab=oscillatorTabs[i]; styleTab(tab,"OSC "+juce::String(int(i)+1),2);
        tab.setTooltip("Opens oscillator "+juce::String(int(i)+1)+" in the OSC panel.");
        tab.onClick=[this,i]{showOscillator(int(i));}; addAndMakeVisible(tab);
    }
    noiseButton.setClickingTogglesState(true);
    noiseButton.setTooltip("Enables the noise oscillator and opens its tuning, filter, and style controls.");
    noiseButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
    noiseButton.setColour(juce::TextButton::buttonOnColourId,accent);
    addAndMakeVisible(noiseButton);
    noiseAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"noise",noiseButton);
    noiseButton.onClick=[this] { showOscillator(3); };
    noiseStyle.addItem("White",1); noiseStyle.addItem("Pink",2); noiseStyle.addItem("Velvet",3);
    noiseStyle.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a));
    noiseStyle.setColour(juce::ComboBox::textColourId,stageStyle::foreground);
    noiseStyle.setTooltip("Selects white, pink, or sparse velvet noise before the tuned filter.");
    noiseStyleLabel.setText("NOISE STYLE",juce::dontSendNotification);
    noiseStyleLabel.setFont(juce::FontOptions(9.f));
    noiseStyleLabel.setJustificationType(juce::Justification::centred);
    noiseStyleLabel.setColour(juce::Label::textColourId,stageStyle::secondary);
    noiseStyleLabel.setTooltip("Selects white, pink, or sparse velvet noise before the tuned filter.");
    addAndMakeVisible(noiseStyle); addAndMakeVisible(noiseStyleLabel);
    noiseStyleAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"noiseStyle",noiseStyle);
    noiseStyle.onChange=[this] { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{34,263,384,96})); };
    for(size_t i=0;i<noiseSliders.size();++i)
    {
        auto& slider=noiseSliders[i]; auto& label=noiseLabels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        slider.setName(noiseControls[i].name); slider.setTitle(noiseControls[i].name);
        label.setText(noiseControls[i].name,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f));
        label.setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        noiseSliderAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,noiseControls[i].id,slider);
        editorTooltips::setHint(slider,label,noiseControls[i].hint);
    }
    noiseSliders[1].textFromValueFunction=[](double value)
    { return (value>0 ? "+" : "")+juce::String(value,1)+" st"; };
    noiseSliders[2].textFromValueFunction=[](double value)
    { return juce::String(juce::roundToInt(value))+" Hz"; };
    noiseSliders[1].updateText(); noiseSliders[2].updateText();
    timesFiveButton.setClickingTogglesState(true);
    timesFiveButton.setTooltip("Multiplies Attack, Decay, and Release of all three envelopes by five while enabled, up to 25000 ms.");
    timesFiveButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
    timesFiveButton.setColour(juce::TextButton::buttonOnColourId,accent);
    timesFiveButton.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
    timesFiveButton.setColour(juce::TextButton::textColourOnId,stageStyle::background);
    addAndMakeVisible(timesFiveButton);
    timesFiveAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"envTimesFive",timesFiveButton);
    filterMergeLabel.setText("MERGE",juce::dontSendNotification);
    filterMergeLabel.setJustificationType(juce::Justification::centred);
    filterMergeLabel.setFont(juce::FontOptions(9.f));
    filterMergeLabel.setColour(juce::Label::textColourId,stageStyle::secondary);
    filterMergeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    filterMergeSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
    filterMergeSlider.setColour(juce::Slider::thumbColourId,accent);
    filterMergeSlider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    filterMergeSlider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
    filterMergeSlider.textFromValueFunction=[](double value) { return juce::String(value,1); };
    editorTooltips::setHint(filterMergeSlider,filterMergeLabel,
        "Blends per-voice filters into one shared master filter at 100%.");
    addAndMakeVisible(filterMergeSlider); addAndMakeVisible(filterMergeLabel);
    filterMergeAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"filterMerge",filterMergeSlider);
    for(size_t i=0;i<envelopeTabs.size();++i)
    {
        auto& tab=envelopeTabs[i]; styleTab(tab,i==0 ? "AMP" : i==1 ? "FLT" : "MOD",3);
        tab.setTooltip(i==0 ? "Opens the amplitude envelope." : i==1 ? "Opens the filter envelope." : "Opens the effect modulation envelope.");
        tab.onClick=[this,i]{showEnvelope(int(i));}; addAndMakeVisible(tab);
    }
    for(size_t i=0;i<fxTabs.size();++i)
    {
        auto& tab=fxTabs[i]; styleTab(tab,i==0 ? "DRIVE" : "DELAY",4);
        tab.setTooltip(i==0 ? "Opens the drive controls." : "Opens Lan's delay and reverb controls.");
        tab.onClick=[this,i]{showFxSection(int(i));}; addAndMakeVisible(tab);
    }
    for(size_t mode=0;mode<zao::circuitNames.size();++mode)
        driveMode.addItem(zao::circuitNames[mode],int(mode)+1);
    driveMode.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a));
    driveMode.setColour(juce::ComboBox::textColourId,stageStyle::foreground);
    driveMode.setTooltip("Selects BJT, op-amp, diode shunt, or series crossover drive from Zao.");
    driveModeLabel.setText("CIRCUIT",juce::dontSendNotification);
    driveModeLabel.setFont(juce::FontOptions(9.f));
    driveModeLabel.setColour(juce::Label::textColourId,stageStyle::secondary);
    driveModeLabel.setTooltip(driveMode.getTooltip());
    addAndMakeVisible(driveMode); addAndMakeVisible(driveModeLabel);
    driveModeAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"driveMode",driveMode);
    driveMode.onChange=[this]
    {
        updateDrivePreview();
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{707,392,182,153}));
    };
    addAndMakeVisible(transformerButton);
    transformerAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"transformer",transformerButton);
    for(size_t i=0;i<sliders.size();++i)
    {
        auto& slider=sliders[i]; auto& label=labels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        slider.setName(controls[i].name); slider.setTitle(controls[i].name);
        label.setText(controls[i].name,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f)); label.setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        sliderAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,controls[i].id,slider);
        editorTooltips::setHint(slider,label,controls[i].hint);
        if(i<18 && i%6==2) slider.onValueChange=[this]{repaint();};
    }
    sliders[30].setSliderStyle(juce::Slider::LinearHorizontal);
    sliders[30].setTextBoxStyle(juce::Slider::TextBoxRight,false,42,18);
    sliders[30].setColour(juce::Slider::thumbColourId,accent);
    curveLabel.setText("CURVE",juce::dontSendNotification);
    curveLabel.setJustificationType(juce::Justification::centred);
    curveLabel.setFont(juce::FontOptions(9.f));
    curveLabel.setColour(juce::Label::textColourId,stageStyle::secondary);
    curveLabel.setTooltip("Negative curve values act sooner, while positive values delay attack, decay, and release movement.");
    addAndMakeVisible(curveLabel);
    constexpr std::array<const char*,3> curveIds {"ampCurve","fltCurve","modCurve"};
    for(size_t i=0;i<curveSliders.size();++i)
    {
        auto& slider=curveSliders[i];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,42,16);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        slider.setTooltip("Negative curve values act sooner, while positive values delay attack, decay, and release movement.");
        addAndMakeVisible(slider);
        curveAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,curveIds[i],slider);
        slider.onValueChange=[this]
        { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,433,240,266})); };
    }
    curveLabel.setText("C",juce::dontSendNotification);
    curveLabel.setFont(juce::FontOptions(10.f).withStyle("bold"));
    curveLabel.setColour(juce::Label::textColourId,stageStyle::foreground.withAlpha(.8f));
    for(size_t i=18;i<=34;++i)
    {
        if(i==30) continue;
        auto& slider=sliders[i];
        slider.setSliderStyle(i<30 ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        if(i<30)
        {
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,42,16);
            labels[i].setText(juce::String::charToString("ADSR"[(i-18)%4]),juce::dontSendNotification);
            labels[i].setFont(juce::FontOptions(10.f).withStyle("bold"));
            labels[i].setColour(juce::Label::textColourId,stageStyle::foreground.withAlpha(.8f));
        }
        else slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.onValueChange=[this,i]
        {
            const auto area=i<30 ? juce::Rectangle<int>(444,433,240,266) : juce::Rectangle<int>(444,90,240,335);
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,area));
            if(i==24) repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,90,240,335}));
        };
    }
    sliders[31].textFromValueFunction=[](double value)
    { return juce::String(juce::roundToInt(value)); };
    sliders[32].textFromValueFunction=[](double value)
    { return juce::String(value,1); };
    for(const auto index:{18,19,21,22,23,25,26,27,29})
    {
        const auto filter=index>=22 && index<=25;
        sliders[size_t(index)].textFromValueFunction=[this,filter](double value)
        {
            const auto multiplier=processor.parameters.getRawParameterValue("envTimesFive")->load()>=.5f ? 5. : 1.;
            const auto ms=value*(filter ? 1. : 1000.)*multiplier;
            return ms>=1000. ? juce::String(ms/1000.,1)+"s"
                : juce::String(juce::roundToInt(ms))+"ms";
        };
        sliders[size_t(index)].valueFromTextFunction=[this,filter](const juce::String& text)
        {
            const auto multiplier=processor.parameters.getRawParameterValue("envTimesFive")->load()>=.5f ? 5. : 1.;
            const auto ms=double(text.getFloatValue())*(text.trim().endsWithIgnoreCase("ms") ? 1. :
                text.trim().endsWithIgnoreCase("s") ? 1000. : 1.);
            return ms/((filter ? 1. : 1000.)*multiplier);
        };
        sliders[size_t(index)].updateText();
    }
    sliders[30].setSliderStyle(juce::Slider::LinearHorizontal);
    sliders[30].setTextBoxStyle(juce::Slider::TextBoxRight,false,42,18);
    timesFiveButton.onClick=[this] { updateEnvelopeTimePresentation(); };
    sliders[24].textFromValueFunction=[](double value)
    { return juce::String(value,1); };
    sliders[24].updateText();
    sliders[37].onValueChange=[this]
    {
        updateDrivePreview();
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{707,392,182,153}));
    };
    for(size_t i=0;i<waves.size();++i)
    {
        auto& wave=waves[i]; auto& label=waveLabels[i];
        wave.setTooltip("Drag within the square to blend oscillator "+juce::String(int(i)+1)+" between saw, pulse, triangle, and sine waves.");
        label.setText("WAVE",juce::dontSendNotification); label.setFont(juce::FontOptions(9.f));
        label.setJustificationType(juce::Justification::centred); label.setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(wave); addAndMakeVisible(label); addAndMakeVisible(retrig[i]);
        const auto prefix="osc"+juce::String(int(i)+1);
        waveXParameters[i]=synth.parameters.getParameter(prefix+"WaveX");
        waveYParameters[i]=synth.parameters.getParameter(prefix+"WaveY");
        wave.setPosition({synth.parameters.getRawParameterValue(prefix+"WaveX")->load()*.01f,
                          synth.parameters.getRawParameterValue(prefix+"WaveY")->load()*.01f});
        wave.onDragStart=[this,i]
        { waveXParameters[i]->beginChangeGesture(); waveYParameters[i]->beginChangeGesture(); };
        wave.onDragEnd=[this,i]
        { waveXParameters[i]->endChangeGesture(); waveYParameters[i]->endChangeGesture(); };
        wave.onPositionChange=[this,i](qin::WavePosition position)
        {
            auto* x=waveXParameters[i]; auto* y=waveYParameters[i];
            x->setValueNotifyingHost(x->convertTo0to1(position.x*100.f));
            y->setValueNotifyingHost(y->convertTo0to1(position.y*100.f));
            if(int(i)==currentOscillator)
                repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{34,263,384,96}));
        };
        retrigAttachments[i]=std::make_unique<State::ButtonAttachment>(synth.parameters,"osc"+juce::String(int(i)+1)+"Retrig",retrig[i]);
    }
    for(const auto* option:{"Bypass","TB-303 ladder","Sallen-Key 18 dB","Digital 12 dB","Digital 24 dB"})
        filterType.addItem(option,filterType.getNumItems()+1);
    modTarget.addItem("Drive",1); modTarget.addItem("Spread",2);
    filterType.setTooltip("Selects the TB-303, 18 dB Sallen-Key, or 12/24 dB digital filter model.");
    modTarget.setTooltip("Routes the modulation envelope to drive or unison spread.");
    for(auto* box:{&filterType,&modTarget})
    { box->setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a)); box->setColour(juce::ComboBox::textColourId,stageStyle::foreground); addAndMakeVisible(*box); }
    for(auto* label:{&filterTypeLabel,&modTargetLabel})
    { label->setFont(juce::FontOptions(9.f)); label->setColour(juce::Label::textColourId,stageStyle::secondary); addAndMakeVisible(*label); }
    filterTypeLabel.setText("FILTER TYPE",juce::dontSendNotification);
    modTargetLabel.setText("MOD TARGET",juce::dontSendNotification);
    filterTypeAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"filterType",filterType);
    modTargetAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"modTarget",modTarget);
    filterType.onChange=[this]
    { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,90,240,335})); };
    oscillatorTabs[0].setToggleState(true,juce::dontSendNotification);
    envelopeTabs[0].setToggleState(true,juce::dontSendNotification);
    fxTabs[0].setToggleState(true,juce::dontSendNotification);
    updateVisibility(); editorScaling::enable(*this,designWidth,designHeight);
    updateDrivePreview(); gpu.attach(*this); startTimerHz(40);
}
void QinSynthEditor::updateEnvelopeTimePresentation()
{
    lastTimesFive=processor.parameters.getRawParameterValue("envTimesFive")->load()>=.5f;
    for(const auto index:{18,19,21,22,23,25,26,27,29}) sliders[size_t(index)].updateText();
    repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,433,240,266}));
}
void QinSynthEditor::updateDrivePreview()
{
    qin::DriveCircuitBank circuit;
    circuit.prepare(48000.0);
    const auto mode=std::clamp(driveMode.getSelectedId()-1,0,3);
    const auto mix=std::clamp(float(sliders[37].getValue())*.01f,0.f,1.f);
    for(int sample=0;sample<384;++sample)
    {
        const auto phase=std::fmod(float(sample)/64.f,1.f);
        const auto original=.55f*(1.f-4.f*std::abs(phase-.5f));
        const auto driven=float(circuit.process(mode,0,original*(1.f+5.f*mix)));
        if(sample>=256)
        {
            const auto index=size_t(sample-256);
            previewInput[index]=original;
            previewOutput[index]=original+mix*(driven-original);
        }
    }
}
void QinSynthEditor::timerCallback()
{
    const auto active=juce::String(processor.getActiveVoiceCount())+" ACTIVE";
    if(activeVoicesLabel.getText()!=active) activeVoicesLabel.setText(active,juce::dontSendNotification);
    if(polyLegatoButton.getToggleState()!=lastPolyLegato) updatePlayAvailability();
    if(const auto enabled=processor.parameters.getRawParameterValue("envTimesFive")->load()>=.5f;
       enabled!=lastTimesFive) updateEnvelopeTimePresentation();
    const auto cutoff=processor.getLiveFilterCutoff();
    if((cutoff<=0)!=(liveCutoff<=0) || (cutoff>0 && std::abs(std::log(cutoff/std::max(1.f,liveCutoff)))>.002f))
    {
        liveCutoff=cutoff;
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{455,185,218,90}));
    }
    if(currentFxSection==0)
    {
        auto next=processor.getDriveScope();
        if(next.epoch!=lastScopeEpoch)
        {
            lastScopeEpoch=next.epoch;
            liveScopeValid=false;
            for(size_t index=0;index<next.input.size();++index)
                if(std::abs(next.input[index])>1.e-4f || std::abs(next.output[index])>1.e-4f)
                { liveScopeValid=true; break; }
            liveScope=std::move(next);
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{707,392,182,153}));
        }
    }
    for(size_t i=0;i<waves.size();++i)
    {
        const auto changed=waves[i].setPosition({waveXParameters[i]->convertFrom0to1(waveXParameters[i]->getValue())*.01f,
                                                 waveYParameters[i]->convertFrom0to1(waveYParameters[i]->getValue())*.01f});
        if(changed && int(i)==currentOscillator)
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{34,263,384,96}));
    }
}
void QinSynthEditor::updatePlayAvailability()
{
    lastPolyLegato=polyLegatoButton.getToggleState();
    legatoRule.setEnabled(lastPolyLegato);
    legatoRuleLabel.setEnabled(lastPolyLegato);
}
void QinSynthEditor::showOscillator(int index)
{ currentOscillator=juce::jlimit(0,3,index); for(size_t i=0;i<3;++i) oscillatorTabs[i].setToggleState(int(i)==currentOscillator,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::showEnvelope(int index)
{ currentEnvelope=juce::jlimit(0,2,index); for(size_t i=0;i<3;++i) envelopeTabs[i].setToggleState(int(i)==currentEnvelope,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::showFxSection(int index)
{ currentFxSection=juce::jlimit(0,1,index); for(size_t i=0;i<2;++i) fxTabs[i].setToggleState(int(i)==currentFxSection,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::updateVisibility()
{
    for(int osc=0;osc<3;++osc)
    {
        const auto selected=osc==currentOscillator; waves[size_t(osc)].setVisible(selected); waveLabels[size_t(osc)].setVisible(selected); retrig[size_t(osc)].setVisible(selected);
        for(int control=0;control<6;++control) { const auto i=size_t(osc*6+control); sliders[i].setVisible(selected); labels[i].setVisible(selected); }
    }
    noiseStyle.setVisible(currentOscillator==3); noiseStyleLabel.setVisible(currentOscillator==3);
    for(size_t i=0;i<noiseSliders.size();++i)
    { noiseSliders[i].setVisible(currentOscillator==3); noiseLabels[i].setVisible(currentOscillator==3); }
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control) { const auto i=size_t(18+env*4+control); sliders[i].setVisible(env==currentEnvelope); labels[i].setVisible(env==currentEnvelope); }
    for(int env=0;env<3;++env) curveSliders[size_t(env)].setVisible(env==currentEnvelope);
    timesFiveButton.setVisible(true);
    sliders[30].setVisible(currentEnvelope==2); labels[30].setVisible(currentEnvelope==2);
    modTarget.setVisible(currentEnvelope==2); modTargetLabel.setVisible(currentEnvelope==2);
    for(size_t i=36;i<sliders.size();++i)
    { const auto selected=i==36 || (currentFxSection==0 ? i==37 : i>=38); sliders[i].setVisible(selected); labels[i].setVisible(selected && i!=36); }
    driveMode.setVisible(currentFxSection==0); driveModeLabel.setVisible(currentFxSection==0);
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,29,360,38);
    voiceCount.setBounds(660,34,94,27);
    activeVoicesLabel.setBounds(761,35,130,25);
    polyLegatoButton.setBounds(190,573,215,30);
    legatoRuleLabel.setBounds(34,606,127,15); legatoRule.setBounds(34,624,127,29);
    slideTimeLabel.setBounds(168,606,80,15); slideTime.setBounds(168,622,80,69);
    slideShapeLabel.setBounds(254,606,81,15); slideShape.setBounds(254,622,81,69);
    for(int i=0;i<3;++i)
    {
        oscillatorTabs[size_t(i)].setBounds(31+i*109,125,103,27);
        envelopeTabs[size_t(i)].setBounds(455+i*73,466,68,27);
        waveLabels[size_t(i)].setBounds(34,153,96,11); waves[size_t(i)].setBounds(34,164,96,96);
        retrig[size_t(i)].setBounds(198,187,100,34);
        for(int control=0;control<6;++control)
        {
            const auto index=size_t(i*6+control); const auto x=35+(control%3)*132, y=367+(control/3)*88;
            labels[index].setBounds(x,y,103,15); sliders[index].setBounds(x,y+15,103,73);
        }
    }
    noiseButton.setBounds(363,125,55,27);
    noiseStyleLabel.setBounds(34,170,118,15); noiseStyle.setBounds(34,186,154,27);
    for(size_t i=0;i<noiseSliders.size();++i)
    {
        const auto x=35+int(i%3)*132, y=367+int(i/3)*88;
        noiseLabels[i].setBounds(x,y,103,15); noiseSliders[i].setBounds(x,y+15,103,73);
    }
    timesFiveButton.setBounds(618,438,51,24);
    labels[35].setBounds(302,160,112,15); sliders[35].setBounds(302,176,112,82);
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(18+env*4+control);
            const auto x=455+control*44;
            labels[index].setBounds(x,550,42,15); sliders[index].setBounds(x,565,42,84);
        }
    curveLabel.setBounds(631,550,42,15);
    for(auto& slider:curveSliders) slider.setBounds(631,565,42,84);
    labels[30].setBounds(455,655,102,14); sliders[30].setBounds(455,672,102,22);
    modTargetLabel.setBounds(561,655,108,14); modTarget.setBounds(561,670,108,25);
    filterTypeLabel.setBounds(455,131,211,15); filterType.setBounds(454,148,215,28);
    for(int index=31;index<=34;++index)
    {
        const auto y=285+(index-31)*27;
        labels[size_t(index)].setBounds(454,y,68,25); sliders[size_t(index)].setBounds(523,y,150,25);
    }
    filterMergeLabel.setBounds(454,393,68,25);
    filterMergeSlider.setBounds(523,393,150,25);
    for(int i=0;i<2;++i) fxTabs[size_t(i)].setBounds(703+i*97,124,92,27);
    driveModeLabel.setBounds(707,178,180,14); driveMode.setBounds(707,194,180,29);
    transformerButton.setBounds(710,588,176,30);
    labels[36].setBounds(760,613,82,14); sliders[36].setBounds(760,619,82,74);
    labels[37].setBounds(760,251,82,14); sliders[37].setBounds(760,265,82,74);
    for(int i=38;i<44;++i)
    {
        const auto n=i-38,x=706+(n%2)*94,y=178+(n/2)*99;
        labels[size_t(i)].setBounds(x,y,90,14); sliders[size_t(i)].setBounds(x+8,y+13,74,75);
    }
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
    for(auto& tab:oscillatorTabs) tab.setTransform(scale);
    noiseButton.setTransform(scale);
    noiseStyle.setTransform(scale); noiseStyleLabel.setTransform(scale);
    for(auto& slider:noiseSliders) slider.setTransform(scale);
    for(auto& label:noiseLabels) label.setTransform(scale);
    timesFiveButton.setTransform(scale);
    filterMergeSlider.setTransform(scale); filterMergeLabel.setTransform(scale);
    for(auto& tab:envelopeTabs) tab.setTransform(scale);
    for(auto& tab:fxTabs) tab.setTransform(scale);
    driveMode.setTransform(scale); driveModeLabel.setTransform(scale);
    transformerButton.setTransform(scale);
    for(auto& slider:sliders) slider.setTransform(scale);
    for(auto& slider:curveSliders) slider.setTransform(scale);
    curveLabel.setTransform(scale);
    for(auto& label:labels) label.setTransform(scale);
    for(auto& wave:waves) wave.setTransform(scale);
    for(auto& button:retrig) button.setTransform(scale);
    voiceCount.setTransform(scale);
    activeVoicesLabel.setTransform(scale);
    polyLegatoButton.setTransform(scale);
    legatoRule.setTransform(scale); legatoRuleLabel.setTransform(scale);
    slideTime.setTransform(scale); slideTimeLabel.setTransform(scale);
    slideShape.setTransform(scale); slideShapeLabel.setTransform(scale);
    for(auto& label:waveLabels) label.setTransform(scale);
    for(auto* box:{&filterType,&modTarget}) box->setTransform(scale);
    for(auto* label:{&filterTypeLabel,&modTargetLabel}) label->setTransform(scale);
}
void QinSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(stageStyle::background); g.saveState(); g.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222228),0,0,stageStyle::background,0,designHeight,false));
    g.fillRect(0,0,designWidth,designHeight);
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(10.f));
    g.drawText("J-SONIC / VIRTUAL ANALOG INSTRUMENT",18,12,500,16,juce::Justification::centredLeft);
    g.setColour(stageStyle::foreground); g.setFont(juce::FontOptions(25.f));
    g.drawText("QIN SYNTH",18,30,360,38,juce::Justification::centredLeft);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(juce::Rectangle<float>(650,30,252,35),5);
    g.setColour(cyan); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    const auto panel=juce::Colour(0xff27272c);
    stageStyle::drawPanel(g,{16,90,420,469},panel);
    stageStyle::drawPanel(g,{16,569,420,130},panel);
    stageStyle::drawPanel(g,{444,90,240,335},panel);
    stageStyle::drawPanel(g,{444,433,240,266},panel);
    stageStyle::drawPanel(g,{692,90,212,460},panel);
    stageStyle::drawPanel(g,{692,560,212,139},panel);
    g.setColour(accent); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText(currentOscillator==3 ? "NOISE" : "OSC",30,99,120,20,juce::Justification::centredLeft);
    g.drawText("PLAY",30,576,120,20,juce::Justification::centredLeft);
    const juce::Rectangle<float> glidePlot(342,624,76,62);
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(9.f));
    g.drawText("CURVE",342,606,76,15,juce::Justification::centred);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(glidePlot,5);
    const auto curveLeft=glidePlot.getX()+5.f, curveRight=glidePlot.getRight()-5.f;
    const auto curveTop=glidePlot.getY()+5.f, curveBottom=glidePlot.getBottom()-5.f;
    g.setColour(stageStyle::grid.withAlpha(.48f));
    g.drawLine(curveLeft,curveBottom,curveRight,curveTop,.8f);
    g.drawLine(curveLeft,glidePlot.getCentreY(),curveRight,glidePlot.getCentreY(),.6f);
    g.drawLine(glidePlot.getCentreX(),curveTop,glidePlot.getCentreX(),curveBottom,.6f);
    const auto glideShapeValue=std::clamp(float(slideShape.getValue())*.01f,0.f,1.f);
    juce::Path glidePath;
    for(int point=0;point<=64;++point)
    {
        const auto t=float(point)/64.f;
        const auto x=curveLeft+(curveRight-curveLeft)*t;
        const auto y=curveBottom-(curveBottom-curveTop)*qin::legatoCurve(t,glideShapeValue);
        if(point==0) glidePath.startNewSubPath(x,y); else glidePath.lineTo(x,y);
    }
    g.setColour(cyan.withAlpha(.18f)); g.strokePath(glidePath,juce::PathStrokeType(4.f));
    g.setColour(cyan); g.strokePath(glidePath,juce::PathStrokeType(1.8f));
    g.drawText("FLT",455,99,120,20,juce::Justification::centredLeft);
    g.drawText("ENV / ADSRC",455,442,160,20,juce::Justification::centredLeft);
    g.drawText("FX",703,99,120,20,juce::Justification::centredLeft);
    g.drawText("OUTPUT",703,566,120,20,juce::Justification::centredLeft);
    const juce::Rectangle<float> filterPlot(455,185,218,90), envelopePlot(455,500,218,50);
    for(const auto plot:{filterPlot,envelopePlot})
    {
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(plot,4);
        g.setColour(stageStyle::grid.withAlpha(.7f));
        g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
    }
    const auto envIndex=size_t(18+currentEnvelope*4);
    const auto timeScale=(currentEnvelope==1 ? .001f : 1.f)
        *(processor.parameters.getRawParameterValue("envTimesFive")->load()>=.5f ? 5.f : 1.f);
    const auto attack=std::min(25.f,float(sliders[envIndex].getValue())*timeScale);
    const auto decay=std::min(25.f,float(sliders[envIndex+1].getValue())*timeScale);
    const auto sustain=std::clamp(float(sliders[envIndex+2].getValue())*.01f,0.f,1.f);
    const auto release=std::min(25.f,float(sliders[envIndex+3].getValue())*timeScale);
    const auto envelopeTimeScale=currentEnvelope==1 ? 1.f : 5.f;
    const std::array<float,4> spans {attack*(currentEnvelope==1 ? 1.f : 6.9f),
                                      decay*envelopeTimeScale,.3f,release*envelopeTimeScale};
    float total=0;
    for(const auto span:spans) total+=span;
    const auto plotLeft=envelopePlot.getX()+5.f, plotWidth=envelopePlot.getWidth()-10.f;
    const auto levelY=[&envelopePlot](float level)
    { return envelopePlot.getBottom()-16.f-(envelopePlot.getHeight()-21.f)*level; };
    juce::Path envelopePath; envelopePath.startNewSubPath(plotLeft,levelY(0));
    auto boundary=plotLeft;
    const auto curve=std::clamp(float(curveSliders[size_t(currentEnvelope)].getValue())*.01f,-1.f,1.f);
    for(size_t stage=0;stage<4;++stage)
    {
        const auto width=plotWidth*spans[stage]/total;
        for(int point=1;point<=16;++point)
        {
            const auto t=float(point)/16.f;
            const auto slope=stage==0 ? 6.9f : 5.f;
            const auto base=(1.f-std::exp(-slope*t))/(1.f-std::exp(-slope));
            const auto shaped=std::pow(base,std::exp2(curve*2.f));
            const auto level=stage==0 ? shaped
                : stage==1 ? 1.f-(1.f-sustain)*shaped
                : stage==2 ? sustain : sustain*(1.f-shaped);
            envelopePath.lineTo(boundary+width*t,levelY(level));
        }
        boundary+=width;
        if(stage<3) { g.setColour(stageStyle::grid); g.drawVerticalLine(juce::roundToInt(boundary),envelopePlot.getY()+3,envelopePlot.getBottom()-16); }
    }
    juce::Path envelopeFill=envelopePath;
    envelopeFill.lineTo(envelopePlot.getRight()-5.f,levelY(0));
    envelopeFill.lineTo(plotLeft,levelY(0)); envelopeFill.closeSubPath();
    g.setColour(cyan.withAlpha(.15f)); g.fillPath(envelopeFill);
    g.setColour(cyan); g.strokePath(envelopePath,juce::PathStrokeType(1.7f));
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(8.f));
    g.drawText("0",459,536,28,10,juce::Justification::centredLeft);
    g.drawText(juce::String(total*.5f,1)+"s",536,536,56,10,juce::Justification::centred);
    g.drawText(juce::String(total,1)+"s",617,536,51,10,juce::Justification::centredRight);
    const auto filterMode=std::max(0,filterType.getSelectedId()-1);
    const auto baseCutoff=float(sliders[31].getValue());
    const auto resonance=std::clamp(float(sliders[32].getValue())*.01f,0.f,1.f);
    const auto keyFollow=std::clamp(float(sliders[33].getValue())*.01f,0.f,1.f);
    const auto envelopeAmount=std::clamp(float(sliders[34].getValue())*.01f,-1.f,1.f);
    const auto fltSustain=std::clamp(float(sliders[24].getValue())*.01f,0.f,1.f);
    // C5 makes key follow visible; the solid trace shows the held envelope,
    // while the faint trace shows its peak at the start of a note.
    const auto heldCutoff=std::clamp(baseCutoff*std::exp2(keyFollow+4.f*envelopeAmount*fltSustain),30.f,18000.f);
    const auto peakCutoff=std::clamp(baseCutoff*std::exp2(keyFollow+4.f*envelopeAmount),30.f,18000.f);
    const auto trace=[&](float cutoff)
    {
        juce::Path path;
        for(int point=0;point<=128;++point)
        {
            const auto proportion=float(point)/128.f;
            const auto hz=40.f*std::pow(450.f,proportion);
            const auto magnitude=filterMagnitude(filterMode,hz,cutoff,resonance);
            const auto db=20.f*std::log10(std::max(magnitude,1.e-5f));
            const auto x=filterPlot.getX()+3.f+(filterPlot.getWidth()-6.f)*proportion;
            const auto y=filterPlot.getY()+4.f+(filterPlot.getHeight()-8.f)*std::clamp((30.f-db)/78.f,0.f,1.f);
            if(point==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
        }
        return path;
    };
    if(liveCutoff>0)
    {
        g.setColour(cyan.withAlpha(.32f)); g.strokePath(trace(heldCutoff),juce::PathStrokeType(1.f));
        g.setColour(accent); g.strokePath(trace(liveCutoff),juce::PathStrokeType(1.8f));
    }
    else
    {
        if(std::abs(peakCutoff-heldCutoff)>1.f)
        { g.setColour(cyan.withAlpha(.42f)); g.strokePath(trace(peakCutoff),juce::PathStrokeType(1.f)); }
        g.setColour(accent); g.strokePath(trace(heldCutoff),juce::PathStrokeType(1.6f));
    }
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(8.f));
    for(const auto frequency:{100.f,1000.f,10000.f})
    {
        const auto x=filterPlot.getX()+3.f+(filterPlot.getWidth()-6.f)
            *std::log(frequency/40.f)/std::log(450.f);
        g.setColour(stageStyle::grid.withAlpha(.55f));
        g.drawVerticalLine(juce::roundToInt(x),filterPlot.getY()+12.f,filterPlot.getBottom()-13.f);
        g.setColour(stageStyle::secondary);
        g.drawText(frequency<1000 ? "100" : frequency<10000 ? "1k" : "10k",
                   juce::roundToInt(x)-15,262,30,11,juce::Justification::centred);
    }
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(9.f));
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(juce::Rectangle<float>(34,282,384,76),6);
    for(int line=1;line<4;++line) { g.setColour(stageStyle::grid); g.drawHorizontalLine(282+line*19,34,418); }
    const auto waveMix=currentOscillator==3 ? std::array<float,4>{1.f,0.f,0.f,0.f}
        : qin::waveWeights(waves[size_t(currentOscillator)].getPosition());
    juce::Path waveform;
    uint32_t noisePreviewState=0x87a3d21fu;
    float pinkPreview=0;
    for(int point=0;point<=380;++point)
    {
        const auto cycle=std::fmod(float(point)/380.f*3.f,1.f);
        noisePreviewState^=noisePreviewState<<13;
        noisePreviewState^=noisePreviewState>>17;
        noisePreviewState^=noisePreviewState<<5;
        const auto white=float(noisePreviewState>>8)*(1.f/8388607.5f)-1.f;
        pinkPreview=.9f*pinkPreview+.1f*white;
        const auto style=noiseStyle.getSelectedId();
        const auto noiseShape=style==2 ? pinkPreview*2.5f
            : style==3 ? ((noisePreviewState&31u)==0 ? white*2.f : 0.f) : white;
        const auto shape=currentOscillator==3 ? std::clamp(noiseShape,-1.f,1.f)
            : waveMix[0]*(2.f*cycle-1.f)
              +waveMix[1]*(cycle<.5f ? 1.f : -1.f)
              +waveMix[2]*(1.f-4.f*std::abs(cycle-.5f))
              +waveMix[3]*std::sin(2.f*juce::MathConstants<float>::pi*cycle);
        const auto x=36.f+point,y=320.f-26.f*shape;
        if(point==0) waveform.startNewSubPath(x,y); else waveform.lineTo(x,y);
    }
    g.setColour(cyan.withAlpha(.2f)); g.strokePath(waveform,juce::PathStrokeType(4));
    g.setColour(cyan); g.strokePath(waveform,juce::PathStrokeType(1.5f));
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(10.f));
    if(currentFxSection==0)
    {
        const juce::Rectangle<float> scope(707,392,182,153);
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(scope,5);
        g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(8.f));
        g.drawText("DRIVE WAVEFORM",714,396,105,12,juce::Justification::centredLeft);
        g.setColour(cyan); g.drawText("IN",818,396,20,12,juce::Justification::centred);
        g.setColour(accent); g.drawText("OUT",845,396,32,12,juce::Justification::centred);
        g.setColour(stageStyle::grid); g.drawHorizontalLine(469,712,884);
        const auto window=liveScopeValid ? qin::waveformWindow(liveScope.input)
            : qin::WaveformWindow{0.f,float(previewInput.size()-1),true};
        float peak=.2f;
        const auto includePeak=[&](const auto& samples)
        {
            for(size_t index=0;index<QinSynthProcessor::driveScopePoints;++index)
            {
                const auto fraction=float(index)/float(QinSynthProcessor::driveScopePoints-1);
                peak=std::max(peak,std::abs(qin::waveformSample(samples,window.start+fraction*window.length)));
            }
        };
        if(liveScopeValid) { includePeak(liveScope.input); includePeak(liveScope.output); }
        else { includePeak(previewInput); includePeak(previewOutput); }
        const auto gain=58.f/std::max(.2f,peak*1.2f);
        const auto drawWave=[&](const auto& samples,juce::Colour colour)
        {
            juce::Path path;
            for(size_t index=0;index<QinSynthProcessor::driveScopePoints;++index)
            {
                const auto fraction=float(index)/float(QinSynthProcessor::driveScopePoints-1);
                const auto x=713.f+170.f*fraction;
                const auto sample=qin::waveformSample(samples,window.start+fraction*window.length);
                const auto y=469.f-std::clamp(sample*gain,-58.f,58.f);
                if(index==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.3f));
        };
        if(liveScopeValid)
        { drawWave(liveScope.input,cyan.withAlpha(.65f)); drawWave(liveScope.output,accent); }
        else { drawWave(previewInput,cyan.withAlpha(.65f)); drawWave(previewOutput,accent); }
    }
    g.restoreState();
}
