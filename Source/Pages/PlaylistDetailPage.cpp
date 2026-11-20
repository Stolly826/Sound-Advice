#include "PlaylistDetailPage.h"
#include "AddSongDialog.h"
#include "PickSongDialog.h"

PlaylistDetailPage::PlaylistDetailPage()
{
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    explicitFilterButton.onClick = [this]
    {
        hideExplicitSongs = !hideExplicitSongs;
        explicitFilterButton.setButtonText(hideExplicitSongs ? "Show explicit" : "Hide explicit");
        rebuildList();
    };
    addAndMakeVisible(explicitFilterButton);

    explicitWordsButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Explicit word list",
            juce::String("Songs are labeled explicit when their transcript contains one or more of these English words:\n\n")
                + getExplicitWordListText());
    };
    addAndMakeVisible(explicitWordsButton);

    emptyStateLabel.setText("No songs match this filter.", juce::dontSendNotification);
    emptyStateLabel.setJustificationType(juce::Justification::centred);
    emptyStateLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
    addChildComponent(emptyStateLabel);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(addSongRow);
    addSongRow.onClick = [this] { showPickSongDialog(); };

    // Sidebar navigation selects a playlist song; adding remains exclusive to the Add Song row.
    sidebarContent.onItemClicked = [this](int index)
    {
        if (currentPlaylist != nullptr && juce::isPositiveAndBelow(index, visibleSongIndices.size())
            && onSongClicked)
            onSongClicked(currentPlaylist, visibleSongIndices[index]);
    };
}

void PlaylistDetailPage::setPlaylist(Playlist* playlist)
{
    currentPlaylist = playlist;
    if (currentPlaylist != nullptr)
        titleLabel.setText(currentPlaylist->name, juce::dontSendNotification);
    rebuildList();
}

void PlaylistDetailPage::refreshSongs()
{
    rebuildList();
}

void PlaylistDetailPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PlaylistDetailPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    titleLabel.setBounds(area.removeFromTop(40));
    auto controls = area.removeFromTop(32);
    explicitWordsButton.setBounds(controls.removeFromRight(142));
    controls.removeFromRight(8);
    explicitFilterButton.setBounds(controls.removeFromRight(112));
    area.removeFromTop(8);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;
    const int pad   = 8;

    int totalH = pad + NewPlaylistRow::kHeight + pad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + pad;
    if (rows.isEmpty())
        totalH += 44;

    listContainer.setSize(viewW, totalH);

    int y = pad;
    addSongRow.setBounds(4, y, rowW, NewPlaylistRow::kHeight);
    y += NewPlaylistRow::kHeight + pad;

    for (auto* row : rows)
    {
        row->setBounds(4, y, rowW, SongRow::kHeight);
        y += SongRow::kHeight + pad;
    }

    emptyStateLabel.setVisible(rows.isEmpty() && currentPlaylist != nullptr);
    emptyStateLabel.setBounds(4, y, rowW, 36);
}

void PlaylistDetailPage::rebuildList()
{
    rows.clear();
    visibleSongIndices.clear();

    if (currentPlaylist == nullptr)
    {
        sidebarContent.updateSongs(juce::Array<Song>{});
        resized();
        return;
    }

    emptyStateLabel.setText(currentPlaylist->songs.isEmpty()
        ? "This playlist has no songs yet."
        : "No non-explicit songs to show.",
        juce::dontSendNotification);

    for (int i = 0; i < currentPlaylist->songs.size(); ++i)
    {
        const auto& song = currentPlaylist->songs[i];
        if (hideExplicitSongs && song.isExplicit())
            continue;

        visibleSongIndices.add(i);
        auto* row = rows.add(new SongRow(song));
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        row->onClicked       = [this, i] { if (onSongClicked) onSongClicked(currentPlaylist, i); };
        listContainer.addAndMakeVisible(row);
    }

    juce::Array<Song> visibleSongs;
    for (const auto index : visibleSongIndices)
        visibleSongs.add(currentPlaylist->songs[index]);
    sidebarContent.updateSongs(visibleSongs);
    resized();
}

void PlaylistDetailPage::handleDeleteRequest(int index)
{
    if (currentPlaylist == nullptr || !juce::isPositiveAndBelow(index, currentPlaylist->songs.size()))
        return;

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Remove \"" + currentPlaylist->songs[index].title + "\"?")
            .withMessage("This will remove it from the playlist.")
            .withButton("Remove")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [this, index](int result)
        {
            if (result == 1 && currentPlaylist != nullptr
                && juce::isPositiveAndBelow(index, currentPlaylist->songs.size()))
            {
                currentPlaylist->songs.remove(index);
                currentPlaylist->lastUpdated = juce::Time::getCurrentTime();
                rebuildList();
            }
        });
}

void PlaylistDetailPage::handleAddSong(Song s)
{
    if (currentPlaylist == nullptr) return;
    currentPlaylist->songs.add(std::move(s));
    currentPlaylist->lastUpdated = juce::Time::getCurrentTime();
    rebuildList();
}

void PlaylistDetailPage::showPickSongDialog()
{
    auto songs = getAllSongs ? getAllSongs() : juce::Array<Song>{};
    auto* content = new PickSongDialog(std::move(songs));

    content->onSongPicked = [this](Song s) { handleAddSong(std::move(s)); };
    content->onAddNewSong = [this]
    {
        juce::MessageManager::callAsync([this] { showAddSongDialog(); });
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Add Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

void PlaylistDetailPage::showAddSongDialog()
{
    auto* content = new AddSongDialog();
    content->onCreated = [this](Song s) { handleAddSong(std::move(s)); };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Add Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
