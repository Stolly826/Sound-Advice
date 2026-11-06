#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"
#include "SongsSidebarContent.h"

// Shows all songs in a single playlist.
// Call setPlaylist() before making the page visible.
class PlaylistDetailPage : public juce::Component
{
public:
    // Fired when the user clicks a song row (not the trash can).
    // Args: the playlist and the song index within it.
    std::function<void(Playlist*, int)> onSongClicked;

    // Provide the full song library so the pick-song dialog can list them.
    std::function<juce::Array<Song>()> getAllSongs;

    // FRONTEND/BACKEND INTEGRATION: Set by MainComponent to reach the song library.
    std::function<Song(const juce::File&)> readSongDetails;
    std::function<void(const Song&)>       onNewSong;

    PlaylistDetailPage();
    void setPlaylist(Playlist* playlist);

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Component& getSidebarContent() { return sidebarContent; }

private:
    Playlist* currentPlaylist = nullptr;

    juce::Label               titleLabel;
    juce::Component           listContainer;
    juce::Viewport            listViewport;
    NewPlaylistRow            addSongRow { "  Add Song" };
    juce::OwnedArray<SongRow> rows;

    SongsSidebarContent sidebarContent;

    void rebuildList();
    void handleDeleteRequest(int index);
    void handleAddSong(Song s);
    void showPickSongDialog();
    void showAddSongDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistDetailPage)
};
