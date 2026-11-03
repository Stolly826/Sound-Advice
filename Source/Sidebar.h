#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Persistent left sidebar shown on sub-pages.
// Pages inject their own content component below the back button via setPageContent().
class Sidebar : public juce::Component
{
public:
    std::function<void()> onBackClicked;

    Sidebar();
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Inject (or clear with nullptr) a component into the lower area.
    // The content is NOT owned by the sidebar — the owning page manages its lifetime.
    void setPageContent(juce::Component* content);

private:
    juce::Label      appNameLabel;
    juce::TextButton backButton    { "< Back" };
    juce::Component* pageContent = nullptr;

    static constexpr int kHeaderHeight = 88; // app name + back button

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sidebar)
};
