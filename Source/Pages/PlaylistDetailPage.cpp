#include "PlaylistDetailPage.h"
#include "AddSongDialog.h"
#include "PickSongDialog.h"

static constexpr int kPad = 8; // gap between rows

PlaylistDetailPage::PlaylistDetailPage()
{
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::FontOptions(12.0f));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    addAndMakeVisible(subtitleLabel);

    editButton.onClick = [this] { if (onEditClicked && currentPlaylist) onEditClicked(currentPlaylist); };
    addAndMakeVisible(editButton);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    listContainer.addAndMakeVisible(addSongRow);
    addSongRow.onClick = [this] { showPickSongDialog(); };

    listContainer.addChildComponent(dropIndicator);

    // sidebar is the library click adds to the end drag drops anywhere
    sidebarContent.setDragEnabled(true);
    sidebarContent.onItemClicked = [this](int index)
    {
        if (index >= 0 && index < librarySongs.size())
            handleAddSong(librarySongs[index]);
    };
}

void PlaylistDetailPage::setPlaylist(Playlist* playlist)
{
    currentPlaylist = playlist;
    refreshHeader();
    rebuildList();
}

void PlaylistDetailPage::refreshHeader()
{
    if (currentPlaylist == nullptr) return;

    titleLabel.setText(currentPlaylist->name, juce::dontSendNotification);

    // eg "Workout · 5 songs"
    const int n = currentPlaylist->songs.size();
    juce::String sub = juce::String(n) + (n == 1 ? " song" : " songs");
    if (currentPlaylist->group.isNotEmpty())
        sub = currentPlaylist->group + juce::String(juce::CharPointer_UTF8(" \xc2\xb7 ")) + sub;
    subtitleLabel.setText(sub, juce::dontSendNotification);
}

void PlaylistDetailPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PlaylistDetailPage::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto header = area.removeFromTop(40);
    editButton.setBounds(header.removeFromRight(70).withSizeKeepingCentre(70, 28));
    titleLabel.setBounds(header);
    subtitleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;

    int totalH = kPad + NewPlaylistRow::kHeight + kPad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + kPad;

    // fill the viewport so drops below the last song still land
    listContainer.setSize(viewW, juce::jmax(totalH, listViewport.getHeight()));

    int y = kPad;
    addSongRow.setBounds(4, y, rowW, NewPlaylistRow::kHeight);
    y += NewPlaylistRow::kHeight + kPad;

    for (auto* row : rows)
    {
        row->setBounds(4, y, rowW, SongRow::kHeight);
        y += SongRow::kHeight + kPad;
    }
}

void PlaylistDetailPage::rebuildList()
{
    rows.clear();
    dropIndicator.setVisible(false);

    // refresh the library in the sidebar
    librarySongs = getAllSongs ? getAllSongs() : juce::Array<Song>{};
    sidebarContent.updateSongs(librarySongs);

    if (currentPlaylist == nullptr)
        return;

    for (int i = 0; i < currentPlaylist->songs.size(); ++i)
    {
        auto* row = rows.add(new SongRow(currentPlaylist->songs[i]));
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        row->onClicked       = [this, i] { if (onSongClicked) onSongClicked(currentPlaylist, i); };
        row->dragDescription = "row:" + juce::String(i); // drag to reorder
        listContainer.addAndMakeVisible(row);
    }

    dropIndicator.toFront(false);
    refreshHeader();
    resized();
}

void PlaylistDetailPage::markChanged()
{
    currentPlaylist->lastUpdated = juce::Time::getCurrentTime();
    rebuildList();
    if (onPlaylistChanged) onPlaylistChanged(); // saves the new order
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
                markChanged();
            }
        });
}

void PlaylistDetailPage::handleAddSong(Song s)
{
    if (currentPlaylist == nullptr) return;
    insertSong(std::move(s), currentPlaylist->songs.size()); // end of the list
}

void PlaylistDetailPage::insertSong(Song s, int index)
{
    if (currentPlaylist == nullptr) return;
    currentPlaylist->songs.insert(juce::jlimit(0, currentPlaylist->songs.size(), index), std::move(s));
    markChanged();
}

void PlaylistDetailPage::moveSong(int from, int to)
{
    if (currentPlaylist == nullptr) return;
    auto& songs = currentPlaylist->songs;
    if (from < 0 || from >= songs.size()) return;

    // to is a slot between songs taking the song out shifts later slots up one
    if (to > from) --to;
    to = juce::jlimit(0, songs.size() - 1, to);
    if (to == from) return;

    songs.move(from, to);
    markChanged();
}

//==============================================================================
// drag and drop

int PlaylistDetailPage::getDropIndex(juce::Point<int> posInPage) const
{
    const auto pos = listContainer.getLocalPoint(this, posInPage);

    // above the middle of a row means before it
    for (int i = 0; i < rows.size(); ++i)
        if (pos.y < rows[i]->getBounds().getCentreY())
            return i;

    return rows.size();
}

bool PlaylistDetailPage::isInterestedInDragSource(const SourceDetails& details)
{
    if (currentPlaylist == nullptr) return false;

    const auto desc = details.description.toString();

    // library songs from our sidebar or rows from this list
    if (desc.startsWith("library:"))
        return details.sourceComponent.get() == &sidebarContent;
    if (desc.startsWith("row:"))
        return details.sourceComponent != nullptr
            && details.sourceComponent->getParentComponent() == &listContainer;

    return false;
}

void PlaylistDetailPage::itemDragMove(const SourceDetails& details)
{
    // scroll when dragging near the top or bottom
    const auto inViewport = listViewport.getLocalPoint(this, details.localPosition);
    listViewport.autoScroll(inViewport.x, inViewport.y, 30, 12);

    // line in the gap above the slot or below the last row
    const int index = getDropIndex(details.localPosition);
    const int lineY = index < rows.size()
                    ? rows[index]->getY() - kPad / 2
                    : (rows.isEmpty() ? addSongRow.getBottom() : rows.getLast()->getBottom()) + kPad / 2;

    dropIndicator.setBounds(4, lineY - 2, listContainer.getWidth() - 16, 3);
    dropIndicator.setVisible(true);
}

void PlaylistDetailPage::itemDragExit(const SourceDetails&)
{
    dropIndicator.setVisible(false);
}

void PlaylistDetailPage::itemDropped(const SourceDetails& details)
{
    dropIndicator.setVisible(false);

    // the dragged row is still handling its mouse up right now
    // so rebuild the list just after instead of deleting it mid event
    const auto desc  = details.description.toString();
    const int  index = getDropIndex(details.localPosition);

    juce::Component::SafePointer<PlaylistDetailPage> safeThis(this);
    juce::MessageManager::callAsync([safeThis, desc, index]
    {
        if (safeThis != nullptr) safeThis->applyDrop(desc, index);
    });
}

void PlaylistDetailPage::applyDrop(const juce::String& desc, int index)
{
    const int from = desc.fromFirstOccurrenceOf(":", false, false).getIntValue();

    if (desc.startsWith("library:") && from >= 0 && from < librarySongs.size())
        insertSong(librarySongs[from], index); // copy of the library song
    else if (desc.startsWith("row:"))
        moveSong(from, index);
}

//==============================================================================

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
    content->onCreated = [this](Song s)
    {
        // goes into the library too so the file gets copied and saved
        if (addSongToLibrary && ! addSongToLibrary(s)) return;
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
