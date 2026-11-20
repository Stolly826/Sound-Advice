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

    std::function<void(const Song&, std::function<void(juce::Result, juce::String)>)>
        onTranscribeRequested;
    std::function<void(const Song&)> onSongUpdated;

    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth = 420;

    // Calculate the exact height needed for this content so the
    // DialogWindow can be sized before showing.
    // headerHeight is the title area, taller when the song has a picture
    static int calcHeight(int headerHeight, int numInfoLines, int numPlaylists, int numTransitions);

private:
    Song              song;
    juce::StringArray playlistNames;
    juce::StringArray transitionNames;
    juce::TextButton  closeButton { "Close" };
    juce::TextButton  transcribeButton { "Transcribe audio" };
    juce::Label       explicitWordsLabel;
    juce::TextEditor  transcriptEditor;

    static constexpr int kImageSize = 96;

    int getHeaderHeight() const;  // title row or picture with the title beside it
    int getNumInfoLines() const;  // artist album genre year duration that are filled in
    void updateTranscriptDetails();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongDetailDialog)
};
