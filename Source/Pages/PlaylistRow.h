#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"

// One playlist entry: [icon | name] [date] [trash]
class PlaylistRow : public juce::Component
{
public:
    std::function<void()> onClicked;       // fires when row is clicked (not on trash)
    std::function<void()> onDeleteClicked;

    explicit PlaylistRow(const Playlist& data);
    void update(const Playlist& data);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    static constexpr int kHeight = 68;

private:
    // Button that draws a trash-can icon instead of text.
    class TrashButton : public juce::Button
    {
    public:
        TrashButton() : juce::Button("delete") {}
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    };

    Playlist    playlist;
    bool        hovered = false;
    juce::Label nameLabel;
    juce::Label dateLabel;
    TrashButton trashButton;

    static void drawMusicNote(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistRow)
};
