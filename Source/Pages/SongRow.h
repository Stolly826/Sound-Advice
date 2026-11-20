#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

// One row in a playlist's song list.
// Layout: [icon | title | artist | album | date | duration | trash]
// stacked layout for narrow lists like the add song picker:
// [icon | title over album | artist | date | duration]
class SongRow : public juce::Component
{
public:
    std::function<void()> onClicked;
    std::function<void()> onDeleteClicked;

    // set to make the row draggable eg playlist reordering
    // this is what the drop target receives
    juce::var dragDescription;

    explicit SongRow(const Song& data);
    void update(const Song& data);

    // album goes under the title in smaller text, bigger title and artist, no trash can
    void setStackedLayout(bool shouldStack);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
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
    bool        stacked = false;
    juce::Label titleLabel;
    juce::Label explicitLabel;
    juce::Label artistLabel;
    juce::Label albumLabel;
    juce::Label dateLabel;
    juce::Label durationLabel;
    TrashButton trashButton;

    static void drawMusicNote(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongRow)
};

// white line showing where a dragged song will land in a list of SongRows
class SongDropIndicator : public juce::Component
{
public:
    SongDropIndicator() { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 1.5f);
    }
};
