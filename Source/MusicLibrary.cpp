#include "MusicLibrary.h"

#include <filesystem>
#include <fstream>

namespace
{
    // juce file -> std path
    // windows needs the wide version so names with accents etc still work
    std::filesystem::path toStdPath (const juce::File& f)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path (f.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path (f.getFullPathName().toStdString());
       #endif
    }
}

MusicLibrary::MusicLibrary()
{
    // os app data folder
    // windows AppData\Roaming  mac ~/Library
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    appData = appData.getChildFile ("Application Support");
   #endif

    libraryFolder = appData.getChildFile ("Sound Advice");

    // make the folders if this is the first run
    getSongsFolder().createDirectory();

    // needed to open audio files and read their length
    formatManager.registerBasicFormats();
}

juce::File MusicLibrary::getLibraryFolder() const { return libraryFolder; }
juce::File MusicLibrary::getSongsFolder() const   { return libraryFolder.getChildFile ("Songs"); }
juce::File MusicLibrary::getIndexFile() const     { return libraryFolder.getChildFile ("library.json"); }

bool MusicLibrary::hasSavedLibrary() const { return getIndexFile().existsAsFile(); }

juce::Array<Song> MusicLibrary::loadSongs() const
{
    juce::Array<Song> songs;

    // missing or broken file just means an empty library
    const auto parsed = juce::JSON::parse (getIndexFile());
    if (auto* list = parsed.getArray())
        for (const auto& item : *list)
            songs.add (varToSong (item));

    return songs;
}

bool MusicLibrary::saveSongs (const juce::Array<Song>& songs) const
{
    juce::Array<juce::var> list;
    for (const auto& s : songs)
        list.add (songToVar (s));

    // replaceWithText writes a temp file then swaps it in
    // so a crash mid save cant wipe the library
    libraryFolder.createDirectory();
    return getIndexFile().replaceWithText (juce::JSON::toString (juce::var (list)));
}

bool MusicLibrary::importSong (Song& song)
{
    const juce::File source = song.audioFile;
    if (! source.existsAsFile())
        return false;

    // make sure its actually audio before copying anything
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (source));
    if (reader == nullptr)
        return false;

    if (song.durationSeconds <= 0 && reader->sampleRate > 0)
        song.durationSeconds = juce::roundToInt ((double) reader->lengthInSamples / reader->sampleRate);

    reader.reset(); // close the original before copying

    // same name as the original
    // getNonexistentSibling adds (2) (3) etc if that name is taken
    getSongsFolder().createDirectory();
    const juce::File dest = getSongsFolder().getChildFile (source.getFileName()).getNonexistentSibling (true);

    if (! copyWithStreams (source, dest))
    {
        dest.deleteFile(); // dont leave half a file behind
        return false;
    }

    song.audioFile = dest;
    return true;
}

void MusicLibrary::deleteSongFile (const Song& song) const
{
    // never delete the users own files only our copies
    if (song.audioFile.isAChildOf (getSongsFolder()))
        song.audioFile.deleteFile();
}

juce::File MusicLibrary::getImagesFolder() const  { return libraryFolder.getChildFile ("Images"); }
juce::File MusicLibrary::getPlaylistsFile() const { return libraryFolder.getChildFile ("playlists.json"); }

bool MusicLibrary::hasSavedPlaylists() const { return getPlaylistsFile().existsAsFile(); }

juce::Array<Playlist> MusicLibrary::loadPlaylists() const
{
    juce::Array<Playlist> playlists;

    // order in the file is the order on screen
    const auto parsed = juce::JSON::parse (getPlaylistsFile());
    if (auto* list = parsed.getArray())
        for (const auto& item : *list)
            playlists.add (varToPlaylist (item));

    return playlists;
}

bool MusicLibrary::savePlaylists (const juce::OwnedArray<Playlist>& playlists) const
{
    juce::Array<juce::var> list;
    for (auto* p : playlists)
        list.add (playlistToVar (*p));

    // same safe swap as the song library
    libraryFolder.createDirectory();
    return getPlaylistsFile().replaceWithText (juce::JSON::toString (juce::var (list)));
}

bool MusicLibrary::importPlaylistImage (Playlist& playlist)
{
    const juce::File source = playlist.imageFile;

    // already ours nothing to copy
    if (source.isAChildOf (getImagesFolder()))
        return true;

    const auto image = juce::ImageFileFormat::loadFrom (source);
    if (image.isNull())
        return false;

    getImagesFolder().createDirectory();
    const juce::File dest = getImagesFolder().getChildFile (source.getFileName()).getNonexistentSibling (true);

    if (! copyWithStreams (source, dest))
    {
        dest.deleteFile();
        return false;
    }

    playlist.imageFile = dest;
    playlist.image     = image;
    return true;
}

void MusicLibrary::deletePlaylistImage (const Playlist& playlist) const
{
    // only our copies never the users original
    if (playlist.imageFile.isAChildOf (getImagesFolder()))
        playlist.imageFile.deleteFile();
}

