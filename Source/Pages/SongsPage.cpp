#include "SongsPage.h"
#include "AddSongDialog.h"
#include "SongDetailDialog.h"

SongsPage::SongsPage()
{
    // load saved songs
    // first run has no library yet so fall through to the demo songs below
    if (library.hasSavedLibrary())
        songs = library.loadSongs();
    else
    {
        // Seed a few placeholder songs that match the playlist contents so that
        // "In Playlists" in the detail popup shows real data immediately.
        auto add = [&](const char* title, const char* artist, const char* album,
                       int year, int secs)
        {
            Song s;
            s.title           = title;
            s.artist          = artist;
            s.album           = album;
            s.releaseYear     = year;
            s.dateAdded       = juce::Time::getCurrentTime();
            s.durationSeconds = secs;
            songs.add(s);
        };

        add("Eye of the Tiger", "Survivor",     "Eye of the Tiger", 1982, 244);
        add("Thunderstruck",    "AC/DC",         "The Razors Edge",  1990, 292);
        add("Lo-fi Dream",      "Various",       "Study Beats",      0,    185);
        add("Blinding Lights",  "The Weeknd",    "After Hours",      2020, 200);
        add("Bohemian Rhapsody","Queen",         "A Night at the Opera", 1975, 354);
        saveLibrary();
    }

    titleLabel.setText("Song Library", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(addSongRow);
    addSongRow.onClick = [this] { showAddSongDialog(); };

    sidebarContent.onItemClicked = [this](int index) { showSongDetail(index); };

    rebuildList();
}

void SongsPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SongsPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(8);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;
    const int pad   = 8;

    int totalH = pad + NewPlaylistRow::kHeight + pad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + pad;

    listContainer.setSize(viewW, totalH);

    int y = pad;
    addSongRow.setBounds(4, y, rowW, NewPlaylistRow::kHeight);
    y += NewPlaylistRow::kHeight + pad;

    for (auto* row : rows)
    {
        row->setBounds(4, y, rowW, SongRow::kHeight);
        y += SongRow::kHeight + pad;
    }
}

void SongsPage::rebuildList()
{
    rows.clear();

    for (int i = 0; i < songs.size(); ++i)
    {
        auto* row = rows.add(new SongRow(songs[i]));
        row->onClicked       = [this, i] { showSongDetail(i); };
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        listContainer.addAndMakeVisible(row);
    }

    sidebarContent.updateSongs(songs);
    resized();
}

void SongsPage::handleAddSong(Song s)
{
    addSongToLibrary(s);
}

bool SongsPage::addSongToLibrary(Song& s)
{
    // copy the picked file into the library folder
    // songs added without a file skip this
    if (s.filePath.isNotEmpty() && ! library.importSong(s))
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Couldn't add song")
                .withMessage("The file couldn't be read or copied into the library.")
                .withButton("OK")
                .withAssociatedComponent(this),
            nullptr);
        return false;
    }

    songs.insert(0, s);
    saveLibrary();
    rebuildList();
    return true;
}

void SongsPage::saveLibrary()
{
    if (! library.saveSongs(songs))
        DBG("couldnt save library to " + library.getIndexFile().getFullPathName());
}

void SongsPage::handleDeleteRequest(int index)
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Remove \"" + songs[index].title + "\"?")
            .withMessage("This will remove it from the library.")
            .withButton("Remove")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [this, index](int result)
        {
            if (result == 1)
            {
                if (onSongRemoved) onSongRemoved(songs[index]);
                library.deleteSongFile(songs[index]); // remove the stored copy too
                songs.remove(index);
                saveLibrary();
                rebuildList();
            }
        });
}

void SongsPage::showAddSongDialog()
{
    auto* content = new AddSongDialog();
    content->readSongDetails = readSongDetails;
    content->onCreated = [this](Song s) { handleAddSong(std::move(s)); };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "New Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

juce::Array<Transition> SongsPage::getTransitionsForPair(const juce::String& s1,
                                                          const juce::String& s2) const
{
    juce::Array<Transition> result;
    for (const auto& song : songs)
        if (song.title == s1)
            for (const auto& t : song.transitions)
                if (t.toSongTitle == s2) result.add(t);
    return result;
}

void SongsPage::addTransitionToSong(const juce::String& s1Title, const Transition& t)
{
    for (auto& song : songs)
        if (song.title == s1Title) { song.transitions.add(t); break; }

    saveLibrary();
}

void SongsPage::updateTransitionInSong(const juce::String& s1Title, int idx, const Transition& t)
{
    for (auto& song : songs)
        if (song.title == s1Title && idx >= 0 && idx < song.transitions.size())
            { song.transitions.set(idx, t); break; }

    saveLibrary();
}

void SongsPage::showSongDetail(int index)
{
    if (index < 0 || index >= songs.size()) return;
    const auto& s = songs[index];

    const auto playlists = getPlaylistsForSong ? getPlaylistsForSong(s) : juce::StringArray{};

    auto* content = new SongDetailDialog(s, playlists, {});

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = s.title;
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
