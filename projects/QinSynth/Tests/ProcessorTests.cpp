#include "../Source/PluginProcessor.h"
#include "../../ParameterRetentionTests.h"
#include "../Source/PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

struct QinSynthTestAccess
{
    static int active(const QinSynthProcessor& p)
    { int count=0; for(const auto& voice:p.voices) if(voice.active) ++count; return count; }
    static float tap(const QinSynthProcessor& p) { return p.onsetTap; }
    static float phase(const QinSynthProcessor& p) { return p.onsetPhase; }
    static uint32_t analysisJobs(const QinSynthProcessor& p) { return p.reverb.analysisJobs(); }
    static bool transformerRunning(const QinSynthProcessor& p) { return p.transformerStage.isRunning(); }
    static int monoNote(const QinSynthProcessor& p) { return p.voices[0].note; }
    static bool monoHeld(const QinSynthProcessor& p) { return p.voices[0].keyDown; }
    static double monoPhase(const QinSynthProcessor& p) { return p.voices[0].oscillators[0][0].phase; }
    static float monoEnvelope(const QinSynthProcessor& p) { return p.voices[0].env[0].value; }
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
        && editor.sliders[size_t(index*6)].isVisible(); }
    static bool voiceDragEnabled(const QinSynthEditor& editor)
    { return editor.voiceCount.isVisible()
        && editor.voiceCount.getSliderStyle()==juce::Slider::RotaryVerticalDrag
        && std::abs(editor.voiceCount.getMinimum()-1)<1.e-6
        && std::abs(editor.voiceCount.getMaximum()-16)<1.e-6; }
    static bool waveSwitchEnabled(const QinSynthEditor& editor,int index)
    { return editor.waves[size_t(index)].getSliderStyle()==juce::Slider::LinearHorizontal
        && std::abs(editor.waves[size_t(index)].getMinimum())<1.e-6
        && std::abs(editor.waves[size_t(index)].getMaximum()-2)<1.e-6; }
    static void setVoiceCount(QinSynthEditor& editor,int count)
    { editor.voiceCount.setValue(count,juce::sendNotificationSync); }
    static void setWave(QinSynthEditor& editor,int index,int wave)
    { editor.waves[size_t(index)].setValue(wave,juce::sendNotificationSync); }
    static void setRetrig(QinSynthEditor& editor,int index,bool enabled)
    { editor.retrig[size_t(index)].setToggleState(enabled,juce::sendNotificationSync); }
    static bool voiceFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[37].isVisible() && editor.sliders[36].isVisible(); }
    static bool delayFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[38].isVisible() && editor.sliders[43].isVisible(); }
    static bool driveVisible(const QinSynthEditor& editor)
    { return editor.sliders[37].isVisible() && editor.driveMode.isVisible(); }
    static bool transformerVisible(const QinSynthEditor& editor) { return editor.transformerButton.isVisible(); }
    static int driveModeCount(const QinSynthEditor& editor) { return editor.driveMode.getNumItems(); }
    static void selectDriveMode(QinSynthEditor& editor,int item)
    { editor.driveMode.setSelectedId(item,juce::sendNotificationSync); }
    static void setTransformer(QinSynthEditor& editor,bool on)
    { editor.transformerButton.setToggleState(on,juce::sendNotificationSync); }
    static bool envelopeVisible(const QinSynthEditor& editor,int index)
    { return editor.sliders[size_t(18+index*4)].isVisible(); }
    static bool filterVisible(const QinSynthEditor& editor)
    { return editor.filterType.isVisible() && editor.sliders[31].isVisible(); }
    static bool modVisible(const QinSynthEditor& editor)
    { return editor.sliders[30].isVisible() && editor.modTarget.isVisible(); }
    static bool noiseVisible(const QinSynthEditor& editor) { return editor.noiseButton.isVisible(); }
    static void enableNoise(QinSynthEditor& editor)
    { editor.noiseButton.setToggleState(true,juce::sendNotificationSync); }
    static bool noisePanelVisible(const QinSynthEditor& editor)
    { return editor.noiseStyle.isVisible() && editor.noiseSliders[0].isVisible() && !editor.waves[0].isVisible(); }
    static int noiseStyleCount(const QinSynthEditor& editor) { return editor.noiseStyle.getNumItems(); }
    static void selectNoiseStyle(QinSynthEditor& editor,int style)
    { editor.noiseStyle.setSelectedId(style,juce::sendNotificationSync); }
    static bool curveVisible(const QinSynthEditor& editor,int index)
    { return editor.curveSliders[size_t(index)].isVisible(); }
    static bool mergeVisible(const QinSynthEditor& editor)
    { return editor.filterMergeSlider.isVisible(); }
    static bool timesFiveVisible(const QinSynthEditor& editor)
    { return editor.timesFiveButton.isVisible(); }
    static void timesFive(QinSynthEditor& editor)
    { editor.timesFiveButton.setToggleState(!editor.timesFiveButton.getToggleState(),juce::sendNotificationSync); }
    static void tick(QinSynthEditor& editor) { editor.timerCallback(); }
    static bool liveScope(const QinSynthEditor& editor) { return editor.liveScopeValid; }
    static float liveCutoff(const QinSynthEditor& editor) { return editor.liveCutoff; }
    static bool shapeControlsAreSliders(const QinSynthEditor& editor)
    {
        for(int index=18;index<=34;++index)
            if(editor.sliders[size_t(index)].getSliderStyle()!=juce::Slider::LinearHorizontal) return false;
        return true;
    }
    static void previewValue(QinSynthEditor& editor,int index,double value)
    { editor.sliders[size_t(index)].setValue(value,juce::sendNotificationSync); }
    static double sliderMidpoint(QinSynthEditor& editor,int index)
    { return editor.sliders[size_t(index)].proportionOfLengthToValue(.5); }
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
int main(int argc,char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        verifyParameterRetention<QinSynthProcessor>();
        QinSynthProcessor synth;
        set(synth,"verbMix",0); synth.prepareToPlay(48000,256);
        if(!synth.acceptsMidi() || !synth.isBusesLayoutSupported(synth.getBusesLayout()))
            throw std::runtime_error("Qin must expose a stereo MIDI instrument bus");
        const auto cutoffMid=synth.parameters.getParameter("filterCutoff")->convertFrom0to1(.5f);
        const auto resonanceMid=synth.parameters.getParameter("filterResonance")->convertFrom0to1(.5f);
        if(std::abs(cutoffMid-std::sqrt(30.f*18000.f))>2.f || std::abs(resonanceMid-(std::sqrt(101.f)-1.f))>.2f)
            throw std::runtime_error("Cutoff and resonance automation must use logarithmic positions");
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
        QinSynthProcessor limited;
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
        QinSynthProcessor dense;
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
        QinSynthProcessor low,high;
        for(auto* p:{&low,&high}) {set(*p,"keyPan",100); set(*p,"verbMix",0); p->prepareToPlay(48000,256);}
        juce::AudioBuffer<float> lowAudio(2,256),highAudio(2,256);
        juce::MidiBuffer lowMidi,highMidi;
        lowMidi.addEvent(juce::MidiMessage::noteOn(1,36,.8f),0);
        highMidi.addEvent(juce::MidiMessage::noteOn(1,96,.8f),0);
        for(int i=0;i<8;++i)
        {low.processBlock(lowAudio,lowMidi); high.processBlock(highAudio,highMidi); lowMidi.clear(); highMidi.clear();}
        if(power(lowAudio,0)<=power(lowAudio,1)*2 || power(highAudio,1)<=power(highAudio,0)*2)
            throw std::runtime_error("Low keys must favor left and high keys must favor right");
        {
            QinSynthProcessor plain, driven;
            for(auto* p:{&plain,&driven}) {set(*p,"verbMix",0); p->prepareToPlay(48000,256);}
            set(driven,"drive",75);
            juce::AudioBuffer<float> a(2,256),b(2,256);
            juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            double difference=0;
            for(int blockIndex=0;blockIndex<8;++blockIndex)
            {
                plain.processBlock(a,note); driven.processBlock(b,note); note.clear();
                for(int i=0;i<256;++i) difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
            }
            if(difference<1) throw std::runtime_error("Zao drive control must alter the generated tone");
        }
        {
            std::array<std::array<float,256>,4> outputs {};
            for(int mode=0;mode<4;++mode)
            {
                QinSynthProcessor instrument;
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
            QinSynthProcessor clean,coloured;
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
            QinSynthProcessor mono;
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
            QinSynthProcessor midiMono;
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
            QinSynthProcessor tail;
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
            QinSynthProcessor left,right;
            for(auto* p:{&left,&right}) { set(*p,"keyPan",0); set(*p,"verbMix",0); p->prepareToPlay(48000,256); }
            set(left,"osc1Pan",-100); set(right,"osc1Pan",100);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<8;++i) {left.processBlock(a,note); right.processBlock(b,note); note.clear();}
            if(power(a,0)<power(a,1)*10 || power(b,1)<power(b,0)*10)
                throw std::runtime_error("Per-oscillator pan must place the whole oscillator in stereo");
        }
        {
            QinSynthProcessor narrow,wide;
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
            QinSynthProcessor filtered,reference;
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
            QinSynthProcessor voices,master;
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
            QinSynthProcessor plain,noisy;
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
            const auto renderNoise=[](int style,float tune,float hpf,float level,float pan)
            {
                QinSynthProcessor instrument;
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
            QinSynthProcessor early,late;
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
                        QinSynthProcessor stressed;
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
            QinSynthProcessor held,fading;
            for(auto* p:{&held,&fading}) { set(*p,"verbMix",0); set(*p,"attack",.001f); p->prepareToPlay(48000,256); }
            set(fading,"ampSustain",0); set(fading,"ampDecay",.01f);
            juce::AudioBuffer<float> a(2,256),b(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            for(int i=0;i<36;++i) { held.processBlock(a,note); fading.processBlock(b,note); note.clear(); }
            if(power(b,0)>power(a,0)*.001)
                throw std::runtime_error("AMP decay and sustain must shape a held note");
        }
        {
            QinSynthProcessor staticFilter,movingFilter;
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
            QinSynthProcessor monitored;
            set(monitored,"verbMix",0); set(monitored,"drive",65);
            set(monitored,"filterType",3); set(monitored,"filterCutoff",500);
            set(monitored,"filterEnvAmount",100); set(monitored,"fltAttack",500);
            set(monitored,"fltSustain",100); monitored.prepareToPlay(48000,256);
            QinSynthEditor editor(monitored);
            juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer note;
            note.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
            monitored.processBlock(audio,note); note.clear();
            QinSynthEditorTestAccess::tick(editor);
            const auto firstCutoff=QinSynthEditorTestAccess::liveCutoff(editor);
            const auto firstPlot=editor.createComponentSnapshot({455,185,218,90});
            for(int blockIndex=0;blockIndex<24;++blockIndex) monitored.processBlock(audio,note);
            QinSynthEditorTestAccess::tick(editor);
            const auto lateCutoff=QinSynthEditorTestAccess::liveCutoff(editor);
            const auto latePlot=editor.createComponentSnapshot({455,185,218,90});
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
        }
        {
            QinSynthProcessor plain,modulated;
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
        {
            QinSynthProcessor preview;
            QinSynthEditor editor(preview);
            if(!QinSynthEditorTestAccess::voiceDragEnabled(editor)
               || !QinSynthEditorTestAccess::waveSwitchEnabled(editor,0)
               || !QinSynthEditorTestAccess::waveSwitchEnabled(editor,1)
               || !QinSynthEditorTestAccess::waveSwitchEnabled(editor,2))
                throw std::runtime_error("Polyphony readout and oscillator waves must use draggable switches");
            QinSynthEditorTestAccess::setVoiceCount(editor,4);
            QinSynthEditorTestAccess::setWave(editor,1,2);
            QinSynthEditorTestAccess::setRetrig(editor,2,true);
            if(std::abs(preview.parameters.getRawParameterValue("voiceCount")->load()-4)>1.e-6
               || std::abs(preview.parameters.getRawParameterValue("osc2Wave")->load()-2)>1.e-6
               || preview.parameters.getRawParameterValue("osc3Retrig")->load()<.5f)
                throw std::runtime_error("Polyphony, waveform, and Retrig UI must update their host parameters");
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
            if(changes({455,500,218,64},[&]{QinSynthEditorTestAccess::previewValue(editor,20,0);})<20)
                throw std::runtime_error("Envelope plot must respond to sustain changes");
            QinSynthEditorTestAccess::previewValue(editor,20,100);
            if(changes({455,500,218,64},[&]{QinSynthEditorTestAccess::previewCurve(editor,0,100);})<20)
                throw std::runtime_error("Envelope plot must respond to curve changes");
            QinSynthEditorTestAccess::previewCurve(editor,0,0);
            QinSynthEditorTestAccess::previewFilter(editor,4);
            QinSynthEditorTestAccess::previewValue(editor,31,200);
            if(changes({455,185,218,90},[&]{QinSynthEditorTestAccess::previewValue(editor,31,5000);})<20)
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
            QinSynthEditorTestAccess::setTransformer(editor,true);
            if(std::abs(preview.parameters.getRawParameterValue("driveMode")->load()-2)>1.e-6
               || preview.parameters.getRawParameterValue("transformer")->load()<.5f)
                throw std::runtime_error("Drive circuit and transformer controls must update host parameters");
            if(changes({707,392,182,153},[&]{QinSynthEditorTestAccess::selectDriveMode(editor,4);})<20)
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
               || !QinSynthEditorTestAccess::transformerVisible(editor))
                throw std::runtime_error("Delay-verb tab must show only the reverb controls");
            if(argc>3) save(argv[3]);
            QinSynthEditorTestAccess::envelope(editor,2);
            if(!QinSynthEditorTestAccess::modVisible(editor) || !QinSynthEditorTestAccess::timesFiveVisible(editor))
                throw std::runtime_error("MOD tab must show destination and depth controls");
            if(argc>4) save(argv[4]);
            QinSynthEditorTestAccess::oscillator(editor,3);
            if(argc>5) save(argv[5]);
        }
        {
            QinSynthProcessor scaled;
            QinSynthEditor editor(scaled);
            const std::array<std::array<float,3>,3> original {
                QinSynthTestAccess::envelopeTimes(scaled,0),
                QinSynthTestAccess::envelopeTimes(scaled,1),
                QinSynthTestAccess::envelopeTimes(scaled,2) };
            QinSynthEditorTestAccess::timesFive(editor);
            if(scaled.parameters.getRawParameterValue("envTimesFive")->load()<.5f)
                throw std::runtime_error("The time multiplier button must update its host parameter");
            for(int env=0;env<3;++env)
            {
                const auto expanded=QinSynthTestAccess::envelopeTimes(scaled,env);
                for(int stage=0;stage<3;++stage)
                    if(std::abs(expanded[size_t(stage)]-5.f*original[size_t(env)][size_t(stage)])>.001f)
                        throw std::runtime_error("The time multiplier must affect all three ADSR time controls in every envelope");
            }
            set(scaled,"fltRelease",5000.f);
            if(std::abs(QinSynthTestAccess::envelopeTimes(scaled,1)[2]-25.f)>.001f
               || scaled.getTailLengthSeconds()<125.)
                throw std::runtime_error("The time multiplier must extend a 5000 ms release to 25 seconds and report its tail");
            set(scaled,"fltRelease",300.f);
            if(std::abs(scaled.parameters.getRawParameterValue("fltAttack")->load()-10.f)>.1f
               || std::abs(scaled.parameters.getRawParameterValue("attack")->load()-.01f)>.0001f)
                throw std::runtime_error("The time multiplier must preserve the underlying envelope values");
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
        std::cout<<"Qin Synth processor checks passed\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