bool MusicLibrary::copyWithStreams (const juce::File& from, const juce::File& to)
{
    // binary so windows doesnt mess with line ending bytes
    std::ifstream in (toStdPath (from), std::ios::binary);
    std::ofstream out (toStdPath (to), std::ios::binary | std::ios::trunc);

    if (! in.is_open() || ! out.is_open())
        return false;

    // copy 64 kb at a time so big files dont need to fit in memory
    std::vector<char> buffer (64 * 1024);

    // read fails on the last partial chunk but gcount still has the bytes
    while (in.read (buffer.data(), (std::streamsize) buffer.size()) || in.gcount() > 0)
    {
        out.write (buffer.data(), in.gcount());
        if (! out)
            return false; // disk full etc
    }

    out.close();
    return ! in.bad() && ! out.fail();
}

juce::var MusicLibrary::songToVar (const Song& s) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("title",           s.title);
    obj->setProperty ("artist",          s.artist);
    obj->setProperty ("album",           s.album);
    obj->setProperty ("releaseYear",     s.releaseYear);
    obj->setProperty ("dateAdded",       s.dateAdded.toMilliseconds());
    obj->setProperty ("durationSeconds", s.durationSeconds);

    // files in our songs folder are saved by name only
    // so the library still works if the folder moves eg windows -> mac
    if (s.audioFile.isAChildOf (getSongsFolder()))
        obj->setProperty ("file", s.audioFile.getRelativePathFrom (getSongsFolder()));
    else if (s.audioFile != juce::File())
        obj->setProperty ("file", s.audioFile.getFullPathName());

    juce::Array<juce::var> transitions;
    for (const auto& t : s.transitions)
    {
        auto* tObj = new juce::DynamicObject();
        tObj->setProperty ("name",         t.name);
        tObj->setProperty ("toSongTitle",  t.toSongTitle);
        tObj->setProperty ("loopPoint",    t.loopPoint);
        tObj->setProperty ("fadeOutStart", t.fadeOutStart);
        tObj->setProperty ("fadeOutEnd",   t.fadeOutEnd);
        tObj->setProperty ("startPoint",   t.startPoint);
        tObj->setProperty ("fadeInStart",  t.fadeInStart);
        tObj->setProperty ("fadeInEnd",    t.fadeInEnd);
        tObj->setProperty ("savedAt",      t.savedAt.toMilliseconds());
        transitions.add (juce::var (tObj));
    }
    obj->setProperty ("transitions", transitions);

    return juce::var (obj);
}

juce::var MusicLibrary::playlistToVar (const Playlist& p) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name",        p.name);
    obj->setProperty ("description", p.description);
    obj->setProperty ("group",       p.group);
    obj->setProperty ("lastUpdated", p.lastUpdated.toMilliseconds());

    if (p.imageFile.isAChildOf (getImagesFolder()))
        obj->setProperty ("image", p.imageFile.getRelativePathFrom (getImagesFolder()));

    // songs in playlist order this is what saves the ordering
    juce::Array<juce::var> songs;
    for (const auto& s : p.songs)
        songs.add (songToVar (s));
    obj->setProperty ("songs", songs);

    return juce::var (obj);
}

Playlist MusicLibrary::varToPlaylist (const juce::var& v) const
{
    Playlist p;
    p.name        = v["name"].toString();
    p.description = v["description"].toString();
    p.group       = v["group"].toString();
    p.lastUpdated = juce::Time ((juce::int64) v["lastUpdated"]);

    const auto image = v["image"].toString();
    if (image.isNotEmpty())
    {
        p.imageFile = getImagesFolder().getChildFile (image);
        p.image     = juce::ImageFileFormat::loadFrom (p.imageFile); // null if the file is gone
    }

    if (auto* songs = v["songs"].getArray())
        for (const auto& sv : *songs)
            p.songs.add (varToSong (sv));

    return p;
}

Song MusicLibrary::varToSong (const juce::var& v) const
{
    Song s;
    s.title           = v["title"].toString();
    s.artist          = v["artist"].toString();
    s.album           = v["album"].toString();
    s.releaseYear     = (int) v["releaseYear"];
    s.dateAdded       = juce::Time ((juce::int64) v["dateAdded"]);
    s.durationSeconds = (int) v["durationSeconds"];

    // name only -> inside songs folder  full path -> use as is
    const auto file = v["file"].toString();
    if (file.isNotEmpty())
        s.audioFile = juce::File::isAbsolutePath (file) ? juce::File (file)
                                                        : getSongsFolder().getChildFile (file);

    if (auto* transitions = v["transitions"].getArray())
    {
        for (const auto& tv : *transitions)
        {
            Transition t;
            t.name         = tv["name"].toString();
            t.toSongTitle  = tv["toSongTitle"].toString();
            t.loopPoint    = (float) tv["loopPoint"];
            t.fadeOutStart = (float) tv["fadeOutStart"];
            t.fadeOutEnd   = (float) tv["fadeOutEnd"];
            t.startPoint   = (float) tv["startPoint"];
            t.fadeInStart  = (float) tv["fadeInStart"];
            t.fadeInEnd    = (float) tv["fadeInEnd"];
            t.savedAt      = juce::Time ((juce::int64) tv["savedAt"]);
            s.transitions.add (t);
        }
    }

    return s;
}
