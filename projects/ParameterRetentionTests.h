#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

// A DAW may close/recreate the editor and serialize a processor when moving
// an insert. Keep the whole host parameter set through both operations.
template <typename Processor> void verifyParameterRetention()
{
    Processor source;
    const auto& parameters=source.getParameters();
    for(size_t index=0;index<size_t(parameters.size());++index)
        if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameters[index]))
        {
            constexpr float positions[] {.23f,.59f,.87f,.04f,.96f};
            for(size_t offset=0;offset<std::size(positions);++offset)
            {
                const auto position=positions[(index+offset)%std::size(positions)];
                const auto snapped=ranged->convertTo0to1(ranged->convertFrom0to1(position));
                if(std::abs(snapped-ranged->getDefaultValue())>1.e-4f)
                { ranged->setValueNotifyingHost(snapped); break; }
            }
        }
    std::vector<float> expected;
    expected.reserve(size_t(parameters.size()));
    for(auto* parameter:parameters) expected.push_back(parameter->getValue());
    const auto check=[&expected](const Processor& processor)
    {
        const auto& current=processor.getParameters();
        if(size_t(current.size())!=expected.size()) throw std::runtime_error("Host parameter count changed during restore");
        for(size_t index=0;index<expected.size();++index)
            if(!std::isfinite(current[index]->getValue())
               || std::abs(current[index]->getValue()-expected[index])>1.e-5f)
                throw std::runtime_error("Host parameter lost after editor recreation or state restore: "
                                         +current[index]->getName(64).toStdString());
    };
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(source.createEditor());
        if(editor==nullptr) throw std::runtime_error("Plugin editor could not be recreated");
    }
    check(source);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    if(state.isEmpty()) throw std::runtime_error("Plugin did not save host parameters");
    Processor moved;
    moved.setStateInformation(state.getData(),int(state.getSize()));
    moved.prepareToPlay(48000,128);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(moved.createEditor());
        if(editor==nullptr) throw std::runtime_error("Restored plugin editor could not open");
    }
    check(moved);
    Processor movedAfterPrepare;
    movedAfterPrepare.prepareToPlay(48000,128);
    movedAfterPrepare.setStateInformation(state.getData(),int(state.getSize()));
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(movedAfterPrepare.createEditor());
        if(editor==nullptr) throw std::runtime_error("Plugin editor could not reopen after live restore");
    }
    check(movedAfterPrepare);
}
