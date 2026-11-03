#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "SongRow.h"
#include "NewPlaylistRow.h"

// Shown when the user taps "Add Song" on a playlist.
// Lists existing library songs using the same SongRow style as SongsPage.
// "Add New Song" at the top opens the full AddSongDialog flow.
class PickSongDialog : public juce::Component,
                       private juce::TextEditor::Listener
{
public:
    std::function<void()>     onAddNewSong;
    std::function<void(Song)> onSongPicked;

    explicit PickSongDialog(juce::Array<Song> librarySongs);

    void paint  (juce::Graphics& g) override;
    void resized()                  override;

    static constexpr int kWidth  = 520;
    static constexpr int kHeight = 520;

private:
    juce::Array<Song>         allSongs;
    juce::Array<Song>         filtered;

    NewPlaylistRow            addNewRow { "  Add New Song" };
    juce::TextEditor          searchEditor;
    juce::Component           listContainer;
    juce::Viewport            listViewport;
    juce::OwnedArray<SongRow> rows;

    void textEditorTextChanged(juce::TextEditor&) override;

    void applyFilter();
    void rebuildRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PickSongDialog)
};
