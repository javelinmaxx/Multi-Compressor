#include "../Source/PluginProcessor.h"
#include "../../ParameterRetentionTests.h"
#include "../Source/PluginEditor.h"
#include "../Source/WaveformSync.h"
#include "../Source/UnisonDetune.h"
#include "../Source/CurveLookup.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

struct QinSynthTestAccess
{
    static int active(const QinSynthProcessor& p)
    { int count=0; for(const auto& voice:p.voices) if(voice.active) ++count; return count; }
    static int note(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].note; }
    static bool held(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].keyDown; }
    static double pitch(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].pitchNote; }
    static float voicePan(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].pan; }
    static float glideIncrement(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].glideIncrement; }
    static double glideStart(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].glideStart; }
    static float glideProgress(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].glideProgress; }
    static float tap(const QinSynthProcessor& p) { return p.onsetTap; }
    static float phase(const QinSynthProcessor& p) { return p.onsetPhase; }
    static uint32_t analysisJobs(const QinSynthProcessor& p) { return p.reverb.analysisJobs(); }
    static bool transformerRunning(const QinSynthProcessor& p) { return p.transformerStage.isRunning(); }
    static int monoNote(const QinSynthProcessor& p) { return p.voices[0].note; }
    static bool monoHeld(const QinSynthProcessor& p) { return p.voices[0].keyDown; }
    static double monoPhase(const QinSynthProcessor& p) { return p.voices[0].oscillators[0][0].phase; }
    static float monoEnvelope(const QinSynthProcessor& p) { return p.voices[0].env[0].value; }
    static float effectiveEnvelope(const QinSynthProcessor& p,int voice,int envelope)
    { return p.voices[size_t(voice)].effectiveEnv[size_t(envelope)]; }
    static bool voiceActive(const QinSynthProcessor& p,int voice) { return p.voices[size_t(voice)].active; }
    static double secondPhase(const QinSynthProcessor& p,int osc)
    { return p.voices[1].oscillators[size_t(osc)][0].phase; }
    static bool allRetriggeredPhasesMatch(const QinSynthProcessor& p)
    {
        for(const auto& oscillator:p.voices[0].oscillators)
            for(const auto& subvoice:oscillator)
                if(std::abs(subvoice.phase)>1.e-12 || std::abs(subvoice.triangle)>1.e-12) return false;
        return true;
    }
    static void noteOn(QinSynthProcessor& p,int note)
    { p.handleMidi(juce::MidiMessage::noteOn(1,note,.7f),p.readSettings()); }
    static void noteOff(QinSynthProcessor& p,int note)
    { p.handleMidi(juce::MidiMessage::noteOff(1,note),p.readSettings()); }
    static std::array<float,3> envelopeTimes(const QinSynthProcessor& p,int index)
    { const auto settings=p.readSettings(); const auto& e=settings.env[size_t(index)]; return {e.attack,e.decay,e.release}; }
    static float detuneShape(const QinSynthProcessor& p,int oscillator)
    { return p.readSettings().oscillator[size_t(oscillator)].detuneShape; }
    static double detuneRatio(const QinSynthProcessor& p,int oscillator,int unisonVoice)
    { return p.detuneRatios[size_t(oscillator)][size_t(unisonVoice)]; }
    static double oscillatorPhase(const QinSynthProcessor& p,int oscillator,int unisonVoice)
    { return p.voices[0].oscillators[size_t(oscillator)][size_t(unisonVoice)].phase; }
    static double tunedFrequency(const QinSynthProcessor& p,int note)
    { return p.readSettings().tuning.frequency(note); }
    static float delayTime(const QinSynthProcessor& p) { return p.readSettings().verbTime; }
    static float modDepth(const QinSynthProcessor& p) { return p.smoothedModDepth; }
    static float coefficient(const QinSynthProcessor& p,float hz) { return p.filterCoefficient(hz); }
    static double sallenGain(QinSynthProcessor& p,float resonance)
    {
        QinSynthProcessor::FilterState state;
        constexpr float frequency=500.f, rate=48000.f;
        const auto g=p.filterCoefficient(frequency), onePole=g/(1.f+g);
        double inPower=0,outPower=0;
        for(int i=0;i<8192;++i)
        {
            const auto input=.02f*std::sin(juce::MathConstants<float>::twoPi*frequency*i/rate);
            const auto output=p.filterSample(state,input,g,onePole,resonance,2);
            if(i>=4096) { inPower+=input*input; outPower+=output*output; }
        }
        return std::sqrt(outPower/inPower);
    }
    static bool stableFilters(const QinSynthProcessor& p)
    {
        for(const auto& filter:p.masterFilter)
            for(const auto value:{filter.s1,filter.s2,filter.t1,filter.t2,filter.pole})
                if(!std::isfinite(value) || std::abs(value)>16.f) return false;
        for(const auto& voice:p.voices)
            for(const auto& filter:voice.filter)
            {
                for(const auto value:{filter.s1,filter.s2,filter.t1,filter.t2,filter.pole})
                    if(!std::isfinite(value) || std::abs(value)>16.f) return false;
                for(const auto value:filter.ladder)
                    if(!std::isfinite(value) || std::abs(value)>16.f) return false;
            }
        return true;
    }
};
struct QinSynthEditorTestAccess
{
    static void oscillator(QinSynthEditor& editor,int index) { editor.showOscillator(index); }
    static void envelope(QinSynthEditor& editor,int index) { editor.showEnvelope(index); }
    static void fxSection(QinSynthEditor& editor,int index) { editor.showFxSection(index); }
    static bool oscillatorVisible(const QinSynthEditor& editor,int index)
    { return editor.waves[size_t(index)].isVisible() && editor.retrig[size_t(index)].isVisible()
        && editor.levelDisplays[size_t(index)].isVisible(); }
    static bool voiceDragEnabled(const QinSynthEditor& editor)
    { return editor.voiceCount.isVisible()
        && editor.voiceCount.getSliderStyle()==juce::Slider::RotaryVerticalDrag
        && std::abs(editor.voiceCount.getMinimum()-1)<1.e-6
        && std::abs(editor.voiceCount.getMaximum()-16)<1.e-6; }
    static bool wavePadEnabled(const QinSynthEditor& editor,int index)
    { return editor.waves[size_t(index)].getWidth()==96 && editor.waves[size_t(index)].getHeight()==96
        && editor.waves[size_t(index)].getX()>304; }
    static void setVoiceCount(QinSynthEditor& editor,int count)
    { editor.voiceCount.setValue(count,juce::sendNotificationSync); }
    static void setWave(QinSynthEditor& editor,int index,qin::WavePosition position)
    { auto& pad=editor.waves[size_t(index)]; pad.setPosition(position); pad.onPositionChange(pad.getPosition()); }
    static qin::WavePosition wavePosition(const QinSynthEditor& editor,int index)
    { return editor.waves[size_t(index)].getPosition(); }
    static void setRetrig(QinSynthEditor& editor,int index,bool enabled)
    { editor.retrig[size_t(index)].setToggleState(enabled,juce::sendNotificationSync); }
    static void setDetuneShape(QinSynthEditor& editor,int index,float shape)
    { editor.detuneShapeSliders[size_t(index)].setValue(shape,juce::sendNotificationSync); }
    static bool oscillatorLayout(const QinSynthEditor& editor)
    { return editor.levelDisplays[0].getY()<editor.sliders[2].getY()
        && editor.fineDisplays[0].getY()<editor.sliders[2].getY()
        && editor.sliders[35].getY()>editor.sliders[2].getY()
        && editor.detuneShapeSliders[0].isVisible()
        && editor.levelDisplays[0].getRight()<editor.fineDisplays[0].getX()
        && editor.retrig[0].getButtonText()=="RETRIG"
        && editor.retrig[0].getHeight()<=30; }
    static bool fineReadoutLayout(const QinSynthEditor& editor,int osc)
    {
        const auto& fine=editor.fineDisplays[size_t(osc)];
        const auto& octave=editor.octaveDisplays[size_t(osc)];
        return fine.isVisible() && fine.getRight()<octave.getX()
            && fine.getY()==octave.getY() && fine.getReadoutText().startsWith("FINE ")
            && !editor.sliders[size_t(osc*6+1)].isVisible();
    }
    static bool compactHeaderLayout(const QinSynthEditor& editor)
    {
        return editor.filterType.getY()<120 && editor.fxTabs[0].getY()<120
            && editor.envelopeTabs[0].getY()<435
            && editor.sliders[31].getWidth()>200
            && editor.filterMergeSlider.getWidth()>200;
    }
    static bool voiceFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[37].isVisible() && editor.sliders[36].isVisible(); }
    static bool delayFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[38].isVisible() && editor.sliders[43].isVisible(); }
    static bool driveVisible(const QinSynthEditor& editor)
    { return editor.sliders[37].isVisible() && editor.driveMode.isVisible() && editor.driveMix.isVisible()
        && editor.driveMix.getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag; }
    static bool transformerVisible(const QinSynthEditor& editor) { return editor.transformerButton.isVisible(); }
    static bool outputSectionVisible(const QinSynthEditor& editor)
    { return editor.transformerButton.isVisible() && editor.sliders[36].isVisible()
        && editor.transformerButton.getY()>=500 && editor.sliders[36].getY()>=500; }
    static bool compactFxKnobs(const QinSynthEditor& editor)
    { return editor.sliders[37].getHeight()<80 && editor.sliders[38].getHeight()<80; }
    static int driveModeCount(const QinSynthEditor& editor)
    { return editor.driveMode.getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag
        ? juce::roundToInt(editor.driveMode.getMaximum()-editor.driveMode.getMinimum())+1 : 0; }
    static void selectDriveMode(QinSynthEditor& editor,int item)
    { editor.driveMode.setValue(item-1,juce::sendNotificationSync); }
    static void setDriveMix(QinSynthEditor& editor,double mix)
    { editor.driveMix.setValue(mix,juce::sendNotificationSync); }
    static void setTransformer(QinSynthEditor& editor,bool on)
    { editor.transformerButton.setToggleState(on,juce::sendNotificationSync); }
    static bool envelopeVisible(const QinSynthEditor& editor,int index)
    { return editor.sliders[size_t(18+index*4)].isVisible(); }
    static bool envelopeMergeVisible(const QinSynthEditor& editor,int index)
    { return editor.envelopeMergeSliders[size_t(index)].isVisible()
        && editor.envelopeMergeSliders[size_t(index)].getWidth()>150
        && editor.envelopeMergeSliders[size_t(index)].getY()>editor.sliders[size_t(18+index*4)].getY(); }
    static void setEnvelopeMerge(QinSynthEditor& editor,int index,float value)
    { editor.envelopeMergeSliders[size_t(index)].setValue(value,juce::sendNotificationSync); }
    static bool filterVisible(const QinSynthEditor& editor)
    { return editor.filterType.isVisible() && editor.sliders[31].isVisible(); }
    static bool modVisible(const QinSynthEditor& editor)
    { return editor.sliders[30].isVisible() && editor.modTarget.isVisible(); }
    static int modDestinationCount(const QinSynthEditor& editor)
    { return editor.modTarget.getNumItems(); }
    static void selectModDestination(QinSynthEditor& editor,int index)
    { editor.modTarget.setSelectedId(index+1,juce::sendNotificationSync); }
    static bool noiseVisible(const QinSynthEditor& editor) { return editor.noiseButton.isVisible(); }
    static void enableNoise(QinSynthEditor& editor)
    { editor.noiseButton.setToggleState(true,juce::sendNotificationSync); }
    static bool noisePanelVisible(const QinSynthEditor& editor)
    { return editor.noiseStyle.isVisible() && editor.noiseSliders[0].isVisible() && !editor.waves[0].isVisible(); }
    static bool fmVisible(const QinSynthEditor& editor,int osc)
    { return editor.fmAmountSliders[size_t(osc)].isVisible() && editor.fmSources[size_t(osc)].isVisible(); }
    static bool tuningMenuVisible(const QinSynthEditor& editor) { return editor.tuningPanel.isVisible(); }
    static void openTuningMenu(QinSynthEditor& editor)
    { editor.tuneButton.setToggleState(true,juce::sendNotificationSync); editor.tuneButton.onClick(); }
    static void closeTuningMenu(QinSynthEditor& editor)
    { editor.tuneButton.setToggleState(false,juce::sendNotificationSync); editor.tuneButton.onClick(); }
    static void setTuning(QinSynthEditor& editor,int degree,float cents)
    { editor.tuningSliders[size_t(degree)].setValue(cents,juce::sendNotificationSync); }
    static void setTuningRoot(QinSynthEditor& editor,int root)
    { editor.tuningRoot.setValue(root,juce::sendNotificationSync); }
    static bool tuningLabel(const QinSynthEditor& editor,int degree,const juce::String& text)
    { return editor.tuningSliders[size_t(degree)].getNoteName()==text; }
    static bool tuningPianoKeys(const QinSynthEditor& editor)
    {
        const auto& root=editor.tuningSliders[0];
        const auto& firstSharp=editor.tuningSliders[1];
        return root.isRootKey() && !root.isEnabled() && std::abs(root.getValue())<1.e-6
            && firstSharp.isBlackKey() && firstSharp.getHeight()<root.getHeight()
            && firstSharp.getX()>root.getX();
    }
    static bool octaveReadoutVisible(const QinSynthEditor& editor,int osc)
    { return editor.octaveDisplays[size_t(osc)].isVisible(); }
    static void setLevel(QinSynthEditor& editor,int osc,float value)
    { editor.levelDisplays[size_t(osc)].setValue(value,juce::sendNotificationSync); }
    static bool levelReadoutVisible(const QinSynthEditor& editor,int osc)
    { return editor.levelDisplays[size_t(osc)].isVisible()
        && !editor.sliders[size_t(osc*6)].isVisible(); }
    static bool levelShows(const QinSynthEditor& editor,int osc,const juce::String& text)
    { return editor.levelDisplays[size_t(osc)].getReadoutText()==text; }
    static void setOctave(QinSynthEditor& editor,int osc,int octave)
    { editor.octaveDisplays[size_t(osc)].setValue(octave,juce::sendNotificationSync); }
    static void setFine(QinSynthEditor& editor,int osc,float cents)
    { editor.fineDisplays[size_t(osc)].setValue(cents,juce::sendNotificationSync); }
    static bool fineShowsCents(const QinSynthEditor& editor,int osc,const juce::String& text)
    { return editor.fineDisplays[size_t(osc)].getReadoutText()==text; }
    static bool resonanceIndicatorActive(const QinSynthEditor& editor)
    { return editor.resonanceIndicator.isAnimating(); }
    static bool modulationOverlayActive(const QinSynthEditor& editor)
    { return editor.modulationOverlay.isAnimating(); }
    static bool modulationOverlayOnLevel(const QinSynthEditor& editor)
    { return editor.modulationOverlay.isAnimating()
        && editor.modulationOverlay.getBounds()==editor.levelDisplays[0].getBounds(); }
    static bool tuneButtonBesideVoices(const QinSynthEditor& editor)
    { return editor.tuneButton.getX()<editor.voiceCount.getX(); }
    static void showDelay(QinSynthEditor& editor) { editor.showFxSection(1); }
    static void showDrive(QinSynthEditor& editor) { editor.showFxSection(0); }
    static void setSync(QinSynthEditor& editor,bool enabled)
    { editor.syncButton.setToggleState(enabled,juce::sendNotificationSync); editor.updateVisibility(); }
    static void setVerbNote(QinSynthEditor& editor,int note)
    { editor.verbNoteSlider.setValue(note,juce::sendNotificationSync); }
    static bool syncNoteVisible(const QinSynthEditor& editor)
    { return editor.verbNoteSlider.isVisible() && !editor.sliders[38].isVisible(); }
    static bool fmGridLayout(const QinSynthEditor& editor)
    {
        for(int osc=0;osc<3;++osc)
        {
            const auto& source=editor.fmSources[size_t(osc)];
            const auto& amount=editor.fmAmountSliders[size_t(osc)];
            const auto& shape=editor.detuneShapeSliders[size_t(osc)];
            if(source.getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag
               || source.getY()!=shape.getY() || amount.getY()!=editor.sliders[35].getY()
               || source.getX()<=shape.getX() || amount.getX()<=editor.sliders[35].getX()
               || amount.getBounds().intersects(editor.sliders[35].getBounds())) return false;
            const std::array<juce::Rectangle<int>,8> controls {
                editor.sliders[size_t(osc*6+2)].getBounds(),
                editor.sliders[size_t(osc*6+3)].getBounds(),
                shape.getBounds(),source.getBounds(),
                editor.sliders[size_t(osc*6+4)].getBounds(),
                editor.sliders[size_t(osc*6+5)].getBounds(),
                editor.sliders[35].getBounds(),amount.getBounds()};
            for(size_t i=0;i<controls.size();++i)
                for(size_t j=i+1;j<controls.size();++j)
                    if(controls[i].intersects(controls[j])) return false;
        }
        return true;
    }
    static void setFmAmount(QinSynthEditor& editor,int osc,float amount)
    { editor.fmAmountSliders[size_t(osc)].setValue(amount,juce::sendNotificationSync); }
    static void selectFmSource(QinSynthEditor& editor,int osc,int item)
    { editor.fmSources[size_t(osc)].setValue(item-1,juce::sendNotificationSync); }
    static int noiseStyleCount(const QinSynthEditor& editor) { return editor.noiseStyle.getNumItems(); }
    static void selectNoiseStyle(QinSynthEditor& editor,int style)
    { editor.noiseStyle.setSelectedId(style,juce::sendNotificationSync); }
    static bool curveVisible(const QinSynthEditor& editor,int index)
    { return editor.curveSliders[size_t(index)].isVisible(); }
    static bool mergeVisible(const QinSynthEditor& editor)
    { return editor.filterMergeSlider.isVisible(); }
    static bool timesFiveVisible(const QinSynthEditor& editor)
    {
        for(size_t index=0;index<editor.timesFiveButtons.size();++index)
            if(editor.timesFiveButtons[index].isVisible()!=(int(index)==editor.currentEnvelope)) return false;
        return true;
    }
    static void timesFive(QinSynthEditor& editor)
    {
        auto& button=editor.timesFiveButtons[size_t(editor.currentEnvelope)];
        button.setToggleState(!button.getToggleState(),juce::sendNotificationSync);
    }
    static void tick(QinSynthEditor& editor) { editor.timerCallback(); }
    static bool playVisible(const QinSynthEditor& editor)
    { return editor.polyLegatoButton.isVisible() && editor.legatoRule.isVisible()
        && editor.slideTime.isVisible() && editor.slideShape.isVisible()
        && editor.slideTime.getY()>=510; }
    static int legatoRules(const QinSynthEditor& editor)
    { return juce::roundToInt(editor.legatoRule.getMaximum()-editor.legatoRule.getMinimum())+1; }
    static bool ruleEnabled(const QinSynthEditor& editor) { return editor.legatoRule.isEnabled(); }
    static void setPolyLegato(QinSynthEditor& editor,bool enabled)
    { editor.polyLegatoButton.setToggleState(enabled,juce::sendNotificationSync); editor.updatePlayAvailability(); }
    static void selectLegatoRule(QinSynthEditor& editor,int item)
    { editor.legatoRule.setValue(item-1,juce::sendNotificationSync); }
    static void setSlide(QinSynthEditor& editor,double time,double shape)
    { editor.slideTime.setValue(time,juce::sendNotificationSync); editor.slideShape.setValue(shape,juce::sendNotificationSync); }
    static juce::String activeLabel(const QinSynthEditor& editor) { return editor.activeVoicesLabel.getText(); }
    static juce::String slideLabel(QinSynthEditor& editor)
    { return editor.slideTime.getTextFromValue(editor.slideTime.getValue()); }
    static juce::String shapeLabel(QinSynthEditor& editor)
    { return editor.slideShape.getTextFromValue(editor.slideShape.getValue()); }
    static bool liveScope(const QinSynthEditor& editor) { return editor.liveScopeValid; }
    static float liveCutoff(const QinSynthEditor& editor) { return editor.liveCutoff; }
    static bool shapeControlsAreSliders(const QinSynthEditor& editor)
    {
        for(int index=18;index<30;++index)
            if(editor.sliders[size_t(index)].getSliderStyle()!=juce::Slider::LinearVertical) return false;
        for(const auto& curve:editor.curveSliders)
            if(curve.getSliderStyle()!=juce::Slider::LinearVertical) return false;
        for(int index=31;index<=34;++index)
            if(editor.sliders[size_t(index)].getSliderStyle()!=juce::Slider::LinearHorizontal) return false;
        return true;
    }
    static void previewValue(QinSynthEditor& editor,int index,double value)
    { editor.sliders[size_t(index)].setValue(value,juce::sendNotificationSync); }
    static double sliderMidpoint(QinSynthEditor& editor,int index)
    { return editor.sliders[size_t(index)].proportionOfLengthToValue(.5); }
    static double envelopeTextValue(QinSynthEditor& editor,int index,const juce::String& text)
    { return editor.sliders[size_t(index)].getValueFromText(text); }
    static void previewFilter(QinSynthEditor& editor,int item)
    { editor.filterType.setSelectedId(item,juce::sendNotificationSync); }
    static void previewCurve(QinSynthEditor& editor,int index,double value)
    { editor.curveSliders[size_t(index)].setValue(value,juce::sendNotificationSync); }
};
static void set(QinSynthProcessor& processor,const char* id,float value)
{
    auto* parameter=processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static double power(const juce::AudioBuffer<float>& buffer,int channel,int first=0)
{
    double sum=0;
    for(int i=first;i<buffer.getNumSamples();++i)
    {
        const auto x=buffer.getSample(channel,i);
        if(!std::isfinite(x) || std::abs(x)>8) throw std::runtime_error("Synth output must remain finite and bounded");
        sum+=double(x)*x;
    }
    return sum;
}
class QinTempoPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<double> bpm {120.0};
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo position; position.setBpm(bpm); return position; }
};
int main(int argc,char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        {
            auto tunedOwner=std::make_unique<QinSynthProcessor>();
            auto& tuned=*tunedOwner;
            set(tuned,"tuneA4",432); set(tuned,"tune9",50);
            set(tuned,"osc1Retrig",1); set(tuned,"verbMix",0);
            tuned.prepareToPlay(48000,32);
            juce::AudioBuffer<float> audio(2,32); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,69,.8f),0);
            tuned.processBlock(audio,note);
            const auto expected=432.0*std::exp2(50.0/1200.0);
            if(std::abs(QinSynthTestAccess::tunedFrequency(tuned,69)-expected)>.01
               || std::abs(QinSynthTestAccess::oscillatorPhase(tuned,0,0)-expected*32/48000)>.0001)
                throw std::runtime_error("Custom pitch-class and A4 tuning must change oscillator frequency");
            set(tuned,"tune1",35);
            if(std::abs(QinSynthTestAccess::tunedFrequency(tuned,61)
                -juce::MidiMessage::getMidiNoteInHertz(61,432.0)*std::exp2(35.0/1200.0))>.01)
                throw std::runtime_error("A cent deviation must follow its degree above C");
            set(tuned,"tuneRoot",2); set(tuned,"tune9",0);
            if(std::abs(QinSynthTestAccess::tunedFrequency(tuned,63)
                -juce::MidiMessage::getMidiNoteInHertz(63,432.0)*std::exp2(35.0/1200.0))>.01
               || std::abs(QinSynthTestAccess::tunedFrequency(tuned,62)
                -juce::MidiMessage::getMidiNoteInHertz(62,432.0))>.01)
                throw std::runtime_error("Changing root must transpose deviations and lock the new root to zero cents");
            juce::MemoryBlock saved;
            tuned.getStateInformation(saved);
            auto restoredOwner=std::make_unique<QinSynthProcessor>();
            auto& restored=*restoredOwner;
            restored.setStateInformation(saved.getData(),int(saved.getSize()));
            if(std::abs(QinSynthTestAccess::tunedFrequency(restored,63)-QinSynthTestAccess::tunedFrequency(tuned,63))>.01)
                throw std::runtime_error("Custom tuning must survive project recall");
            auto fineOwner=std::make_unique<QinSynthProcessor>();
            auto& fine=*fineOwner;
            set(fine,"osc1Retrig",1); set(fine,"osc1Fine",1200); set(fine,"osc1Octave",-1);
            set(fine,"verbMix",0); fine.prepareToPlay(48000,32);
            juce::MidiBuffer fineNote; fineNote.addEvent(juce::MidiMessage::noteOn(1,69,.8f),0);
            fine.processBlock(audio,fineNote);
            if(std::abs(QinSynthTestAccess::oscillatorPhase(fine,0,0)-440.0*32/48000)>.0001)
                throw std::runtime_error("Fine pitch and octave transpose must combine in cents");
        }
        {
            QinTempoPlayHead host;
            auto syncedOwner=std::make_unique<QinSynthProcessor>();
            auto& synced=*syncedOwner;
            synced.setPlayHead(&host);
            set(synced,"verbSync",1); set(synced,"verbNote",13);
            synced.prepareToPlay(48000,64);
            juce::AudioBuffer<float> audio(2,64); juce::MidiBuffer midi;
            host.bpm=90.0; synced.processBlock(audio,midi);
            if(std::abs(QinSynthTestAccess::delayTime(synced)-60000.0/90.0)>.01)
                throw std::runtime_error("Synced Delay Time must follow host quarter-note tempo");
            host.bpm=120.0; synced.processBlock(audio,midi);
            if(std::abs(QinSynthTestAccess::delayTime(synced)-500.0)>.01)
                throw std::runtime_error("Synced Delay Time must follow tempo changes");
            host.bpm=juce::Optional<double>{}; synced.processBlock(audio,midi);
            if(std::abs(synced.getHostTempo()-120.0)>.01)
                throw std::runtime_error("Missing host tempo must retain last valid tempo");
            set(synced,"verbSync",0); set(synced,"time",137);
            if(std::abs(QinSynthTestAccess::delayTime(synced)-137.0)>.01)
                throw std::runtime_error("Delay Time must retain its free value when Sync is off");
            synced.setPlayHead(nullptr);
        }
        {
            const auto forward=qin::FmRouting::calculate({-1,0,1});
            const auto reverse=qin::FmRouting::calculate({2,-1,1});
            const auto cycle=qin::FmRouting::calculate({1,0,1});
            const auto self=qin::FmRouting::calculate({0,-1,-1});
            if(forward.order!=std::array<int,3>{0,1,2}
               || reverse.order!=std::array<int,3>{1,2,0}
               || forward.delayedEdge[0] || forward.delayedEdge[1] || forward.delayedEdge[2]
               || !cycle.delayedEdge[0] || !cycle.delayedEdge[1] || cycle.delayedEdge[2]
               || cycle.order[2]!=2 || !self.delayedEdge[0])
                throw std::runtime_error("FM precedence must stack free-order sources and delay cycle edges");
            auto storedOwner=std::make_unique<QinSynthProcessor>();
            auto& stored=*storedOwner;
            set(stored,"osc1FmSource",4); set(stored,"osc2FmSource",3);
            set(stored,"osc3FmSource",1); set(stored,"osc2FmAmount",73);
            juce::MemoryBlock state;
            stored.getStateInformation(state);
            auto restoredOwner=std::make_unique<QinSynthProcessor>();
            auto& restored=*restoredOwner;
            restored.setStateInformation(state.getData(),int(state.getSize()));
            if(std::abs(restored.parameters.getRawParameterValue("osc1FmSource")->load()-4)>1.e-6
               || std::abs(restored.parameters.getRawParameterValue("osc2FmSource")->load()-3)>1.e-6
               || std::abs(restored.parameters.getRawParameterValue("osc3FmSource")->load()-1)>1.e-6
               || std::abs(restored.parameters.getRawParameterValue("osc2FmAmount")->load()-73)>.1)
                throw std::runtime_error("FM sources and amount must survive host state recall");
        }
        for(int c=-100;c<=100;c+=5)
            for(int i=0;i<=1000;++i)
            {
                const auto progress=i==1 ? 1.e-8 : double(i)/1000;
                for(int stage=0;stage<2;++stage)
                {
                    const auto slope=stage==0 ? 6.9 : 5.0;
                    const auto base=(1-std::exp(-slope*progress))/(1-std::exp(-slope));
                    const auto expected=std::pow(base,std::exp2(c*.02));
                    if(std::abs(qin::curveLookup.envelopeAt(float(progress),c*.01f,stage==0)-expected)>1.e-4)
                        throw std::runtime_error("Envelope lookup must preserve the curve under long attacks and automation");
                }
            }
        for(int i=0;i<8192;++i)
        {
            const auto phase=double(i)/8192;
            if(std::abs(qin::curveLookup.sineAt(phase)-std::sin(2*juce::MathConstants<double>::pi*phase))>1.e-6)
                throw std::runtime_error("Oscillator sine lookup must preserve phase and amplitude");
        }
        for(const auto& [position,expected]:{
                std::pair{qin::WavePosition{0.f,0.f},std::array<float,4>{1.f,0.f,0.f,0.f}},
                std::pair{qin::WavePosition{1.f,0.f},std::array<float,4>{0.f,1.f,0.f,0.f}},
                std::pair{qin::WavePosition{0.f,1.f},std::array<float,4>{0.f,0.f,1.f,0.f}},
                std::pair{qin::WavePosition{1.f,1.f},std::array<float,4>{0.f,0.f,0.f,1.f}},
                std::pair{qin::WavePosition{.5f,.5f},std::array<float,4>{.25f,.25f,.25f,.25f}}})
        {
            const auto actual=qin::waveWeights(position);
            for(size_t index=0;index<actual.size();++index)
                if(std::abs(actual[index]-expected[index])>1.e-5f)
                    throw std::runtime_error("Square pad must use bilinear waveform blends");
        }
        for(int voice=0;voice<8;++voice)
        {
            const auto linear=qin::detunePosition(voice,8,0.f);
            const auto middle=qin::detunePosition(voice,8,-1.f);
            const auto edges=qin::detunePosition(voice,8,1.f);
            if(std::abs(linear-(2.0*voice/7.0-1.0))>1.e-10
               || std::abs(middle)>std::abs(linear)+1.e-10
               || std::abs(edges)+1.e-10<std::abs(linear)
               || std::abs(middle+qin::detunePosition(7-voice,8,-1.f))>1.e-10)
                throw std::runtime_error("Detune Shape must preserve symmetric endpoints and redistribute inner voices");
        }
        if(std::abs(qin::detunePosition(0,1,1.f))>1.e-10)
            throw std::runtime_error("One unison voice must stay centered");
        for(const auto period:{27.3f,32.f,64.f,184.f,368.5f})
        {
            std::array<float,QinSynthProcessor::driveScopeCapturePoints> samples {};
            for(size_t index=0;index<samples.size();++index)
            {
                const auto phase=2.f*juce::MathConstants<float>::pi*(float(index)/period+.23f);
                samples[index]=std::sin(phase)+.2f*std::sin(phase*2.f);
            }
            const auto window=qin::waveformWindow(samples);
            if(!window.synchronized || std::abs(window.length-2.f*period)>.5f
               || std::abs(qin::waveformSample(samples,window.start)
                    -qin::waveformSample(samples,window.start+window.length))>.02f)
                throw std::runtime_error("Drive waveform must align complete input periods");
        }
        if(qin::waveformWindow(std::array<float,QinSynthProcessor::driveScopeCapturePoints>{}).synchronized)
            throw std::runtime_error("Silent drive scope must not invent a period");
        std::array<float,QinSynthProcessor::driveScopeCapturePoints> noiseScope {};
        uint32_t noiseState=0x691ade8bu;
        for(auto& sample:noiseScope)
        {
            noiseState^=noiseState<<13; noiseState^=noiseState>>17; noiseState^=noiseState<<5;
            sample=float(noiseState>>8)*(1.f/8388607.5f)-1.f;
        }
        if(qin::waveformWindow(noiseScope).synchronized)
            throw std::runtime_error("Noise scope must not invent a waveform period");
        verifyParameterRetention<QinSynthProcessor>();
        auto synthOwner=std::make_unique<QinSynthProcessor>();
        auto& synth=*synthOwner;
        set(synth,"osc1DetuneShape",-100.f);
        if(std::abs(QinSynthTestAccess::detuneShape(synth,0)+1.f)>1.e-6f)
            throw std::runtime_error("Detune Shape parameter must reach the oscillator settings");
        juce::MemoryBlock detuneState;
        synth.getStateInformation(detuneState);
        auto restoredDetune=std::make_unique<QinSynthProcessor>();
        restoredDetune->setStateInformation(detuneState.getData(),int(detuneState.getSize()));
        if(std::abs(QinSynthTestAccess::detuneShape(*restoredDetune,0)+1.f)>1.e-6f)
            throw std::runtime_error("Detune Shape must survive session recall");
        set(synth,"osc1DetuneShape",0.f);
        set(synth,"verbMix",0); synth.prepareToPlay(48000,256);
        for(const auto hz:{30.f,250.f,1000.f,10000.f,18000.f})
        {
            const auto expected=std::tan(juce::MathConstants<double>::pi*hz/48000.);
            if(std::abs(QinSynthTestAccess::coefficient(synth,hz)-expected)>expected*.001)
                throw std::runtime_error("Filter coefficient lookup must track the bilinear transform");
        }
        if(QinSynthTestAccess::sallenGain(synth,1.f)<QinSynthTestAccess::sallenGain(synth,0.f)*2.)
            throw std::runtime_error("Sallen-Key feedback must produce a stable resonant peak");
        if(!synth.acceptsMidi() || !synth.isBusesLayoutSupported(synth.getBusesLayout()))
            throw std::runtime_error("Qin must expose a stereo MIDI instrument bus");
        const auto cutoffMid=synth.parameters.getParameter("filterCutoff")->convertFrom0to1(.5f);
        auto* resonance=synth.parameters.getParameter("filterResonance");
        const auto resonanceMid=resonance->convertFrom0to1(.5f);
        const auto lowSpan=resonance->convertFrom0to1(.2f)-resonance->convertFrom0to1(.1f);
        const auto highSpan=resonance->convertFrom0to1(.9f)-resonance->convertFrom0to1(.8f);
        if(std::abs(cutoffMid-std::sqrt(30.f*18000.f))>2.f
           || std::abs(resonanceMid-100.f*std::log(51.f)/std::log(101.f))>.2f
           || highSpan>=lowSpan*.3f)
            throw std::runtime_error("Resonance slider must allocate finer resolution to high values");
        for(const auto value:{0.f,25.f,75.f,95.f,99.9f,100.f})
            if(std::abs(resonance->convertFrom0to1(resonance->convertTo0to1(value))-value)>.11f)
                throw std::runtime_error("Resonance mapping must round-trip displayed percentages");
        set(synth,"filterResonance",95.f);
        juce::MemoryBlock resonanceState;
        synth.getStateInformation(resonanceState);
        auto restoredResonance=std::make_unique<QinSynthProcessor>();
        restoredResonance->setStateInformation(resonanceState.getData(),int(resonanceState.getSize()));
        if(std::abs(restoredResonance->parameters.getRawParameterValue("filterResonance")->load()-95.f)>.11f)
            throw std::runtime_error("Saved high resonance must reload at the same physical value");
        set(synth,"filterResonance",0.f);
        for(const auto* id:{"fltAttack","fltDecay","fltRelease"})
            if(std::abs(synth.parameters.getParameter(id)->convertFrom0to1(.5f)-std::sqrt(5000.f))>1.f)
                throw std::runtime_error("Filter envelope time sliders must use logarithmic 1–5000 ms positions");
        for(const auto* id:{"attack","ampDecay","release","modAttack","modDecay","modRelease"})
            if(std::abs(synth.parameters.getParameter(id)->convertFrom0to1(.5f)*1000.f-std::sqrt(5000.f))>1.f)
                throw std::runtime_error("AMP and MOD envelope time sliders must use logarithmic 1–5000 ms positions");
        if(std::abs(synth.parameters.getParameter("fltSustain")->convertFrom0to1(.5f)
                    -(std::sqrt(101.f)-1.f))>.2f)
            throw std::runtime_error("Filter sustain slider must have finer resolution near zero");
        juce::AudioBuffer<float> block(2,256); juce::MidiBuffer midi;
        synth.processBlock(block,midi);
        if(power(block,0)>1.e-12 || power(block,1)>1.e-12) throw std::runtime_error("Idle instrument must be silent");
        midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),64);
        synth.processBlock(block,midi);
        if(power(block,0,64)<=1.e-12) throw std::runtime_error("Note-on must create audio");
        for(int i=0;i<64;++i)
            if(std::abs(block.getSample(0,i))>1.e-7f || std::abs(block.getSample(1,i))>1.e-7f)
                throw std::runtime_error("MIDI note-on must start at its sample offset");
        if(QinSynthTestAccess::active(synth)!=1 || std::abs(QinSynthTestAccess::tap(synth))<1.e-6f || QinSynthTestAccess::phase(synth)<1.e-6f)
            throw std::runtime_error("Note-on must start a voice and steer reverb tap/phase");
        const auto tap=QinSynthTestAccess::tap(synth), phase=QinSynthTestAccess::phase(synth);
        midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
        synth.processBlock(block,midi);
        if(std::abs(QinSynthTestAccess::tap(synth)-tap)>1.e-6f || std::abs(QinSynthTestAccess::phase(synth)-phase)>1.e-6f)
            throw std::runtime_error("Note-off must not steer Lan's tap or phase targets");
        set(synth,"release",.01f); midi.clear();
        for(int i=0;i<60;++i) synth.processBlock(block,midi);
        if(QinSynthTestAccess::active(synth)!=0) throw std::runtime_error("Released notes must free their voices");
        if(QinSynthTestAccess::analysisJobs(synth)!=0)
            throw std::runtime_error("MIDI-steered reverb must not run Lan's resonance analyser");
        auto limitedOwner=std::make_unique<QinSynthProcessor>();
        auto& limited=*limitedOwner;
        set(limited,"verbMix",0); set(limited,"voiceCount",2);
        for(const auto* id:{"osc1Retrig","osc2Retrig","osc3Retrig"}) set(limited,id,1);
        limited.prepareToPlay(48000,256);
        midi.clear();
        for(int note=60;note<64;++note) midi.addEvent(juce::MidiMessage::noteOn(1,note,.7f),0);
        limited.processBlock(block,midi);
        if(QinSynthTestAccess::active(limited)!=2)
            throw std::runtime_error("Voice count must limit simultaneous notes");
        QinSynthTestAccess::noteOn(limited,68);
        if(QinSynthTestAccess::active(limited)!=2 || !QinSynthTestAccess::allRetriggeredPhasesMatch(limited))
            throw std::runtime_error("Retrig must start every oscillator and unison copy at the same phase");
        set(limited,"osc1Retrig",0);
        const auto freePhase=QinSynthTestAccess::secondPhase(limited,0);
        QinSynthTestAccess::noteOn(limited,70);
        if(std::abs(QinSynthTestAccess::secondPhase(limited,0)-freePhase)>1.e-6)
            throw std::runtime_error("Free-running oscillator must preserve phase across stolen voices");
        auto denseOwner=std::make_unique<QinSynthProcessor>();
        auto& dense=*denseOwner;
        set(dense,"osc1Unison",8); set(dense,"osc2Level",75); set(dense,"osc2Unison",8);
        set(dense,"osc3Level",75); set(dense,"osc3Unison",8);
        set(dense,"drive",50); set(dense,"verbMix",0);
        dense.prepareToPlay(48000,256);
        midi.clear();
        for(int note=48;note<65;++note) midi.addEvent(juce::MidiMessage::noteOn(1,note,.7f),0);
        for(int i=0;i<8;++i) dense.processBlock(block,midi),midi.clear();
        if(QinSynthTestAccess::active(dense)!=16)
            throw std::runtime_error("Seventeenth note must steal one of sixteen voices");
        if(power(block,0)+power(block,1)<1.e-6) throw std::runtime_error("Three oscillators with eightfold unison must sound");
        {
            std::array<std::unique_ptr<QinSynthProcessor>,5> owned;
            for(auto& processor:owned) processor=std::make_unique<QinSynthProcessor>();
            std::array<QinSynthProcessor*,5> instruments
                {owned[0].get(),owned[1].get(),owned[2].get(),owned[3].get(),owned[4].get()};
            auto& mixed=*owned[4];
            constexpr std::array<qin::WavePosition,5> positions {{{0,0},{1,0},{0,1},{1,1},{.5f,.5f}}};
            for(size_t index=0;index<instruments.size();++index)
            {
                auto& instrument=*instruments[index];
                set(instrument,"osc1WaveX",positions[index].x*100.f);
                set(instrument,"osc1WaveY",positions[index].y*100.f);
                set(instrument,"verbMix",0); set(instrument,"filterType",0);
                instrument.prepareToPlay(48000,256);
            }
            juce::AudioBuffer<float> a(2,256),b(2,256),c(2,256),d(2,256),e(2,256);
            std::array<juce::AudioBuffer<float>*,5> outputs {&a,&b,&c,&d,&e};
            juce::MidiBuffer events;
            events.addEvent(juce::MidiMessage::noteOn(1,60,.7f),0);
            for(int blockIndex=0;blockIndex<16;++blockIndex)
            {
                for(size_t index=0;index<instruments.size();++index)
                    instruments[index]->processBlock(*outputs[index],events);
                events.clear();
            }
            const auto actual=qin::waveWeights({mixed.parameters.getRawParameterValue("osc1WaveX")->load()*.01f,
                                                mixed.parameters.getRawParameterValue("osc1WaveY")->load()*.01f});
            double error=0,difference=0;
            for(int sample=0;sample<256;++sample)
            {
                const auto expected=actual[0]*a.getSample(0,sample)+actual[1]*b.getSample(0,sample)
                    +actual[2]*c.getSample(0,sample)+actual[3]*d.getSample(0,sample);
                error+=std::abs(e.getSample(0,sample)-expected);
                difference+=std::abs(e.getSample(0,sample)-a.getSample(0,sample));
            }
            if(error>.01 || difference<.05)
                throw std::runtime_error("OSC pad must blend all four audible waveform signals");
        }
        auto lowOwner=std::make_unique<QinSynthProcessor>();
        auto& low=*lowOwner;
        auto highOwner=std::make_unique<QinSynthProcessor>();
        auto& high=*highOwner;
        for(auto* p:{&low,&high}) {set(*p,"keyPan",100); set(*p,"verbMix",0); p->prepareToPlay(48000,256);}
        juce::AudioBuffer<float> lowAudio(2,256),highAudio(2,256);
        juce::MidiBuffer lowMidi,highMidi;
        lowMidi.addEvent(juce::MidiMessage::noteOn(1,36,.8f),0);
        highMidi.addEvent(juce::MidiMessage::noteOn(1,96,.8f),0);
        for(int i=0;i<8;++i)
        {low.processBlock(lowAudio,lowMidi); high.processBlock(highAudio,highMidi); lowMidi.clear(); highMidi.clear();}
        if(power(lowAudio,0)<=power(lowAudio,1)*2 || power(highAudio,1)<=power(highAudio,0)*2)
            throw std::runtime_error("Low keys must favor left and high keys must favor right");
        auto reverseLowOwner=std::make_unique<QinSynthProcessor>();
        auto& reverseLow=*reverseLowOwner;
        auto reverseHighOwner=std::make_unique<QinSynthProcessor>();
        auto& reverseHigh=*reverseHighOwner;
        for(auto* p:{&reverseLow,&reverseHigh})
        { set(*p,"keyPan",-100); set(*p,"verbMix",0); p->prepareToPlay(48000,256); }
        juce::AudioBuffer<float> reverseLowAudio(2,256),reverseHighAudio(2,256);
        lowMidi.addEvent(juce::MidiMessage::noteOn(1,36,.8f),0);
        highMidi.addEvent(juce::MidiMessage::noteOn(1,96,.8f),0);
        for(int i=0;i<8;++i)
        {
            reverseLow.processBlock(reverseLowAudio,lowMidi);
            reverseHigh.processBlock(reverseHighAudio,highMidi);
            lowMidi.clear(); highMidi.clear();
        }
        if(power(reverseLowAudio,1)<=power(reverseLowAudio,0)*2
           || power(reverseHighAudio,0)<=power(reverseHighAudio,1)*2)
            throw std::runtime_error("Negative Key Pan must send high notes left and low notes right");
        {
            auto plainOwner=std::make_unique<QinSynthProcessor>();
            auto& plain=*plainOwner;
            auto drivenOwner=std::make_unique<QinSynthProcessor>();
            auto& driven=*drivenOwner;
            auto dryMixOwner=std::make_unique<QinSynthProcessor>();
            auto& dryMix=*dryMixOwner;
            for(auto* p:{&plain,&driven,&dryMix}) {set(*p,"verbMix",0); p->prepareToPlay(48000,256);}
            set(driven,"drive",75);
            set(dryMix,"drive",75); set(dryMix,"driveMix",0);
            juce::AudioBuffer<float> a(2,256),b(2,256),c(2,256);
            juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double difference=0,dryDifference=0;
            for(int blockIndex=0;blockIndex<8;++blockIndex)
            {
                plain.processBlock(a,note); driven.processBlock(b,note); dryMix.processBlock(c,note); note.clear();
                for(int i=0;i<256;++i)
                {
                    difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
                    dryDifference+=std::abs(a.getSample(0,i)-c.getSample(0,i));
                }
            }
            if(difference<1 || dryDifference>1.e-4)
                throw std::runtime_error("Drive Mix must independently blend the driven and dry oscillator signals");
        }
        {
            std::array<std::array<float,256>,4> outputs {};
            for(int mode=0;mode<4;++mode)
            {
                auto instrumentOwner=std::make_unique<QinSynthProcessor>();
                auto& instrument=*instrumentOwner;
                set(instrument,"drive",80); set(instrument,"driveMode",float(mode));
                set(instrument,"verbMix",0); instrument.prepareToPlay(48000,256);
                juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer events;
                events.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
                for(int blockIndex=0;blockIndex<12;++blockIndex)
                { instrument.processBlock(audio,events); events.clear(); }
                if(power(audio,0)<1.e-8) throw std::runtime_error("Every Zao circuit must produce finite audio");
                for(int sample=0;sample<256;++sample)
                    outputs[size_t(mode)][size_t(sample)]=audio.getSample(0,sample);
            }
            for(int first=0;first<4;++first)
                for(int second=first+1;second<4;++second)
                {
                    double difference=0;
                    for(size_t sample=0;sample<256;++sample)
                        difference+=std::abs(outputs[size_t(first)][sample]-outputs[size_t(second)][sample]);
                    if(difference<.01) throw std::runtime_error("Zao drive modes must select distinct circuits");
                }
        }
        {
            auto cleanOwner=std::make_unique<QinSynthProcessor>();
            auto& clean=*cleanOwner;
            auto colouredOwner=std::make_unique<QinSynthProcessor>();
            auto& coloured=*colouredOwner;
            for(auto* p:{&clean,&coloured})
            { set(*p,"verbMix",0); p->prepareToPlay(48000,256); }
            if(clean.getLatencySamples()!=ren::TransformerStage::latency
               || QinSynthTestAccess::transformerRunning(clean))
                throw std::runtime_error("Transformer-off path must report fixed latency and skip the model");
            set(coloured,"transformer",1);
            juce::AudioBuffer<float> dry(2,256),wet(2,256); juce::MidiBuffer events;
            events.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<12;++blockIndex)
            {
                clean.processBlock(dry,events); coloured.processBlock(wet,events); events.clear();
                for(int sample=0;sample<256;++sample)
                    difference+=std::abs(dry.getSample(0,sample)-wet.getSample(0,sample));
            }
            if(difference<.01 || !QinSynthTestAccess::transformerRunning(coloured))
                throw std::runtime_error("Transformer-on must colour the synth output");
            set(coloured,"transformer",0);
            for(int blockIndex=0;blockIndex<12;++blockIndex)
            { clean.processBlock(dry,events); coloured.processBlock(wet,events); }
            double offDifference=0;
            for(int sample=0;sample<256;++sample)
                offDifference+=std::abs(dry.getSample(0,sample)-wet.getSample(0,sample));
            if(QinSynthTestAccess::transformerRunning(coloured) || offDifference>.001)
                throw std::runtime_error("Transformer-off must stop the model and return to the clean path");
        }
        {
            auto monoOwner=std::make_unique<QinSynthProcessor>();
            auto& mono=*monoOwner;
            set(mono,"voiceCount",1); set(mono,"osc1Retrig",1); set(mono,"verbMix",0);
            mono.prepareToPlay(48000,256);
            QinSynthTestAccess::noteOn(mono,60);
            juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer noEvents;
            mono.processBlock(audio,noEvents);
            const auto phaseBefore=QinSynthTestAccess::monoPhase(mono);
            const auto envelopeBefore=QinSynthTestAccess::monoEnvelope(mono);
            QinSynthTestAccess::noteOn(mono,64);
            if(QinSynthTestAccess::monoNote(mono)!=64 || QinSynthTestAccess::active(mono)!=1
               || std::abs(QinSynthTestAccess::monoPhase(mono)-phaseBefore)>1.e-9
               || std::abs(QinSynthTestAccess::monoEnvelope(mono)-envelopeBefore)>1.e-7)
                throw std::runtime_error("Mono legato must change pitch without restarting the voice");
            QinSynthTestAccess::noteOn(mono,67);
            QinSynthTestAccess::noteOff(mono,64);
            if(QinSynthTestAccess::monoNote(mono)!=67)
                throw std::runtime_error("Releasing an older key must keep the current legato note");
            QinSynthTestAccess::noteOff(mono,67);
            if(QinSynthTestAccess::monoNote(mono)!=60 || !QinSynthTestAccess::monoHeld(mono))
                throw std::runtime_error("Releasing the top key must return to the previous held pitch");
            QinSynthTestAccess::noteOff(mono,60);
            if(QinSynthTestAccess::monoHeld(mono))
                throw std::runtime_error("Mono envelope must release when no keys remain held");
            auto midiMonoOwner=std::make_unique<QinSynthProcessor>();
            auto& midiMono=*midiMonoOwner;
            set(midiMono,"voiceCount",1); set(midiMono,"verbMix",0);
            midiMono.prepareToPlay(48000,256);
            juce::MidiBuffer events;
            events.addEvent(juce::MidiMessage::noteOn(1,60,.7f),0);
            events.addEvent(juce::MidiMessage::noteOn(1,64,.7f),64);
            events.addEvent(juce::MidiMessage::noteOn(1,67,.7f),96);
            events.addEvent(juce::MidiMessage::noteOff(1,67),128);
            midiMono.processBlock(audio,events);
            if(QinSynthTestAccess::monoNote(midiMono)!=64 || !QinSynthTestAccess::monoHeld(midiMono))
                throw std::runtime_error("Sample-offset MIDI legato must return to the previous held note");
            events.clear(); events.addEvent(juce::MidiMessage::noteOff(1,64),32);
            midiMono.processBlock(audio,events);
            if(QinSynthTestAccess::monoNote(midiMono)!=60 || !QinSynthTestAccess::monoHeld(midiMono))
                throw std::runtime_error("Mono MIDI note stack must survive across process blocks");
            events.clear(); events.addEvent(juce::MidiMessage::noteOff(1,60),64);
            midiMono.processBlock(audio,events);
            if(QinSynthTestAccess::monoHeld(midiMono))
                throw std::runtime_error("Final MIDI note-off must release the mono voice");
        }
        {
            constexpr std::array<int,4> selectedVoices {1,2,0,1};
            constexpr std::array<int,3> pitches {60,72,84};
            for(int mode=0;mode<4;++mode)
            {
                auto instrument=std::make_unique<QinSynthProcessor>();
                set(*instrument,"voiceCount",3); set(*instrument,"polyLegato",1);
                set(*instrument,"legatoMode",float(mode)); set(*instrument,"slideTime",100);
                set(*instrument,"verbMix",0); instrument->prepareToPlay(48000,256);
                for(const auto note:pitches) QinSynthTestAccess::noteOn(*instrument,note);
                juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer empty;
                instrument->processBlock(audio,empty);
                if(instrument->getActiveVoiceCount()!=3)
                    throw std::runtime_error("Active voice readout must count the sounding voices");
                QinSynthTestAccess::noteOn(*instrument,70);
                const auto selected=selectedVoices[size_t(mode)];
                if(QinSynthTestAccess::note(*instrument,selected)!=70
                   || QinSynthTestAccess::active(*instrument)!=3
                   || std::abs(QinSynthTestAccess::glideStart(*instrument,selected)-pitches[size_t(selected)])>1.e-6
                   || QinSynthTestAccess::glideProgress(*instrument,selected)>.001f)
                    throw std::runtime_error("Poly legato must choose and glide the voice specified by its rule");
                QinSynthTestAccess::noteOff(*instrument,70);
                if(QinSynthTestAccess::note(*instrument,selected)!=pitches[size_t(selected)]
                   || !QinSynthTestAccess::held(*instrument,selected))
                    throw std::runtime_error("Releasing a poly legato note must restore its held predecessor");
            }
            for(int mode=2;mode<=3;++mode)
            {
                auto instrument=std::make_unique<QinSynthProcessor>();
                set(*instrument,"voiceCount",3); set(*instrument,"polyLegato",1);
                set(*instrument,"legatoMode",float(mode)); instrument->prepareToPlay(48000,256);
                for(const auto note:pitches) QinSynthTestAccess::noteOn(*instrument,note);
                QinSynthTestAccess::noteOn(*instrument,mode==2 ? 48 : 96);
                if(QinSynthTestAccess::note(*instrument,mode==2 ? 0 : 2)!=(mode==2 ? 48 : 96))
                    throw std::runtime_error("Directional legato must fall back to the closest voice at the range edge");
            }
            {
                auto instrument=std::make_unique<QinSynthProcessor>();
                set(*instrument,"voiceCount",3); set(*instrument,"polyLegato",1);
                instrument->prepareToPlay(48000,256);
                for(const auto note:pitches) QinSynthTestAccess::noteOn(*instrument,note);
                QinSynthTestAccess::noteOff(*instrument,72);
                QinSynthTestAccess::noteOn(*instrument,70);
                if(QinSynthTestAccess::note(*instrument,1)!=70
                   || QinSynthTestAccess::active(*instrument)!=3)
                    throw std::runtime_error("Poly legato must reuse a released voice before stealing a held one");
            }
            std::array<double,3> quarterPitches {};
            for(int shape=0;shape<3;++shape)
            {
                auto instrument=std::make_unique<QinSynthProcessor>();
                set(*instrument,"voiceCount",1); set(*instrument,"slideTime",100);
                set(*instrument,"slideShape",float(shape*50)); set(*instrument,"verbMix",0);
                instrument->prepareToPlay(48000,256);
                QinSynthTestAccess::noteOn(*instrument,60);
                QinSynthTestAccess::noteOn(*instrument,72);
                juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer empty;
                for(int blockIndex=0;blockIndex<5;++blockIndex) instrument->processBlock(audio,empty);
                quarterPitches[size_t(shape)]=QinSynthTestAccess::pitch(*instrument,0);
                for(int blockIndex=0;blockIndex<16;++blockIndex) instrument->processBlock(audio,empty);
                if(std::abs(QinSynthTestAccess::pitch(*instrument,0)-72.0)>.001)
                    throw std::runtime_error("Slide time must reach the destination pitch");
            }
            if(!(quarterPitches[0]>quarterPitches[1]+1.0
                 && quarterPitches[1]>quarterPitches[2]+.5))
                throw std::runtime_error("Log, linear, and sigmoid glide shapes must differ in order");
            {
                auto instrument=std::make_unique<QinSynthProcessor>();
                set(*instrument,"voiceCount",1); set(*instrument,"slideTime",0);
                instrument->prepareToPlay(48000,256);
                QinSynthTestAccess::noteOn(*instrument,60);
                QinSynthTestAccess::noteOn(*instrument,72);
                if(std::abs(QinSynthTestAccess::pitch(*instrument,0)-72.0)>.001)
                    throw std::runtime_error("Zero slide time must move immediately to the new pitch");
            }
        }
        {
            auto tailOwner=std::make_unique<QinSynthProcessor>();
            auto& tail=*tailOwner;
            set(tail,"verbMix",100); set(tail,"time",25); set(tail,"decay",2);
            tail.prepareToPlay(48000,256);
            juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<4;++i) {tail.processBlock(block,note); note.clear();}
            note.addEvent(juce::MidiMessage::allSoundOff(1),0);
            double tailPower=0;
            for(int i=0;i<40;++i)
            {
                tail.processBlock(block,note); note.clear();
                tailPower+=power(block,0)+power(block,1);
            }
            if(tailPower<1.e-6) throw std::runtime_error("Lan delay-reverb must retain an audible tail after note silence");
        }
        {
            auto leftOwner=std::make_unique<QinSynthProcessor>();
            auto& left=*leftOwner;
            auto rightOwner=std::make_unique<QinSynthProcessor>();
            auto& right=*rightOwner;
            for(auto* p:{&left,&right}) { set(*p,"keyPan",0); set(*p,"verbMix",0); p->prepareToPlay(48000,256); }
            set(left,"osc1Pan",-100); set(right,"osc1Pan",100);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<8;++i) {left.processBlock(a,note); right.processBlock(b,note); note.clear();}
            if(power(a,0)<power(a,1)*10 || power(b,1)<power(b,0)*10)
                throw std::runtime_error("Per-oscillator pan must place the whole oscillator in stereo");
        }
        {
            const auto renderFm=[](std::array<int,3> sources,std::array<float,3> amounts,int audible,int blockSize=256)
            {
                auto instrumentOwner=std::make_unique<QinSynthProcessor>();
                auto& instrument=*instrumentOwner;
                for(const auto* id:{"osc1Level","osc2Level","osc3Level","verbMix","driveMix","keyPan"})
                    set(instrument,id,0);
                const std::array<const char*,3> levels {"osc1Level","osc2Level","osc3Level"};
                const std::array<const char*,3> sourceIds {"osc1FmSource","osc2FmSource","osc3FmSource"};
                const std::array<const char*,3> amountIds {"osc1FmAmount","osc2FmAmount","osc3FmAmount"};
                set(instrument,levels[size_t(audible)],100);
                for(size_t osc=0;osc<3;++osc)
                { set(instrument,sourceIds[osc],float(sources[osc])); set(instrument,amountIds[osc],amounts[osc]); }
                instrument.prepareToPlay(48000,blockSize);
                juce::MidiBuffer events;
                events.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
                std::vector<float> output;
                for(int done=0;done<3072;)
                {
                    const auto count=std::min(blockSize,3072-done);
                    juce::AudioBuffer<float> audio(2,count);
                    instrument.processBlock(audio,events); events.clear();
                    power(audio,0); power(audio,1);
                    for(int sample=0;sample<audio.getNumSamples();++sample)
                        output.push_back(audio.getSample(0,sample));
                    done+=count;
                }
                return output;
            };
            const auto difference=[](const auto& left,const auto& right)
            {
                double total=0;
                for(size_t i=0;i<left.size();++i) total+=std::abs(left[i]-right[i]);
                return total/left.size();
            };
            const auto plain=renderFm({0,0,0},{0,0,0},2);
            const auto direct=renderFm({0,0,2},{0,0,100},2);
            const auto stacked=renderFm({0,1,2},{0,100,100},2);
            const auto noise=renderFm({0,0,4},{0,0,100},2);
            const auto cycle=renderFm({2,1,2},{100,100,100},2);
            const auto cycleSmallBlocks=renderFm({2,1,2},{100,100,100},2,17);
            const auto reversePlain=renderFm({0,0,0},{0,0,0},0);
            const auto reverseStack=renderFm({3,0,2},{100,0,100},0);
            if(difference(plain,direct)<1.e-4 || difference(direct,stacked)<1.e-4
               || difference(plain,noise)<1.e-4 || difference(direct,cycle)<1.e-4
               || difference(reversePlain,reverseStack)<1.e-4
               || difference(cycle,cycleSmallBlocks)>1.e-4)
                throw std::runtime_error("FM must use muted modulators, stack in either order, feed back, and accept noise");
            auto stressOwner=std::make_unique<QinSynthProcessor>();
            auto& stress=*stressOwner;
            for(int osc=1;osc<=3;++osc)
            {
                const auto prefix="osc"+juce::String(osc);
                auto* source=stress.parameters.getParameter(prefix+"FmSource");
                auto* amount=stress.parameters.getParameter(prefix+"FmAmount");
                auto* unison=stress.parameters.getParameter(prefix+"Unison");
                source->setValueNotifyingHost(source->convertTo0to1(float(osc%3+1)));
                amount->setValueNotifyingHost(amount->convertTo0to1(100.f));
                unison->setValueNotifyingHost(unison->convertTo0to1(8.f));
            }
            set(stress,"osc2Level",100); set(stress,"osc3Level",100);
            set(stress,"filterType",1); set(stress,"filterResonance",100);
            set(stress,"filterCutoff",18000); set(stress,"verbMix",0);
            stress.prepareToPlay(96000,128);
            juce::AudioBuffer<float> stressedAudio(2,128); juce::MidiBuffer stressedMidi;
            stressedMidi.addEvent(juce::MidiMessage::noteOn(1,108,1.f),0);
            for(int blockIndex=0;blockIndex<20;++blockIndex)
            {
                stress.processBlock(stressedAudio,stressedMidi); stressedMidi.clear();
                power(stressedAudio,0); power(stressedAudio,1);
                if(!QinSynthTestAccess::stableFilters(stress))
                    throw std::runtime_error("Full-depth FM feedback must keep high-resonance filter state finite");
            }
        }
        {
            auto narrowOwner=std::make_unique<QinSynthProcessor>();
            auto& narrow=*narrowOwner;
            auto wideOwner=std::make_unique<QinSynthProcessor>();
            auto& wide=*wideOwner;
            for(auto* p:{&narrow,&wide})
            { set(*p,"keyPan",0); set(*p,"verbMix",0); set(*p,"osc1Unison",8); set(*p,"osc1Detune",20); p->prepareToPlay(48000,256); }
            set(narrow,"osc1Spread",0); set(wide,"osc1Spread",100);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double narrowSide=0,wideSide=0;
            for(int blockIndex=0;blockIndex<8;++blockIndex)
            {
                narrow.processBlock(a,note); wide.processBlock(b,note); note.clear();
                for(int i=0;i<256;++i)
                { narrowSide+=std::abs(a.getSample(0,i)-a.getSample(1,i)); wideSide+=std::abs(b.getSample(0,i)-b.getSample(1,i)); }
            }
            if(narrowSide>1.e-5 || wideSide<.01)
                throw std::runtime_error("Spread must move detuned copies without moving the oscillator center");
        }
        for(int type=1;type<=4;++type)
        {
            auto filteredOwner=std::make_unique<QinSynthProcessor>();
            auto& filtered=*filteredOwner;
            auto referenceOwner=std::make_unique<QinSynthProcessor>();
            auto& reference=*referenceOwner;
            for(auto* p:{&filtered,&reference}) { set(*p,"verbMix",0); p->prepareToPlay(48000,256); }
            set(filtered,"filterType",float(type)); set(filtered,"filterCutoff",250);
            juce::AudioBuffer<float> audio(2,256),dry(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,84,1.f),0);
            for(int blockIndex=0;blockIndex<16;++blockIndex)
            { filtered.processBlock(audio,note); reference.processBlock(dry,note); note.clear(); }
            const auto filteredPower=power(audio,0)+power(audio,1);
            if(filteredPower<1.e-10 || filteredPower>=.5*(power(dry,0)+power(dry,1)))
                throw std::runtime_error("Each filter type must attenuate a note above its cutoff");
        }
        {
            auto voicesOwner=std::make_unique<QinSynthProcessor>();
            auto& voices=*voicesOwner;
            auto masterOwner=std::make_unique<QinSynthProcessor>();
            auto& master=*masterOwner;
            for(auto* p:{&voices,&master})
            {
                set(*p,"verbMix",0); set(*p,"filterType",3); set(*p,"filterCutoff",900);
                set(*p,"filterResonance",100); set(*p,"filterKeyFollow",100);
                set(*p,"filterEnvAmount",100);
                set(*p,"fltAttack",1); set(*p,"fltSustain",100);
                p->prepareToPlay(48000,256);
            }
            set(master,"filterMerge",100);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer notes;
            for(const auto pitch:{48,60,72}) notes.addEvent(juce::MidiMessage::noteOn(1,pitch,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<32;++blockIndex)
            {
                voices.processBlock(a,notes); master.processBlock(b,notes); notes.clear();
                power(a,0); power(b,0);
                if(!QinSynthTestAccess::stableFilters(voices) || !QinSynthTestAccess::stableFilters(master))
                    throw std::runtime_error("Merged or per-voice filter state became unstable");
                if(blockIndex>=16)
                    for(int sample=0;sample<256;++sample)
                        difference+=std::abs(a.getSample(0,sample)-b.getSample(0,sample));
            }
            if(difference<.1)
                throw std::runtime_error("Full Merge must use a shared filter rather than per-voice filters");
        }
        {
            auto plainOwner=std::make_unique<QinSynthProcessor>();
            auto& plain=*plainOwner;
            auto noisyOwner=std::make_unique<QinSynthProcessor>();
            auto& noisy=*noisyOwner;
            for(auto* p:{&plain,&noisy})
            {
                set(*p,"osc1Level",0); set(*p,"osc2Level",0); set(*p,"osc3Level",0);
                set(*p,"verbMix",0); p->prepareToPlay(48000,256);
            }
            set(noisy,"noise",1);
            juce::AudioBuffer<float> quiet(2,256),sound(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<8;++i) { plain.processBlock(quiet,note); noisy.processBlock(sound,note); note.clear(); }
            if(power(quiet,0)>1.e-12 || power(sound,0)<1.e-5)
                throw std::runtime_error("N must generate noise independently of the tonal oscillators");
        }
        {
            // LUT interpolation preserves equal-power pan even at the edges.
            for(int i=0;i<=10000;++i)
            {
                const auto pan=-.98f+1.96f*i/10000.f;
                const auto gains=qin::stereoPanLookup.evaluate(pan);
                if(std::abs(gains.left-std::sqrt((1.f-pan)*.5f))>1.e-5f
                   || std::abs(gains.right-std::sqrt((1.f+pan)*.5f))>1.e-5f)
                    throw std::runtime_error("Cached pan gains changed the equal-power curve");
            }
        }
        {
            const auto renderNoise=[](int style,float tune,float hpf,float level,float pan)
            {
                auto instrumentOwner=std::make_unique<QinSynthProcessor>();
                auto& instrument=*instrumentOwner;
                for(const auto* id:{"osc1Level","osc2Level","osc3Level","verbMix","keyPan"}) set(instrument,id,0);
                set(instrument,"noise",1); set(instrument,"noiseStyle",float(style));
                set(instrument,"noiseTune",tune); set(instrument,"noiseHpf",hpf);
                set(instrument,"noiseLevel",level); set(instrument,"noisePan",pan);
                instrument.prepareToPlay(48000,256);
                std::array<std::vector<float>,2> output;
                juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer note;
                note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
                for(int blockIndex=0;blockIndex<32;++blockIndex)
                {
                    instrument.processBlock(audio,note); note.clear();
                    power(audio,0); power(audio,1);
                    for(int channel=0;channel<2;++channel)
                        for(int i=0;i<256;++i) output[size_t(channel)].push_back(audio.getSample(channel,i));
                }
                return output;
            };
            const auto white=renderNoise(0,0,20,100,0);
            const auto pink=renderNoise(1,0,20,100,0);
            const auto velvet=renderNoise(2,0,20,100,0);
            const auto lowTune=renderNoise(0,-24,20,100,0);
            const auto highTune=renderNoise(0,24,20,100,0);
            const auto highPassed=renderNoise(0,0,1000,100,0);
            const auto muted=renderNoise(0,0,20,0,0);
            const auto right=renderNoise(0,0,20,100,100);
            const auto energy=[](const std::vector<float>& data)
            { double result=0; for(size_t i=1024;i<data.size();++i) result+=double(data[i])*data[i]; return result; };
            const auto roughness=[](const std::vector<float>& data)
            { double result=0; for(size_t i=1025;i<data.size();++i) result+=std::pow(double(data[i])-data[i-1],2); return result; };
            double pinkDifference=0, velvetDifference=0;
            for(size_t i=1024;i<white[0].size();++i)
            { pinkDifference+=std::abs(white[0][i]-pink[0][i]); velvetDifference+=std::abs(white[0][i]-velvet[0][i]); }
            if(energy(white[0])<.001 || pinkDifference<.1 || velvetDifference<.1)
                throw std::runtime_error("White, Pink, and Velvet must produce distinct audible noise textures");
            if(roughness(highTune[0])/energy(highTune[0]) < 2*roughness(lowTune[0])/energy(lowTune[0]))
                throw std::runtime_error("Noise Tune must raise the frequency of its note-tracked resonance");
            if(energy(highPassed[0]) > .4*energy(white[0]))
                throw std::runtime_error("Noise HPF must remove low-frequency output energy");
            if(energy(muted[0])>1.e-12 || energy(right[0])>.02*energy(right[1]))
                throw std::runtime_error("Noise Level and Pan must control their independent signal path");
        }
        {
            // A legato noise glide must keep its coefficient interpolation
            // continuous when the host changes process-block boundaries.
            const auto render=[](int blockSize)
            {
                auto instrumentOwner=std::make_unique<QinSynthProcessor>();
                auto& instrument=*instrumentOwner;
                for(const auto* id:{"osc1Level","osc2Level","osc3Level","verbMix","driveMix"})
                    set(instrument,id,0);
                set(instrument,"voiceCount",1); set(instrument,"noise",1);
                set(instrument,"noiseLevel",100); set(instrument,"noiseTune",12);
                set(instrument,"noiseHpf",300); set(instrument,"noisePan",25);
                set(instrument,"keyPan",50); set(instrument,"slideTime",90);
                instrument.prepareToPlay(48000,blockSize);
                std::vector<float> output;
                output.reserve(1280);
                const auto append=[&](int samples)
                {
                    for(int done=0;done<samples;)
                    {
                        const auto count=std::min(blockSize,samples-done);
                        juce::AudioBuffer<float> audio(2,count); juce::MidiBuffer midi;
                        instrument.processBlock(audio,midi);
                        for(int i=0;i<count;++i) output.push_back(audio.getSample(0,i));
                        done+=count;
                    }
                };
                QinSynthTestAccess::noteOn(instrument,60);
                append(250);
                QinSynthTestAccess::noteOn(instrument,72);
                append(257);
                QinSynthTestAccess::noteOn(instrument,55);
                append(773);
                return output;
            };
            const auto large=render(128),small=render(17);
            double maxDifference=0;
            for(size_t i=0;i<large.size();++i)
            {
                if(!std::isfinite(large[i]) || !std::isfinite(small[i]))
                    throw std::runtime_error("Interpolated noise glide became non-finite");
                maxDifference=std::max(maxDifference,double(std::abs(large[i]-small[i])));
            }
            if(maxDifference>1.e-4)
                throw std::runtime_error("Noise glide changed across host block sizes");
        }
        {
            auto earlyOwner=std::make_unique<QinSynthProcessor>();
            auto& early=*earlyOwner;
            auto lateOwner=std::make_unique<QinSynthProcessor>();
            auto& late=*lateOwner;
            for(auto* p:{&early,&late})
            { set(*p,"verbMix",0); set(*p,"attack",.03f); p->prepareToPlay(48000,256); }
            set(early,"ampCurve",-100); set(late,"ampCurve",100);
            juce::AudioBuffer<float> earlyAudio(2,256),lateAudio(2,256);
            juce::MidiBuffer note; note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double earlyPower=0,latePower=0;
            for(int blockIndex=0;blockIndex<4;++blockIndex)
            {
                early.processBlock(earlyAudio,note); late.processBlock(lateAudio,note); note.clear();
                earlyPower+=power(earlyAudio,0); latePower+=power(lateAudio,0);
            }
            if(earlyPower<latePower*1.5)
                throw std::runtime_error("Envelope curve must alter the rendered attack shape");
        }
        for(const auto rate:{44100.,48000.,96000.})
            for(int type=1;type<=4;++type)
                for(const auto cutoff:{30.f,18000.f})
                    for(const auto envelopeAmount:{-100.f,100.f})
                    {
                        auto stressedOwner=std::make_unique<QinSynthProcessor>();
                        auto& stressed=*stressedOwner;
                        set(stressed,"verbMix",0); set(stressed,"filterType",float(type));
                        set(stressed,"filterCutoff",cutoff); set(stressed,"filterResonance",100);
                        set(stressed,"filterEnvAmount",envelopeAmount);
                        set(stressed,"fltAttack",1.f); set(stressed,"fltSustain",100);
                        set(stressed,"noise",1); stressed.prepareToPlay(rate,128);
                        juce::AudioBuffer<float> audio(2,128); juce::MidiBuffer events;
                        for(int blockIndex=0;blockIndex<40;++blockIndex)
                        {
                            if(blockIndex%10==0) events.addEvent(juce::MidiMessage::noteOn(1,blockIndex%20==0 ? 24 : 108,1.f),0);
                            stressed.processBlock(audio,events); events.clear();
                            power(audio,0); power(audio,1);
                            for(int channel=0;channel<2;++channel)
                                for(int sample=0;sample<audio.getNumSamples();++sample)
                                    if(std::abs(audio.getSample(channel,sample))>4.01f)
                                        throw std::runtime_error("Extreme filter settings exceeded the output ceiling");
                            if(!QinSynthTestAccess::stableFilters(stressed))
                                throw std::runtime_error("Filter state diverged at full resonance and extreme envelope movement");
                        }
                    }
        {
            auto heldOwner=std::make_unique<QinSynthProcessor>();
            auto& held=*heldOwner;
            auto fadingOwner=std::make_unique<QinSynthProcessor>();
            auto& fading=*fadingOwner;
            for(auto* p:{&held,&fading}) { set(*p,"verbMix",0); set(*p,"attack",.001f); p->prepareToPlay(48000,256); }
            set(fading,"ampSustain",0); set(fading,"ampDecay",.01f);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<36;++i) { held.processBlock(a,note); fading.processBlock(b,note); note.clear(); }
            if(power(b,0)>power(a,0)*.001)
                throw std::runtime_error("AMP decay and sustain must shape a held note");
        }
        {
            auto staticFilterOwner=std::make_unique<QinSynthProcessor>();
            auto& staticFilter=*staticFilterOwner;
            auto movingFilterOwner=std::make_unique<QinSynthProcessor>();
            auto& movingFilter=*movingFilterOwner;
            for(auto* p:{&staticFilter,&movingFilter})
            {
                set(*p,"verbMix",0); set(*p,"filterType",3); set(*p,"filterCutoff",200);
                set(*p,"fltAttack",1.f); set(*p,"fltSustain",100); p->prepareToPlay(48000,256);
            }
            set(movingFilter,"filterEnvAmount",100);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,72,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<16;++blockIndex)
            {
                staticFilter.processBlock(a,note); movingFilter.processBlock(b,note); note.clear();
                for(int i=0;i<256;++i) difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
            }
            if(difference<.1) throw std::runtime_error("FLT envelope must move the filter cutoff");
        }
        {
            auto monitoredOwner=std::make_unique<QinSynthProcessor>();
            auto& monitored=*monitoredOwner;
            set(monitored,"verbMix",0); set(monitored,"drive",65);
            set(monitored,"filterType",3); set(monitored,"filterCutoff",500);
            set(monitored,"filterEnvAmount",100); set(monitored,"fltAttack",500);
            set(monitored,"fltSustain",100);
            set(monitored,"modAttack",.5f); set(monitored,"modSustain",100);
            monitored.prepareToPlay(48000,256);
            QinSynthEditor editor(monitored);
            juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            monitored.processBlock(audio,note); note.clear();
            QinSynthEditorTestAccess::tick(editor);
            if(QinSynthEditorTestAccess::activeLabel(editor)!="1 ACTIVE")
                throw std::runtime_error("Active voice display must follow the audio processor");
            const auto firstCutoff=QinSynthEditorTestAccess::liveCutoff(editor);
            const auto firstPlot=editor.createComponentSnapshot({455,140,218,85});
            for(int blockIndex=0;blockIndex<24;++blockIndex) monitored.processBlock(audio,note);
            QinSynthEditorTestAccess::tick(editor);
            const auto lateCutoff=QinSynthEditorTestAccess::liveCutoff(editor);
            const auto latePlot=editor.createComponentSnapshot({455,140,218,85});
            if(lateCutoff<firstCutoff*1.5f || !QinSynthEditorTestAccess::liveScope(editor))
                throw std::runtime_error("Filter response and drive scope must follow active audio");
            int changed=0;
            for(int y=0;y<firstPlot.getHeight();y+=2)
                for(int x=0;x<firstPlot.getWidth();x+=2)
                    changed+=firstPlot.getPixelAt(x,y)!=latePlot.getPixelAt(x,y);
            if(changed<20) throw std::runtime_error("Live filter plot must follow envelope motion");
            const auto scope=monitored.getDriveScope();
            double driveDifference=0;
            for(size_t index=0;index<scope.input.size();++index)
                driveDifference+=std::abs(scope.input[index]-scope.output[index]);
            if(scope.epoch==0 || driveDifference<.01)
                throw std::runtime_error("Drive scope must show the processed waveform");
            set(monitored,"filterResonance",30);
            monitored.processBlock(audio,note); QinSynthEditorTestAccess::tick(editor);
            const auto baselineResonance=editor.createComponentSnapshot({455,140,218,85});
            set(monitored,"modTarget",float(qin::modTargetIndex("filterResonance")));
            set(monitored,"modDepth",80);
            monitored.processBlock(audio,note);
            const auto earlyResonance=monitored.getLiveFilterResonance();
            for(int blockIndex=0;blockIndex<4;++blockIndex) monitored.processBlock(audio,note);
            QinSynthEditorTestAccess::tick(editor);
            const auto activeResonance=editor.createComponentSnapshot({455,140,218,85});
            int resonancePixels=0;
            for(int y=0;y<baselineResonance.getHeight();y+=2)
                for(int x=0;x<baselineResonance.getWidth();x+=2)
                    resonancePixels+=baselineResonance.getPixelAt(x,y)!=activeResonance.getPixelAt(x,y);
            if(monitored.getLiveFilterResonance()<earlyResonance+.01f
               || !QinSynthEditorTestAccess::resonanceIndicatorActive(editor)
               || !QinSynthEditorTestAccess::modulationOverlayActive(editor)
               || resonancePixels<20
               || std::abs(monitored.parameters.getRawParameterValue("filterResonance")->load()-30)>.1f)
                throw std::runtime_error("MOD resonance must animate its slider marker and filter curve without changing the host value");
            set(monitored,"modDepth",0); monitored.processBlock(audio,note);
            QinSynthEditorTestAccess::tick(editor);
            if(QinSynthEditorTestAccess::resonanceIndicatorActive(editor)
                || QinSynthEditorTestAccess::modulationOverlayActive(editor))
                throw std::runtime_error("The resonance modulation marker must clear when MOD depth is zero");
            set(monitored,"modTarget",float(qin::modTargetIndex("osc1Level")));
            set(monitored,"modDepth",80);
            monitored.processBlock(audio,note); QinSynthEditorTestAccess::tick(editor);
            if(monitored.getLiveModulation().target!=qin::modTargetIndex("osc1Level")
               || !QinSynthEditorTestAccess::modulationOverlayOnLevel(editor)
               || std::abs(monitored.parameters.getRawParameterValue("osc1Level")->load()-80.f)>.1f)
                throw std::runtime_error("Direct MOD destinations must animate their effective value without changing the host control");
        }
        {
            auto plainOwner=std::make_unique<QinSynthProcessor>();
            auto& plain=*plainOwner;
            auto modulatedOwner=std::make_unique<QinSynthProcessor>();
            auto& modulated=*modulatedOwner;
            for(auto* p:{&plain,&modulated})
            { set(*p,"verbMix",0); set(*p,"modAttack",.001f); set(*p,"modSustain",100); p->prepareToPlay(48000,256); }
            set(modulated,"modDepth",100); set(modulated,"modTarget",0);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<16;++blockIndex)
            {
                plain.processBlock(a,note); modulated.processBlock(b,note); note.clear();
                for(int i=0;i<256;++i) difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
            }
            if(difference<.01) throw std::runtime_error("MOD envelope must change the chosen effect");
        }
        for(const auto destination:{2,3})
        {
            auto plainOwner=std::make_unique<QinSynthProcessor>();
            auto& plain=*plainOwner;
            auto modulatedOwner=std::make_unique<QinSynthProcessor>();
            auto& modulated=*modulatedOwner;
            for(auto* p:{&plain,&modulated})
            {
                set(*p,"verbMix",0); set(*p,"modAttack",.001f); set(*p,"modSustain",100);
                set(*p,"filterType",3); set(*p,"filterCutoff",600);
                p->prepareToPlay(48000,256);
            }
            set(modulated,"modDepth",100); set(modulated,"modTarget",float(destination));
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<12;++blockIndex)
            {
                plain.processBlock(a,note); modulated.processBlock(b,note); note.clear();
                for(int i=0;i<256;++i) difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
            }
            if(difference<.01)
                throw std::runtime_error("MOD Pitch and Filter destinations must affect rendered audio");
            if(std::abs(QinSynthTestAccess::modDepth(modulated)-1.f)>1.e-4f)
                throw std::runtime_error("MOD depth ramp must reach the automated value");
        }
        {
            auto plainOwner=std::make_unique<QinSynthProcessor>();
            auto raisedOwner=std::make_unique<QinSynthProcessor>();
            auto loweredOwner=std::make_unique<QinSynthProcessor>();
            auto& plain=*plainOwner;
            auto& raised=*raisedOwner;
            auto& lowered=*loweredOwner;
            for(const auto& route:qin::continuousModTargets)
                if(plain.parameters.getParameter(route.id)==nullptr)
                    throw std::runtime_error("Every listed continuous MOD destination must be a host parameter");
            if(plain.parameters.getParameter("tune0")!=nullptr
               || qin::modTargetIndex("osc1Level")<qin::legacyModTargetCount
               || qin::modTargetIndex("output")<qin::legacyModTargetCount)
                throw std::runtime_error("MOD routing and locked root must expose the expected parameters");
            for(auto* instrument:{&plain,&raised,&lowered})
            {
                set(*instrument,"verbMix",0); set(*instrument,"filterType",0);
                set(*instrument,"osc1Level",20); set(*instrument,"osc1Retrig",1);
                set(*instrument,"modAttack",.001f); set(*instrument,"modSustain",100);
                set(*instrument,"modTarget",float(qin::modTargetIndex("osc1Level")));
                instrument->prepareToPlay(48000,256);
            }
            set(raised,"modDepth",80); set(lowered,"modDepth",-80);
            juce::AudioBuffer<float> a(2,256),b(2,256),c(2,256);
            juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
            for(int blockIndex=0;blockIndex<12;++blockIndex)
            {
                plain.processBlock(a,note); raised.processBlock(b,note); lowered.processBlock(c,note);
                note.clear();
            }
            const auto baseline=power(a,0), louder=power(b,0), quieter=power(c,0);
            if(!(louder>baseline*1.5 && quieter<baseline*.5)
               || std::abs(raised.parameters.getRawParameterValue("osc1Level")->load()-20)>.1)
                throw std::runtime_error("Positive and negative MOD routing must affect audio without rewriting the knob");
            const auto outputTarget=qin::modTargetIndex("output");
            set(raised,"modTarget",float(outputTarget));
            set(raised,"output",-18);
            set(raised,"modDepth",100);
            for(int blockIndex=0;blockIndex<4;++blockIndex) raised.processBlock(b,note);
            if(power(b,0)<baseline*.5 || std::abs(raised.parameters.getRawParameterValue("output")->load()+18)>.1)
                throw std::runtime_error("MOD must reach global output gain without changing its host value");
        }
        {
            auto instrumentOwner=std::make_unique<QinSynthProcessor>();
            auto& instrument=*instrumentOwner;
            set(instrument,"verbMix",0); set(instrument,"voiceCount",1);
            set(instrument,"modAttack",.001f); set(instrument,"modSustain",100);
            set(instrument,"modDepth",100);
            set(instrument,"modTarget",float(qin::modTargetIndex("keyPan")));
            instrument.prepareToPlay(48000,256);
            juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,96,.8f),0);
            for(int blockIndex=0;blockIndex<8;++blockIndex)
            { instrument.processBlock(audio,note); note.clear(); }
            if(QinSynthTestAccess::voicePan(instrument,0)<.4f)
                throw std::runtime_error("MOD Key Pan must update an already sounding voice");
            set(instrument,"modTarget",float(qin::modTargetIndex("slideTime")));
            note.addEvent(juce::MidiMessage::noteOn(1,100,.8f),0);
            instrument.processBlock(audio,note);
            if(QinSynthTestAccess::glideIncrement(instrument,0)>1.f/4800.f)
                throw std::runtime_error("MOD Slide Time must affect legato note events");
            note.clear();
            set(instrument,"modDepth",20);
            set(instrument,"modTarget",float(qin::modTargetIndex("modDepth")));
            for(int blockIndex=0;blockIndex<4;++blockIndex) instrument.processBlock(audio,note);
            if(QinSynthTestAccess::modDepth(instrument)<.25f)
                throw std::runtime_error("MOD Depth must also be a bounded modulation destination");
        }
        {
            auto pitchedOwner=std::make_unique<QinSynthProcessor>();
            auto& pitched=*pitchedOwner;
            set(pitched,"verbMix",0); set(pitched,"osc1Unison",8);
            set(pitched,"osc1Detune",0); set(pitched,"osc1Retrig",1); pitched.prepareToPlay(48000,256);
            set(pitched,"osc1Detune",30); set(pitched,"osc1DetuneShape",100);
            juce::AudioBuffer<float> audio(2,64); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            pitched.processBlock(audio,note);
            const auto expected=std::exp2(qin::detunePosition(1,8,1.f)*30./1200.);
            if(std::abs(QinSynthTestAccess::detuneRatio(pitched,0,1)-expected)>1.e-8)
                throw std::runtime_error("Detune pitch ramp must reach its target at the block boundary");
            const auto outer=std::exp2(30./1200.);
            const auto phaseExpected=juce::MidiMessage::getMidiNoteInHertz(60)/48000.
                *(64.+(outer-1.)*65./2.);
            if(std::abs(QinSynthTestAccess::oscillatorPhase(pitched,0,7)-phaseExpected)>1.e-5)
                throw std::runtime_error("Unison phase increment must ramp within the audio block");
        }
        {
            auto previewOwner=std::make_unique<QinSynthProcessor>();
            auto& preview=*previewOwner;
            QinSynthEditor editor(preview);
            if(editor.getWidth()!=920 || editor.getHeight()!=640
               || QinSynthEditorTestAccess::modDestinationCount(editor)
                    !=qin::legacyModTargetCount+qin::continuousModTargetCount
               || !QinSynthEditorTestAccess::voiceDragEnabled(editor)
               || !QinSynthEditorTestAccess::outputSectionVisible(editor)
               || !QinSynthEditorTestAccess::compactFxKnobs(editor)
               || !QinSynthEditorTestAccess::compactHeaderLayout(editor)
               || !QinSynthEditorTestAccess::wavePadEnabled(editor,0)
               || !QinSynthEditorTestAccess::wavePadEnabled(editor,1)
               || !QinSynthEditorTestAccess::wavePadEnabled(editor,2)
               || !QinSynthEditorTestAccess::oscillatorLayout(editor)
               || !QinSynthEditorTestAccess::fmGridLayout(editor))
                throw std::runtime_error("Polyphony readout and oscillator pads must be available");
            if(!QinSynthEditorTestAccess::fmVisible(editor,0)
               || QinSynthEditorTestAccess::fmVisible(editor,1))
                throw std::runtime_error("FM controls must follow the selected OSC tab");
            if(!QinSynthEditorTestAccess::octaveReadoutVisible(editor,0)
               || !QinSynthEditorTestAccess::levelReadoutVisible(editor,0)
               || !QinSynthEditorTestAccess::fineReadoutLayout(editor,0)
               || QinSynthEditorTestAccess::octaveReadoutVisible(editor,1)
               || QinSynthEditorTestAccess::tuningMenuVisible(editor))
                throw std::runtime_error("Octave readout and hidden TUNE menu must initialize correctly");
            QinSynthEditorTestAccess::setOctave(editor,0,-2);
            QinSynthEditorTestAccess::setFine(editor,0,650);
            QinSynthEditorTestAccess::setLevel(editor,0,65);
            if(std::abs(preview.parameters.getRawParameterValue("osc1Octave")->load()+2)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("osc1Fine")->load()-650)>.1
               || std::abs(preview.parameters.getRawParameterValue("osc1Level")->load()-65)>.1
               || !QinSynthEditorTestAccess::levelShows(editor,0,"LEVEL 65.0%")
               || !QinSynthEditorTestAccess::fineShowsCents(editor,0,"FINE +650.0c"))
                throw std::runtime_error("Level, Octave, and Fine readouts must update separate host parameters");
            QinSynthEditorTestAccess::setOctave(editor,0,0);
            QinSynthEditorTestAccess::setFine(editor,0,0);
            QinSynthEditorTestAccess::setLevel(editor,0,80);
            QinSynthEditorTestAccess::openTuningMenu(editor);
            if(!QinSynthEditorTestAccess::tuningMenuVisible(editor)
               || !QinSynthEditorTestAccess::tuneButtonBesideVoices(editor))
                throw std::runtime_error("TUNE button must open a hidden menu beside the voice count");
            QinSynthEditorTestAccess::setTuning(editor,1,25);
            QinSynthEditorTestAccess::setTuningRoot(editor,2);
            if(argc>6)
            {
                const auto image=editor.createComponentSnapshot(editor.getLocalBounds());
                const juce::File file(argv[6]);
                file.deleteFile();
                juce::FileOutputStream stream{file};
                juce::PNGImageFormat png;
                if(!stream.openedOk() || !png.writeImageToStream(image,stream))
                    throw std::runtime_error("TUNE menu preview could not be saved");
            }
            if(!QinSynthEditorTestAccess::tuningLabel(editor,0,"D")
               || !QinSynthEditorTestAccess::tuningLabel(editor,1,"D#")
               || !QinSynthEditorTestAccess::tuningPianoKeys(editor)
               || std::abs(preview.parameters.getRawParameterValue("tune1")->load()-25)>.1)
                throw std::runtime_error("TUNE piano keys must transpose deviations and lock the root");
            QinSynthEditorTestAccess::closeTuningMenu(editor);
            if(QinSynthEditorTestAccess::tuningMenuVisible(editor))
                throw std::runtime_error("TUNE menu must close from its header button");
            QinSynthEditorTestAccess::showDelay(editor);
            QinSynthEditorTestAccess::setSync(editor,true);
            QinSynthEditorTestAccess::setVerbNote(editor,13);
            if(!QinSynthEditorTestAccess::syncNoteVisible(editor)
               || std::abs(preview.parameters.getRawParameterValue("verbNote")->load()-13)>1.e-6)
                throw std::runtime_error("Delay Sync must replace free Time with an automatable note selector");
            QinSynthEditorTestAccess::setSync(editor,false);
            QinSynthEditorTestAccess::showDrive(editor);
            QinSynthEditorTestAccess::selectFmSource(editor,0,5);
            QinSynthEditorTestAccess::setFmAmount(editor,0,68);
            if(std::abs(preview.parameters.getRawParameterValue("osc1FmSource")->load()-4)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("osc1FmAmount")->load()-68)>.1)
                throw std::runtime_error("FM source and amount controls must update host parameters");
            QinSynthEditorTestAccess::oscillator(editor,2);
            if(!QinSynthEditorTestAccess::fmVisible(editor,2)
               || QinSynthEditorTestAccess::fmVisible(editor,0)
               || !QinSynthEditorTestAccess::fineReadoutLayout(editor,2))
                throw std::runtime_error("FM and Fine controls must switch with OSC tabs");
            QinSynthEditorTestAccess::setFine(editor,2,-1200);
            if(std::abs(preview.parameters.getRawParameterValue("osc3Fine")->load()+1200)>.1
               || !QinSynthEditorTestAccess::fineShowsCents(editor,2,"FINE -1200.0c"))
                throw std::runtime_error("The third oscillator Fine readout must retain its full cent range");
            QinSynthEditorTestAccess::setFine(editor,2,0);
            QinSynthEditorTestAccess::oscillator(editor,0);
            QinSynthEditorTestAccess::setDetuneShape(editor,0,75.f);
            if(std::abs(preview.parameters.getRawParameterValue("osc1DetuneShape")->load()-75.f)>.1f)
                throw std::runtime_error("Detune Shape knob must update the host parameter");
            QinSynthEditorTestAccess::setDetuneShape(editor,0,0.f);
            QinSynthEditorTestAccess::setVoiceCount(editor,4);
            QinSynthEditorTestAccess::setWave(editor,1,{.75f,.5f});
            QinSynthEditorTestAccess::setRetrig(editor,2,true);
            if(std::abs(preview.parameters.getRawParameterValue("voiceCount")->load()-4)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("osc2WaveX")->load()-75)>1.e-3
               || std::abs(preview.parameters.getRawParameterValue("osc2WaveY")->load()-50)>1.e-3
               || preview.parameters.getRawParameterValue("osc3Retrig")->load()<.5f)
                throw std::runtime_error("Polyphony, waveform pad, and Retrig UI must update their host parameters");
            set(preview,"osc2WaveX",50); set(preview,"osc2WaveY",100);
            QinSynthEditorTestAccess::tick(editor);
            const auto automated=QinSynthEditorTestAccess::wavePosition(editor,1);
            if(std::abs(automated.x-.5f)>.001f || std::abs(automated.y-1.f)>.001f)
                throw std::runtime_error("Waveform pad must follow host automation");
            if(!QinSynthEditorTestAccess::playVisible(editor)
               || QinSynthEditorTestAccess::legatoRules(editor)!=4
               || QinSynthEditorTestAccess::ruleEnabled(editor)
               || QinSynthEditorTestAccess::activeLabel(editor)!="0 ACTIVE")
                throw std::runtime_error("Play panel and active-voice readout must initialize correctly");
            QinSynthEditorTestAccess::setPolyLegato(editor,true);
            QinSynthEditorTestAccess::selectLegatoRule(editor,4);
            QinSynthEditorTestAccess::setSlide(editor,125,100);
            if(!QinSynthEditorTestAccess::ruleEnabled(editor)
               || std::abs(preview.parameters.getRawParameterValue("polyLegato")->load()-1)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("legatoMode")->load()-3)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("slideTime")->load()-125)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("slideShape")->load()-100)>1.e-6
               || QinSynthEditorTestAccess::slideLabel(editor)!="125 ms"
               || QinSynthEditorTestAccess::shapeLabel(editor)!="SIGMOID")
                throw std::runtime_error("Play controls must update host parameters and show their glide values");
            QinSynthEditorTestAccess::selectLegatoRule(editor,1);
            QinSynthEditorTestAccess::setSlide(editor,80,50);
            if(!QinSynthEditorTestAccess::oscillatorVisible(editor,0)
               || QinSynthEditorTestAccess::oscillatorVisible(editor,1)
               || !QinSynthEditorTestAccess::voiceFxVisible(editor)
               || !QinSynthEditorTestAccess::envelopeVisible(editor,0)
               || !QinSynthEditorTestAccess::filterVisible(editor)
               || !QinSynthEditorTestAccess::noiseVisible(editor)
               || !QinSynthEditorTestAccess::curveVisible(editor,0))
                throw std::runtime_error("OSC, ENV, FLT, and FX panels must be present together");
            if(!QinSynthEditorTestAccess::shapeControlsAreSliders(editor))
                throw std::runtime_error("Envelope and filter controls must use sliders");
            for(const auto index:{18,19,21,22,23,25,26,27,29})
            {
                const auto milliseconds=QinSynthEditorTestAccess::sliderMidpoint(editor,index)*(index>=22 && index<=25 ? 1. : 1000.);
                if(std::abs(milliseconds-std::sqrt(5000.))>1.)
                    throw std::runtime_error("Every envelope time slider must use exponential millisecond positioning");
            }
            if(std::abs(QinSynthEditorTestAccess::envelopeTextValue(editor,18,"150ms")-.15)>.001
               || std::abs(QinSynthEditorTestAccess::envelopeTextValue(editor,22,"1.5s")-1500.)>.01)
                throw std::runtime_error("Compact vertical envelope values must accept milliseconds and seconds");
            QinSynthEditorTestAccess::timesFive(editor);
            if(std::abs(QinSynthEditorTestAccess::envelopeTextValue(editor,18,"150ms")-.03)>.001)
                throw std::runtime_error("Envelope text entry must respect the times-five multiplier");
            QinSynthEditorTestAccess::timesFive(editor);
            if(!QinSynthEditorTestAccess::mergeVisible(editor))
                throw std::runtime_error("Filter panel must show the Merge slider");
            const auto changes=[&editor](juce::Rectangle<int> area,auto change)
            {
                const auto before=editor.createComponentSnapshot(area);
                change();
                const auto after=editor.createComponentSnapshot(area);
                int different=0;
                for(int y=0;y<before.getHeight();y+=2)
                    for(int x=0;x<before.getWidth();x+=2)
                        different+=before.getPixelAt(x,y)!=after.getPixelAt(x,y);
                return different;
            };
            if(changes({34,198,270,54},[&]{QinSynthEditorTestAccess::setWave(editor,0,{1.f,1.f});})<30)
                throw std::runtime_error("OSC waveform plot must follow the square pad");
            QinSynthEditorTestAccess::setWave(editor,0,{0.f,0.f});
            QinSynthEditorTestAccess::setSlide(editor,80,0);
            if(changes({297,554,44,55},[&]{QinSynthEditorTestAccess::setSlide(editor,80,50);})<12
               || changes({297,554,44,55},[&]{QinSynthEditorTestAccess::setSlide(editor,80,100);})<12)
                throw std::runtime_error("Legato curve preview must follow the Shape knob");
            QinSynthEditorTestAccess::setSlide(editor,80,50);
            if(changes({455,448,218,45},[&]{QinSynthEditorTestAccess::previewValue(editor,20,0);})<20)
                throw std::runtime_error("Envelope plot must respond to sustain changes");
            QinSynthEditorTestAccess::previewValue(editor,20,100);
            if(changes({455,448,218,45},[&]{QinSynthEditorTestAccess::previewCurve(editor,0,100);})<20)
                throw std::runtime_error("Envelope plot must respond to curve changes");
            QinSynthEditorTestAccess::previewCurve(editor,0,0);
            QinSynthEditorTestAccess::previewFilter(editor,4);
            QinSynthEditorTestAccess::previewValue(editor,31,200);
            if(changes({455,140,218,85},[&]{QinSynthEditorTestAccess::previewValue(editor,31,5000);})<20)
                throw std::runtime_error("Filter plot must respond to cutoff changes");
            QinSynthEditorTestAccess::previewValue(editor,31,2500);
            QinSynthEditorTestAccess::previewValue(editor,32,95);
            QinSynthEditorTestAccess::previewValue(editor,34,35);
            QinSynthEditorTestAccess::previewValue(editor,24,20);
            QinSynthEditorTestAccess::previewFilter(editor,5);
            QinSynthEditorTestAccess::oscillator(editor,2);
            if(!QinSynthEditorTestAccess::oscillatorVisible(editor,2)
               || QinSynthEditorTestAccess::oscillatorVisible(editor,0))
                throw std::runtime_error("Oscillator tabs must change the visible Sound controls");
            QinSynthEditorTestAccess::oscillator(editor,0);
            QinSynthEditorTestAccess::enableNoise(editor);
            if(!QinSynthEditorTestAccess::noisePanelVisible(editor)
               || QinSynthEditorTestAccess::noiseStyleCount(editor)!=3
               || preview.parameters.getRawParameterValue("noise")->load()<.5f)
                throw std::runtime_error("N must open a matching oscillator panel with White, Pink, and Velvet styles");
            QinSynthEditorTestAccess::selectNoiseStyle(editor,3);
            if(preview.parameters.getRawParameterValue("noiseStyle")->load()<1.5f)
                throw std::runtime_error("Noise Style must update its host parameter");
            QinSynthEditorTestAccess::oscillator(editor,0);
            QinSynthEditorTestAccess::envelope(editor,1);
            if(!QinSynthEditorTestAccess::envelopeVisible(editor,1)
               || QinSynthEditorTestAccess::envelopeVisible(editor,0)
               || !QinSynthEditorTestAccess::curveVisible(editor,1)
               || QinSynthEditorTestAccess::curveVisible(editor,0)
               || !QinSynthEditorTestAccess::timesFiveVisible(editor))
                throw std::runtime_error("Envelope tabs must select their four ADSR controls");
            QinSynthEditorTestAccess::envelope(editor,0);
            if(!QinSynthEditorTestAccess::timesFiveVisible(editor))
                throw std::runtime_error("The time multiplier must remain available on the AMP envelope");
            QinSynthEditorTestAccess::previewValue(editor,37,65);
            if(!QinSynthEditorTestAccess::transformerVisible(editor)
               || QinSynthEditorTestAccess::driveModeCount(editor)!=4)
                throw std::runtime_error("FX must expose four Zao circuits and the output transformer");
            QinSynthEditorTestAccess::selectDriveMode(editor,3);
            QinSynthEditorTestAccess::setDriveMix(editor,45);
            QinSynthEditorTestAccess::setTransformer(editor,true);
            if(std::abs(preview.parameters.getRawParameterValue("driveMode")->load()-2)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("driveMix")->load()-45)>1.e-3
               || preview.parameters.getRawParameterValue("transformer")->load()<.5f)
                throw std::runtime_error("Drive circuit, mix, and transformer controls must update host parameters");
            if(changes({707,334,182,133},[&]{QinSynthEditorTestAccess::selectDriveMode(editor,4);})<20)
                throw std::runtime_error("Drive preview must follow the selected circuit");
            QinSynthEditorTestAccess::selectDriveMode(editor,2);
            QinSynthEditorTestAccess::setTransformer(editor,false);
            const auto save=[&editor](const char* name)
            {
                const auto image=editor.createComponentSnapshot(editor.getLocalBounds());
                const juce::File file(name);
                file.deleteFile();
                juce::FileOutputStream stream{file};
                juce::PNGImageFormat png;
                if(!stream.openedOk() || !png.writeImageToStream(image,stream))
                    throw std::runtime_error("Qin UI preview could not be saved");
            };
            if(argc>1) save(argv[1]);
            if(!QinSynthEditorTestAccess::voiceFxVisible(editor)
               || QinSynthEditorTestAccess::delayFxVisible(editor))
                throw std::runtime_error("FX panel must open on drive controls");
            QinSynthEditorTestAccess::envelope(editor,1);
            QinSynthEditorTestAccess::timesFive(editor);
            if(argc>2) save(argv[2]);
            QinSynthEditorTestAccess::timesFive(editor);
            QinSynthEditorTestAccess::envelope(editor,0);
            QinSynthEditorTestAccess::fxSection(editor,1);
            if(!QinSynthEditorTestAccess::delayFxVisible(editor)
               || QinSynthEditorTestAccess::driveVisible(editor)
               || !QinSynthEditorTestAccess::transformerVisible(editor)
               || !QinSynthEditorTestAccess::outputSectionVisible(editor))
                throw std::runtime_error("Delay-verb tab must show only the reverb controls");
            if(argc>3) save(argv[3]);
            QinSynthEditorTestAccess::envelope(editor,2);
            if(!QinSynthEditorTestAccess::modVisible(editor) || !QinSynthEditorTestAccess::timesFiveVisible(editor))
                throw std::runtime_error("MOD tab must show destination and depth controls");
            QinSynthEditorTestAccess::selectModDestination(editor,qin::modTargetIndex("osc1Level"));
            if(std::abs(preview.parameters.getRawParameterValue("modTarget")->load()
                -qin::modTargetIndex("osc1Level"))>1.e-6)
                throw std::runtime_error("The MOD destination menu must update the host parameter");
            if(argc>4) save(argv[4]);
            QinSynthEditorTestAccess::oscillator(editor,3);
            if(argc>5) save(argv[5]);
        }
        {
            auto scaledOwner=std::make_unique<QinSynthProcessor>();
            auto& scaled=*scaledOwner;
            QinSynthEditor editor(scaled);
            const std::array<std::array<float,3>,3> original {
                QinSynthTestAccess::envelopeTimes(scaled,0),
                QinSynthTestAccess::envelopeTimes(scaled,1),
                QinSynthTestAccess::envelopeTimes(scaled,2) };
            constexpr std::array<const char*,3> ids {"ampTimesFive","fltTimesFive","modTimesFive"};
            for(int selected=0;selected<3;++selected)
            {
                QinSynthEditorTestAccess::envelope(editor,selected);
                QinSynthEditorTestAccess::timesFive(editor);
                for(int env=0;env<3;++env)
                {
                    const auto actual=QinSynthTestAccess::envelopeTimes(scaled,env);
                    const auto multiplier=env==selected ? 5.f : 1.f;
                    if(!QinSynthEditorTestAccess::timesFiveVisible(editor)
                       || std::abs(scaled.parameters.getRawParameterValue(ids[size_t(env)])->load()
                           -(env==selected ? 1.f : 0.f))>.01f)
                        throw std::runtime_error("Each envelope must retain its own times-five switch state");
                    for(int stage=0;stage<3;++stage)
                        if(std::abs(actual[size_t(stage)]-multiplier*original[size_t(env)][size_t(stage)])>.001f)
                            throw std::runtime_error("Times-five must affect only the selected envelope");
                }
                QinSynthEditorTestAccess::timesFive(editor);
            }
            QinSynthEditorTestAccess::envelope(editor,1);
            QinSynthEditorTestAccess::timesFive(editor);
            set(scaled,"fltRelease",5000.f);
            if(std::abs(QinSynthTestAccess::envelopeTimes(scaled,1)[2]-25.f)>.001f)
                throw std::runtime_error("The FLT time multiplier must extend a 5000 ms release to 25 seconds");
            set(scaled,"fltRelease",300.f);
            if(std::abs(scaled.parameters.getRawParameterValue("fltAttack")->load()-10.f)>.1f
               || std::abs(scaled.parameters.getRawParameterValue("attack")->load()-.01f)>.0001f)
                throw std::runtime_error("The time multiplier must preserve the underlying envelope values");
            QinSynthEditorTestAccess::timesFive(editor);
            QinSynthEditorTestAccess::envelope(editor,0);
            QinSynthEditorTestAccess::timesFive(editor);
            if(scaled.getTailLengthSeconds()<125.)
                throw std::runtime_error("The AMP time multiplier must report its extended audio tail");
            QinSynthEditorTestAccess::timesFive(editor);
            for(int env=0;env<3;++env)
            {
                const auto restored=QinSynthTestAccess::envelopeTimes(scaled,env);
                for(int stage=0;stage<3;++stage)
                    if(std::abs(restored[size_t(stage)]-original[size_t(env)][size_t(stage)])>.001f)
                        throw std::runtime_error("Turning off the time multiplier must restore all envelope times");
            }
            if(std::abs(scaled.parameters.getRawParameterValue("fltSustain")->load())>1.e-6f)
                throw std::runtime_error("The time multiplier must leave Sustain unchanged");
        }
        for(int envelope=0;envelope<3;++envelope)
        {
            auto separateOwner=std::make_unique<QinSynthProcessor>();
            auto mergedOwner=std::make_unique<QinSynthProcessor>();
            auto& separate=*separateOwner; auto& merged=*mergedOwner;
            for(auto* instrument:{&separate,&merged})
            {
                set(*instrument,"verbMix",0);
                set(*instrument,"attack",.2f); set(*instrument,"fltAttack",200.f);
                set(*instrument,"modAttack",.2f);
                set(*instrument,"ampSustain",100); set(*instrument,"fltSustain",100);
                set(*instrument,"modSustain",100);
                instrument->prepareToPlay(48000,256);
            }
            constexpr std::array<const char*,3> mergeIds {"ampMerge","fltMerge","modMerge"};
            set(merged,mergeIds[size_t(envelope)],100);
            juce::AudioBuffer<float> dry(2,256),wet(2,256);
            juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            separate.processBlock(dry,note); merged.processBlock(wet,note); note.clear();
            for(int blockIndex=0;blockIndex<10;++blockIndex)
            { separate.processBlock(dry,note); merged.processBlock(wet,note); }
            note.addEvent(juce::MidiMessage::noteOn(1,64,1.f),0);
            separate.processBlock(dry,note); merged.processBlock(wet,note);
            if(QinSynthTestAccess::effectiveEnvelope(merged,1,envelope)
                  <=QinSynthTestAccess::effectiveEnvelope(separate,1,envelope)+.03f)
                throw std::runtime_error("Envelope Merge must give a new overlapping note the shared envelope level");
            const auto other=(envelope+1)%3;
            if(std::abs(QinSynthTestAccess::effectiveEnvelope(merged,1,other)
                 -QinSynthTestAccess::effectiveEnvelope(separate,1,other))>.01f)
                throw std::runtime_error("Envelope Merge must affect only the selected envelope");
            if(envelope==0)
            {
                set(merged,"release",.01f);
                note.clear(); note.addEvent(juce::MidiMessage::noteOff(1,64),0);
                for(int blockIndex=0;blockIndex<50;++blockIndex)
                { merged.processBlock(wet,note); note.clear(); }
                if(QinSynthTestAccess::voiceActive(merged,1) || !QinSynthTestAccess::voiceActive(merged,0))
                    throw std::runtime_error("Merged AMP must release one note while another remains held");
            }
        }
        {
            QinSynthProcessor automation;
            QinSynthEditor editor(automation);
            constexpr std::array<const char*,3> ids {"ampMerge","fltMerge","modMerge"};
            for(int env=0;env<3;++env)
            {
                QinSynthEditorTestAccess::envelope(editor,env);
                if(!QinSynthEditorTestAccess::envelopeMergeVisible(editor,env))
                    throw std::runtime_error("Each ENV tab needs its horizontal Merge slider below ADSRC");
                QinSynthEditorTestAccess::setEnvelopeMerge(editor,env,25.f*(env+1));
                if(std::abs(automation.parameters.getRawParameterValue(ids[size_t(env)])->load()-25.f*(env+1))>.1f)
                    throw std::runtime_error("Each envelope Merge slider must retain its own host value");
            }
            juce::MemoryBlock state;
            automation.getStateInformation(state);
            QinSynthProcessor restored;
            restored.setStateInformation(state.getData(),int(state.getSize()));
            for(int env=0;env<3;++env)
                if(std::abs(restored.parameters.getRawParameterValue(ids[size_t(env)])->load()-25.f*(env+1))>.1f)
                    throw std::runtime_error("The three Merge controls must survive host state restoration");
        }
        std::cout<<"Qin Synth processor checks passed\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
