#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

// Read-only popup showing song metadata, the playlists it belongs to,
// and any transitions saved to it.
class SongDetailDialog : public juce::Component
{
public:
    SongDetailDialog(const Song& song,
                     juce::StringArray inPlaylists,
                     juce::StringArray inTransitions);

    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth = 420;

    // Calculate the exact height needed for this content so the
    // DialogWindow can be sized before showing.
    static int calcHeight(int numInfoLines, int numPlaylists, int numTransitions);

private:
    Song              song;
    juce::StringArray playlistNames;
    juce::StringArray transitionNames;
    juce::TextButton  closeButton { "Close" };

    // Vertical positions computed once in resized() and reused in paint().
    int infoBottom    = 0;
    int playlistsTop  = 0;
    int transTop      = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongDetailDialog)
};
