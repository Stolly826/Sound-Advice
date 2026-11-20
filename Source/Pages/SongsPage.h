#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"
#include "SongsSidebarContent.h"
#include "SongSort.h"
#include "../MusicLibrary.h"

// Global song library. Every song the user has ever added lives here.
// Clicking a row opens a detail popup (name, artist, album, year,
// which playlists it's in, and saved transitions).
// sort dropdown changes how the list is shown, in custom order rows can be dragged to reorder
class SongsPage : public juce::Component,
                  public juce::DragAndDropTarget
{
public:
    // Set by MainComponent so the detail dialog can query playlist membership.
    std::function<juce::StringArray(const Song&)> getPlaylistsForSong;
    std::function<void(const Song&)> onSongTranscriptChanged;
    std::function<void(const Song&, std::function<void(juce::Result, juce::String)>)>
        onTranscribeSong;

    // FRONTEND/BACKEND INTEGRATION: Set by MainComponent.
    std::function<Song(const juce::File&)> readSongDetails; // tags for the add song dialog
    std::function<void(const Song&)>       onSongRemoved;   // so its settings can go too

    // a duplicate upload replaced oldSong, playlists should point at newSong instead
    std::function<void(const Song& oldSong, const Song& newSong)> onSongReplaced;

    SongsPage();
    void paint(juce::Graphics& g) override;
    void resized() override;

    const juce::Array<Song>& getSongs() const { return songs; }

    // copy the file and picture in add to the library and save
    // if the same audio file is already in the library the user is asked whether to replace the old song
    // onAdded gets the stored song once its in, it isnt called if nothing was added
    void addSongToLibrary(Song s, std::function<void(const Song&)> onAdded = nullptr);

    juce::Array<Transition> getTransitionsForPair(const juce::String& s1,
                                                   const juce::String& s2) const;
    void addTransitionToSong    (const juce::String& s1Title, const Transition& t);
    void updateTransitionInSong (const juce::String& s1Title, int idx, const Transition& t);
    void updateSongTranscript(const Song& song);

    juce::Component& getSidebarContent() { return sidebarContent; }

    MusicLibrary& getLibrary() { return library; }

    // drag rows to reorder, only in custom order
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    MusicLibrary                  library; // saves songs to disk
    juce::Array<Song>             songs;

    juce::Label                   titleLabel;
    juce::Label                   sortLabel { {}, "Sort by" };
    juce::ComboBox                sortBox;
    juce::Component               listContainer;
    juce::Viewport                listViewport;
    NewPlaylistRow                addSongRow { "  New Song" };
    juce::OwnedArray<SongRow>     rows;
    SongDropIndicator             dropIndicator;

    SongSortMode                  sortMode = SongSortMode::custom;
    juce::Array<int>              displayOrder; // songs index of each row on screen

    SongsSidebarContent sidebarContent;

    void rebuildList();
    void saveLibrary();
    void handleAddSong(Song s);
    bool importIntoLibrary(Song& s);  // copies the file and picture, false if the file failed
    void replaceSong(const Song& oldSong, Song newSong, std::function<void(const Song&)> onAdded);
    void removeSongAt(int index);     // also deletes its stored file picture and settings
    void moveSong(int from, int to);
    int  getDropIndex(juce::Point<int> posInPage) const;
    void handleDeleteRequest(int index);
    void showAddSongDialog();
    void showSongDetail(int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongsPage)
};
