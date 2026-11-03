#include "PlaylistsPage.h"
#include "NewPlaylistDialog.h"

PlaylistsPage::PlaylistsPage()
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
    for (int i = 0; i < rows.size(); ++i)
        totalH += PlaylistRow::kHeight + pad;

    listContainer.setSize(viewW, totalH);

    int y = pad;
    newPlaylistRow.setBounds(4, y, rowW, NewPlaylistRow::kHeight);
    y += NewPlaylistRow::kHeight + pad;

    for (auto* row : rows)
    {
        row->setBounds(4, y, rowW, PlaylistRow::kHeight);
        y += PlaylistRow::kHeight + pad;
    }
}

void PlaylistsPage::rebuildList()
{
    rows.clear();

    for (int i = 0; i < playlists.size(); ++i)
    {
        auto* row = rows.add(new PlaylistRow(*playlists[i]));
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        row->onClicked       = [this, i] { if (onPlaylistClicked) onPlaylistClicked(playlists[i]); };
        listContainer.addAndMakeVisible(row);
    }

    // Build snapshot for sidebar (OwnedArray → Array copy)
    juce::Array<Playlist> snapshot;
    for (auto* p : playlists) snapshot.add(*p);
    sidebarContent.updatePlaylists(snapshot);

    resized();
}

void PlaylistsPage::handleDeleteRequest(int index)
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Delete \"" + playlists[index]->name + "\"?")
            .withMessage("This will permanently delete the playlist.")
            .withButton("Delete")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [this, index](int result)
        {
            if (result == 1)
            {
                playlists.remove(index);
                rebuildList();
            }
        });
}

void PlaylistsPage::handleNewPlaylist(Playlist p)
{
    playlists.insert(0, new Playlist(std::move(p)));
    rebuildList();
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
    auto* content = new NewPlaylistDialog();
    content->onCreated = [this](Playlist p) { handleNewPlaylist(std::move(p)); };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "New Playlist";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
