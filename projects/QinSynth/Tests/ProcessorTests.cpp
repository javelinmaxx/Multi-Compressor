#include "../Source/PluginProcessor.h"
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
    { return editor.waves[size_t(index)].isVisible() && editor.sliders[size_t(index*6)].isVisible(); }
    static bool voiceFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[37].isVisible() && editor.sliders[36].isVisible(); }
    static bool delayFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[38].isVisible() && editor.sliders[43].isVisible(); }
    static bool driveVisible(const QinSynthEditor& editor) { return editor.sliders[37].isVisible(); }
    static bool envelopeVisible(const QinSynthEditor& editor,int index)
    { return editor.sliders[size_t(18+index*4)].isVisible(); }
    static bool filterVisible(const QinSynthEditor& editor)
    { return editor.filterType.isVisible() && editor.sliders[31].isVisible(); }
    static bool modVisible(const QinSynthEditor& editor)
    { return editor.sliders[30].isVisible() && editor.modTarget.isVisible(); }
    static bool noiseVisible(const QinSynthEditor& editor) { return editor.noiseButton.isVisible(); }
    static bool curveVisible(const QinSynthEditor& editor,int index)
    { return editor.curveSliders[size_t(index)].isVisible(); }
    static bool mergeVisible(const QinSynthEditor& editor)
    { return editor.filterMergeSlider.isVisible(); }
    static bool timesFiveVisible(const QinSynthEditor& editor)
    { return editor.filterTimesFive.isVisible(); }
    static void timesFive(QinSynthEditor& editor) { editor.filterTimesFive.onClick(); }
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
            QinSynthEditorTestAccess::envelope(editor,1);
            if(!QinSynthEditorTestAccess::envelopeVisible(editor,1)
               || QinSynthEditorTestAccess::envelopeVisible(editor,0)
               || !QinSynthEditorTestAccess::curveVisible(editor,1)
               || QinSynthEditorTestAccess::curveVisible(editor,0)
               || !QinSynthEditorTestAccess::timesFiveVisible(editor))
                throw std::runtime_error("Envelope tabs must select their four ADSR controls");
            QinSynthEditorTestAccess::envelope(editor,0);
            QinSynthEditorTestAccess::previewValue(editor,37,65);
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
            if(argc>2) save(argv[2]);
            QinSynthEditorTestAccess::envelope(editor,0);
            QinSynthEditorTestAccess::fxSection(editor,1);
            if(!QinSynthEditorTestAccess::delayFxVisible(editor)
               || QinSynthEditorTestAccess::driveVisible(editor))
                throw std::runtime_error("Delay-verb tab must show only the reverb controls");
            if(argc>3) save(argv[3]);
            QinSynthEditorTestAccess::envelope(editor,2);
            if(!QinSynthEditorTestAccess::modVisible(editor))
                throw std::runtime_error("MOD tab must show destination and depth controls");
            if(argc>4) save(argv[4]);
        }
        {
            QinSynthProcessor scaled;
            QinSynthEditor editor(scaled);
            QinSynthEditorTestAccess::envelope(editor,1);
            QinSynthEditorTestAccess::timesFive(editor);
            for(const auto& pair:{std::pair{"fltAttack",50.f},
                                  std::pair{"fltDecay",1000.f},
                                  std::pair{"fltRelease",1500.f}})
                if(std::abs(scaled.parameters.getRawParameterValue(pair.first)->load()-pair.second)>.1f)
                    throw std::runtime_error("Filter time multiplier must scale attack, decay, and release");
            if(std::abs(scaled.parameters.getRawParameterValue("fltSustain")->load())>1.e-6f)
                throw std::runtime_error("Filter time multiplier must leave sustain unchanged");
        }
        std::cout<<"Qin Synth processor checks passed\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
