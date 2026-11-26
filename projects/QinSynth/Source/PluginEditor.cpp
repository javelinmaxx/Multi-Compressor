#include "PluginEditor.h"
#include "LegatoCurve.h"
#include "WaveformSync.h"
#include "UnisonDetune.h"
#include "../../EditorScaling.h"
#include "../../PanelSurface.h"
#include "../../AnalogToggleHardware.h"
#include <cmath>
#include <complex>

namespace
{
const auto panelRed=juce::Colour(0xff4a1d29);
const auto panelInk=juce::Colour(0xffe3e9ec);
const auto buttonInk=juce::Colour(0xff192b3b);
const auto keyInk=juce::Colour(0xff192b3b);
const auto keyRootRed=juce::Colour(0xff82313d);
const auto backgroundBlack=juce::Colour(0xff111215);
const auto secondaryRed=juce::Colour(0xffe99daa);
const auto plotRed=juce::Colour(0xfff3aab0);
const auto curveBlue=juce::Colour(0xff87c9ed);
const auto accent=juce::Colour(0xffbd4e60);
const auto postFilterAccent=juce::Colour(0xffdc7180);
const auto highlightRed=plotRed;
const auto knobWhite=juce::Colour(0xffe8edf0);
const auto buttonIdle=knobWhite;
const auto buttonActive=juce::Colour(0xffe8a4af);
struct Control { const char* id; const char* name; const char* hint; };
constexpr std::array<Control,44> controls {{
    {"osc1Level","LEVEL","Sets oscillator one's contribution to the mix."},
    {"osc1Fine","FINE","Offsets oscillator one by up to 700 cents in either direction."},
    {"osc1Unison","UNISON","Sets the number of oscillator one copies."},
    {"osc1Detune","DETUNE","Offsets oscillator one's unison copies in cents."},
    {"osc1Spread","SPREAD","Sets how far oscillator one's detuned copies spread across stereo."},
    {"osc1Pan","PAN","Positions oscillator one as a whole in stereo."},
    {"osc2Level","LEVEL","Sets oscillator two's contribution to the mix."},
    {"osc2Fine","FINE","Offsets oscillator two by up to 700 cents in either direction."},
    {"osc2Unison","UNISON","Sets the number of oscillator two copies."},
    {"osc2Detune","DETUNE","Offsets oscillator two's unison copies in cents."},
    {"osc2Spread","SPREAD","Sets how far oscillator two's detuned copies spread across stereo."},
    {"osc2Pan","PAN","Positions oscillator two as a whole in stereo."},
    {"osc3Level","LEVEL","Sets oscillator three's contribution to the mix."},
    {"osc3Fine","FINE","Offsets oscillator three by up to 700 cents in either direction."},
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
    {"modDepth","MOD DEPTH","Sets the modulation amount, with negative values moving the destination downward."},
    {"filterCutoff","FREQ","Sets the filter's base cutoff on a logarithmic frequency scale."},
    {"filterResonance","RESONANCE","Emphasizes the cutoff while a blue marker shows the MOD envelope's effective resonance."},
    {"filterKeyFollow","KEY FOLLOW","Raises the cutoff with MIDI note pitch."},
    {"filterEnvAmount","ENV AMOUNT","Moves the filter cutoff by up to thirty octaves in either direction."},
    {"keyPan","KEY PAN","Sets pitch panning from high notes left at -100% to high notes right at +100%."},
    {"output","OUTPUT","Sets the final synth level."},
    {"drive","DRIVE","Sets how hard the oscillator signal drives the selected circuit."},
    {"time","TIME","Sets Lan's delay and reverb time."},
    {"decay","DECAY","Sets how long Lan's feedback tail lasts."},
    {"scatter","SCATTER","Morphs Lan's delay into a diffuse reverb."},
    {"verbMix","MIX","Blends the Lan effect with the synth."},
    {"tapMotion","TAP MOTION","Sets how far note onsets move Lan's feedback taps."},
    {"phaseMotion","PHASE MOTION","Sets how strongly note onsets change Lan's phase scatter."}
}};
constexpr std::array<Control,4> noiseControls {{
    {"noiseLevel","LEVEL","Sets the noise oscillator level, shown in dB from silence to 0 dB."},
    {"noiseTune","TUNE","Shifts the note-tracked noise resonance by up to two octaves in semitones."},
    {"noiseHpf","HPF","Removes noise below the selected high-pass frequency."},
    {"noisePan","PAN","Positions the noise oscillator independently in stereo."}
}};
juce::String gainDecibels(double percent)
{
    return percent<=0.0 ? "-INF dB" : juce::String(20.0*std::log10(percent*.01),1)+" dB";
}
void styleTab(juce::TextButton& button,const juce::String& title,int group)
{
    button.setButtonText(title); button.setClickingTogglesState(true); button.setRadioGroupId(group);
    button.setColour(juce::TextButton::buttonColourId,buttonIdle);
    button.setColour(juce::TextButton::buttonOnColourId,buttonActive);
    button.setColour(juce::TextButton::textColourOffId,buttonInk);
    button.setColour(juce::TextButton::textColourOnId,buttonInk);
}
void setCompactButtonBounds(juce::Button& button,int x,int y,int width,int height)
{
    const auto compactWidth=juce::roundToInt(width*.8f);
    const auto compactHeight=juce::roundToInt(height*.8f);
    button.setBounds(x+(width-compactWidth)/2,y+(height-compactHeight)/2,
                     compactWidth,compactHeight);
}
float filterMagnitude(int type,float frequency,float cutoff,float resonance)
{
    if(type==0) return 1.f;
    const auto openBlend=std::clamp((cutoff-13500.f)/4500.f,0.f,1.f);
    const auto ratio=frequency/std::max(30.f,cutoff);
    if(type==1)
    {
        const std::complex<float> pole=1.f/(std::complex<float>(1.f,ratio));
        const auto fourth=pole*pole*pole*pole;
        const auto filtered=std::abs(fourth*(1.f+3.5f*resonance)/(1.f+3.5f*resonance*fourth));
        return filtered+(1.f-filtered)*openBlend;
    }
    const auto k=type==2 ? 2.f-1.7f*resonance : std::clamp(1.4f-1.1f*resonance,.3f,1.4f);
    const auto second=1.f/std::sqrt(std::pow(1.f-ratio*ratio,2.f)+k*k*ratio*ratio);
    if(type==2)
    {
        const auto filtered=second/std::sqrt(1.f+ratio*ratio);
        return filtered+(1.f-filtered)*openBlend;
    }
    if(type==4)
    {
        const auto secondDamping=1.4f-.55f*resonance;
        const auto final=1.f/std::sqrt(std::pow(1.f-ratio*ratio,2.f)
                                       +secondDamping*secondDamping*ratio*ratio);
        const auto filtered=second*final;
        return filtered+(1.f-filtered)*openBlend;
    }
    return second+(1.f-second)*openBlend;
}
}
void QinTuningPanel::paint(juce::Graphics& g)
{
    g.fillAll(backgroundBlack);
    g.setColour(panelRed);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2),9);
    g.setColour(secondaryRed.withAlpha(.7f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2),9,1.2f);
    g.setColour(panelInk); g.setFont(juce::FontOptions(16.f).withStyle("bold"));
    g.drawText("TUNE",20,16,100,25,juce::Justification::centredLeft);
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(10.f));
    g.drawText("Drag keys to tune; changing ROOT transposes the pattern and locks it to 0 cents.",20,45,getWidth()-40,20,
               juce::Justification::centredLeft);
}
QinTuningKey::QinTuningKey()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setRange(-100,100,.1);
    setMouseDragSensitivity(190);
    setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
}
void QinTuningKey::setIdentity(juce::String note,bool black,bool root)
{
    noteName=std::move(note); blackKey=black; rootKey=root;
    setEnabled(!rootKey);
    setTooltip(rootKey ? "The selected root is locked to zero cents."
        : "Drag this piano key to adjust its pitch deviation in cents.");
    repaint();
}
void QinTuningKey::paint(juce::Graphics& g)
{
    const auto area=getLocalBounds().toFloat().reduced(1.f);
    const auto text=blackKey ? stageStyle::foreground : keyInk;
    g.setColour(juce::Colours::black.withAlpha(.45f));
    g.fillRoundedRectangle(area.translated(1.f,2.f),blackKey ? 4.f : 5.f);
    g.setGradientFill(juce::ColourGradient(
        blackKey ? juce::Colour(0xff526170) : juce::Colour(0xfff0f4f6),area.getTopLeft(),
        blackKey ? juce::Colour(0xff18232e) : juce::Colour(0xffa5b3bf),area.getBottomLeft(),false));
    g.fillRoundedRectangle(area,blackKey ? 4.f : 5.f);
    g.setColour(juce::Colours::white.withAlpha(blackKey ? .18f : .72f));
    g.drawHorizontalLine(juce::roundToInt(area.getY()+2.f),area.getX()+4.f,area.getRight()-4.f);
    g.drawVerticalLine(juce::roundToInt(area.getX()+2.f),area.getY()+5.f,area.getBottom()-5.f);
    g.setColour(juce::Colour(0xff465766).withAlpha(blackKey ? .55f : .35f));
    g.drawVerticalLine(juce::roundToInt(area.getRight()-2.f),area.getY()+5.f,area.getBottom()-5.f);
    g.setColour(rootKey ? accent : juce::Colour(0xff5d7081));
    g.drawRoundedRectangle(area,blackKey ? 4.f : 5.f,rootKey ? 2.f : 1.f);
    if(rootKey)
    {
        g.setColour(accent.withAlpha(.12f));
        g.fillRoundedRectangle(area.reduced(3.f),3.f);
    }
    const auto cents=rootKey ? 0.0 : getValue();
    g.setColour(rootKey ? (blackKey ? plotRed : keyRootRed) : text);
    g.setFont(juce::FontOptions(blackKey ? 10.f : 13.f).withStyle("bold"));
    g.drawText(noteName,3,getHeight()-(blackKey ? 51 : 75),getWidth()-6,18,juce::Justification::centred);
    g.setFont(juce::FontOptions(blackKey ? 8.f : 10.f));
    g.drawText((cents>0 ? "+" : "")+juce::String(cents,1),2,
        getHeight()-(blackKey ? 31 : 51),getWidth()-4,17,juce::Justification::centred);
    if(rootKey)
    {
        g.setFont(juce::FontOptions(blackKey ? 7.f : 9.f).withStyle("bold"));
        g.drawText("ROOT",2,getHeight()-(blackKey ? 15 : 30),getWidth()-4,14,juce::Justification::centred);
    }
}
void QinResonanceIndicator::setPosition(float normalized,bool enabled)
{
    normalized=std::clamp(normalized,0.f,1.f);
    if(active==enabled && std::abs(position-normalized)<.001f) return;
    active=enabled; position=normalized; repaint();
}
void QinResonanceIndicator::paint(juce::Graphics& g)
{
    if(!active) return;
    const auto x=6.f+position*std::max(1.f,float(getWidth()-58));
    const auto y=float(getHeight())*.5f;
    g.setColour(secondaryRed.withAlpha(.3f)); g.fillEllipse(x-6.f,y-6.f,12.f,12.f);
    g.setColour(secondaryRed); g.fillEllipse(x-2.5f,y-2.5f,5.f,5.f);
}
void QinModulationOverlay::setDisplay(juce::Rectangle<int> area,float x,float y,Shape nextShape,bool enabled)
{
    x=std::clamp(x,0.f,1.f); y=std::clamp(y,0.f,1.f);
    if(!enabled || area.isEmpty()) { if(active) { active=false; setVisible(false); } return; }
    const auto moved=getBounds()!=area || !active || shape!=nextShape
        || std::abs(positionX-x)>.001f || std::abs(positionY-y)>.001f;
    active=true; positionX=x; positionY=y; shape=nextShape;
    if(getBounds()!=area) setBounds(area);
    setVisible(true);
    if(moved) repaint();
}
void QinModulationOverlay::paint(juce::Graphics& g)
{
    if(!active) return;
    const auto w=float(getWidth()), h=float(getHeight());
    g.setColour(highlightRed.withAlpha(.22f));
    g.drawRoundedRectangle(juce::Rectangle<float>(1.f,1.f,w-2.f,h-2.f),4.f,1.4f);
    float x=w*.5f,y=h*.5f;
    switch(shape)
    {
        case Shape::horizontal: x=6.f+positionX*std::max(1.f,w-58.f); break;
        case Shape::vertical: y=6.f+(1.f-positionX)*std::max(1.f,h-28.f); break;
        case Shape::readout: x=5.f+positionX*std::max(1.f,w-10.f); y=h-4.f; break;
        case Shape::pad: x=5.f+positionX*std::max(1.f,w-10.f);
            y=5.f+(1.f-positionY)*std::max(1.f,h-10.f); break;
        case Shape::key: x=5.f+positionX*std::max(1.f,w-10.f); y=h*.48f; break;
        case Shape::rotary:
        {
            const auto angle=-2.35f+positionX*4.7f;
            const auto radius=std::min(w,h)*.29f;
            x=w*.5f+radius*std::sin(angle);
            y=h*.41f-radius*std::cos(angle);
            break;
        }
    }
    g.setColour(highlightRed.withAlpha(.19f)); g.fillEllipse(x-7.f,y-7.f,14.f,14.f);
    g.setColour(highlightRed); g.fillEllipse(x-3.f,y-3.f,6.f,6.f);
}
QinVoiceSlider::QinVoiceSlider()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setRange(1,16,1);
    setMouseDragSensitivity(140);
    setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    setTooltip("Drag up or down to set the maximum number of simultaneous notes from one to sixteen.");
}
juce::String QinVoiceSlider::getReadoutText() const
{
    if(prefix=="LEVEL") return prefix+" "+gainDecibels(getValue());
    if(prefix=="RETRIG") return prefix+(getValue()>=.5 ? " ON" : " OFF");
    auto number=decimalPlaces>0 ? juce::String(getValue(),decimalPlaces)
        : juce::String(juce::roundToInt(getValue()));
    if(showPositiveSign && getValue()>0) number="+"+number;
    return (prefix.isEmpty() ? juce::String() : prefix+" ")+number
        +((suffix=="c" || suffix=="%") ? suffix : " "+suffix);
}
void QinVoiceSlider::paint(juce::Graphics& g)
{
    const auto area=getLocalBounds().toFloat().reduced(1);
    g.setColour(stageStyle::grid);
    g.drawRoundedRectangle(area,4,1);
    g.setColour(prefix=="RETRIG" ? (getValue()>=.5 ? buttonActive : buttonIdle)
                              : suffix=="VOICES" ? plotRed : secondaryRed);
    g.setFont(juce::FontOptions(prefix.isEmpty() ? 12.f : prefix=="LEVEL" ? 11.f : 10.f).withStyle("bold"));
    g.drawText(getReadoutText(),getLocalBounds(),juce::Justification::centred);
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
    g.setColour(highlightRed.withAlpha(.35f)); g.drawLine(square.getCentreX(),square.getCentreY(),x,y,1.f);
    g.setColour(accent.withAlpha(.3f)); g.fillEllipse(x-9.f,y-9.f,18.f,18.f);
    g.setColour(juce::Colour(0xfff4f0e6)); g.fillEllipse(x-5.f,y-5.f,10.f,10.f);
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(9.f));
    g.drawText("TRI",0,0,37,13,juce::Justification::centredLeft);
    g.drawText("SINE",getWidth()-39,0,39,13,juce::Justification::centredRight);
    g.drawText("SAW",0,juce::roundToInt(bottom)+2,37,13,juce::Justification::centredLeft);
    g.drawText("PULSE",getWidth()-47,juce::roundToInt(bottom)+2,47,13,juce::Justification::centredRight);
}
QinPhaseStartLine::QinPhaseStartLine()
{
    setName("PHASE START"); setTitle("Oscillator retrigger phase");
    setTooltip("Drag the phase line to choose where this oscillator starts when Retrig is enabled.");
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void QinPhaseStartLine::setPhase(float next)
{
    next=std::clamp(next,0.f,1.f);
    if(std::abs(next-phase)<.0005f) return;
    phase=next; repaint();
}
void QinPhaseStartLine::mouseDown(const juce::MouseEvent& event)
{
    if(!event.mods.isLeftButtonDown()) return;
    dragging=true;
    if(onDragStart) onDragStart();
    mouseDrag(event);
}
void QinPhaseStartLine::mouseDrag(const juce::MouseEvent& event)
{
    if(!dragging) return;
    const auto next=std::clamp((event.position.x-2.f)/float(getWidth()-4),0.f,1.f);
    if(std::abs(next-phase)<.0005f) return;
    setPhase(next);
    if(onPhaseChange) onPhaseChange(phase);
}
void QinPhaseStartLine::mouseUp(const juce::MouseEvent&)
{
    if(!dragging) return;
    dragging=false;
    if(onDragEnd) onDragEnd();
}
void QinPhaseStartLine::paint(juce::Graphics& g)
{
    const auto x=2.f+(getWidth()-4.f)*phase;
    g.setColour(accent.withAlpha(.22f)); g.drawVerticalLine(juce::roundToInt(x),3.f,float(getHeight()-3));
    g.setColour(accent); g.fillRoundedRectangle(x-1.f,3.f,2.f,float(getHeight()-6),1.f);
    juce::Path handle;
    handle.startNewSubPath(x-5.f,2.f); handle.lineTo(x+5.f,2.f);
    handle.lineTo(x,9.f); handle.closeSubPath();
    g.fillPath(handle);
}
QinAnalogSwitch::QinAnalogSwitch(const juce::String& caption) : ToggleButton(caption)
{
    setTooltip(caption=="RETRIG"
        ? "Restarts this oscillator's waveform phase at each note onset when enabled."
        : caption=="TRANSFORMER" ? "Adds Ren's transformer to the output and saves CPU when off."
        : caption=="*5" ? "Multiplies Attack, Decay, and Release of all three envelopes by five while enabled, up to 25000 ms."
        : "Enables voice matching and pitch slides when polyphony exceeds one voice.");
}
void QinAnalogSwitch::advance(float seconds)
{
    const auto target=getToggleState() ? 1.f : 0.f;
    if(std::abs(target-position)<.001f) return;
    position+= (target-position)*(1.f-std::exp(-seconds/.055f));
    if(std::abs(target-position)<.001f) position=target;
    repaint();
}
void QinAnalogSwitch::paintButton(juce::Graphics& g,bool highlighted,bool down)
{
    const auto compact=getHeight()<62;
    const auto headerless=getButtonText()=="*5";
    const auto cx=float(getWidth())*.5f;
    const auto titleHeight=headerless ? 0.f : compact ? 11.f : 13.f;
    const auto pivotY=titleHeight+(float(getHeight())-titleHeight)*.5f;
    const auto nutRadius=compact ? 11.f : 14.f;
    const auto flangeRadius=compact ? 8.5f : 11.f;
    g.setColour(getToggleState() ? buttonActive : buttonIdle);
    g.setFont(juce::FontOptions(compact ? 8.f : 9.f).withStyle("bold"));
    if(!headerless)
        g.drawFittedText(getButtonText(),1,0,getWidth()-2,int(titleHeight),juce::Justification::centred,1);
    const auto onCaption=getButtonText()=="*5" ? "5X" : "ON";
    const auto offCaption=getButtonText()=="*5" ? "1X" : "OFF";
    g.setFont(juce::FontOptions(7.f).withStyle("bold"));
    g.setColour(getToggleState() ? buttonActive : buttonIdle.withAlpha(.6f));
    g.drawText(onCaption,0,int(titleHeight)+1,getWidth(),9,juce::Justification::centred);
    g.setColour(getToggleState() ? buttonActive.withAlpha(.55f) : buttonIdle);
    g.drawText(offCaption,0,getHeight()-10,getWidth(),9,juce::Justification::centred);
    const auto tipY=pivotY+(.5f-position)*(compact ? 12.f : 17.f);
    const auto flange=analogToggleHardware::draw(g,cx,pivotY,tipY,nutRadius,flangeRadius,4.5f,
                                                 buttonActive,buttonIdle,stageStyle::inset,
                                                 getToggleState(),highlighted,down);
    if(hasKeyboardFocus(true)) { g.setColour(accent); g.drawEllipse(flange.expanded(3.f),1.f); }
}
void QinDualFilterSlider::setRingValues(float pre,float post)
{
    pre=std::clamp(pre,0.f,1.f); post=std::clamp(post,0.f,1.f);
    if(std::abs(pre-preRing)<.0001f && std::abs(post-postRing)<.0001f) return;
    preRing=pre; postRing=post; repaint();
}
void QinDualFilterSlider::mouseDown(const juce::MouseEvent& event)
{
    clicked=event.mods.isLeftButtonDown() && event.getNumberOfClicks()==1;
    dragged=false;
    juce::Slider::mouseDown(event);
}
void QinDualFilterSlider::mouseDrag(const juce::MouseEvent& event)
{
    dragged|=event.getDistanceFromDragStart()>3;
    juce::Slider::mouseDrag(event);
}
void QinDualFilterSlider::mouseUp(const juce::MouseEvent& event)
{
    const auto toggle=clicked && !dragged && event.getDistanceFromDragStart()<=3;
    juce::Slider::mouseUp(event);
    clicked=false;
    if(toggle && onClick) onClick();
}
void QinLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
    float value,float start,float end,juce::Slider& slider)
{
    auto* dual=dynamic_cast<QinDualFilterSlider*>(&slider);
    if(dual==nullptr)
    { StageLookAndFeel::drawRotarySlider(g,x,y,width,height,value,start,end,slider); return; }
    const auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(5.f);
    const auto centre=bounds.getCentre();
    const auto radius=.5f*std::min(bounds.getWidth(),bounds.getHeight())-1.f;
    const auto ring=[&](float r,float amount,juce::Colour colour,bool selected)
    {
        const auto arc=[&](float stop)
        {
            juce::Path path;
            constexpr int steps=48;
            for(int step=0;step<=steps;++step)
            {
                const auto angle=start+(stop-start)*float(step)/steps;
                const auto px=centre.x+std::sin(angle)*r;
                const auto py=centre.y-std::cos(angle)*r;
                if(step==0) path.startNewSubPath(px,py);
                else path.lineTo(px,py);
            }
            return path;
        };
        const auto track=arc(end),active=arc(start+amount*(end-start));
        g.setColour(trackColour.brighter(.3f)); g.strokePath(track,juce::PathStrokeType(2.f));
        g.setColour(colour.withAlpha(selected ? 1.f : .52f));
        g.strokePath(active,juce::PathStrokeType(selected ? 2.8f : 2.2f));
    };
    ring(radius,dual->getPostRing(),postFilterAccent,dual->isPostSelected());
    ring(radius-7.f,dual->getPreRing(),accentColour,!dual->isPostSelected());
    const auto dial=juce::Rectangle<float>((radius-12.f)*2.f,(radius-12.f)*2.f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(.32f)); g.fillEllipse(dial.translated(0.f,2.f));
    const auto angle=start+value*(end-start);
    panelSurface::drawMetalKnobFace(g,dial,dialTopColour,dialBottomColour,markColour,angle);
    const auto tip=radius-13.f;
    g.setColour(dual->isPostSelected() ? postFilterAccent : accentColour);
    g.fillEllipse(centre.x+std::sin(angle)*tip-2.f,
                  centre.y-std::cos(angle)*tip-2.f,4.f,4.f);
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
    : AudioProcessorEditor(&synth), processor(synth),presets(synth)
{
    setOpaque(true); look.accentColour=accent; look.markColour=panelInk;
    look.trackColour=juce::Colour(0xff6f7c89);
    look.dialTopColour=knobWhite;
    look.dialBottomColour=juce::Colour(0xff8d9aa7); setLookAndFeel(&look);
    addAndMakeVisible(titleHint);
    for(auto* button:{&presetMenuButton,&presetPreviousButton,&presetNextButton})
    {
        button->setColour(juce::TextButton::buttonColourId,buttonIdle);
        button->setColour(juce::TextButton::textColourOffId,buttonInk);
        addAndMakeVisible(*button);
    }
    presetMenuButton.setTooltip("Opens factory and saved presets, with commands to save or delete your own presets.");
    presetPreviousButton.setTooltip("Loads the previous preset in the list.");
    presetNextButton.setTooltip("Loads the next preset in the list.");
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setFont(juce::FontOptions(12.f).withStyle("bold"));
    presetNameLabel.setColour(juce::Label::backgroundColourId,stageStyle::inset);
    presetNameLabel.setColour(juce::Label::textColourId,curveBlue);
    presetNameLabel.setColour(juce::Label::outlineColourId,stageStyle::grid);
    presetNameLabel.setTooltip("Shows the current preset name saved with this project.");
    addAndMakeVisible(presetNameLabel);
    presetMenuButton.onClick=[this] { showPresetMenu(); };
    presetPreviousButton.onClick=[this] { stepPreset(-1); };
    presetNextButton.onClick=[this] { stepPreset(1); };
    refreshPresetName();
    addAndMakeVisible(voiceCount);
    voiceCountAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"voiceCount",voiceCount);
    for(size_t i=0;i<octaveDisplays.size();++i)
    {
        auto& level=levelDisplays[i];
        level.setRange(0,100,.1);
        level.setSuffix("dB");
        level.setReadoutFormat("LEVEL",1,false);
        level.setMouseDragSensitivity(180);
        level.setDoubleClickReturnValue(true,i==0 ? 80.0 : 0.0);
        level.setTooltip("Drag to set this oscillator's level from silence to 0 dB; double-click to reset.");
        level.setComponentID("osc"+juce::String(int(i)+1)+"Level");
        addAndMakeVisible(level);
        levelAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"Level",level);
        auto& display=octaveDisplays[i];
        display.setRange(-2,2,1);
        display.setSuffix("OCT");
        display.setTooltip("Drag up or down to transpose this oscillator by whole octaves.");
        addAndMakeVisible(display);
        octaveAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"Octave",display);
        auto& fine=fineDisplays[i];
        fine.setRange(-700,700,.1);
        fine.setSuffix("c");
        fine.setReadoutFormat("FINE",1,true);
        fine.setMouseDragSensitivity(1400);
        fine.setDoubleClickReturnValue(true,0);
        fine.setTooltip("Drag slowly to shift this oscillator by up to 700 cents in either direction; double-click to reset.");
        fine.setComponentID("osc"+juce::String(int(i)+1)+"Fine");
        addAndMakeVisible(fine);
        fineAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"Fine",fine);
    }
    tuneButton.setClickingTogglesState(true);
    tuneButton.setColour(juce::TextButton::buttonColourId,buttonIdle);
    tuneButton.setColour(juce::TextButton::buttonOnColourId,buttonActive);
    tuneButton.setColour(juce::TextButton::textColourOffId,buttonInk);
    tuneButton.setColour(juce::TextButton::textColourOnId,buttonInk);
    tuneButton.setTooltip("Opens the custom tuning table and A4 reference controls.");
    addAndMakeVisible(tuneButton);
    tuneButton.onClick=[this]
    {
        tuningPanel.setVisible(tuneButton.getToggleState());
        if(tuningPanel.isVisible()) tuningPanel.toFront(false);
    };
    activeVoicesLabel.setText("0 ACTIVE",juce::dontSendNotification);
    activeVoicesLabel.setFont(juce::FontOptions(11.f).withStyle("bold"));
    activeVoicesLabel.setJustificationType(juce::Justification::centred);
    activeVoicesLabel.setColour(juce::Label::textColourId,highlightRed);
    activeVoicesLabel.setTooltip("Shows the number of allocated voices, including release tails.");
    addAndMakeVisible(activeVoicesLabel);
    addAndMakeVisible(polyLegatoButton);
    polyLegatoAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"polyLegato",polyLegatoButton);
    polyLegatoButton.onClick=[this] { updatePlayAvailability(); };
    legatoRule.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    legatoRule.setRange(0,3,1);
    legatoRule.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
    legatoRule.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    legatoRule.setColour(juce::Slider::textBoxTextColourId,panelInk);
    legatoRule.setTooltip("Closest and Farthest use pitch distance, while Upwards and Downwards prefer voices below or above the new note.");
    legatoRuleLabel.setText("VOICE MATCH",juce::dontSendNotification);
    legatoRuleLabel.setTooltip(legatoRule.getTooltip());
    for(auto* label:{&legatoRuleLabel,&slideTimeLabel,&slideShapeLabel})
    {
        label->setFont(juce::FontOptions(9.f));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(*label);
    }
    addAndMakeVisible(legatoRule);
    legatoRuleAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"legatoMode",legatoRule);
    legatoRule.textFromValueFunction=[](double value)
    {
        constexpr std::array<const char*,4> names {"Closest","Farthest","Upwards","Downwards"};
        return juce::String(names[size_t(std::clamp(juce::roundToInt(value),0,3))]);
    };
    legatoRule.setTextBoxIsEditable(false);
    legatoRule.updateText();
    slideTimeLabel.setText("SLIDE",juce::dontSendNotification);
    slideShapeLabel.setText("SHAPE",juce::dontSendNotification);
    for(auto* slider:{&slideTime,&slideShape})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,17);
        slider->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider->setColour(juce::Slider::textBoxTextColourId,panelInk);
        addAndMakeVisible(*slider);
    }
    slideTime.setTooltip("Sets mono and polyphonic legato slide time from instant to two seconds.");
    slideTime.setComponentID("slideTime");
    slideTimeLabel.setTooltip(slideTime.getTooltip());
    slideShape.setTooltip("Morphs the pitch glide from logarithmic through linear to sigmoid.");
    slideShape.setComponentID("slideShape");
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
    { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{glidePlotX,glidePlotY,glidePlotSize,glidePlotSize})); };
    updatePlayAvailability();
    for(size_t i=0;i<oscillatorTabs.size();++i)
    {
        auto& tab=oscillatorTabs[i]; styleTab(tab,"OSC "+juce::String(int(i)+1),2);
        tab.setTooltip("Opens oscillator "+juce::String(int(i)+1)+" in the OSC panel.");
        tab.onClick=[this,i]{showOscillator(int(i));}; addAndMakeVisible(tab);
    }
    noiseButton.setClickingTogglesState(true);
    noiseButton.setTooltip("Enables the noise oscillator and opens its tuning, filter, and style controls.");
    noiseButton.setColour(juce::TextButton::buttonColourId,buttonIdle);
    noiseButton.setColour(juce::TextButton::buttonOnColourId,buttonActive);
    noiseButton.setColour(juce::TextButton::textColourOffId,buttonInk);
    noiseButton.setColour(juce::TextButton::textColourOnId,buttonInk);
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
    noiseStyleLabel.setColour(juce::Label::textColourId,secondaryRed);
    noiseStyleLabel.setTooltip("Selects white, pink, or sparse velvet noise before the tuned filter.");
    addAndMakeVisible(noiseStyle); addAndMakeVisible(noiseStyleLabel);
    noiseStyleAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"noiseStyle",noiseStyle);
    noiseStyle.onChange=[this] { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight})); };
    for(size_t i=0;i<noiseSliders.size();++i)
    {
        auto& slider=noiseSliders[i]; auto& label=noiseLabels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        slider.setName(noiseControls[i].name); slider.setTitle(noiseControls[i].name);
        slider.setComponentID(noiseControls[i].id);
        label.setText(noiseControls[i].name,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f));
        label.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        noiseSliderAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,noiseControls[i].id,slider);
        editorTooltips::setHint(slider,label,noiseControls[i].hint);
    }
    noiseSliders[1].textFromValueFunction=[](double value)
    { return (value>0 ? "+" : "")+juce::String(value,1)+" st"; };
    noiseSliders[2].textFromValueFunction=[](double value)
    { return juce::String(juce::roundToInt(value))+" Hz"; };
    noiseSliders[0].textFromValueFunction=gainDecibels;
    noiseSliders[0].valueFromTextFunction=[](const juce::String& text)
    {
        if(text.containsIgnoreCase("inf")) return 0.0;
        return std::clamp(100.0*std::pow(10.0,text.getDoubleValue()/20.0),0.0,100.0);
    };
    noiseSliders[0].updateText(); noiseSliders[1].updateText(); noiseSliders[2].updateText();
    for(size_t i=0;i<detuneShapeSliders.size();++i)
    {
        auto& slider=detuneShapeSliders[i]; auto& label=detuneShapeLabels[i];
        slider.setComponentID("osc"+juce::String(int(i)+1)+"DetuneShape");
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        label.setText("DETUNE SHAPE",juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f));
        label.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        detuneShapeAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"DetuneShape",slider);
        editorTooltips::setHint(slider,label,
            "Moves inner unison pitches toward center at -100% or toward the outer voices at +100%.");
    }
    for(size_t i=0;i<dutySliders.size();++i)
    {
        auto& slider=dutySliders[i]; auto& label=dutyLabels[i];
        slider.setComponentID("osc"+juce::String(int(i)+1)+"Duty");
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        label.setText("DUTY",juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f));
        label.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        dutyAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,
            "osc"+juce::String(int(i)+1)+"Duty",slider);
        editorTooltips::setHint(slider,label,
            "Changes pulse width, narrows triangle peaks, and adds a flat end to saw without changing sine.");
        slider.onValueChange=[this,i]
        {
            if(int(i)==currentOscillator)
                repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
                    {oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight}));
        };
    }
    for(size_t i=0;i<fmAmountSliders.size();++i)
    {
        const auto prefix="osc"+juce::String(int(i)+1);
        auto& amount=fmAmountSliders[i];
        amount.setComponentID(prefix+"FmAmount");
        auto& amountLabel=fmAmountLabels[i];
        amount.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        amount.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        amount.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        amount.setColour(juce::Slider::textBoxTextColourId,panelInk);
        amountLabel.setText("FM AMT",juce::dontSendNotification);
        amountLabel.setJustificationType(juce::Justification::centred);
        amountLabel.setFont(juce::FontOptions(9.f));
        amountLabel.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(amount); addAndMakeVisible(amountLabel);
        fmAmountAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,prefix+"FmAmount",amount);
        editorTooltips::setHint(amount,amountLabel,
            "Sets this oscillator's bipolar frequency deviation up to its base frequency.");

        auto& source=fmSources[i];
        auto& sourceLabel=fmSourceLabels[i];
        source.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        source.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        source.setTextBoxIsEditable(false);
        source.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        source.setColour(juce::Slider::textBoxTextColourId,panelInk);
        source.textFromValueFunction=[](double value)
        {
            constexpr std::array<const char*,5> names {"OFF","OSC 1","OSC 2","OSC 3","NOISE"};
            return juce::String(names[size_t(std::clamp(juce::roundToInt(value),0,4))]);
        };
        sourceLabel.setText("FM SOURCE",juce::dontSendNotification);
        sourceLabel.setJustificationType(juce::Justification::centred);
        sourceLabel.setFont(juce::FontOptions(9.f));
        sourceLabel.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(source); addAndMakeVisible(sourceLabel);
        fmSourceAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,prefix+"FmSource",source);
        source.updateText();
        const auto sourceHint="Selects an oscillator or noise as the FM source, using the prior sample for feedback cycles.";
        source.setTooltip(sourceHint); sourceLabel.setTooltip(sourceHint);
    }
    constexpr std::array<const char*,3> timeIds {"ampTimesFive","fltTimesFive","modTimesFive"};
    constexpr std::array<const char*,3> envelopeNames {"AMP","FLT","MOD"};
    for(size_t index=0;index<timesFiveButtons.size();++index)
    {
        auto& button=timesFiveButtons[index];
        button.setClickingTogglesState(true);
        button.setTooltip("Multiplies this "+juce::String(envelopeNames[index])+" envelope's attack, decay, and release by five.");
        addAndMakeVisible(button);
        timesFiveAttachments[index]=std::make_unique<State::ButtonAttachment>(synth.parameters,timeIds[index],button);
        button.onClick=[this] { updateEnvelopeTimePresentation(); };
    }
    filterMergeLabel.setText("MERGE",juce::dontSendNotification);
    filterMergeLabel.setJustificationType(juce::Justification::centred);
    filterMergeLabel.setFont(juce::FontOptions(9.f));
    filterMergeLabel.setColour(juce::Label::textColourId,secondaryRed);
    filterMergeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    filterMergeSlider.setComponentID("filterMerge");
    filterMergeSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
    filterMergeSlider.setColour(juce::Slider::thumbColourId,accent);
    filterMergeSlider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    filterMergeSlider.setColour(juce::Slider::textBoxTextColourId,panelInk);
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
    syncButton.setClickingTogglesState(true);
    syncButton.setColour(juce::TextButton::buttonColourId,buttonIdle);
    syncButton.setColour(juce::TextButton::buttonOnColourId,buttonActive);
    syncButton.setColour(juce::TextButton::textColourOffId,buttonInk);
    syncButton.setColour(juce::TextButton::textColourOnId,buttonInk);
    syncButton.setTooltip("Synchronizes Delay Time to the host tempo using straight, dotted, or triplet notes.");
    addAndMakeVisible(syncButton);
    syncAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"verbSync",syncButton);
    syncButton.onClick=[this] { updateVisibility(); };
    verbNoteSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    verbNoteSlider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
    verbNoteSlider.setTextBoxIsEditable(false);
    verbNoteSlider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    verbNoteSlider.setColour(juce::Slider::textBoxTextColourId,panelInk);
    verbNoteSlider.setTooltip("Selects the note length for tempo-synchronized delay and reverb time.");
    addAndMakeVisible(verbNoteSlider);
    verbNoteAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"verbNote",verbNoteSlider);
    driveMode.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveMode.setRange(0,3,1);
    driveMode.setTextBoxStyle(juce::Slider::TextBoxBelow,false,88,17);
    driveMode.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    driveMode.setColour(juce::Slider::textBoxTextColourId,panelInk);
    driveMode.setTooltip("Selects BJT, op-amp, diode shunt, or series crossover drive from Zao.");
    driveModeLabel.setText("CIRCUIT",juce::dontSendNotification);
    driveModeLabel.setFont(juce::FontOptions(9.f));
    driveModeLabel.setJustificationType(juce::Justification::centred);
    driveModeLabel.setColour(juce::Label::textColourId,secondaryRed);
    driveModeLabel.setTooltip(driveMode.getTooltip());
    addAndMakeVisible(driveMode); addAndMakeVisible(driveModeLabel);
    driveModeAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"driveMode",driveMode);
    driveMode.textFromValueFunction=[](double value)
    {
        constexpr std::array<const char*,4> names {"BJT CE","OPAMP FB","SHUNT","CROSSOVER"};
        return juce::String(names[size_t(std::clamp(juce::roundToInt(value),0,3))]);
    };
    driveMode.setTextBoxIsEditable(false);
    driveMode.updateText();
    driveMode.onValueChange=[this]
    {
        updateDrivePreview();
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
            {driveScopeX,driveScopeY,driveScopeWidth,driveScopeHeight}));
    };
    driveMix.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveMix.setComponentID("driveMix");
    driveMix.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
    driveMix.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    driveMix.setColour(juce::Slider::textBoxTextColourId,panelInk);
    driveMixLabel.setText("MIX",juce::dontSendNotification);
    driveMixLabel.setFont(juce::FontOptions(9.f));
    driveMixLabel.setJustificationType(juce::Justification::centred);
    driveMixLabel.setColour(juce::Label::textColourId,secondaryRed);
    editorTooltips::setHint(driveMix,driveMixLabel,"Blends the driven signal with the dry oscillator signal.");
    addAndMakeVisible(driveMix); addAndMakeVisible(driveMixLabel);
    driveMixAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"driveMix",driveMix);
    driveMix.onValueChange=[this]
    {
        updateDrivePreview();
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
            {driveScopeX,driveScopeY,driveScopeWidth,driveScopeHeight}));
    };
    constexpr std::array<const char*,2> cutNames{"LOW CUT","HIGH CUT"};
    constexpr std::array<const char*,2> cutHints{
        "Click to select the low cut before or after drive, then drag; the inner and outer rings show their settings.",
        "Click to select the high cut before or after drive, then drag; the inner and outer rings show their settings."};
    for(size_t i=0;i<driveCutSliders.size();++i)
    {
        auto& slider=driveCutSliders[i]; auto& label=driveCutLabels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        slider.setName(cutNames[i]); slider.setTitle(cutNames[i]);
        label.setFont(juce::FontOptions(9.f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,secondaryRed);
        editorTooltips::setHint(slider,label,cutHints[i]);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        slider.onClick=[this,i]
        {
            auto& control=driveCutSliders[i];
            control.setPostSelected(!control.isPostSelected());
            bindDriveCut(i);
        };
        slider.onValueChange=[this]
        {
            updateDriveCutRings();
            updateDrivePreview();
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
                {driveScopeX,driveScopeY,driveScopeWidth,driveScopeHeight}));
        };
        bindDriveCut(i);
    }
    updateDriveCutRings();
    addAndMakeVisible(transformerButton);
    transformerAttachment=std::make_unique<State::ButtonAttachment>(synth.parameters,"transformer",transformerButton);
    for(size_t i=0;i<sliders.size();++i)
    {
        if(i<18 && i%6<2) continue;
        auto& slider=sliders[i]; auto& label=labels[i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        slider.setName(controls[i].name); slider.setTitle(controls[i].name);
        slider.setComponentID(controls[i].id);
        label.setText(controls[i].name,juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(9.f)); label.setColour(juce::Label::textColourId,secondaryRed);
        addAndMakeVisible(slider); addAndMakeVisible(label);
        sliderAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,controls[i].id,slider);
        editorTooltips::setHint(slider,label,controls[i].hint);
        if(i<18 && i%6==2) slider.onValueChange=[this]{repaint();};
    }
    sliders[30].setSliderStyle(juce::Slider::LinearHorizontal);
    sliders[30].setTextBoxStyle(juce::Slider::TextBoxRight,false,42,18);
    sliders[30].setColour(juce::Slider::thumbColourId,accent);
    sliders[30].setSliderSnapsToMousePosition(false);
    sliders[30].setMouseDragSensitivity(600);
    curveLabel.setText("CURVE",juce::dontSendNotification);
    curveLabel.setJustificationType(juce::Justification::centred);
    curveLabel.setFont(juce::FontOptions(9.f));
    curveLabel.setColour(juce::Label::textColourId,secondaryRed);
    curveLabel.setTooltip("Negative curve values act sooner, while positive values delay attack, decay, and release movement.");
    addAndMakeVisible(curveLabel);
    constexpr std::array<const char*,3> curveIds {"ampCurve","fltCurve","modCurve"};
    for(size_t i=0;i<curveSliders.size();++i)
    {
        auto& slider=curveSliders[i];
        slider.setComponentID(curveIds[i]);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,42,16);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        slider.setTooltip("Negative curve values act sooner, while positive values delay attack, decay, and release movement.");
        addAndMakeVisible(slider);
        curveAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,curveIds[i],slider);
        slider.onValueChange=[this]
        { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,376,240,254})); };
    }
    envelopeMergeLabel.setText("MERGE",juce::dontSendNotification);
    envelopeMergeLabel.setFont(juce::FontOptions(9.f));
    envelopeMergeLabel.setColour(juce::Label::textColourId,secondaryRed);
    envelopeMergeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(envelopeMergeLabel);
    constexpr std::array<const char*,3> mergeIds {"ampMerge","fltMerge","modMerge"};
    for(size_t i=0;i<envelopeMergeSliders.size();++i)
    {
        auto& slider=envelopeMergeSliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,43,17);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,panelInk);
        slider.setTooltip("Blends each note's envelope with one shared envelope, from separate at zero to merged at 100 percent.");
        slider.setComponentID(mergeIds[i]);
        addAndMakeVisible(slider);
        envelopeMergeAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,mergeIds[i],slider);
    }
    envelopeMergeLabel.setTooltip(envelopeMergeSliders[0].getTooltip());
    curveLabel.setText("C",juce::dontSendNotification);
    curveLabel.setFont(juce::FontOptions(10.f).withStyle("bold"));
    curveLabel.setColour(juce::Label::textColourId,panelInk.withAlpha(.9f));
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
            labels[i].setColour(juce::Label::textColourId,panelInk.withAlpha(.9f));
        }
        else slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,18);
        slider.setColour(juce::Slider::thumbColourId,accent);
        slider.onValueChange=[this,i]
        {
            const auto area=i<30 ? juce::Rectangle<int>(444,376,240,254) : juce::Rectangle<int>(444,90,240,278);
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,area));
            if(i==24) repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,90,240,278}));
        };
    }
    sliders[31].textFromValueFunction=[](double value)
    { return juce::String(juce::roundToInt(value)); };
    sliders[32].textFromValueFunction=[](double value)
    { return juce::String(value,1); };
    for(const auto index:{18,19,21,22,23,25,26,27,29})
    {
        const auto filter=index>=22 && index<=25;
        sliders[size_t(index)].textFromValueFunction=[this,filter,index](double value)
        {
            constexpr std::array<const char*,3> ids {"ampTimesFive","fltTimesFive","modTimesFive"};
            const auto multiplier=processor.parameters.getRawParameterValue(ids[filter ? 1 : index>=26 ? 2 : 0])->load()>=.5f ? 5. : 1.;
            const auto ms=value*(filter ? 1. : 1000.)*multiplier;
            return ms>=1000. ? juce::String(ms/1000.,1)+"s"
                : juce::String(juce::roundToInt(ms))+"ms";
        };
        sliders[size_t(index)].valueFromTextFunction=[this,filter,index](const juce::String& text)
        {
            constexpr std::array<const char*,3> ids {"ampTimesFive","fltTimesFive","modTimesFive"};
            const auto multiplier=processor.parameters.getRawParameterValue(ids[filter ? 1 : index>=26 ? 2 : 0])->load()>=.5f ? 5. : 1.;
            const auto ms=double(text.getFloatValue())*(text.trim().endsWithIgnoreCase("ms") ? 1. :
                text.trim().endsWithIgnoreCase("s") ? 1000. : 1.);
            return ms/((filter ? 1. : 1000.)*multiplier);
        };
        sliders[size_t(index)].updateText();
    }
    sliders[30].setSliderStyle(juce::Slider::LinearHorizontal);
    sliders[30].setTextBoxStyle(juce::Slider::TextBoxRight,false,42,18);
    sliders[24].textFromValueFunction=[](double value)
    { return juce::String(value,1); };
    sliders[24].updateText();
    sliders[37].onValueChange=[this]
    {
        updateDrivePreview();
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
            {driveScopeX,driveScopeY,driveScopeWidth,driveScopeHeight}));
    };
    for(size_t i=0;i<waves.size();++i)
    {
        auto& wave=waves[i]; auto& label=waveLabels[i]; auto& retrigger=retrig[i];
        auto& phaseLine=phaseStartLines[i];
        wave.setComponentID("osc"+juce::String(int(i)+1)+"WaveX");
        wave.setTooltip("Drag within the square to blend oscillator "+juce::String(int(i)+1)+" between saw, pulse, triangle, and sine waves.");
        label.setText("WAVE",juce::dontSendNotification); label.setFont(juce::FontOptions(9.f));
        label.setJustificationType(juce::Justification::centred); label.setColour(juce::Label::textColourId,secondaryRed);
        retrigger.setRange(0,1,1);
        retrigger.setSuffix("");
        retrigger.setReadoutFormat("RETRIG",0,false);
        retrigger.setMouseDragSensitivity(40);
        retrigger.setTooltip("Drag up to enable phase retriggering at note onset or down to disable it.");
        addAndMakeVisible(wave); addAndMakeVisible(label); addAndMakeVisible(retrigger);
        addAndMakeVisible(phaseLine);
        const auto prefix="osc"+juce::String(int(i)+1);
        waveXParameters[i]=synth.parameters.getParameter(prefix+"WaveX");
        waveYParameters[i]=synth.parameters.getParameter(prefix+"WaveY");
        phaseStartParameters[i]=synth.parameters.getParameter(prefix+"PhaseStart");
        phaseLine.setPhase(synth.parameters.getRawParameterValue(prefix+"PhaseStart")->load()*.01f);
        phaseLine.onDragStart=[this,i] { phaseStartParameters[i]->beginChangeGesture(); };
        phaseLine.onDragEnd=[this,i] { phaseStartParameters[i]->endChangeGesture(); };
        phaseLine.onPhaseChange=[this,i](float phase)
        {
            auto* parameter=phaseStartParameters[i];
            parameter->setValueNotifyingHost(parameter->convertTo0to1(phase*100.f));
        };
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
                repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight}));
        };
        retrigAttachments[i]=std::make_unique<State::SliderAttachment>(synth.parameters,"osc"+juce::String(int(i)+1)+"Retrig",retrig[i]);
    }
    for(const auto* option:{"Bypass","TB-303 ladder","Sallen-Key 18 dB","Digital 12 dB","Digital 24 dB"})
        filterType.addItem(option,filterType.getNumItems()+1);
    modTarget.addItem("Drive",1); modTarget.addItem("Spread",2);
    modTarget.addItem("Pitch",3); modTarget.addItem("Filter",4);
    for(int index=0;index<qin::continuousModTargetCount;++index)
        modTarget.addItem(qin::continuousModTargets[index].label,qin::legacyModTargetCount+index+1);
    filterType.setTooltip("Selects the TB-303, 18 dB Sallen-Key, or 12/24 dB digital filter model.");
    modTarget.setTooltip("Routes the modulation envelope to any continuous synth control.");
    for(auto* box:{&filterType,&modTarget})
    { box->setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff29272a)); box->setColour(juce::ComboBox::textColourId,stageStyle::foreground); addAndMakeVisible(*box); }
    modTargetLabel.setFont(juce::FontOptions(9.f));
    modTargetLabel.setColour(juce::Label::textColourId,secondaryRed);
    addAndMakeVisible(modTargetLabel);
    modTargetLabel.setText("TARGET",juce::dontSendNotification);
    filterTypeAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"filterType",filterType);
    modTargetAttachment=std::make_unique<State::ComboBoxAttachment>(synth.parameters,"modTarget",modTarget);
    filterType.onChange=[this]
    { repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,90,240,278})); };
    tuningPanel.setOpaque(true);
    addAndMakeVisible(tuningPanel);
    for(int degree=0;degree<12;++degree)
    {
        auto& slider=tuningSliders[size_t(degree)];
        slider.setComponentID("tune"+juce::String(degree));
        tuningPanel.addAndMakeVisible(slider);
        if(degree>0)
            tuningAttachments[size_t(degree)]=std::make_unique<State::SliderAttachment>(synth.parameters,
                "tune"+juce::String(degree),slider);
    }
    for(auto* slider:{&tuningRoot,&tuningA4})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,17);
        slider->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider->setColour(juce::Slider::textBoxTextColourId,panelInk);
        tuningPanel.addAndMakeVisible(*slider);
    }
    for(auto* label:{&tuningRootLabel,&tuningA4Label})
    {
        label->setFont(juce::FontOptions(9.f));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId,secondaryRed);
        tuningPanel.addAndMakeVisible(*label);
    }
    tuningRootLabel.setText("ROOT",juce::dontSendNotification);
    tuningA4Label.setText("A4 BASE",juce::dontSendNotification);
    tuningRoot.setTextBoxIsEditable(false);
    tuningA4.setComponentID("tuneA4");
    tuningRootAttachment=std::make_unique<State::SliderAttachment>(synth.parameters,"tuneRoot",tuningRoot);
    tuningA4Attachment=std::make_unique<State::SliderAttachment>(synth.parameters,"tuneA4",tuningA4);
    tuningRoot.onValueChange=[this] { updateTuningLabels(); };
    editorTooltips::setHint(tuningRoot,tuningRootLabel,"Transposes the deviation pattern to this pitch class and locks it at zero cents.");
    editorTooltips::setHint(tuningA4,tuningA4Label,"Sets the global A4 reference pitch before pitch-class offsets.");
    updateTuningLabels();
    tuningPanel.setVisible(false);
    addAndMakeVisible(resonanceIndicator);
    addAndMakeVisible(modulationOverlay);
    modulationOverlay.setVisible(false);
    oscillatorTabs[0].setToggleState(true,juce::dontSendNotification);
    envelopeTabs[0].setToggleState(true,juce::dontSendNotification);
    fxTabs[0].setToggleState(true,juce::dontSendNotification);
    polyLegatoButton.snapToState();
    for(auto& button:timesFiveButtons) button.snapToState();
    transformerButton.snapToState();
    updateVisibility(); editorScaling::enable(*this,designWidth,designHeight);
    updateDrivePreview(); gpu.attach(*this); startTimerHz(40);
    dimmer.attachHostBypass(*this,processor.bypassActivity());
}
void QinSynthEditor::bindDriveCut(size_t index)
{
    constexpr std::array<const char*,2> preIds{"driveLowCut","driveHighCut"};
    constexpr std::array<const char*,2> postIds{"drivePostLowCut","drivePostHighCut"};
    constexpr std::array<const char*,2> names{"LOW CUT","HIGH CUT"};
    auto& slider=driveCutSliders[index];
    driveCutAttachments[index].reset();
    const auto post=slider.isPostSelected();
    const auto* id=post ? postIds[index] : preIds[index];
    slider.setComponentID(id);
    driveCutLabels[index].setText(juce::String(names[index])+(post ? " / POST" : " / PRE"),
                                  juce::dontSendNotification);
    driveCutAttachments[index]=std::make_unique<State::SliderAttachment>(processor.parameters,id,slider);
    slider.textFromValueFunction=[index](double value)
    {
        if((index==0 && value<=0.) || (index==1 && value>=20000.)) return juce::String("OFF");
        if(value>=1000.) return juce::String(value/1000.,1)+"k";
        return juce::String(juce::roundToInt(value))+" Hz";
    };
    slider.valueFromTextFunction=[index](const juce::String& text)
    {
        if(text.trim().equalsIgnoreCase("OFF")) return index==0 ? 0. : 20000.;
        return double(text.getFloatValue())*(text.containsIgnoreCase("k") ? 1000. : 1.);
    };
    slider.updateText();
    updateDriveCutRings();
}
void QinSynthEditor::updateDriveCutRings()
{
    constexpr std::array<const char*,2> preIds{"driveLowCut","driveHighCut"};
    constexpr std::array<const char*,2> postIds{"drivePostLowCut","drivePostHighCut"};
    for(size_t index=0;index<driveCutSliders.size();++index)
        driveCutSliders[index].setRingValues(processor.parameters.getParameter(preIds[index])->getValue(),
                                             processor.parameters.getParameter(postIds[index])->getValue());
}
void QinSynthEditor::updateEnvelopeTimePresentation()
{
    constexpr std::array<const char*,3> timeIds {"ampTimesFive","fltTimesFive","modTimesFive"};
    lastTimesFive=processor.parameters.getRawParameterValue(timeIds[size_t(currentEnvelope)])->load()>=.5f;
    for(const auto index:{18,19,21,22,23,25,26,27,29}) sliders[size_t(index)].updateText();
    repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{444,376,240,254}));
}
void QinSynthEditor::updateDrivePreview()
{
    qin::DriveCircuitBank circuit;
    circuit.prepare(48000.0);
    const auto mode=std::clamp(juce::roundToInt(driveMode.getValue()),0,3);
    const auto drive=std::clamp(float(sliders[37].getValue())*.01f,0.f,1.f);
    const auto mix=std::clamp(float(driveMix.getValue())*.01f,0.f,1.f);
    qin::DriveInputFilter inputFilter;
    inputFilter.prepare(48000.0,processor.parameters.getRawParameterValue("driveLowCut")->load(),
        processor.parameters.getRawParameterValue("driveHighCut")->load());
    qin::DriveInputFilter postFilter;
    postFilter.prepare(48000.0,processor.parameters.getRawParameterValue("drivePostLowCut")->load(),
        processor.parameters.getRawParameterValue("drivePostHighCut")->load());
    for(int sample=0;sample<384;++sample)
    {
        const auto phase=std::fmod(float(sample)/64.f,1.f);
        const auto original=.55f*(1.f-4.f*std::abs(phase-.5f));
        inputFilter.beginSample();
        postFilter.beginSample();
        const auto filtered=inputFilter.process(0,original);
        const auto driven=drive>1.e-4f ? float(circuit.process(mode,0,filtered*(1.f+5.f*drive))) : original;
        const auto shaped=drive>1.e-4f && postFilter.isActive() ? postFilter.process(0,driven) : driven;
        if(sample>=256)
        {
            const auto index=size_t(sample-256);
            previewInput[index]=original;
            previewOutput[index]=original+mix*(shaped-original);
        }
    }
}
void QinSynthEditor::timerCallback()
{
    if(processor.getPresetStateEpoch()!=lastPresetStateEpoch) refreshPresetName();
    updateDriveCutRings();
    polyLegatoButton.advance(1.f/40.f);
    for(auto& button:timesFiveButtons) button.advance(1.f/40.f);
    transformerButton.advance(1.f/40.f);
    const auto active=juce::String(processor.getActiveVoiceCount())+" ACTIVE";
    if(activeVoicesLabel.getText()!=active) activeVoicesLabel.setText(active,juce::dontSendNotification);
    if(polyLegatoButton.getToggleState()!=lastPolyLegato) updatePlayAvailability();
    if(syncButton.getToggleState()!=lastVerbSync) updateVisibility();
    constexpr std::array<const char*,3> timeIds {"ampTimesFive","fltTimesFive","modTimesFive"};
    if(const auto enabled=processor.parameters.getRawParameterValue(timeIds[size_t(currentEnvelope)])->load()>=.5f;
       enabled!=lastTimesFive) updateEnvelopeTimePresentation();
    const auto cutoff=processor.getLiveFilterCutoff();
    if((cutoff<=0)!=(liveCutoff<=0) || (cutoff>0 && std::abs(std::log(cutoff/std::max(1.f,liveCutoff)))>.002f))
    {
        liveCutoff=cutoff;
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{455,138,218,67}));
    }
    const auto target=juce::roundToInt(processor.parameters.getRawParameterValue("modTarget")->load());
    const auto depth=processor.parameters.getRawParameterValue("modDepth")->load();
    const auto modulating=target==qin::modTargetIndex("filterResonance")
        && std::abs(depth)>.001f && processor.getActiveVoiceCount()>0;
    const auto nextResonance=processor.getLiveFilterResonance();
    if(resonanceModulated!=modulating
       || (modulating && std::abs(liveResonance-nextResonance)>.001f))
    {
        resonanceModulated=modulating;
        liveResonance=nextResonance;
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{455,138,218,67}));
    }
    const auto* parameter=processor.parameters.getParameter("filterResonance");
    resonanceIndicator.setPosition(parameter!=nullptr
        ? parameter->convertTo0to1(nextResonance*100.f) : nextResonance,modulating);
    const auto liveMod=processor.getLiveModulation();
    juce::Component* destination=nullptr;
    float markerX=liveMod.normalized,markerY=liveMod.normalized;
    const auto direct=std::abs(depth)>.001f && liveMod.target>=qin::legacyModTargetCount
        && liveMod.target<qin::legacyModTargetCount+qin::continuousModTargetCount;
    const auto routeId=direct ? juce::String(qin::continuousModTargets[liveMod.target-qin::legacyModTargetCount].id)
        : juce::String();
    const auto previewPrefix=currentOscillator==3 ? juce::String("noise")
        : "osc"+juce::String(currentOscillator+1);
    const auto previewModTarget=routeId.startsWith(previewPrefix) ? liveMod.target : -1;
    std::array<float,13> previewValues {};
    if(currentOscillator==3)
    {
        constexpr std::array<const char*,4> names {"Level","Tune","Hpf","Pan"};
        for(size_t i=0;i<names.size();++i)
            previewValues[i]=processor.parameters.getRawParameterValue(previewPrefix+names[i])->load();
        previewValues[4]=float(noiseStyle.getSelectedId());
    }
    else
    {
        constexpr std::array<const char*,13> names {"WaveX","WaveY","Duty","Level","Fine",
            "Unison","Detune","DetuneShape","Spread","Pan","FmAmount","FmSource","Octave"};
        for(size_t i=0;i<names.size();++i)
            previewValues[i]=processor.parameters.getRawParameterValue(previewPrefix+names[i])->load();
    }
    if(lastOscPreviewIndex!=currentOscillator || lastOscPreviewModTarget!=previewModTarget
       || (previewModTarget>=0 && std::abs(lastOscPreviewModValue-liveMod.normalized)>.001f)
       || previewValues!=lastOscPreviewValues)
    {
        lastOscPreviewIndex=currentOscillator;
        lastOscPreviewModTarget=previewModTarget;
        lastOscPreviewModValue=previewModTarget>=0 ? liveMod.normalized : 0.f;
        lastOscPreviewValues=previewValues;
        repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
            {oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight}));
    }
    if(direct)
    {
        const auto searchId=routeId.endsWith("WaveY") ? routeId.dropLastCharacters(1)+"X" : routeId;
        const std::function<juce::Component*(juce::Component&)> find=[&](juce::Component& parent)->juce::Component*
        {
            for(int index=0;index<parent.getNumChildComponents();++index)
            {
                auto* child=parent.getChildComponent(index);
                if(child->getComponentID()==searchId) return child;
                if(auto* match=find(*child)) return match;
            }
            return nullptr;
        };
        destination=find(*this);
    }
    if(destination!=nullptr && destination->isVisible()
       && (destination->getParentComponent()==this || destination->getParentComponent()->isVisible()))
    {
        auto bounds=destination->getBounds();
        if(auto* parent=destination->getParentComponent();parent!=this)
            bounds.translate(parent->getX(),parent->getY());
        auto shape=QinModulationOverlay::Shape::rotary;
        if(auto* wave=dynamic_cast<QinWavePad*>(destination))
        {
            shape=QinModulationOverlay::Shape::pad;
            markerX=routeId.endsWith("WaveY") ? wave->getPosition().x : liveMod.normalized;
            markerY=routeId.endsWith("WaveY") ? liveMod.normalized : wave->getPosition().y;
        }
        else if(dynamic_cast<QinVoiceSlider*>(destination)) shape=QinModulationOverlay::Shape::readout;
        else if(dynamic_cast<QinTuningKey*>(destination)) shape=QinModulationOverlay::Shape::key;
        else if(auto* slider=dynamic_cast<juce::Slider*>(destination))
        {
            if(slider->getSliderStyle()==juce::Slider::LinearVertical)
                shape=QinModulationOverlay::Shape::vertical;
            else if(slider->getSliderStyle()==juce::Slider::LinearHorizontal)
                shape=QinModulationOverlay::Shape::horizontal;
        }
        modulationOverlay.setDisplay(bounds,markerX,markerY,shape,true);
        modulationOverlay.toFront(false);
    }
    else modulationOverlay.setDisplay({},0,0,QinModulationOverlay::Shape::rotary,false);
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
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,
                {driveScopeX,driveScopeY,driveScopeWidth,driveScopeHeight}));
        }
    }
    for(size_t i=0;i<waves.size();++i)
    {
        const auto changed=waves[i].setPosition({waveXParameters[i]->convertFrom0to1(waveXParameters[i]->getValue())*.01f,
                                                 waveYParameters[i]->convertFrom0to1(waveYParameters[i]->getValue())*.01f});
        if(changed && int(i)==currentOscillator)
            repaint(editorScaling::repaintArea(*this,designWidth,designHeight,{oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight}));
        phaseStartLines[i].setPhase(phaseStartParameters[i]->convertFrom0to1(
            phaseStartParameters[i]->getValue())*.01f);
    }
}
void QinSynthEditor::updatePlayAvailability()
{
    lastPolyLegato=polyLegatoButton.getToggleState();
    legatoRule.setEnabled(lastPolyLegato);
    legatoRuleLabel.setEnabled(lastPolyLegato);
}
void QinSynthEditor::updateTuningLabels()
{
    constexpr std::array<const char*,12> names {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    constexpr std::array<int,12> whiteIndex {0,-1,1,-1,2,3,-1,4,-1,5,-1,6};
    constexpr std::array<int,12> blackAfter {0,0,1,1,2,3,3,4,4,5,5,6};
    const auto root=std::clamp(juce::roundToInt(tuningRoot.getValue()),0,11);
    for(int degree=0;degree<12;++degree)
    {
        const auto key=(root+degree)%12;
        const auto black=whiteIndex[size_t(key)]<0;
        auto& slider=tuningSliders[size_t(degree)];
        slider.setIdentity(names[size_t(key)],black,degree==0);
        slider.setBounds(black ? 40+(blackAfter[size_t(key)]+1)*56-17 : 40+whiteIndex[size_t(key)]*56,
            91,black ? 34 : 56,black ? 119 : 191);
    }
    for(int degree=0;degree<12;++degree)
        if(tuningSliders[size_t(degree)].isBlackKey()) tuningSliders[size_t(degree)].toFront(false);
}
void QinSynthEditor::showOscillator(int index)
{ currentOscillator=juce::jlimit(0,3,index); for(size_t i=0;i<3;++i) oscillatorTabs[i].setToggleState(int(i)==currentOscillator,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::showEnvelope(int index)
{ currentEnvelope=juce::jlimit(0,2,index); for(size_t i=0;i<3;++i) envelopeTabs[i].setToggleState(int(i)==currentEnvelope,juce::dontSendNotification); updateVisibility(); updateEnvelopeTimePresentation(); repaint(); }
void QinSynthEditor::showFxSection(int index)
{ currentFxSection=juce::jlimit(0,1,index); for(size_t i=0;i<2;++i) fxTabs[i].setToggleState(int(i)==currentFxSection,juce::dontSendNotification); updateVisibility(); repaint(); }
void QinSynthEditor::refreshPresetName()
{
    const auto name=presets.currentName();
    if(presetNameLabel.getText()!=name) presetNameLabel.setText(name,juce::dontSendNotification);
    lastPresetStateEpoch=processor.getPresetStateEpoch();
}
void QinSynthEditor::stepPreset(int direction)
{
    const auto list=presets.entries();
    if(list.empty()) return;
    const auto current=presets.currentIndex(list);
    const auto next=current<0 ? (direction>0 ? 0 : int(list.size())-1)
        : (current+direction+int(list.size()))%int(list.size());
    if(presets.load(list[size_t(next)])) { refreshPresetName(); repaint(); }
}
void QinSynthEditor::promptSavePreset()
{
    auto* dialog=new juce::AlertWindow("Save Qin preset","Choose a name for this sound.",
                                       juce::AlertWindow::NoIcon);
    dialog->addTextEditor("name",presets.currentIsUser() ? presets.currentName() : "New Preset","Name");
    dialog->addButton("Save",1);
    dialog->addButton("Cancel",0);
    const juce::Component::SafePointer<QinSynthEditor> safe(this);
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog](int result)
    {
        if(result!=1 || safe==nullptr) return;
        if(safe->presets.saveAs(dialog->getTextEditorContents("name"))) safe->refreshPresetName();
        else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Preset not saved",
            "Choose a new name and make sure the preset folder is writable.","OK",safe.getComponent());
    }),true);
}
void QinSynthEditor::showPresetMenu()
{
    const auto list=presets.entries();
    const auto selected=presets.currentIndex(list);
    juce::PopupMenu menu;
    menu.addSectionHeader("Factory presets");
    for(size_t index=0;index<list.size();++index)
    {
        if(index==QinPresetManager::factoryPresetCount)
        { menu.addSeparator(); menu.addSectionHeader("User presets"); }
        menu.addItem(int(index)+1,list[index].name,true,int(index)==selected);
    }
    const auto saveId=int(list.size())+1;
    const auto overwriteId=saveId+1;
    const auto deleteId=saveId+2;
    menu.addSeparator();
    menu.addItem(saveId,"Save As...");
    if(presets.currentIsUser())
    {
        menu.addItem(overwriteId,"Overwrite Current Preset");
        menu.addItem(deleteId,"Delete Current Preset");
    }
    const juce::Component::SafePointer<QinSynthEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenuButton),
        [safe,list,saveId,overwriteId,deleteId](int result)
        {
            if(safe==nullptr || result==0) return;
            if(result>=1 && result<=int(list.size()))
            {
                if(safe->presets.load(list[size_t(result-1)])) { safe->refreshPresetName(); safe->repaint(); }
                return;
            }
            if(result==saveId) { safe->promptSavePreset(); return; }
            if(result==overwriteId)
            {
                if(safe->presets.saveAs(safe->presets.currentName(),true)) safe->refreshPresetName();
                else juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Preset not saved","The preset folder is not writable.","OK",safe.getComponent());
                return;
            }
            if(result==deleteId)
            {
                juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,"Delete Qin preset",
                    "Delete this saved preset?", "Delete", "Cancel",safe.getComponent(),
                    juce::ModalCallbackFunction::create([safe](int choice)
                    {
                        if(choice==1 && safe!=nullptr && safe->presets.deleteCurrentUser())
                            safe->refreshPresetName();
                    }));
            }
        });
}
void QinSynthEditor::updateVisibility()
{
    for(int osc=0;osc<3;++osc)
    {
        const auto selected=osc==currentOscillator; waves[size_t(osc)].setVisible(selected); waveLabels[size_t(osc)].setVisible(selected); retrig[size_t(osc)].setVisible(selected);
        phaseStartLines[size_t(osc)].setVisible(selected);
        levelDisplays[size_t(osc)].setVisible(selected);
        octaveDisplays[size_t(osc)].setVisible(selected);
        fineDisplays[size_t(osc)].setVisible(selected);
        detuneShapeSliders[size_t(osc)].setVisible(selected); detuneShapeLabels[size_t(osc)].setVisible(selected);
        dutySliders[size_t(osc)].setVisible(selected); dutyLabels[size_t(osc)].setVisible(selected);
        fmAmountSliders[size_t(osc)].setVisible(selected); fmAmountLabels[size_t(osc)].setVisible(selected);
        fmSources[size_t(osc)].setVisible(selected); fmSourceLabels[size_t(osc)].setVisible(selected);
        for(int control=0;control<6;++control)
        {
            if(control<2) continue;
            const auto i=size_t(osc*6+control);
            sliders[i].setVisible(selected); labels[i].setVisible(selected);
        }
    }
    noiseStyle.setVisible(currentOscillator==3); noiseStyleLabel.setVisible(currentOscillator==3);
    for(size_t i=0;i<noiseSliders.size();++i)
    { noiseSliders[i].setVisible(currentOscillator==3); noiseLabels[i].setVisible(currentOscillator==3); }
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control) { const auto i=size_t(18+env*4+control); sliders[i].setVisible(env==currentEnvelope); labels[i].setVisible(env==currentEnvelope); }
    for(int env=0;env<3;++env) curveSliders[size_t(env)].setVisible(env==currentEnvelope);
    for(int env=0;env<3;++env) envelopeMergeSliders[size_t(env)].setVisible(env==currentEnvelope);
    for(size_t index=0;index<timesFiveButtons.size();++index)
        timesFiveButtons[index].setVisible(int(index)==currentEnvelope);
    sliders[30].setVisible(currentEnvelope==2); labels[30].setVisible(currentEnvelope==2);
    modTarget.setVisible(currentEnvelope==2); modTargetLabel.setVisible(currentEnvelope==2);
    for(size_t i=36;i<sliders.size();++i)
    { const auto selected=i==36 || (currentFxSection==0 ? i==37 : i>=38); sliders[i].setVisible(selected); labels[i].setVisible(selected && i!=36); }
    lastVerbSync=syncButton.getToggleState();
    syncButton.setVisible(currentFxSection==1);
    sliders[38].setVisible(currentFxSection==1 && !lastVerbSync);
    verbNoteSlider.setVisible(currentFxSection==1 && lastVerbSync);
    driveMode.setVisible(currentFxSection==0); driveModeLabel.setVisible(currentFxSection==0);
    driveMix.setVisible(currentFxSection==0); driveMixLabel.setVisible(currentFxSection==0);
    for(size_t i=0;i<driveCutSliders.size();++i)
    { driveCutSliders[i].setVisible(currentFxSection==0); driveCutLabels[i].setVisible(currentFxSection==0); }
}
void QinSynthEditor::resized()
{
    titleHint.setBounds(18,29,195,38);
    setCompactButtonBounds(presetMenuButton,226,34,50,27);
    presetNameLabel.setBounds(281,34,204,27);
    setCompactButtonBounds(presetPreviousButton,490,34,31,27);
    setCompactButtonBounds(presetNextButton,527,34,31,27);
    setCompactButtonBounds(tuneButton,574,34,74,27);
    tuningPanel.setBounds(430,78,472,416);
    updateTuningLabels();
    tuningRootLabel.setBounds(50,316,86,15); tuningRoot.setBounds(50,331,86,75);
    tuningA4Label.setBounds(154,316,86,15); tuningA4.setBounds(154,331,86,75);
    voiceCount.setBounds(660,34,94,27);
    activeVoicesLabel.setBounds(761,35,130,25);
    setCompactButtonBounds(polyLegatoButton,351,526,79,72);
    legatoRuleLabel.setBounds(30,522,83,13); legatoRule.setBounds(30,535,83,70);
    slideTimeLabel.setBounds(119,522,83,13); slideTime.setBounds(119,535,83,70);
    slideShapeLabel.setBounds(208,522,83,13); slideShape.setBounds(208,535,83,70);
    for(int i=0;i<3;++i)
    {
        setCompactButtonBounds(oscillatorTabs[size_t(i)],31+i*109,101,103,27);
        setCompactButtonBounds(envelopeTabs[size_t(i)],455+i*55,380,52,27);
        waveLabels[size_t(i)].setBounds(318,164,96,11); waves[size_t(i)].setBounds(318,175,96,96);
        dutyLabels[size_t(i)].setBounds(218,185,86,15);
        dutySliders[size_t(i)].setBounds(218,200,86,65);
        phaseStartLines[size_t(i)].setBounds(oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight);
        retrig[size_t(i)].setBounds(333,139,85,26);
        levelDisplays[size_t(i)].setBounds(34,139,105,26);
        fineDisplays[size_t(i)].setBounds(143,139,102,26);
        octaveDisplays[size_t(i)].setBounds(249,139,78,26);
        for(int control=0;control<6;++control)
        {
            const auto index=size_t(i*6+control);
            const auto x=control==0 ? 185 : control<2 ? 235 : 39+((control-2)%2)*96;
            const auto y=control<2 ? 139 : 296+((control-2)/2)*80;
            const auto width=86;
            labels[index].setBounds(x,y,width,15); sliders[index].setBounds(x,y+15,width,65);
        }
        detuneShapeLabels[size_t(i)].setBounds(231,296,86,15);
        detuneShapeSliders[size_t(i)].setBounds(231,311,86,65);
        fmSourceLabels[size_t(i)].setBounds(327,296,86,15);
        fmSources[size_t(i)].setBounds(327,311,86,65);
        fmAmountLabels[size_t(i)].setBounds(327,376,86,15);
        fmAmountSliders[size_t(i)].setBounds(327,391,86,65);
    }
    setCompactButtonBounds(noiseButton,363,101,55,27);
    noiseStyleLabel.setBounds(34,146,118,15); noiseStyle.setBounds(34,162,154,27);
    for(size_t i=0;i<noiseSliders.size();++i)
    {
        const auto x=43+int(i%3)*132, y=296+int(i/3)*80;
        noiseLabels[i].setBounds(x,y,103,15); noiseSliders[i].setBounds(x,y+15,103,65);
    }
    for(auto& button:timesFiveButtons) setCompactButtonBounds(button,622,381,50,49);
    labels[35].setBounds(231,376,86,15); sliders[35].setBounds(231,391,86,65);
    for(int env=0;env<3;++env)
        for(int control=0;control<4;++control)
        {
            const auto index=size_t(18+env*4+control);
            const auto x=455+control*44;
            labels[index].setBounds(x,462,42,12); sliders[index].setBounds(x,474,42,96);
        }
    curveLabel.setBounds(631,462,42,12);
    for(auto& slider:curveSliders) slider.setBounds(631,474,42,96);
    envelopeMergeLabel.setBounds(455,578,50,14);
    for(auto& slider:envelopeMergeSliders) slider.setBounds(505,575,168,20);
    labels[30].setBounds(455,599,72,10); sliders[30].setBounds(455,609,72,17);
    modTargetLabel.setBounds(531,599,138,10); modTarget.setBounds(531,609,138,17);
    filterType.setBounds(454,103,215,28);
    for(int index=31;index<=34;++index)
    {
        const auto y=207+(index-31)*30;
        labels[size_t(index)].setBounds(455,y,218,10);
        sliders[size_t(index)].setBounds(455,y+10,218,20);
    }
    filterMergeLabel.setBounds(455,332,218,10);
    filterMergeSlider.setBounds(455,342,218,20);
    resonanceIndicator.setBounds(sliders[32].getBounds());
    resonanceIndicator.toFront(false);
    for(int i=0;i<2;++i) setCompactButtonBounds(fxTabs[size_t(i)],703+i*97,103,92,27);
    setCompactButtonBounds(syncButton,752,137,44,18);
    verbNoteSlider.setBounds(714,158,74,75);
    driveModeLabel.setBounds(706,145,90,14); driveMode.setBounds(706,159,90,79);
    labels[37].setBounds(802,145,90,14); sliders[37].setBounds(802,159,90,79);
    driveMixLabel.setBounds(707,241,88,14); driveMix.setBounds(707,255,88,79);
    for(size_t i=0;i<driveCutSliders.size();++i)
    {
        const auto x=706+int(i)*96;
        driveCutLabels[i].setBounds(x,350,90,14);
        driveCutSliders[i].setBounds(x,364,90,80);
    }
    setCompactButtonBounds(transformerButton,805,524,90,78);
    labels[36].setBounds(712,522,82,13); sliders[36].setBounds(712,535,82,73);
    for(int i=38;i<44;++i)
    {
        const auto n=i-38,x=706+(n%2)*94,y=145+(n/2)*92;
        labels[size_t(i)].setBounds(x,y,90,14); sliders[size_t(i)].setBounds(x+8,y+13,74,75);
    }
    labels[38].setBounds(706,145,42,14);
    const auto scale=editorScaling::transform(*this,designWidth,designHeight);
    titleHint.setTransform(scale);
    presetMenuButton.setTransform(scale);
    presetNameLabel.setTransform(scale);
    presetPreviousButton.setTransform(scale);
    presetNextButton.setTransform(scale);
    tuneButton.setTransform(scale);
    tuningPanel.setTransform(scale);
    syncButton.setTransform(scale); verbNoteSlider.setTransform(scale);
    for(auto& tab:oscillatorTabs) tab.setTransform(scale);
    noiseButton.setTransform(scale);
    noiseStyle.setTransform(scale); noiseStyleLabel.setTransform(scale);
    for(auto& slider:noiseSliders) slider.setTransform(scale);
    for(auto& label:noiseLabels) label.setTransform(scale);
    for(auto& slider:detuneShapeSliders) slider.setTransform(scale);
    for(auto& label:detuneShapeLabels) label.setTransform(scale);
    for(auto& slider:dutySliders) slider.setTransform(scale);
    for(auto& label:dutyLabels) label.setTransform(scale);
    for(auto& slider:fmAmountSliders) slider.setTransform(scale);
    for(auto& label:fmAmountLabels) label.setTransform(scale);
    for(auto& source:fmSources) source.setTransform(scale);
    for(auto& label:fmSourceLabels) label.setTransform(scale);
    for(auto& button:timesFiveButtons) button.setTransform(scale);
    for(auto& slider:envelopeMergeSliders) slider.setTransform(scale);
    envelopeMergeLabel.setTransform(scale);
    filterMergeSlider.setTransform(scale); filterMergeLabel.setTransform(scale);
    resonanceIndicator.setTransform(scale);
    modulationOverlay.setTransform(scale);
    for(auto& tab:envelopeTabs) tab.setTransform(scale);
    for(auto& tab:fxTabs) tab.setTransform(scale);
    driveMode.setTransform(scale); driveModeLabel.setTransform(scale);
    driveMix.setTransform(scale); driveMixLabel.setTransform(scale);
    for(auto& slider:driveCutSliders) slider.setTransform(scale);
    for(auto& label:driveCutLabels) label.setTransform(scale);
    transformerButton.setTransform(scale);
    for(auto& slider:sliders) slider.setTransform(scale);
    for(auto& slider:curveSliders) slider.setTransform(scale);
    curveLabel.setTransform(scale);
    for(auto& label:labels) label.setTransform(scale);
    for(auto& wave:waves) wave.setTransform(scale);
    for(auto& line:phaseStartLines) line.setTransform(scale);
    for(auto& button:retrig) button.setTransform(scale);
    voiceCount.setTransform(scale);
    for(auto& display:levelDisplays) display.setTransform(scale);
    for(auto& display:octaveDisplays) display.setTransform(scale);
    for(auto& display:fineDisplays) display.setTransform(scale);
    activeVoicesLabel.setTransform(scale);
    polyLegatoButton.setTransform(scale);
    legatoRule.setTransform(scale); legatoRuleLabel.setTransform(scale);
    slideTime.setTransform(scale); slideTimeLabel.setTransform(scale);
    slideShape.setTransform(scale); slideShapeLabel.setTransform(scale);
    for(auto& label:waveLabels) label.setTransform(scale);
    for(auto* box:{&filterType,&modTarget}) box->setTransform(scale);
    modTargetLabel.setTransform(scale);
    if(tuningPanel.isVisible()) tuningPanel.toFront(false);
    modulationOverlay.toFront(false);
}
void QinSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundBlack); g.saveState(); g.addTransform(editorScaling::transform(*this,designWidth,designHeight));
    g.setGradientFill(juce::ColourGradient(backgroundBlack.brighter(.08f),0,0,
                                           backgroundBlack,0,designHeight,false));
    g.fillRect(0,0,designWidth,designHeight);
    g.setColour(plotRed); g.setFont(juce::FontOptions(10.f));
    g.drawText("J-SONIC / VIRTUAL ANALOG INSTRUMENT",18,12,500,16,juce::Justification::centredLeft);
    g.setColour(stageStyle::foreground); g.setFont(juce::FontOptions(25.f));
    g.drawText("QIN SYNTH",18,30,360,38,juce::Justification::centredLeft);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(juce::Rectangle<float>(650,30,252,35),5);
    g.setColour(highlightRed); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    stageStyle::drawPanel(g,{16,90,420,392},panelRed);
    stageStyle::drawPanel(g,{16,492,420,138},panelRed);
    stageStyle::drawPanel(g,{444,90,240,278},panelRed);
    stageStyle::drawPanel(g,{444,376,240,254},panelRed);
    stageStyle::drawPanel(g,{692,90,212,400},panelRed);
    stageStyle::drawPanel(g,{692,500,212,130},panelRed);
    g.setColour(accent); g.setFont(juce::FontOptions(12.f).withStyle("bold"));
    g.drawText("PLAY",30,492,120,20,juce::Justification::centredLeft);
    const juce::Rectangle<float> glidePlot(glidePlotX,glidePlotY,glidePlotSize,glidePlotSize);
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(9.f));
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
    g.setColour(curveBlue.withAlpha(.2f)); g.strokePath(glidePath,juce::PathStrokeType(4.f));
    g.setColour(curveBlue); g.strokePath(glidePath,juce::PathStrokeType(1.8f));
    g.setColour(accent);
    g.drawText("OUTPUT",703,503,120,20,juce::Justification::centredLeft);
    const juce::Rectangle<float> filterPlot(455,138,218,54), envelopePlot(455,409,218,45);
    for(const auto plot:{filterPlot,envelopePlot})
    {
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(plot,4);
        g.setColour(stageStyle::grid.withAlpha(.7f));
        g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
    }
    const auto envIndex=size_t(18+currentEnvelope*4);
    const auto timeScale=(currentEnvelope==1 ? .001f : 1.f)
        *(timesFiveButtons[size_t(currentEnvelope)].getToggleState() ? 5.f : 1.f);
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
    g.setColour(curveBlue.withAlpha(.16f)); g.fillPath(envelopeFill);
    g.setColour(curveBlue); g.strokePath(envelopePath,juce::PathStrokeType(1.7f));
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(8.f));
    g.drawText("0",459,443,28,10,juce::Justification::centredLeft);
    g.drawText(juce::String(total*.5f,1)+"s",536,443,56,10,juce::Justification::centred);
    g.drawText(juce::String(total,1)+"s",617,443,51,10,juce::Justification::centredRight);
    const auto filterMode=std::max(0,filterType.getSelectedId()-1);
    const auto baseCutoff=float(sliders[31].getValue());
    const auto resonance=resonanceModulated ? liveResonance
        : std::clamp(float(sliders[32].getValue())*.01f,0.f,1.f);
    const auto keyFollow=std::clamp(float(sliders[33].getValue())*.01f,0.f,1.f);
    const auto envelopeAmount=std::clamp(float(sliders[34].getValue())*.01f,-1.f,1.f);
    const auto fltSustain=std::clamp(float(sliders[24].getValue())*.01f,0.f,1.f);
    // C5 makes key follow visible; the solid trace shows the held envelope,
    // while the faint trace shows its peak at the start of a note.
    const auto heldCutoff=std::clamp(baseCutoff*std::exp2(keyFollow+QinSynthProcessor::filterEnvOctaves*envelopeAmount*fltSustain),30.f,18000.f);
    const auto peakCutoff=std::clamp(baseCutoff*std::exp2(keyFollow+QinSynthProcessor::filterEnvOctaves*envelopeAmount),30.f,18000.f);
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
        g.setColour(curveBlue.withAlpha(.38f)); g.strokePath(trace(heldCutoff),juce::PathStrokeType(1.f));
        g.setColour(curveBlue); g.strokePath(trace(liveCutoff),juce::PathStrokeType(1.8f));
    }
    else
    {
        if(std::abs(peakCutoff-heldCutoff)>1.f)
        { g.setColour(curveBlue.withAlpha(.42f)); g.strokePath(trace(peakCutoff),juce::PathStrokeType(1.f)); }
        g.setColour(curveBlue); g.strokePath(trace(heldCutoff),juce::PathStrokeType(1.6f));
    }
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(8.f));
    for(const auto frequency:{100.f,1000.f,10000.f})
    {
        const auto x=filterPlot.getX()+3.f+(filterPlot.getWidth()-6.f)
            *std::log(frequency/40.f)/std::log(450.f);
        g.setColour(stageStyle::grid.withAlpha(.55f));
        g.drawVerticalLine(juce::roundToInt(x),filterPlot.getY()+12.f,filterPlot.getBottom()-13.f);
        g.setColour(secondaryRed);
        g.drawText(frequency<1000 ? "100" : frequency<10000 ? "1k" : "10k",
                   juce::roundToInt(x)-15,194,30,11,juce::Justification::centred);
    }
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(9.f));
    const juce::Rectangle<float> oscPlot(oscPlotX,oscPlotY,oscPlotWidth,oscPlotHeight);
    g.setColour(stageStyle::inset); g.fillRoundedRectangle(oscPlot,6);
    for(int line=1;line<4;++line)
    { g.setColour(stageStyle::grid); g.drawHorizontalLine(juce::roundToInt(oscPlotY+line*oscPlotHeight*.25f),oscPlotX,oscPlotX+oscPlotWidth); }
    const auto previewPrefix=currentOscillator==3 ? juce::String("noise")
        : "osc"+juce::String(currentOscillator+1);
    const auto liveOscMod=processor.getLiveModulation();
    const auto liveOscRoute=liveOscMod.target>=qin::legacyModTargetCount
        && liveOscMod.target<qin::legacyModTargetCount+qin::continuousModTargetCount
        ? juce::String(qin::continuousModTargets[liveOscMod.target-qin::legacyModTargetCount].id)
        : juce::String();
    const auto effective=[&](const juce::String& id)
    {
        const auto* raw=processor.parameters.getRawParameterValue(id);
        const auto base=raw!=nullptr ? raw->load(std::memory_order_relaxed) : 0.f;
        if(id!=liveOscRoute) return base;
        const auto* parameter=processor.parameters.getParameter(id);
        return parameter!=nullptr ? parameter->convertFrom0to1(liveOscMod.normalized) : base;
    };
    const auto oscillator=currentOscillator<3;
    const auto level=std::clamp(effective(previewPrefix+"Level")*.01f,0.f,1.f);
    const auto wavePosition=oscillator ? qin::clampWavePosition(
        effective(previewPrefix+"WaveX")*.01f,effective(previewPrefix+"WaveY")*.01f)
        : qin::WavePosition {};
    const auto waveMix=oscillator ? qin::waveWeights(wavePosition)
        : std::array<float,4>{1.f,0.f,0.f,0.f};
    const auto dutyShape=qin::makeDutyShape(oscillator
        ? effective(previewPrefix+"Duty")*.01f : .5f);
    const auto unison=oscillator ? std::clamp(juce::roundToInt(effective(previewPrefix+"Unison")),1,8) : 1;
    const auto detune=oscillator ? effective(previewPrefix+"Detune") : 0.f;
    const auto detuneShape=oscillator ? effective(previewPrefix+"DetuneShape")*.01f : 0.f;
    const auto spread=oscillator ? effective(previewPrefix+"Spread")*.01f : 0.f;
    const auto pan=effective(previewPrefix+"Pan")*.01f;
    const auto fmSource=oscillator ? juce::roundToInt(effective(previewPrefix+"FmSource")) : 0;
    const auto fmAmount=oscillator ? effective(previewPrefix+"FmAmount")*.01f : 0.f;
    const auto sampleWave=[](float phase,const std::array<float,4>& weights,const qin::DutyShape& shape)
    {
        phase-=std::floor(phase);
        const auto sine=weights[3]==0.f ? 0.f
            : weights[3]*std::sin(2.f*juce::MathConstants<float>::pi*phase);
        return float(weights[0]*qin::dutySaw(phase,shape)
            +weights[1]*qin::dutyPulse(phase,shape)
            +weights[2]*qin::dutyTriangle(phase,shape)
            +sine);
    };
    std::array<float,4> sourceMix {};
    qin::DutyShape sourceDuty {};
    if(fmSource>=1 && fmSource<=3)
    {
        const auto sourcePrefix="osc"+juce::String(fmSource);
        sourceMix=qin::waveWeights(qin::clampWavePosition(
            effective(sourcePrefix+"WaveX")*.01f,effective(sourcePrefix+"WaveY")*.01f));
        sourceDuty=qin::makeDutyShape(effective(sourcePrefix+"Duty")*.01f);
    }
    std::array<float,8> unisonRatios {}, unisonMidGains {};
    for(int u=0;u<unison;++u)
    {
        const auto offset=unison==1 ? 0.f : 2.f*u/float(unison-1)-1.f;
        unisonRatios[size_t(u)]=float(std::exp2(
            qin::detunePosition(u,unison,detuneShape)*detune/1200.f));
        const auto voicePan=std::clamp(pan+offset*.35f*spread,-.98f,.98f);
        unisonMidGains[size_t(u)]=(std::sqrt((1.f-voicePan)*.5f)
            +std::sqrt((1.f+voicePan)*.5f))*.70710678f;
    }
    juce::Path waveform;
    uint32_t noisePreviewState=0x87a3d21fu;
    float pinkPreview=0;
    float noiseBand=0,noiseLow=0,noiseHighPassLow=0;
    const auto noiseTune=oscillator ? 0.f : effective("noiseTune");
    const auto noiseHighPass=oscillator ? 20.f : effective("noiseHpf");
    const auto noiseCenter=std::clamp(261.63f*std::exp2(noiseTune/12.f),30.f,12000.f);
    const auto centerStep=1.f-std::exp(-2.f*juce::MathConstants<float>::pi*noiseCenter/48000.f);
    const auto highPassStep=1.f-std::exp(-2.f*juce::MathConstants<float>::pi*noiseHighPass/48000.f);
    float carrierPhase=0;
    for(int point=0;point<=266;++point)
    {
        // An unmodulated oscillator covers exactly one cycle. Modulated
        // oscillators show their effective unison and FM over that period.
        const auto cycle=oscPreviewPhase(point);
        noisePreviewState^=noisePreviewState<<13;
        noisePreviewState^=noisePreviewState>>17;
        noisePreviewState^=noisePreviewState<<5;
        const auto white=float(noisePreviewState>>8)*(1.f/8388607.5f)-1.f;
        pinkPreview=.9f*pinkPreview+.1f*white;
        const auto style=noiseStyle.getSelectedId();
        const auto noiseShape=style==2 ? pinkPreview*2.5f
            : style==3 ? ((noisePreviewState&31u)==0 ? white*2.f : 0.f) : white;
        float shape=0;
        if(!oscillator)
        {
            noiseBand+=centerStep*(noiseShape-noiseBand);
            noiseLow+=centerStep*(noiseBand-noiseLow);
            const auto band=noiseBand-noiseLow;
            noiseHighPassLow+=highPassStep*(band-noiseHighPassLow);
            const auto midGain=(std::sqrt((1.f-pan)*.5f)
                +std::sqrt((1.f+pan)*.5f))*.70710678f;
            shape=std::clamp((band-noiseHighPassLow)*15.f,-1.f,1.f)*level*midGain;
        }
        else
        {
            const auto source=fmSource==4 ? noiseShape
                : fmSource>=1 && fmSource<=3 ? sampleWave(cycle,sourceMix,sourceDuty) : 0.f;
            if(point>0) carrierPhase+=(1.f+fmAmount*std::clamp(source,-1.f,1.f))/266.f;
            for(int u=0;u<unison;++u)
            {
                const auto ratio=unisonRatios[size_t(u)];
                const auto phase=(fmSource>0 ? carrierPhase : cycle)*ratio+15.f*(ratio-1.f);
                shape+=sampleWave(phase,waveMix,dutyShape)*unisonMidGains[size_t(u)]*level/unison;
            }
        }
        const auto x=oscPlotX+2.f+(oscPlotWidth-4.f)*float(point)/266.f;
        const auto y=oscPlot.getCentreY()-40.f*shape;
        if(point==0) waveform.startNewSubPath(x,y); else waveform.lineTo(x,y);
    }
    g.setColour(curveBlue.withAlpha(.22f)); g.strokePath(waveform,juce::PathStrokeType(4));
    g.setColour(curveBlue); g.strokePath(waveform,juce::PathStrokeType(1.5f));
    if(liveOscRoute.startsWith(previewPrefix))
    {
        const auto parameterName=liveOscRoute.substring(previewPrefix.length()).toUpperCase();
        g.setColour(curveBlue.withAlpha(.9f));
        g.setFont(juce::FontOptions(8.f).withStyle("bold"));
        g.drawText("MOD "+parameterName+" "+juce::String(effective(liveOscRoute),1),
            oscPlotX+5,oscPlotY+3,oscPlotWidth-10,11,juce::Justification::centredRight);
    }
    g.setColour(secondaryRed); g.setFont(juce::FontOptions(10.f));
    if(currentFxSection==0)
    {
        const juce::Rectangle<float> scope{float(driveScopeX),float(driveScopeY),
            float(driveScopeWidth),float(driveScopeHeight)};
        g.setColour(stageStyle::inset); g.fillRoundedRectangle(scope,5);
        g.setColour(plotRed); g.setFont(juce::FontOptions(7.f));
        g.drawText("WAVE",805,249,30,10,juce::Justification::centredLeft);
        g.setColour(highlightRed); g.drawText("IN",839,249,16,10,juce::Justification::centred);
        g.setColour(curveBlue); g.drawText("OUT",857,249,27,10,juce::Justification::centred);
        g.setColour(stageStyle::grid); g.drawHorizontalLine(296,804,888);
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
        const auto gain=29.f/std::max(.2f,peak*1.2f);
        const auto drawWave=[&](const auto& samples,juce::Colour colour)
        {
            juce::Path path;
            constexpr size_t displayPoints=80;
            for(size_t index=0;index<displayPoints;++index)
            {
                const auto fraction=float(index)/float(displayPoints-1);
                const auto x=804.f+84.f*fraction;
                const auto sample=qin::waveformSample(samples,window.start+fraction*window.length);
                const auto y=296.f-std::clamp(sample*gain,-29.f,29.f);
                if(index==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.3f));
        };
        if(liveScopeValid)
        { drawWave(liveScope.input,highlightRed.withAlpha(.65f)); drawWave(liveScope.output,curveBlue); }
        else { drawWave(previewInput,highlightRed.withAlpha(.65f)); drawWave(previewOutput,curveBlue); }
    }
    g.restoreState();
}
