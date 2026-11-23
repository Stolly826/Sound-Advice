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

    // last sort choice comes back after a restart
    sortMode = songSortModeFromId(library.loadLibrarySortMode());
    fillSongSortBox(sortBox);
    selectSongSortMode(sortBox, sortMode);
    sortBox.onChange = [this]
    {
        sortMode = getSelectedSongSortMode(sortBox);
        library.saveLibrarySortMode(songSortModeToId(sortMode));
        rebuildList();
    };
    sortLabel.setJustificationType(juce::Justification::centredRight);
    sortLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(sortLabel);
    addAndMakeVisible(sortBox);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(addSongRow);
    addSongRow.onClick = [this] { showAddSongDialog(); };

    listContainer.addChildComponent(dropIndicator);

    // sidebar is in the same order as the list
    sidebarContent.onItemClicked = [this](int index)
    {
        if (juce::isPositiveAndBelow(index, displayOrder.size()))
            showSongDetail(displayOrder[index]);
    };

    rebuildList();
}

void SongsPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SongsPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(40);
    sortBox.setBounds(header.removeFromRight(150).withSizeKeepingCentre(150, 28));
    header.removeFromRight(6);
    sortLabel.setBounds(header.removeFromRight(60));
    titleLabel.setBounds(header);
    area.removeFromTop(8);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;
    const int pad   = 8;

    int totalH = pad + NewPlaylistRow::kHeight + pad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + pad;

    // fill the viewport so drops below the last song still land
    listContainer.setSize(viewW, juce::jmax(totalH, listViewport.getHeight()));

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
    dropIndicator.setVisible(false);

    displayOrder = getSortedSongOrder(songs, sortMode);

    juce::Array<Song> shown; // sidebar matches the list
    for (const int i : displayOrder)
    {
        shown.add(songs[i]);
        auto* row = rows.add(new SongRow(songs[i]));
        row->onClicked       = [this, i] { showSongDetail(i); };
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        if (sortMode == SongSortMode::custom)
            row->dragDescription = "row:" + juce::String(i); // drag to reorder
        listContainer.addAndMakeVisible(row);
    }

    dropIndicator.toFront(false);
    sidebarContent.updateSongs(shown);
    resized();
}

void SongsPage::handleAddSong(Song s)
{
    addSongToLibrary(std::move(s));
}

void SongsPage::addSongToLibrary(Song s, std::function<void(const Song&)> onAdded)
{
    // same audio file already in the library, ask before adding it twice
    const int duplicate = s.filePath.isNotEmpty()
                        ? library.findSongWithSameFile(juce::File(s.filePath), songs)
                        : -1;

    if (duplicate < 0)
    {
        if (! importIntoLibrary(s)) return;

        songs.insert(0, s);
        saveLibrary();
        rebuildList();
        if (onAdded) onAdded(s);
        return;
    }

    const Song existing = songs[duplicate];
    juce::Component::SafePointer<SongsPage> safeThis(this);

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Duplicate song")
            .withMessage("This audio file is already in your library as \"" + existing.title + "\".\n\n"
                         "Do you want to replace the old song with this one?")
            .withButton("Replace")
            .withButton("Don't Add")
            .withAssociatedComponent(this),
        [safeThis, existing, s, onAdded](int result) mutable
        {
            if (safeThis == nullptr) return;

            if (result == 1)
            {
                safeThis->replaceSong(existing, std::move(s), std::move(onAdded));
                return;
            }

            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::InfoIcon)
                    .withTitle("Song not added")
                    .withMessage("\"" + s.title + "\" wasn't added. Your library still has the original \""
                                 + existing.title + "\".")
                    .withButton("OK")
                    .withAssociatedComponent(safeThis.getComponent()),
                nullptr);
        });
}

void SongsPage::replaceSong(const Song& oldSong, Song newSong, std::function<void(const Song&)> onAdded)
{
    // copy the new one in first so a failed copy doesnt lose the old song
    if (! importIntoLibrary(newSong)) return;

    for (int i = songs.size(); --i >= 0;)
        if (songsHaveSameIdentity(songs[i], oldSong))
            removeSongAt(i);

    // then it goes in like any other upload
    songs.insert(0, newSong);
    saveLibrary();
    rebuildList();

    if (onSongReplaced) onSongReplaced(oldSong, newSong);
    if (onAdded) onAdded(newSong);
}

void SongsPage::removeSongAt(int index)
{
    if (! juce::isPositiveAndBelow(index, songs.size())) return;

    if (onSongRemoved) onSongRemoved(songs[index]);
    library.deleteSongFile(songs[index]);  // remove the stored copy too
    library.deleteSongImage(songs[index]);
    songs.remove(index);
}

