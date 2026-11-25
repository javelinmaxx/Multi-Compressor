#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <stdexcept>

namespace editorDimmingPreview
{
template <typename Processor>
void render (const juce::File& directory)
{
    if (directory.createDirectory().failed()) throw std::runtime_error ("Could not create dimming preview directory");
    for (const auto* state : { "normal", "bypass", "delta" })
    {
        Processor processor;
        processor.prepareToPlay (48000, 128);
        if (juce::String (state) != "normal")
        {
            const auto* id = juce::String (state) == "bypass" ? "bypass" : "deltaSolo";
            auto* parameter = processor.parameters.getParameter (id);
            if (parameter == nullptr) throw std::runtime_error ("Missing dimming preview parameter");
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setVisible (true);
        const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
        juce::MemoryOutputStream png;
        if (! snapshot.isValid() || ! juce::PNGImageFormat().writeImageToStream (snapshot, png)
            || ! directory.getChildFile (juce::String (state) + ".png")
                    .replaceWithData (png.getData(), png.getDataSize()))
            throw std::runtime_error ("Dimming preview could not be saved");
    }
}

template <typename Processor>
void renderHostBypass (const juce::File& directory)
{
    if (directory.createDirectory().failed()) throw std::runtime_error ("Could not create host-bypass preview directory");
    for (const auto* state : { "normal", "bypass" })
    {
        Processor processor;
        processor.prepareToPlay (48000, 128);
        juce::AudioBuffer<float> audio (2, 128);
        juce::MidiBuffer midi;
        if (juce::String (state) == "bypass") processor.processBlockBypassed (audio, midi);
        else processor.processBlock (audio, midi);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setVisible (true);
        const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
        juce::MemoryOutputStream png;
        if (! snapshot.isValid() || ! juce::PNGImageFormat().writeImageToStream (snapshot, png)
            || ! directory.getChildFile (juce::String (state) + ".png")
                    .replaceWithData (png.getData(), png.getDataSize()))
            throw std::runtime_error ("Host-bypass preview could not be saved");
    }
}
}
