#include "PlaylistsPage.h"
#include "NewPlaylistDialog.h"

static constexpr int kHeaderH = 28; // group header height

PlaylistsPage::PlaylistsPage()
{
    // load saved playlists first run has none so make the demo ones
    if (library.hasSavedPlaylists())
    {
        for (const auto& p : library.loadPlaylists())
            playlists.add(new Playlist(p));
    }
    else
    {
        seedDemoPlaylists();
        savePlaylists();
    }

    titleLabel.setText("Playlists", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(newPlaylistRow);
    newPlaylistRow.onClick = [this] { showNewPlaylistDialog(); };

    // Sidebar item click mirrors clicking the same playlist row.
    sidebarContent.onItemClicked = [this](int index)
    {
        if (index >= 0 && index < playlists.size())
            if (onPlaylistClicked) onPlaylistClicked(playlists[index]);
    };

    rebuildList();
}

void PlaylistsPage::seedDemoPlaylists()
{
    // Seed two placeholder playlists (newest first).
    {
        auto* p       = playlists.add(new Playlist());
        p->name        = "Workout Mix";
        p->description = "High energy tracks";
        p->lastUpdated = juce::Time::getCurrentTime() - juce::RelativeTime::days(2);

        // Placeholder songs
        Song s1; s1.title = "Eye of the Tiger"; s1.artist = "Survivor";
        s1.album = "Eye of the Tiger"; s1.dateAdded = p->lastUpdated;
        s1.durationSeconds = 244;
        p->songs.add(s1);

        Song s2; s2.title = "Thunderstruck"; s2.artist = "AC/DC";
        s2.album = "The Razors Edge"; s2.dateAdded = p->lastUpdated;
        s2.durationSeconds = 292;
        p->songs.add(s2);
    }
    {
        auto* p       = playlists.add(new Playlist());
        p->name        = "Chill Vibes";
        p->lastUpdated = juce::Time::getCurrentTime() - juce::RelativeTime::days(7);

        Song s1; s1.title = "Lo-fi Dream"; s1.artist = "Various";
        s1.album = "Study Beats"; s1.dateAdded = p->lastUpdated;
        s1.durationSeconds = 185;
        p->songs.add(s1);
    }
}

void PlaylistsPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PlaylistsPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(8);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;
    const int pad   = 8;

    int totalH = pad + NewPlaylistRow::kHeight + pad;
    for (const auto& item : layoutItems)
        totalH += item.second + pad;

    listContainer.setSize(viewW, totalH);

    int y = pad;
    newPlaylistRow.setBounds(4, y, rowW, NewPlaylistRow::kHeight);
    y += NewPlaylistRow::kHeight + pad;

    // headers and rows stacked in group order
    for (const auto& item : layoutItems)
    {
        item.first->setBounds(4, y, rowW, item.second);
        y += item.second + pad;
    }
}

juce::StringArray PlaylistsPage::getGroupNames() const
{
    juce::StringArray groups;
    for (auto* p : playlists)
        if (p->group.isNotEmpty())
            groups.addIfNotAlreadyThere(p->group, true); // ignore case

    groups.sortNatural();
    return groups;
}

void PlaylistsPage::rebuildList()
{
    rows.clear();
    groupHeaders.clear();
    layoutItems.clear();

    // groups a to z then playlists with no group under other
    // headers only show once at least one playlist has a group
    auto groups = getGroupNames();
    const bool showHeaders = ! groups.isEmpty();
    groups.add({});

    for (const auto& group : groups)
    {
        juce::Array<Playlist*> inGroup;
        for (auto* p : playlists)
            if (p->group.equalsIgnoreCase(group))
                inGroup.add(p);

        if (inGroup.isEmpty())
            continue;

        if (showHeaders)
        {
            auto* header = groupHeaders.add(new juce::Label({}, group.isEmpty() ? "Other" : group));
            header->setFont(juce::FontOptions(13.0f, juce::Font::bold));
            header->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
            header->setJustificationType(juce::Justification::bottomLeft);
            listContainer.addAndMakeVisible(header);
            layoutItems.push_back({ header, kHeaderH });
        }

        for (auto* p : inGroup)
        {
            // rows hold the playlist pointer so delete still works when grouped
            auto* row = rows.add(new PlaylistRow(*p));
            row->onDeleteClicked = [this, p] { handleDeleteRequest(p); };
            row->onClicked       = [this, p] { if (onPlaylistClicked) onPlaylistClicked(p); };
            listContainer.addAndMakeVisible(row);
            layoutItems.push_back({ row, PlaylistRow::kHeight });
        }
    }

    // Build snapshot for sidebar (OwnedArray → Array copy)
    juce::Array<Playlist> snapshot;
    for (auto* p : playlists) snapshot.add(*p);
    sidebarContent.updatePlaylists(snapshot);

    resized();
}

void PlaylistsPage::savePlaylists()
{
    if (! library.savePlaylists(playlists))
        DBG("couldnt save playlists to " + library.getPlaylistsFile().getFullPathName());
}

void PlaylistsPage::playlistChanged()
{
    savePlaylists();
    rebuildList();
}

void PlaylistsPage::handleDeleteRequest(Playlist* playlist)
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Delete \"" + playlist->name + "\"?")
            .withMessage("This will permanently delete the playlist.")
            .withButton("Delete")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [this, playlist](int result)
        {
            const int index = playlists.indexOf(playlist);
            if (result == 1 && index >= 0)
            {
                library.deletePlaylistImage(*playlist); // remove the copied image too
                playlists.remove(index);
                playlistChanged();
            }
        });
}

void PlaylistsPage::handleNewPlaylist(Playlist p)
{
    // copy the picked image into the library keep going without it if that fails
    if (p.imageFile != juce::File() && ! library.importPlaylistImage(p))
    {
        p.image     = {};
        p.imageFile = juce::File();
    }

    playlists.insert(0, new Playlist(std::move(p)));
    playlistChanged();
}

juce::StringArray PlaylistsPage::getPlaylistNamesContaining(const Song& song) const
{
    juce::StringArray result;
    for (auto* p : playlists)
        for (const auto& s : p->songs)
            if (s.title == song.title && s.artist == song.artist)
            {
                result.add(p->name);
                break;
            }
    return result;
}

void PlaylistsPage::showNewPlaylistDialog()
{
    auto* content = new NewPlaylistDialog(getGroupNames());
    content->onCreated = [this](Playlist p) { handleNewPlaylist(std::move(p)); };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "New Playlist";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

void PlaylistsPage::showEditDialog(Playlist* playlist, juce::Component* centreAround, std::function<void()> onDone)
{
    if (playlist == nullptr) return;

    auto* content = new NewPlaylistDialog(getGroupNames(), playlist);

    content->onCreated = [this, playlist, onDone](Playlist edited)
    {
        if (playlists.indexOf(playlist) < 0) return;

        // new image picked copy it in and drop the old copy
        if (edited.imageFile != playlist->imageFile)
        {
            if (edited.imageFile == juce::File() || library.importPlaylistImage(edited))
            {
                library.deletePlaylistImage(*playlist);
                playlist->image     = edited.image;
                playlist->imageFile = edited.imageFile;
            }
        }

        // songs stay as they are only the details change
        playlist->name        = edited.name;
        playlist->group       = edited.group;
        playlist->description = edited.description;
        playlist->lastUpdated = juce::Time::getCurrentTime();

        playlistChanged();
        if (onDone) onDone();
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Edit Playlist";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = centreAround != nullptr ? centreAround : this;
    opts.launchAsync();
}
