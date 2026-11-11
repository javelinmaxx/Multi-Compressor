#include "PluginEditor.h"
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
    {"attack","ATTACK","Sets how quickly the amplitude envelope rises."},
    {"ampDecay","DECAY","Sets how quickly amplitude falls from its peak to sustain."},
    {"ampSustain","SUSTAIN","Sets amplitude while the key remains held."},
    {"release","RELEASE","Sets how quickly amplitude fades after note-off."},
    {"fltAttack","ATTACK","Sets the filter envelope rise from 1 to 5000 ms with logarithmic control."},
    {"fltDecay","DECAY","Sets the filter envelope decay from 1 to 5000 ms with logarithmic control."},
    {"fltSustain","SUSTAIN","Sets the held filter-envelope level with finer control near zero."},
    {"fltRelease","RELEASE","Sets the filter envelope release from 1 to 5000 ms with logarithmic control."},
    {"modAttack","ATTACK","Sets how quickly the modulation envelope rises."},
    {"modDecay","DECAY","Sets how quickly the modulation envelope falls to sustain."},
    {"modSustain","SUSTAIN","Sets the held level of effect modulation."},
    {"modRelease","RELEASE","Sets how quickly effect modulation fades after note-off."},
    {"modDepth","MOD DEPTH","Sets how strongly the modulation envelope affects its destination."},
    {"filterCutoff","FREQ","Sets the filter's base cutoff on a logarithmic frequency scale."},
    {"filterResonance","RESONANCE","Emphasizes the filter cutoff with logarithmic control resolution."},
    {"filterKeyFollow","KEY FOLLOW","Raises the cutoff with MIDI note pitch."},
    {"filterEnvAmount","ENV AMOUNT","Sets how far the filter envelope moves the cutoff."},
    {"keyPan","KEY PAN","Places low MIDI notes left and high notes right."},
    {"output","OUTPUT","Sets the final synth level."},
    {"drive","DRIVE","Blends in the op-amp clipping circuit."},
    {"time","TIME","Sets Lan's delay and reverb time."},
    {"decay","DECAY","Sets how long Lan's feedback tail lasts."},
    {"scatter","SCATTER","Morphs Lan's delay into a diffuse reverb."},
    {"verbMix","MIX","Blends the Lan effect with the synth."},
    {"tapMotion","TAP MOTION","Sets how far note onsets move Lan's feedback taps."},
    {"phaseMotion","PHASE MOTION","Sets how strongly note onsets change Lan's phase scatter."}
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
    for(size_t i=0;i<oscillatorTabs.size();++i)
    {
        auto& tab=oscillatorTabs[i]; styleTab(tab,"OSC "+juce::String(int(i)+1),2);
        tab.setTooltip("Opens oscillator "+juce::String(int(i)+1)+" in the OSC panel.");
        tab.onClick=[this,i]{showOscillator(int(i));}; addAndMakeVisible(tab);
    }
    noiseButton.setClickingTogglesState(true);
    noiseButton.setTooltip("Adds a noise oscillator to each active MIDI voice.");
    noiseButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
    noiseButton.setColour(juce::TextButton::buttonOnColourId,accent);
    addAndMakeVisible(noiseButton);
    noiseAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"noise",noiseButton);
    filterTimesFive.setTooltip("Multiplies the filter envelope's attack, decay, and release times by five, up to 5000 ms.");
    filterTimesFive.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29272c));
    filterTimesFive.setColour(juce::TextButton::textColourOffId,stageStyle::foreground);
    filterTimesFive.onClick=[this] { multiplyFilterTimes(); };
    addAndMakeVisible(filterTimesFive);
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
        tab.setTooltip(i==0 ? "Opens drive and output controls." : "Opens Lan's delay and reverb controls.");
        tab.onClick=[this,i]{showFxSection(int(i));}; addAndMakeVisible(tab);
    }
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
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,stageStyle::foreground);
        slider.setTooltip("Negative curve values act sooner, while positive values delay attack, decay, and release movement.");
        addAndMakeVisible(slider);
        curveAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,curveIds[i],slider);
        slider.onValueChange=[this]
        { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,433,240,312})); };
    }
    for(size_t i=18;i<=34;++i)
    {
        if(i==30) continue;
        auto& slider=sliders[i]; slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.onValueChange=[this,i]
        {
            const auto area=i<30 ? juce::Rectangle<int>(444,433,240,312) : juce::Rectangle<int>(444,90,240,335);
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,area));
            if(i==24) repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,90,240,335}));
        };
    }
    sliders[31].textFromValueFunction=[](double value)
    { return juce::String(juce::roundToInt(value)); };
    sliders[32].textFromValueFunction=[](double value)
    { return juce::String(value,1); };
    for(const auto index:{22,23,25})
    {
        sliders[size_t(index)].textFromValueFunction=[](double value)
        { return juce::String(juce::roundToInt(value))+"ms"; };
        sliders[size_t(index)].updateText();
    }
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
        wave.addItem("Saw",1); wave.addItem("Pulse",2); wave.addItem("Triangle",3);
        wave.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a));
        wave.setColour(juce::ComboBox::textColourId,stageStyle::foreground);
        wave.setTooltip("Chooses oscillator "+juce::String(int(i)+1)+"'s virtual-analog waveform.");
        label.setText("WAVE",juce::dontSendNotification); label.setFont(juce::FontOptions(9.f));
        label.setJustificationType(juce::Justification::centred); label.setColour(juce::Label::textColourId,stageStyle::secondary);
        addAndMakeVisible(wave); addAndMakeVisible(label);
        waveAttachments[i]=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"osc"+juce::String(int(i)+1)+"Wave",wave);
        wave.onChange=[this]{repaint();};
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
void QinSynthEditor::multiplyFilterTimes()
{
    for(const auto* id:{"fltAttack","fltDecay","fltRelease"})
    {
        auto* parameter=processor.parameters.getParameter(id);
        const auto current=parameter->convertFrom0to1(parameter->getValue());
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(std::min(5000.f,current*5.f)));
        parameter->endChangeGesture();
    }
}
void QinSynthEditor::updateDrivePreview()
{
    zao::OpAmpFeedbackClipper circuit;
    circuit.prepare(48000.0);
    const auto mix=std::clamp(float(sliders[37].getValue())*.01f,0.f,1.f);
    for(int sample=0;sample<384;++sample)
    {
        const auto phase=std::fmod(float(sample)/64.f,1.f);
        const auto original=.55f*(1.f-4.f*std::abs(phase-.5f));
        const auto driven=float(circuit.process(original*(1.f+5.f*mix)));
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
}
void QinSynthEditor::showOscillator(int index)
{ currentOscillator=juce::jlimit(0,2,index); for(size_t i=0;i<3;++i) oscillatorTabs[i].setToggleState(int(i)==currentOscillator,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::showEnvelope(int index)
{ currentEnvelope=juce::jlimit(0,2,index); for(size_t i=0;i<3;++i) envelopeTabs[i].setToggleState(int(i)==currentEnvelope,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::showFxSection(int index)
{ currentFxSection=juce::jlimit(0,1,index); for(size_t i=0;i<2;++i) fxTabs[i].setToggleState(int(i)==currentFxSection,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::updateVisibility()
{
    for(int osc=0;osc<3;++osc)
    {
        const auto selected=osc==currentOscillator; waves[size_t(osc)].setVisible(selected); waveLabels[size_t(osc)].setVisible(selected);
        for(int control=0;control<6;++control) { const auto i=size_t(osc*6+control); sliders[i].setVisible(selected); labels[i].setVisible(selected); }
    }
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control) { const auto i=size_t(18+env*4+control); sliders[i].setVisible(env==currentEnvelope); labels[i].setVisible(env==currentEnvelope); }
    for(int env=0;env<3;++env) curveSliders[size_t(env)].setVisible(env==currentEnvelope);
    filterTimesFive.setVisible(currentEnvelope==1);
    sliders[30].setVisible(currentEnvelope==2); labels[30].setVisible(currentEnvelope==2);
    modTarget.setVisible(currentEnvelope==2); modTargetLabel.setVisible(currentEnvelope==2);
    for(size_t i=36;i<sliders.size();++i)
    { const auto selected=i==36 || (currentFxSection==0 ? i==37 : i>=38); sliders[i].setVisible(selected); labels[i].setVisible(selected); }
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,29,360,38);
    for(int i=0;i<3;++i)
    {
        oscillatorTabs[size_t(i)].setBounds(31+i*109,125,103,27);
        envelopeTabs[size_t(i)].setBounds(455+i*73,466,68,27);
        waveLabels[size_t(i)].setBounds(34,170,118,15); waves[size_t(i)].setBounds(34,186,154,27);
        for(int control=0;control<6;++control)
        {
            const auto index=size_t(i*6+control); const auto x=35+(control%3)*132, y=463+(control/3)*102;
            labels[index].setBounds(x,y,103,15); sliders[index].setBounds(x,y+16,103,82);
        }
    }
    noiseButton.setBounds(363,125,55,27);
    filterTimesFive.setBounds(618,438,51,24);
    labels[35].setBounds(302,160,112,15); sliders[35].setBounds(302,176,112,82);
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(18+env*4+control); const auto y=570+control*26;
            labels[index].setBounds(454,y,52,25); sliders[index].setBounds(507,y,166,25);
        }
    curveLabel.setBounds(454,678,52,25);
    for(auto& slider:curveSliders) slider.setBounds(507,678,166,25);
    labels[30].setBounds(451,706,104,14); sliders[30].setBounds(451,720,104,23);
    modTargetLabel.setBounds(560,706,108,14); modTarget.setBounds(560,719,108,24);
    filterTypeLabel.setBounds(455,131,211,15); filterType.setBounds(454,148,215,28);
    for(int index=31;index<=34;++index)
    {
        const auto y=285+(index-31)*27;
        labels[size_t(index)].setBounds(454,y,68,25); sliders[size_t(index)].setBounds(523,y,150,25);
    }
    filterMergeLabel.setBounds(454,393,68,25);
    filterMergeSlider.setBounds(523,393,150,25);
    for(int i=0;i<2;++i) fxTabs[size_t(i)].setBounds(703+i*97,124,92,27);
    labels[36].setBounds(744,587,110,16); sliders[36].setBounds(744,604,110,83);
    labels[37].setBounds(738,245,116,16); sliders[37].setBounds(738,261,116,95);
    for(int i=38;i<44;++i)
    {
        const auto n=i-38,x=700+(n%2)*101,y=179+(n/2)*117;
        labels[size_t(i)].setBounds(x,y,97,15); sliders[size_t(i)].setBounds(x,y+15,97,90);
    }
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
    for(auto& tab:oscillatorTabs) tab.setTransform(scale);
    noiseButton.setTransform(scale);
    filterTimesFive.setTransform(scale);
    filterMergeSlider.setTransform(scale); filterMergeLabel.setTransform(scale);
    for(auto& tab:envelopeTabs) tab.setTransform(scale);
    for(auto& tab:fxTabs) tab.setTransform(scale);
    for(auto& slider:sliders) slider.setTransform(scale);
    for(auto& slider:curveSliders) slider.setTransform(scale);
    curveLabel.setTransform(scale);
    for(auto& label:labels) label.setTransform(scale);
    for(auto& wave:waves) wave.setTransform(scale);
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
    g.drawText("16 VOICES  /  3 OSC  /  8x UNI",661,36,231,23,juce::Justification::centred);
    const auto panel=juce::Colour(0xff27272c);
    stageStyle::drawPanel(g,{16,90,420,655},panel);
    stageStyle::drawPanel(g,{444,90,240,335},panel);
    stageStyle::drawPanel(g,{444,433,240,312},panel);
    stageStyle::drawPanel(g,{692,90,212,655},panel);
    g.setColour(accent); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText("OSC",30,99,120,20,juce::Justification::centredLeft);
    g.drawText("FLT",455,99,120,20,juce::Justification::centredLeft);
    g.drawText("ENV",455,442,120,20,juce::Justification::centredLeft);
    g.drawText("FX",703,99,120,20,juce::Justification::centredLeft);
    const juce::Rectangle<float> filterPlot(455,185,218,90), envelopePlot(455,500,218,64);
    for(const auto plot:{filterPlot,envelopePlot})
    {
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(plot,4);
        g.setColour(stageStyle::grid.withAlpha(.7f));
        g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
    }
    const auto envIndex=size_t(18+currentEnvelope*4);
    const auto timeScale=currentEnvelope==1 ? .001f : 1.f;
    const auto attack=float(sliders[envIndex].getValue())*timeScale;
    const auto decay=float(sliders[envIndex+1].getValue())*timeScale;
    const auto sustain=std::clamp(float(sliders[envIndex+2].getValue())*.01f,0.f,1.f);
    const auto release=float(sliders[envIndex+3].getValue())*timeScale;
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
    g.drawText("ADSR  /  TIME",461,502,165,10,juce::Justification::centredLeft);
    g.drawText("0",459,550,28,10,juce::Justification::centredLeft);
    g.drawText(juce::String(total*.5f,1)+"s",536,550,56,10,juce::Justification::centred);
    g.drawText(juce::String(total,1)+"s",617,550,51,10,juce::Justification::centredRight);
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
    g.drawText(liveCutoff>0 ? "LIVE / FILTER" : "C5  /  HELD",461,186,90,10,juce::Justification::centredLeft);
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
    g.drawText("OSCILLATOR "+juce::String(currentOscillator+1)+" / WAVEFORM",34,276,322,16,juce::Justification::centredLeft);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(juce::Rectangle<float>(34,295,384,147),6);
    for(int line=1;line<4;++line) { g.setColour(stageStyle::grid); g.drawHorizontalLine(295+line*36,34,418); }
    const auto wave=waves[size_t(currentOscillator)].getSelectedId();
    juce::Path waveform;
    for(int point=0;point<=380;++point)
    {
        const auto cycle=std::fmod(point/125.f,1.f);
        const auto shape=wave==2 ? (cycle<.5f ? 1.f : -1.f)
            : wave==3 ? (1.f-4.f*std::abs(cycle-.5f)) : 2.f*cycle-1.f;
        const auto x=36.f+point,y=368.f-50.f*shape;
        if(point==0) waveform.startNewSubPath(x,y); else waveform.lineTo(x,y);
    }
    g.setColour(cyan.withAlpha(.2f)); g.strokePath(waveform,juce::PathStrokeType(4));
    g.setColour(cyan); g.strokePath(waveform,juce::PathStrokeType(1.5f));
    g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(10.f));
    g.drawText(currentFxSection==0 ? "DRIVE / SATURATION" : "LAN / DELAY-VERB",704,160,185,15,juce::Justification::centredLeft);
    if(currentFxSection==0)
    {
        const juce::Rectangle<float> scope(707,392,182,153);
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(scope,5);
        g.setColour(stageStyle::secondary); g.setFont(juce::FontOptions(8.f));
        g.drawText("DRIVE WAVEFORM",714,396,105,12,juce::Justification::centredLeft);
        g.setColour(cyan); g.drawText("IN",818,396,20,12,juce::Justification::centred);
        g.setColour(accent); g.drawText("OUT",845,396,32,12,juce::Justification::centred);
        g.setColour(stageStyle::grid); g.drawHorizontalLine(469,712,884);
        const auto& input=liveScopeValid ? liveScope.input : previewInput;
        const auto& output=liveScopeValid ? liveScope.output : previewOutput;
        float peak=.2f;
        for(size_t index=0;index<input.size();++index)
            peak=std::max({peak,std::abs(input[index]),std::abs(output[index])});
        const auto gain=58.f/std::max(.2f,peak*1.2f);
        const auto drawWave=[&](const auto& samples,juce::Colour colour)
        {
            juce::Path path;
            for(size_t index=0;index<samples.size();++index)
            {
                const auto x=713.f+170.f*float(index)/float(samples.size()-1);
                const auto y=469.f-std::clamp(samples[index]*gain,-58.f,58.f);
                if(index==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.3f));
        };
        drawWave(input,cyan.withAlpha(.65f)); drawWave(output,accent);
    }
    g.restoreState();
}
