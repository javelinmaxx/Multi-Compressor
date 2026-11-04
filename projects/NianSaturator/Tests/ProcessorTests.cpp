#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <complex>
#include <stdexcept>
static void set (NianSaturatorProcessor& p, const char* id, float value)
{ auto* parameter=p.parameters.getParameter(id); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); }
int main(int argc,char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        NianSaturatorProcessor p; set(p,"lowDrive",0); set(p,"highDrive",0); p.prepareToPlay(48000,128);
        juce::MidiBuffer midi; juce::AudioBuffer<float> audio(2,128);
        for(int block=0;block<20;++block)
        {
            for(int i=0;i<128;++i) { const auto x=float(.2*std::sin((block*128+i)*.04)); audio.setSample(0,i,x); audio.setSample(1,i,-x); }
            p.processBlock(audio,midi);
            for(int i=0;i<128;++i)
            {
                const auto x=float(.2*std::sin((block*128+i)*.04));
                if(std::abs(audio.getSample(0,i)-x)>1.e-5 || std::abs(audio.getSample(1,i)+x)>1.e-5) throw std::runtime_error("neutral crossover must reconstruct input");
            }
        }
        set(p,"mode",1); set(p,"lowDrive",100); set(p,"highDrive",100); double difference=0;
        for(int i=0;i<128;++i) { const auto x=float(.5*std::sin(i*.1)); audio.setSample(0,i,x); audio.setSample(1,i,x); }
        p.processBlock(audio,midi);
        for(int i=0;i<128;++i) { const auto x=float(.5*std::sin(i*.1)); difference+=std::abs(audio.getSample(0,i)-x); }
        if(difference<1) throw std::runtime_error("drive must change the signal");
        NianSaturatorProcessor flat, enhanced;
        for (auto* processor : {&flat,&enhanced}) { set(*processor,"crossover",1000); processor->prepareToPlay(48000,128); }
        set(flat,"lowDrive",0); set(flat,"highDrive",0); set(enhanced,"lowDrive",100); set(enhanced,"highDrive",100);
        juce::AudioBuffer<float> clean(2,128), shaped(2,128); std::array<std::complex<double>,2> before {}, after {};
        for(int block=0;block<400;++block)
        {
            for(int i=0;i<128;++i) { const auto n=block*128+i; const auto x=float(.08*std::sin(juce::MathConstants<double>::twoPi*700*n/48000)+.08*std::sin(juce::MathConstants<double>::twoPi*4000*n/48000)); for(int ch=0;ch<2;++ch) {clean.setSample(ch,i,x); shaped.setSample(ch,i,x);} }
            flat.processBlock(clean,midi); enhanced.processBlock(shaped,midi);
            if(block>=300) for(int i=0;i<128;++i) for(int band=0;band<2;++band)
            { const auto hz=band==0?700.:4000.; const auto phase=std::polar(1.,-juce::MathConstants<double>::twoPi*hz*(block*128+i)/48000); before[size_t(band)]+=double(clean.getSample(0,i))*phase; after[size_t(band)]+=double(shaped.getSample(0,i))*phase; }
        }
        for(int band=0;band<2;++band) if(std::abs(after[size_t(band)])<std::abs(before[size_t(band)])*1.2) throw std::runtime_error("F mode must enhance detected peaks in both bands");
        set(p,"bypass",1);
        for(int block=0;block<12;++block)
        {
            for(int i=0;i<128;++i) { const auto x=float(.2*std::sin((block*128+i)*.04)); audio.setSample(0,i,x); audio.setSample(1,i,-x); }
            p.processBlock(audio,midi);
            if(block==11) for(int i=0;i<128;++i) if(std::abs(audio.getSample(0,i)-float(.2*std::sin((block*128+i)*.04)))>1.e-5) throw std::runtime_error("bypass must settle to the original signal");
        }
        set(p,"bypass",0);
        for(int block=0;block<100;++block)
        {
            if(block%10==0) { set(p,"crossover",block%20==0?80:8000); set(p,"mode",block%20==0?0:1); }
            for(int i=0;i<128;++i) { const auto x=float(.2*std::sin((block*128+i)*.08)); audio.setSample(0,i,x); audio.setSample(1,i,-x); }
            p.processBlock(audio,midi);
            for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i)
                if(!std::isfinite(audio.getSample(ch,i)) || std::abs(audio.getSample(ch,i))>4) throw std::runtime_error("automated crossover and mode must remain finite and bounded");
        }
        set(p,"mode",0); set(p,"crossover",1000); set(p,"lowDrive",30); set(p,"highDrive",30);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor()); editor->addToDesktop(0); editor->setVisible(true);
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        if(!image.isValid()) throw std::runtime_error("editor must render");
        if(argc>1) { juce::File file(argv[1]); file.getParentDirectory().createDirectory(); auto out=file.createOutputStream(); if(!out || !out->setPosition(0) || out->truncate().failed() || !juce::PNGImageFormat().writeImageToStream(image,*out)) throw std::runtime_error("preview write failed"); }
        juce::ComboBox* selector=nullptr;
        for(auto* child : editor->getChildren()) if(auto* box=dynamic_cast<juce::ComboBox*>(child)) selector=box;
        if(!selector) throw std::runtime_error("mode selector missing");
        selector->setSelectedId(2,juce::sendNotificationSync);
        if(p.parameters.getRawParameterValue("mode")->load()<.5f) throw std::runtime_error("mode selector must update the host parameter");
        for (float scale : {.5f,1.2f})
        {
            editor->setSize(juce::roundToInt(444*scale),juce::roundToInt(260*scale));
            for(auto* child : editor->getChildren())
                if(dynamic_cast<juce::Slider*>(child) || dynamic_cast<juce::ComboBox*>(child) || dynamic_cast<juce::Button*>(child))
                {
                    const auto bounds=editor->getLocalArea(child,child->getLocalBounds());
                    auto* hit=editor->getComponentAt(bounds.getCentre());
                    if(!editor->getLocalBounds().contains(bounds) || (hit!=child && !child->isParentOf(hit))) throw std::runtime_error("scaled controls must remain visible and clickable");
                }
        }
        std::cout << "Nian processor checks passed\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
