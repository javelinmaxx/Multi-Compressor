#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class QinSynthProcessor;

class QinPresetManager
{
public:
    static constexpr int factoryPresetCount=5;
    struct Entry
    {
        juce::String name;
        int factoryIndex=-1;
        juce::File file;
        bool isFactory() const noexcept { return factoryIndex>=0; }
    };

    explicit QinPresetManager(QinSynthProcessor&,juce::File directory=defaultDirectory());
    static juce::File defaultDirectory();
    std::vector<Entry> entries() const;
    juce::String currentName() const;
    bool currentIsUser() const;
    int currentIndex(const std::vector<Entry>&) const;
    bool load(const Entry&);
    bool saveAs(juce::String name,bool overwrite=false);
    bool deleteCurrentUser();

private:
    QinSynthProcessor& processor;
    juce::File directory;
};
