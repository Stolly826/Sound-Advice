#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "PlaylistRow.h"
#include "NewPlaylistRow.h"
#include "PlaylistsSidebarContent.h"

class PlaylistsPage : public juce::Component
{
public:
    // MainComponent sets this to navigate to a playlist's detail view.
    std::function<void(Playlist*)> onPlaylistClicked;

    PlaylistsPage();
    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Component& getSidebarContent() { return sidebarContent; }

    // Returns the names of all playlists that contain a song matching by title+artist.
    juce::StringArray getPlaylistNamesContaining(const Song& song) const;

private:
    juce::OwnedArray<Playlist>    playlists; // OwnedArray keeps pointers stable

    juce::Label                   titleLabel;
    juce::Component               listContainer;
    juce::Viewport                listViewport;
    NewPlaylistRow                newPlaylistRow;
    juce::OwnedArray<PlaylistRow> rows;

    PlaylistsSidebarContent sidebarContent;

    void rebuildList();
    void handleDeleteRequest(int index);
    void handleNewPlaylist(Playlist p);
    void showNewPlaylistDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistsPage)
};
