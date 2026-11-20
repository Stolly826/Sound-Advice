#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"
#include "SongsSidebarContent.h"
#include "SongSort.h"

// Shows all songs in a single playlist.
// Call setPlaylist() before making the page visible.
// sidebar shows the whole library drag a song from it into the list
// drag rows in the list to change the order (custom order only)
// the sort dropdown picks how songs are shown and is saved with the playlist
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

    // add new song puts it in the library first
    // the callback gets the stored song and isnt called if it wasnt added eg a skipped duplicate
    std::function<void(Song, std::function<void(const Song&)>)> addSongToLibrary;

    // FRONTEND/BACKEND INTEGRATION: reads tags for the add song dialog (set by MainComponent)
    std::function<Song(const juce::File&)> readSongDetails;

    PlaylistDetailPage();
    void setPlaylist(Playlist* playlist);
    void refreshSongs();

    // redo title group and description after an edit
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
    Playlist* currentPlaylist = nullptr;
    juce::Array<Song> librarySongs; // whats in the sidebar

    juce::Label               titleLabel;
    juce::Label               subtitleLabel; // group and song count
    juce::Label               descriptionLabel;
    juce::Label               sortLabel { {}, "Sort by" };
    juce::ComboBox            sortBox;
    juce::TextButton          editButton { "Edit" };
    juce::TextButton          explicitFilterButton { "Hide explicit" };
    juce::TextButton          explicitWordsButton { "Explicit word list" };
    juce::Label               emptyStateLabel;
    juce::Component           listContainer;
    juce::Viewport            listViewport;
    NewPlaylistRow            addSongRow { "  Add Song" };
    juce::OwnedArray<SongRow> rows;
    SongDropIndicator         dropIndicator;
    juce::Array<int>          visibleSongIndices; // playlist index of each shown row

    SongsSidebarContent sidebarContent;
    bool hideExplicitSongs = false;

    SongSortMode getSortMode() const;
    bool isCustomOrder() const { return getSortMode() == SongSortMode::custom; }
    int  getDescriptionHeight(int width) const; // 0 when there is no description

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
    // row slot from getDropIndex to a position in the whole playlist (explicit filter can hide rows)
    int toPlaylistIndex(int rowSlot) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistDetailPage)
};
