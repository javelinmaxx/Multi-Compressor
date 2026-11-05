#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include <functional>
#include <iostream>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = std::unique_ptr<juce::AudioProcessor> (createPluginFilter());
    if (processor == nullptr) return 1;
    processor->prepareToPlay (48000, 256);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor->createEditorAndMakeActive());
    if (editor == nullptr) return 2;
    const auto original = editor->getBounds();
    editor->addToDesktop (0);
    editor->setVisible (true);
    const bool software = juce::SystemStats::getEnvironmentVariable ("J_SONIC_SOFTWARE_UI", "") == "1";
    bool gpuActive = false;
    struct Tick final : juce::Timer
    {
        std::function<void()> step;
        void timerCallback() override { step(); }
    } timer;
    int frames = 0;
    timer.step = [&]
    {
        ++frames;
        if (frames == 10) editor->setSize (original.getWidth() / 2, original.getHeight() / 2);
        if (frames == 20) editor->setSize (juce::roundToInt (original.getWidth() * 1.2),
                                           juce::roundToInt (original.getHeight() * 1.2));
        auto* context = juce::OpenGLContext::getContextAttachedTo (*editor);
        gpuActive = context != nullptr && context->isAttached();
        if (frames == 75) juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    timer.startTimer (20);
    juce::MessageManager::getInstance()->runDispatchLoop();
    timer.stopTimer();
    const auto ok = gpuActive != software;
    editor.reset();
    processor->releaseResources();
    std::cout << processor->getName() << (software ? " software" : " OpenGL")
              << (ok ? " editor smoke passed\n" : " editor smoke failed\n");
    return ok ? 0 : 3;
}
