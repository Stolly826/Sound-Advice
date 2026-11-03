#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

// One row in a playlist's song list.
// Layout: [icon | title | artist | album | date | duration | trash]
class SongRow : public juce::Component
{
public:
    std::function<void()> onClicked;
    std::function<void()> onDeleteClicked;

    explicit SongRow(const Song& data);
    void update(const Song& data);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    static constexpr int kHeight = 56;

private:
    class TrashButton : public juce::Button
    {
    public:
        TrashButton() : juce::Button("delete") {}
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    };

    Song        song;
    bool        hovered = false;
    juce::Label titleLabel;
    juce::Label artistLabel;
    juce::Label albumLabel;
    juce::Label dateLabel;
    juce::Label durationLabel;
    TrashButton trashButton;

    static void drawMusicNote(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongRow)
};
