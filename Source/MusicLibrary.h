#pragma once

#include <juce_audio_formats/juce_audio_formats.h> // reading song length
#include "Pages/Playlist.h"

// local storage for the song library
// uploaded files get copied into the app data folder with iostream
// song info gets saved to library.json next to them so it survives restarts
// playlists go in playlists.json in their saved order with images in Images
//
// windows  C:\Users\<you>\AppData\Roaming\Sound Advice
// mac      ~/Library/Application Support/Sound Advice
class MusicLibrary
{
public:
    MusicLibrary();

    // folders
    juce::File getLibraryFolder() const;   // sound advice folder
    juce::File getSongsFolder() const;     // where copied audio files go
    juce::File getIndexFile() const;       // library.json

    // true once the library has been saved at least once
    bool hasSavedLibrary() const;

    // read and write library.json
    juce::Array<Song> loadSongs() const;
    bool saveSongs (const juce::Array<Song>& songs) const;

    // copy song.audioFile into the songs folder and point the song at the copy
    // also fills in the length if it isnt set
    // false if the file is missing not audio or the copy failed
    bool importSong (Song& song);

    // delete the stored copy only touches files inside the songs folder
    void deleteSongFile (const Song& song) const;

    // playlists
    juce::File getImagesFolder() const;    // copied playlist images
    juce::File getPlaylistsFile() const;   // playlists.json
    bool hasSavedPlaylists() const;
    juce::Array<Playlist> loadPlaylists() const;
    bool savePlaylists (const juce::OwnedArray<Playlist>& playlists) const;

    // copy playlist.imageFile into the images folder and load it
    // false if the image cant be read or copied
    bool importPlaylistImage (Playlist& playlist);
    void deletePlaylistImage (const Playlist& playlist) const;

private:
    juce::File libraryFolder;
    juce::AudioFormatManager formatManager;

    // byte for byte copy using ifstream and ofstream
    static bool copyWithStreams (const juce::File& from, const juce::File& to);

    // song <-> json
    juce::var  songToVar (const Song& song) const;
    Song       varToSong (const juce::var& v) const;

    // playlist <-> json
    juce::var  playlistToVar (const Playlist& p) const;
    Playlist   varToPlaylist (const juce::var& v) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicLibrary)
};
