#include "PresetManager.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <array>

namespace
{
constexpr std::array<const char*,QinPresetManager::factoryPresetCount> factoryNames
    {"Init","Glass Keys","Warm Pad","Wide Pulse","Soft Bass"};
const juce::Identifier nameProperty {"qinPresetName"};
const juce::Identifier kindProperty {"qinPresetKind"};
const juce::Identifier idProperty {"id"};
const juce::Identifier valueProperty {"value"};

void setParameter(juce::ValueTree& state,const char* id,float value)
{
    if(auto child=state.getChildWithProperty(idProperty,id);child.isValid())
        child.setProperty(valueProperty,value,nullptr);
}

void setFactoryValues(juce::ValueTree& state,int index)
{
    const auto set=[&state](const char* id,float value) { setParameter(state,id,value); };
    switch(index)
    {
        case 1: // Glass Keys
            set("osc1WaveX",100); set("osc1WaveY",100);
            set("osc2Level",35); set("osc2WaveY",100); set("osc2Fine",7);
            set("attack",.003f); set("ampDecay",.5f); set("ampSustain",30);
            set("release",1.f); set("filterType",4); set("filterCutoff",9000);
            set("driveMix",0); set("verbMix",20);
            break;
        case 2: // Warm Pad
            set("osc1WaveY",100); set("osc1Unison",3); set("osc1Detune",8);
            set("osc1Spread",65); set("osc2Level",45); set("osc2WaveX",100);
            set("osc2WaveY",100); set("osc2Fine",-8);
            set("attack",1.5f); set("release",2.5f);
            set("filterType",4); set("filterCutoff",2800); set("verbMix",35);
            break;
        case 3: // Wide Pulse
            set("osc1WaveX",100); set("osc1Duty",35); set("osc1Unison",5);
            set("osc1Detune",12); set("osc1Spread",90);
            set("osc2Level",35); set("osc2WaveX",100); set("osc2Duty",65);
            set("osc2Fine",-7); set("verbMix",20);
            break;
        case 4: // Soft Bass
            set("osc1Octave",-1); set("osc1Duty",55);
            set("filterType",1); set("filterCutoff",500); set("filterResonance",35);
            set("ampDecay",.35f); set("ampSustain",65); set("release",.2f);
            set("verbMix",0);
            break;
        default: break;
    }
}
}

QinPresetManager::QinPresetManager(QinSynthProcessor& owner,juce::File location)
    : processor(owner),directory(std::move(location)) {}

juce::File QinPresetManager::defaultDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("J-Sonic").getChildFile("Qin Synth").getChildFile("Presets");
}

std::vector<QinPresetManager::Entry> QinPresetManager::entries() const
{
    std::vector<Entry> result;
    for(size_t index=0;index<factoryNames.size();++index)
        result.push_back({factoryNames[index],int(index),{}});
    juce::Array<juce::File> files;
    directory.findChildFiles(files,juce::File::findFiles,false,"*.qinpreset");
    for(const auto& file:files)
        result.push_back({file.getFileNameWithoutExtension(),-1,file});
    std::sort(result.begin()+factoryNames.size(),result.end(),[](const Entry& a,const Entry& b)
    { return a.name.compareIgnoreCase(b.name)<0; });
    return result;
}

juce::String QinPresetManager::currentName() const
{
    const auto name=processor.parameters.copyState().getProperty(nameProperty).toString();
    return name.isEmpty() ? "Custom" : name;
}

bool QinPresetManager::currentIsUser() const
{
    return processor.parameters.copyState().getProperty(kindProperty).toString()=="user";
}

int QinPresetManager::currentIndex(const std::vector<Entry>& list) const
{
    const auto state=processor.parameters.copyState();
    const auto name=state.getProperty(nameProperty).toString();
    const auto kind=state.getProperty(kindProperty).toString();
    for(size_t index=0;index<list.size();++index)
        if(list[index].isFactory()==(kind=="factory")
           && list[index].name==name) return int(index);
    return -1;
}

bool QinPresetManager::load(const Entry& entry)
{
    if(entry.isFactory())
    {
        if(entry.factoryIndex>=int(factoryNames.size())) return false;
        auto state=processor.parameters.copyState();
        for(auto child:state)
            if(auto* parameter=processor.parameters.getParameter(child.getProperty(idProperty).toString()))
                child.setProperty(valueProperty,parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);
        setFactoryValues(state,entry.factoryIndex);
        state.setProperty(nameProperty,factoryNames[size_t(entry.factoryIndex)],nullptr);
        state.setProperty(kindProperty,"factory",nullptr);
        processor.parameters.replaceState(state);
        return true;
    }
    if(!entry.file.existsAsFile()) return false;
    auto xml=juce::XmlDocument::parse(entry.file);
    if(xml==nullptr || !xml->hasTagName(processor.parameters.state.getType())) return false;
    auto state=juce::ValueTree::fromXml(*xml);
    if(!state.isValid()) return false;
    state.setProperty(nameProperty,entry.name,nullptr);
    state.setProperty(kindProperty,"user",nullptr);
    processor.parameters.replaceState(state);
    return true;
}

bool QinPresetManager::saveAs(juce::String name,bool overwrite)
{
    name=juce::File::createLegalFileName(name.trim());
    if(name.isEmpty() || name=="." || name=="..") return false;
    for(const auto* factory:factoryNames)
        if(name.equalsIgnoreCase(factory)) return false;
    if(directory.createDirectory().failed()) return false;
    const auto file=directory.getChildFile(name+".qinpreset");
    if(file.existsAsFile() && !overwrite) return false;
    auto state=processor.parameters.copyState();
    state.setProperty(nameProperty,name,nullptr);
    state.setProperty(kindProperty,"user",nullptr);
    auto xml=state.createXml();
    if(xml==nullptr || !file.replaceWithText(xml->toString())) return false;
    processor.parameters.replaceState(state);
    return true;
}

bool QinPresetManager::deleteCurrentUser()
{
    if(!currentIsUser()) return false;
    const auto name=currentName();
    const auto file=directory.getChildFile(juce::File::createLegalFileName(name)+".qinpreset");
    if(!file.existsAsFile() || !file.deleteFile()) return false;
    auto state=processor.parameters.copyState();
    state.setProperty(nameProperty,"Custom",nullptr);
    state.setProperty(kindProperty,"custom",nullptr);
    processor.parameters.replaceState(state);
    return true;
}
