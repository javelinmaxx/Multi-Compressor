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
                set(*p,"fltAttack",.001f); set(*p,"fltSustain",100); p->prepareToPlay(48000,256);
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
               || !QinSynthEditorTestAccess::filterVisible(editor))
                throw std::runtime_error("OSC, ENV, FLT, and FX panels must be present together");
            if(!QinSynthEditorTestAccess::shapeControlsAreSliders(editor))
                throw std::runtime_error("Envelope and filter controls must use sliders");
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
            if(changes({455,449,218,66},[&]{QinSynthEditorTestAccess::previewValue(editor,20,0);})<20)
                throw std::runtime_error("Envelope plot must respond to sustain changes");
            QinSynthEditorTestAccess::previewValue(editor,20,100);
            QinSynthEditorTestAccess::previewFilter(editor,4);
            QinSynthEditorTestAccess::previewValue(editor,31,200);
            if(changes({455,185,218,67},[&]{QinSynthEditorTestAccess::previewValue(editor,31,5000);})<20)
                throw std::runtime_error("Filter plot must respond to cutoff changes");
            QinSynthEditorTestAccess::previewValue(editor,31,2500);
            QinSynthEditorTestAccess::previewValue(editor,32,35);
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
               || QinSynthEditorTestAccess::envelopeVisible(editor,0))
                throw std::runtime_error("Envelope tabs must select their four ADSR controls");
            QinSynthEditorTestAccess::envelope(editor,0);
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
        std::cout<<"Qin Synth processor checks passed\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
