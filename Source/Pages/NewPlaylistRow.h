#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Top-of-list action row. Defaults to "New Playlist" text; pass a label
// to reuse it for other actions (e.g. "Add Song").
class NewPlaylistRow : public juce::Component
{
public:
    std::function<void()> onClick;

    explicit NewPlaylistRow(const juce::String& label = "New Playlist");
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    static constexpr int kHeight = 68;

private:
    juce::String rowLabel;
    bool         hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewPlaylistRow)
};