bool SongsPage::importIntoLibrary(Song& s)
{
    // the picture is optional keep the song without it if it cant be copied
    if (s.imageFile != juce::File() && ! library.importSongImage(s))
    {
        s.image     = {};
        s.imageFile = juce::File();
    }

    // copy the picked file into the library folder
    // songs added without a file skip this
    if (s.filePath.isNotEmpty() && ! library.importSong(s))
    {
        library.deleteSongImage(s); // dont keep a picture for a song that wasnt added

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
            if (result == 1 && juce::isPositiveAndBelow(index, songs.size()))
            {
                removeSongAt(index);
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

//==============================================================================
// drag rows to reorder, the order is saved to library.json as the custom order

int SongsPage::getDropIndex(juce::Point<int> posInPage) const
{
    const auto pos = listContainer.getLocalPoint(this, posInPage);

    // above the middle of a row means before it
    for (int i = 0; i < rows.size(); ++i)
        if (pos.y < rows[i]->getBounds().getCentreY())
            return i;

    return rows.size();
}

bool SongsPage::isInterestedInDragSource(const SourceDetails& details)
{
    return sortMode == SongSortMode::custom
        && details.description.toString().startsWith("row:")
        && details.sourceComponent != nullptr
        && details.sourceComponent->getParentComponent() == &listContainer;
}

void SongsPage::itemDragMove(const SourceDetails& details)
{
    // scroll when dragging near the top or bottom
    const auto inViewport = listViewport.getLocalPoint(this, details.localPosition);
    listViewport.autoScroll(inViewport.x, inViewport.y, 30, 12);

    const int pad   = 8;
    const int index = getDropIndex(details.localPosition);
    const int lineY = index < rows.size()
                    ? rows[index]->getY() - pad / 2
                    : (rows.isEmpty() ? addSongRow.getBottom() : rows.getLast()->getBottom()) + pad / 2;

    dropIndicator.setBounds(4, lineY - 2, listContainer.getWidth() - 16, 3);
    dropIndicator.setVisible(true);
}

void SongsPage::itemDragExit(const SourceDetails&)
{
    dropIndicator.setVisible(false);
}

void SongsPage::itemDropped(const SourceDetails& details)
{
    dropIndicator.setVisible(false);

    // the dragged row is still handling its mouse up so rebuild just after
    const int from = details.description.toString().fromFirstOccurrenceOf(":", false, false).getIntValue();
    const int to   = getDropIndex(details.localPosition); // custom order so rows match songs

    juce::Component::SafePointer<SongsPage> safeThis(this);
    juce::MessageManager::callAsync([safeThis, from, to]
    {
        if (safeThis != nullptr) safeThis->moveSong(from, to);
    });
}

void SongsPage::moveSong(int from, int to)
{
    if (! juce::isPositiveAndBelow(from, songs.size())) return;

    // to is a slot between songs taking the song out shifts later slots up one
    if (to > from) --to;
    to = juce::jlimit(0, songs.size() - 1, to);
    if (to == from) return;

    songs.move(from, to);
    saveLibrary();
    rebuildList();
}

//==============================================================================

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
    // idx counts only this pair's transitions, the same list getTransitionsForPair returns
    for (auto& song : songs)
        if (song.title == s1Title)
        {
            int pairIndex = 0;
            for (int i = 0; i < song.transitions.size(); ++i)
                if (song.transitions[i].toSongTitle == t.toSongTitle && pairIndex++ == idx)
                    { song.transitions.set(i, t); break; }
            break;
        }

    saveLibrary();
}

void SongsPage::updateSongTranscript(const Song& updatedSong)
{
    bool changed = false;
    for (auto& song : songs)
    {
        if (songsHaveSameIdentity(song, updatedSong))
        {
            song.transcript = updatedSong.transcript;
            changed = true;
        }
    }

    if (changed)
        rebuildList();

    if (onSongTranscriptChanged)
        onSongTranscriptChanged(updatedSong);
}

void SongsPage::updateSongBpm(const Song& updatedSong)
{
    bool changed = false;
    for (auto& song : songs)
    {
        if (songsHaveSameIdentity(song, updatedSong) && song.bpm != updatedSong.bpm)
        {
            song.bpm = updatedSong.bpm;
            changed = true;
        }
    }

    if (! changed) return;

    saveLibrary();
    rebuildList(); // a bpm sort may move it

    if (onSongBpmChanged)
        onSongBpmChanged(updatedSong);
}

void SongsPage::showSongDetail(int index)
{
    if (index < 0 || index >= songs.size()) return;
    const auto& s = songs[index];

    const auto playlists = getPlaylistsForSong ? getPlaylistsForSong(s) : juce::StringArray{};

    auto* content = new SongDetailDialog(s, playlists, {});
    content->onSongUpdated = [this](const Song& updatedSong)
    {
        updateSongTranscript(updatedSong);
    };
    content->onBpmChanged = [this](const Song& updatedSong) { updateSongBpm(updatedSong); };
    content->onTranscribeRequested = [this](
        const Song& song,
        std::function<void(juce::Result, juce::String)> completion)
    {
        if (onTranscribeSong)
            onTranscribeSong(song, std::move(completion));
        else
            completion(juce::Result::fail("Local transcription is not configured."), {});
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = s.title;
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
