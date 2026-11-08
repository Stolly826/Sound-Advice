#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "PlaylistRow.h"
#include "NewPlaylistRow.h"
#include "PlaylistsSidebarContent.h"
#include "../MusicLibrary.h"

// playlists listed under a header per group
// everything is saved to playlists.json whenever it changes
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

    // every group name in use sorted
    juce::StringArray getGroupNames() const;

    // call after a playlist changes somewhere else eg song order on its page
    // saves and redraws the list
    void playlistChanged();

    // edit dialog for name group description image
    // centreAround is the page the dialog shows over onDone runs after saving
    void showEditDialog(Playlist* playlist, juce::Component* centreAround, std::function<void()> onDone);

private:
    MusicLibrary                  library; // saves playlists to disk
    juce::OwnedArray<Playlist>    playlists; // OwnedArray keeps pointers stable

    juce::Label                   titleLabel;
    juce::Component               listContainer;
    juce::Viewport                listViewport;
    NewPlaylistRow                newPlaylistRow;
    juce::OwnedArray<PlaylistRow> rows;
    juce::OwnedArray<juce::Label> groupHeaders;

    // rows and headers in screen order with their heights
    std::vector<std::pair<juce::Component*, int>> layoutItems;

    PlaylistsSidebarContent sidebarContent;

    void rebuildList();
    void savePlaylists();
    void seedDemoPlaylists();
    void handleDeleteRequest(Playlist* playlist);
    void handleNewPlaylist(Playlist p);
    void showNewPlaylistDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistsPage)
};
