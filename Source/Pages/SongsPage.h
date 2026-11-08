#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"
#include "SongsSidebarContent.h"
#include "../MusicLibrary.h"

// Global song library. Every song the user has ever added lives here.
// Clicking a row opens a detail popup (name, artist, album, year,
// which playlists it's in, and saved transitions).
class SongsPage : public juce::Component
{
public:
    // Set by MainComponent so the detail dialog can query playlist membership.
    std::function<juce::StringArray(const Song&)> getPlaylistsForSong;

    SongsPage();
    void paint(juce::Graphics& g) override;
    void resized() override;

    const juce::Array<Song>& getSongs() const { return songs; }

    // copy the file in add to the library and save
    // song is updated to point at the stored copy false if it failed
    bool addSongToLibrary(Song& s);

    juce::Array<Transition> getTransitionsForPair(const juce::String& s1,
                                                   const juce::String& s2) const;
    void addTransitionToSong    (const juce::String& s1Title, const Transition& t);
    void updateTransitionInSong (const juce::String& s1Title, int idx, const Transition& t);

    juce::Component& getSidebarContent() { return sidebarContent; }

private:
    MusicLibrary                  library; // saves songs to disk
    juce::Array<Song>             songs;

    juce::Label                   titleLabel;
    juce::Component               listContainer;
    juce::Viewport                listViewport;
    NewPlaylistRow                addSongRow { "  New Song" };
    juce::OwnedArray<SongRow>     rows;

    SongsSidebarContent sidebarContent;

    void rebuildList();
    void saveLibrary();
    void handleAddSong(Song s);
    void handleDeleteRequest(int index);
    void showAddSongDialog();
    void showSongDetail(int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongsPage)
};
