#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <complex>
#include <stdexcept>
struct NianSaturatorTestAccess
{
    static int active(const NianSaturatorProcessor& p)
    {
        int count=0; for(const auto target : p.peakTarget) if(target>.5f) ++count;
        return count;
    }
    static float envelope(const NianSaturatorProcessor& p,int slot) { return p.peakEnvelope[size_t(slot)]; }
    static float frequency(const NianSaturatorProcessor& p,int slot) { return p.peakFrequency[size_t(slot)]; }
    static float target(const NianSaturatorProcessor& p,int slot) { return p.peakTarget[size_t(slot)]; }
};
static void set (NianSaturatorProcessor& p, const char* id, float value)
{ auto* parameter=p.parameters.getParameter(id); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); }
int main(int argc,char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        NianSaturatorProcessor p; set(p,"lowDrive",0); set(p,"highDrive",0); p.prepareToPlay(48000,128);
        if(p.getLatencySamples()!=64) throw std::runtime_error("4x converter must report 64 samples of latency");
        {
            NianSaturatorProcessor impulse;
            set(impulse,"lowDrive",0); set(impulse,"highDrive",0); impulse.prepareToPlay(48000,128);
            juce::AudioBuffer<float> probe(2,128); probe.clear(); probe.setSample(0,0,1);
            juce::MidiBuffer empty; impulse.processBlock(probe,empty);
            int peak=0; for(int i=1;i<128;++i) if(std::abs(probe.getSample(0,i))>std::abs(probe.getSample(0,peak))) peak=i;
            if(peak!=impulse.getLatencySamples()) throw std::runtime_error("converter peak must agree with reported latency");
        }
        juce::MidiBuffer midi; juce::AudioBuffer<float> audio(2,128);
        for(int block=0;block<20;++block)
        {
            for(int i=0;i<128;++i) { const auto x=float(.2*std::sin((block*128+i)*.04)); audio.setSample(0,i,x); audio.setSample(1,i,-x); }
            p.processBlock(audio,midi);
            for(int i=0;i<128;++i)
            {
                const auto x=float(.2*std::sin((block*128+i-64)*.04));
                if(block>2 && (std::abs(audio.getSample(0,i)-x)>2.e-3 || std::abs(audio.getSample(1,i)+x)>2.e-3)) throw std::runtime_error("neutral crossover must reconstruct latency-aligned input");
            }
        }
        set(p,"mode",1); set(p,"lowDrive",100); set(p,"highDrive",100); double difference=0;
        for(int i=0;i<128;++i) { const auto x=float(.5*std::sin(i*.1)); audio.setSample(0,i,x); audio.setSample(1,i,x); }
        p.processBlock(audio,midi);
        for(int i=0;i<128;++i) { const auto x=float(.5*std::sin(i*.1)); difference+=std::abs(audio.getSample(0,i)-x); }
        if(difference<1) throw std::runtime_error("drive must change the signal");
        {
            NianSaturatorProcessor dry, half, wet;
            for(auto* processor : {&dry,&half,&wet})
            {
                set(*processor,"mode",1); set(*processor,"lowDrive",90); set(*processor,"highDrive",90);
            }
            set(dry,"mix",0); set(half,"mix",50); set(wet,"mix",100);
            for(auto* processor : {&dry,&half,&wet}) processor->prepareToPlay(48000,128);
            juce::AudioBuffer<float> a(2,128), b(2,128), c(2,128); juce::MidiBuffer empty;
            for(int block=0;block<60;++block)
            {
                for(int i=0;i<128;++i)
                {
                    const auto n=block*128+i;
                    const auto x=float(.3*std::sin(juce::MathConstants<double>::twoPi*300*n/48000)
                                      +.17*std::sin(juce::MathConstants<double>::twoPi*3200*n/48000));
                    for(auto* target : {&a,&b,&c}) {target->setSample(0,i,x);target->setSample(1,i,-x);}
                }
                dry.processBlock(a,empty); half.processBlock(b,empty); wet.processBlock(c,empty);
                if(block>3) for(int i=0;i<128;++i)
                {
                    const auto n=block*128+i-64;
                    const auto delayed=float(.3*std::sin(juce::MathConstants<double>::twoPi*300*n/48000)
                                             +.17*std::sin(juce::MathConstants<double>::twoPi*3200*n/48000));
                    if(std::abs(a.getSample(0,i)-delayed)>1.e-6f) throw std::runtime_error("zero mix must retain latency-aligned dry audio");
                    if(std::abs(b.getSample(0,i)-.5f*(a.getSample(0,i)+c.getSample(0,i)))>1.e-5f)
                        throw std::runtime_error("half mix must sit midway between dry and wet");
                }
            }
            std::array<NianSaturatorProcessor::DisplaySample,1024> captured {};
            if(wet.readDisplay(captured.data(),int(captured.size()))!=int(captured.size()))
                throw std::runtime_error("spectrum must receive full-rate audio samples");
        }
        {
            NianSaturatorProcessor alias;
            set(alias,"mode",1); set(alias,"highDrive",100); set(alias,"lowDrive",0); alias.prepareToPlay(48000,128);
            std::complex<double> fundamental {}, folded {};
            juce::AudioBuffer<float> tone(2,128); juce::MidiBuffer empty;
            for(int block=0;block<250;++block)
            {
                for(int i=0;i<128;++i) { const auto n=block*128+i; const auto x=float(.65*std::sin(juce::MathConstants<double>::twoPi*10000*n/48000)); tone.setSample(0,i,x); tone.setSample(1,i,x); }
                alias.processBlock(tone,empty);
                if(block>50) for(int i=0;i<128;++i)
                {
                    const auto n=block*128+i;
                    fundamental+=double(tone.getSample(0,i))*std::polar(1.,-juce::MathConstants<double>::twoPi*10000*n/48000);
                    folded+=double(tone.getSample(0,i))*std::polar(1.,-juce::MathConstants<double>::twoPi*18000*n/48000);
                }
            }
            if(std::abs(folded)>std::abs(fundamental)*.03) throw std::runtime_error("4x drive must suppress folded third harmonic");
        }
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
        {
            if(NianSaturatorProcessor::fBandCount!=12) throw std::runtime_error("F mode needs twelve peak bands");
            constexpr std::array<double,12> frequencies {53,88,150,260,445,760,1280,2090,3400,5550,9050,14750};
            NianSaturatorProcessor many;
            set(many,"crossover",1000); set(many,"lowDrive",80); set(many,"highDrive",80);
            many.prepareToPlay(48000,128);
            juce::AudioBuffer<float> tones(2,128); juce::MidiBuffer empty;
            for(int block=0;block<320;++block)
            {
                for(int i=0;i<128;++i)
                {
                    const auto n=block*128+i;
                    double value=0;
                    for(const auto hz : frequencies) value+=.012*std::sin(juce::MathConstants<double>::twoPi*hz*n/48000);
                    tones.setSample(0,i,float(value)); tones.setSample(1,i,float(value));
                }
                many.processBlock(tones,empty);
            }
            if(NianSaturatorTestAccess::active(many)!=12)
            {
                for(int slot=0;slot<12;++slot)
                    std::cerr << "F slot " << slot << " target=" << NianSaturatorTestAccess::target(many,slot)
                              << " frequency=" << NianSaturatorTestAccess::frequency(many,slot) << '\n';
                throw std::runtime_error("F detector must track twelve simultaneous peaks across both sides of the crossover");
            }
            for(size_t slot=0;slot<frequencies.size();++slot)
                if(std::abs(NianSaturatorTestAccess::frequency(many,int(slot))-frequencies[slot])
                   >std::max(15.,frequencies[slot]*.04))
                    throw std::runtime_error("F bands must track distinct peaks in their own frequency cells");
        }
        {
            NianSaturatorProcessor bass;
            set(bass,"crossover",1000); set(bass,"lowDrive",100); set(bass,"highDrive",0);
            bass.prepareToPlay(48000,128);
            juce::AudioBuffer<float> pulse(2,128); juce::MidiBuffer empty; pulse.clear();
            for(int block=0;block<50;++block) bass.processBlock(pulse,empty);
            int detected=-1,risen=-1; float previous=0,maxJump=0;
            for(int block=0;block<35;++block)
            {
                for(int i=0;i<128;++i)
                {
                    const auto x=float(.12*std::sin(juce::MathConstants<double>::twoPi*88*(block*128+i)/48000));
                    pulse.setSample(0,i,x); pulse.setSample(1,i,x);
                }
                bass.processBlock(pulse,empty);
                for(int i=0;i<128;++i)
                {
                    const auto value=pulse.getSample(0,i);
                    maxJump=std::max(maxJump,std::abs(value-previous)); previous=value;
                }
                if(detected<0 && NianSaturatorTestAccess::envelope(bass,1)>0) detected=block*128;
                if(risen<0 && NianSaturatorTestAccess::envelope(bass,1)>.63f) risen=block*128;
            }
            if(detected<0 || risen<0 || risen-detected>512 || maxJump>.08f)
                throw std::runtime_error("low F band must acquire quickly without an abrupt waveform jump");
        }
        {
            NianSaturatorProcessor tube;
            set(tube,"mode",1); set(tube,"crossover",1000); set(tube,"lowDrive",0); set(tube,"highDrive",100);
            tube.prepareToPlay(48000,128);
            juce::AudioBuffer<float> tone(2,128); juce::MidiBuffer empty;
            double inputPower=0,outputPower=0,mean=0; int count=0;
            std::array<std::complex<double>,3> harmonics {};
            for(int block=0;block<240;++block)
            {
                for(int i=0;i<128;++i)
                {
                    const auto x=float(.25*std::sin(juce::MathConstants<double>::twoPi*3000*(block*128+i)/48000));
                    tone.setSample(0,i,x); tone.setSample(1,i,x);
                }
                tube.processBlock(tone,empty);
                if(block>60) for(int i=0;i<128;++i)
                {
                    const auto n=block*128+i;
                    const auto x=.25*std::sin(juce::MathConstants<double>::twoPi*3000*n/48000);
                    const auto y=double(tone.getSample(0,i));
                    inputPower+=x*x; outputPower+=y*y; mean+=y; ++count;
                    for(int order=1;order<=3;++order)
                        harmonics[size_t(order-1)]+=y*std::polar(1.,-juce::MathConstants<double>::twoPi*3000*order*n/48000);
                }
            }
            const auto rmsRatio=std::sqrt(outputPower/inputPower);
            if(rmsRatio<1.05 || rmsRatio>1.6 || std::abs(mean/count)>.01
               || std::abs(harmonics[1])<std::abs(harmonics[0])*.0002
               || std::abs(harmonics[2])>std::abs(harmonics[0])*.1)
                throw std::runtime_error("T mode must give subtle drive without a DC offset");
        }
        set(p,"bypass",1);
        for(int block=0;block<12;++block)
        {
            for(int i=0;i<128;++i) { const auto x=float(.2*std::sin((block*128+i)*.04)); audio.setSample(0,i,x); audio.setSample(1,i,-x); }
            p.processBlock(audio,midi);
            if(block==11) for(int i=0;i<128;++i) if(std::abs(audio.getSample(0,i)-float(.2*std::sin((block*128+i-64)*.04)))>1.e-5) throw std::runtime_error("bypass must settle to the delayed original signal");
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
        set(p,"mode",1); set(p,"crossover",1000); set(p,"lowDrive",85); set(p,"highDrive",85);
        for(int block=0;block<40;++block)
        {
            for(int i=0;i<128;++i)
            {
                const auto n=block*128+i;
                const auto x=float(.20*std::sin(juce::MathConstants<double>::twoPi*250*n/48000)
                                  +.12*std::sin(juce::MathConstants<double>::twoPi*2300*n/48000));
                audio.setSample(0,i,x); audio.setSample(1,i,x);
            }
            p.processBlock(audio,midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor()); editor->addToDesktop(0); editor->setVisible(true);
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        if(!image.isValid()) throw std::runtime_error("editor must render");
        if(argc>1) { juce::File file(argv[1]); file.getParentDirectory().createDirectory(); auto out=file.createOutputStream(); if(!out || !out->setPosition(0) || out->truncate().failed() || !juce::PNGImageFormat().writeImageToStream(image,*out)) throw std::runtime_error("preview write failed"); }
        juce::ComboBox* selector=nullptr;
        for(auto* child : editor->getChildren()) if(auto* box=dynamic_cast<juce::ComboBox*>(child)) selector=box;
        if(!selector) throw std::runtime_error("mode selector missing");
        selector->setSelectedId(2,juce::sendNotificationSync);
        if(p.parameters.getRawParameterValue("mode")->load()<.5f) throw std::runtime_error("mode selector must update the host parameter");
        juce::Slider* mixControl=nullptr;
        for(auto* child : editor->getChildren()) if(auto* slider=dynamic_cast<juce::Slider*>(child))
            if(slider->getName()=="MIX") mixControl=slider;
        if(!mixControl) throw std::runtime_error("mix knob missing");
        mixControl->setValue(37,juce::sendNotificationSync);
        if(std::abs(p.parameters.getRawParameterValue("mix")->load()-37.f)>.11f)
            throw std::runtime_error("mix knob must update the host parameter");
        for (float scale : {.5f,1.2f})
        {
            editor->setSize(juce::roundToInt(444*scale),juce::roundToInt(432*scale));
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
