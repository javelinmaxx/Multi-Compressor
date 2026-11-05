#pragma once

#include <juce_opengl/juce_opengl.h>
#include <atomic>

namespace pluginGpu
{
// Owns the optional JUCE OpenGL renderer for one editor. A failed context is
// detached on the message thread so the normal Component paint path resumes.
class Renderer final : private juce::Timer, private juce::OpenGLRenderer
{
public:
    ~Renderer() override { detach(); }

    void attach (juce::Component& target)
    {
        component = &target;
        if (juce::SystemStats::getEnvironmentVariable ("J_SONIC_SOFTWARE_UI", "") == "1")
        {
            softwareFallback = true;
            return;
        }
        context.setComponentPaintingEnabled (true);
        context.setContinuousRepainting (false);
        context.setRenderer (this);
        context.attachTo (target);
        startTimerHz (10);
    }

    void detach()
    {
        stopTimer();
        context.detach();
        component = nullptr;
    }

    bool isActive() const noexcept
    {
        return ! softwareFallback && context.isAttached()
            && frameSeen.load (std::memory_order_acquire);
    }

private:
    void newOpenGLContextCreated() override { frameSeen.store (false, std::memory_order_relaxed); }
    void renderOpenGL() override { frameSeen.store (true, std::memory_order_release); }
    void openGLContextClosing() override { frameSeen.store (false, std::memory_order_relaxed); }

    void timerCallback() override
    {
        if (component == nullptr || ! component->isShowing() || component->getPeer() == nullptr)
        {
            waitStart = 0;
            return;
        }
        const auto now = juce::Time::getMillisecondCounterHiRes() * .001;
        if (isActive()) waitStart = now;
        else if (waitStart <= 0) waitStart = now;
        else if (now - waitStart > 1.0)
        {
            context.detach();
            softwareFallback = true;
            stopTimer();
            component->repaint();
        }
    }

    juce::Component* component = nullptr;
    juce::OpenGLContext context;
    std::atomic<bool> frameSeen { false };
    double waitStart = 0;
    bool softwareFallback = false;
};
}
