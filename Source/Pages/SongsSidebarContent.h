#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

// Renders the top N song titles inside the sidebar, filling ~80% of its height.
class SongsSidebarContent : public juce::Component
{
public:
    SongsSidebarContent() = default;

    // Fired with the index of the clicked item (same order as the main list).
    std::function<void(int)> onItemClicked;

    // when on items can be dragged out drop targets get "library:<index>"
    void setDragEnabled(bool shouldDrag) { dragEnabled = shouldDrag; }

    void updateSongs(const juce::Array<Song>& songs);
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    juce::Array<Song> items;
    int hoveredIndex = -1;
    bool dragEnabled = false;

    static constexpr int   kItemHeight = 38;
    static constexpr float kFillRatio  = 0.8f;

    int visibleCount() const;
    int getItemIndexAt(juce::Point<int> pos) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongsSidebarContent)
};
