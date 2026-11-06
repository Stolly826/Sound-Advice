#include "PlaylistDetailPage.h"
#include "AddSongDialog.h"
#include "PickSongDialog.h"

PlaylistDetailPage::PlaylistDetailPage()
{
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(addSongRow);
    addSongRow.onClick = [this] { showPickSongDialog(); };

    // Sidebar item click mirrors clicking the same song row.
    sidebarContent.onItemClicked = [this](int index)
    {
        if (index >= 0 && index < rows.size())
            if (rows[index]->onClicked) rows[index]->onClicked();
    };
}

void PlaylistDetailPage::setPlaylist(Playlist* playlist)
{
    currentPlaylist = playlist;
    if (currentPlaylist != nullptr)
        titleLabel.setText(currentPlaylist->name, juce::dontSendNotification);
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

void PlaylistDetailPage::rebuildList()
{
    rows.clear();

    if (currentPlaylist == nullptr)
        return;

    for (int i = 0; i < currentPlaylist->songs.size(); ++i)
    {
        auto* row = rows.add(new SongRow(currentPlaylist->songs[i]));
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        row->onClicked       = [this, i] { if (onSongClicked) onSongClicked(currentPlaylist, i); };
        listContainer.addAndMakeVisible(row);
    }

    sidebarContent.updateSongs(currentPlaylist->songs);
    resized();
}

void PlaylistDetailPage::handleDeleteRequest(int index)
{
    if (currentPlaylist == nullptr) return;

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
            if (result == 1 && currentPlaylist != nullptr)
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
    content->readSongDetails = readSongDetails;
    content->onCreated = [this](Song s)
    {
        if (onNewSong) onNewSong(s); // A new song joins the library as well as this playlist.
        handleAddSong(std::move(s));
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Add Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
