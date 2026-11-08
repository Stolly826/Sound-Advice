#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"
#include "SongsSidebarContent.h"

// Shows all songs in a single playlist.
// Call setPlaylist() before making the page visible.
// sidebar shows the whole library drag a song from it into the list
// drag rows in the list to change the order
class PlaylistDetailPage : public juce::Component,
                           public juce::DragAndDropTarget
{
public:
    // Fired when the user clicks a song row (not the trash can).
    // Args: the playlist and the song index within it.
    std::function<void(Playlist*, int)> onSongClicked;

    // Provide the full song library so the pick-song dialog can list them.
    std::function<juce::Array<Song>()> getAllSongs;

    // songs added removed or reordered main component saves
    std::function<void()> onPlaylistChanged;

    // edit button main component opens the edit dialog
    std::function<void(Playlist*)> onEditClicked;

    // add new song puts it in the library first false if that failed
    std::function<bool(Song&)> addSongToLibrary;

    PlaylistDetailPage();
    void setPlaylist(Playlist* playlist);

    // redo title and group after an edit
    void refreshHeader();

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Component& getSidebarContent() { return sidebarContent; }

    // drag and drop
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    // white line showing where a dropped song will go
    struct DropIndicator : public juce::Component
    {
        DropIndicator() { setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 1.5f);
        }
    };

    Playlist* currentPlaylist = nullptr;
    juce::Array<Song> librarySongs; // whats in the sidebar

    juce::Label               titleLabel;
    juce::Label               subtitleLabel; // group and song count
    juce::TextButton          editButton { "Edit" };
    juce::Component           listContainer;
    juce::Viewport            listViewport;
    NewPlaylistRow            addSongRow { "  Add Song" };
    juce::OwnedArray<SongRow> rows;
    DropIndicator             dropIndicator;

    SongsSidebarContent sidebarContent;

    void rebuildList();
    void handleDeleteRequest(int index);
    void handleAddSong(Song s);
    void insertSong(Song s, int index);
    void moveSong(int from, int to);
    void applyDrop(const juce::String& description, int index);
    void markChanged();
    void showPickSongDialog();
    void showAddSongDialog();

    // which slot a drop at this point would land in 0 to number of songs
    int getDropIndex(juce::Point<int> posInPage) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistDetailPage)
};
