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
    static void page(QinSynthEditor& editor,int index) { editor.showPage(index); }
    static void fxSection(QinSynthEditor& editor,int index) { editor.showFxSection(index); }
    static bool oscillatorVisible(const QinSynthEditor& editor,int index)
    { return editor.waves[size_t(index)].isVisible() && editor.sliders[size_t(index*4)].isVisible(); }
    static bool voiceFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[12].isVisible() && editor.sliders[16].isVisible(); }
    static bool delayFxVisible(const QinSynthEditor& editor)
    { return editor.sliders[17].isVisible() && editor.sliders[22].isVisible(); }
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
            QinSynthProcessor preview;
            QinSynthEditor editor(preview);
            if(!QinSynthEditorTestAccess::oscillatorVisible(editor,0)
               || QinSynthEditorTestAccess::oscillatorVisible(editor,1)
               || QinSynthEditorTestAccess::voiceFxVisible(editor))
                throw std::runtime_error("Sound page must show only the selected oscillator");
            QinSynthEditorTestAccess::oscillator(editor,2);
            if(!QinSynthEditorTestAccess::oscillatorVisible(editor,2)
               || QinSynthEditorTestAccess::oscillatorVisible(editor,0))
                throw std::runtime_error("Oscillator tabs must change the visible Sound controls");
            QinSynthEditorTestAccess::oscillator(editor,0);
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
            QinSynthEditorTestAccess::page(editor,1);
            if(!QinSynthEditorTestAccess::voiceFxVisible(editor)
               || QinSynthEditorTestAccess::delayFxVisible(editor)
               || QinSynthEditorTestAccess::oscillatorVisible(editor,0))
                throw std::runtime_error("FX page must open on voice and drive controls");
            if(argc>2) save(argv[2]);
            QinSynthEditorTestAccess::fxSection(editor,1);
            if(!QinSynthEditorTestAccess::delayFxVisible(editor)
               || QinSynthEditorTestAccess::voiceFxVisible(editor))
                throw std::runtime_error("Delay-verb tab must show only the reverb controls");
            if(argc>3) save(argv[3]);
        }
        std::cout<<"Qin Synth processor checks passed\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
