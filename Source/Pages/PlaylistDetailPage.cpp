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

    descriptionLabel.setFont(juce::FontOptions(14.0f));
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    descriptionLabel.setJustificationType(juce::Justification::topLeft);
    descriptionLabel.setMinimumHorizontalScale(1.0f); // wrap instead of squashing the text
    addAndMakeVisible(descriptionLabel);

    fillSongSortBox(sortBox);
    sortBox.onChange = [this]
    {
        if (currentPlaylist == nullptr) return;

        // saved with the playlist so it opens the same way next time
        currentPlaylist->sortMode = songSortModeToId(getSelectedSongSortMode(sortBox));
        rebuildList();
        if (onPlaylistChanged) onPlaylistChanged();
    };
    sortLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(sortLabel);
    addAndMakeVisible(sortBox);

    editButton.onClick = [this] { if (onEditClicked && currentPlaylist) onEditClicked(currentPlaylist); };
    addAndMakeVisible(editButton);

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
    listContainer.addChildComponent(emptyStateLabel); // sits under the last row

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
    selectSongSortMode(sortBox, getSortMode());
    refreshHeader();
    rebuildList();
}

void PlaylistDetailPage::refreshSongs()
{
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

    descriptionLabel.setText(currentPlaylist->description, juce::dontSendNotification);
    resized(); // description height can change
}

SongSortMode PlaylistDetailPage::getSortMode() const
{
    return currentPlaylist != nullptr ? songSortModeFromId(currentPlaylist->sortMode)
                                      : SongSortMode::custom;
}

int PlaylistDetailPage::getDescriptionHeight(int width) const
{
    const auto text = descriptionLabel.getText();
    if (text.isEmpty() || width <= 0) return 0;

    // measure the wrapped text, long descriptions stop at 4 lines
    juce::AttributedString attributed;
    attributed.append(text, descriptionLabel.getFont());
    juce::TextLayout layout;
    layout.createLayout(attributed, (float) width);

    const int lineH = juce::roundToInt(descriptionLabel.getFont().getHeight()) + 2;
    const int border = descriptionLabel.getBorderSize().getTopAndBottom();
    return juce::jmin(juce::roundToInt(layout.getHeight()) + 4, lineH * 4) + border;
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

    // description under the title, hidden when there isnt one
    const int border = descriptionLabel.getBorderSize().getLeftAndRight();
    const int descH  = getDescriptionHeight(area.getWidth() - border);
    descriptionLabel.setVisible(descH > 0);
    if (descH > 0)
    {
        area.removeFromTop(4);
        descriptionLabel.setBounds(area.removeFromTop(descH));
    }
    area.removeFromTop(6);

    auto controls = area.removeFromTop(32);
    sortLabel.setBounds(controls.removeFromLeft(56));
    sortBox.setBounds(controls.removeFromLeft(150).withSizeKeepingCentre(150, 28));
    explicitWordsButton.setBounds(controls.removeFromRight(142));
    controls.removeFromRight(8);
    explicitFilterButton.setBounds(controls.removeFromRight(112));
    area.removeFromTop(8);
    listViewport.setBounds(area);

    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;

    int totalH = kPad + NewPlaylistRow::kHeight + kPad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + kPad;
    if (rows.isEmpty())
        totalH += 44; // room for the empty message

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

    emptyStateLabel.setVisible(rows.isEmpty() && currentPlaylist != nullptr);
    emptyStateLabel.setBounds(4, y, rowW, 36);
}

void PlaylistDetailPage::rebuildList()
{
    rows.clear();
    visibleSongIndices.clear();
    dropIndicator.setVisible(false);

    // refresh the library in the sidebar
    librarySongs = getAllSongs ? getAllSongs() : juce::Array<Song>{};
    sidebarContent.updateSongs(librarySongs);

    if (currentPlaylist == nullptr)
    {
        resized();
        return;
    }

    emptyStateLabel.setText(currentPlaylist->songs.isEmpty()
        ? "This playlist has no songs yet."
        : "No non-explicit songs to show.",
        juce::dontSendNotification);

    for (const int i : getSortedSongOrder(currentPlaylist->songs, getSortMode()))
    {
        const auto& song = currentPlaylist->songs[i];
        if (hideExplicitSongs && song.isExplicit())
            continue;

        visibleSongIndices.add(i);
        auto* row = rows.add(new SongRow(song));
        row->onDeleteClicked = [this, i] { handleDeleteRequest(i); };
        row->onClicked       = [this, i] { if (onSongClicked) onSongClicked(currentPlaylist, i); };
        if (isCustomOrder())
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
    // sorted lists have no in between spot, dropped songs go on the end
    if (! isCustomOrder())
        return rows.size();

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
        return isCustomOrder()
            && details.sourceComponent != nullptr
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

int PlaylistDetailPage::toPlaylistIndex(int rowSlot) const
{
    if (currentPlaylist == nullptr) return 0;

    // with explicit songs hidden the rows skip some songs
    // dropping before a row means before that song, past the last row means just after the last shown song
    if (juce::isPositiveAndBelow(rowSlot, visibleSongIndices.size()))
        return visibleSongIndices[rowSlot];
    if (! visibleSongIndices.isEmpty())
        return visibleSongIndices.getLast() + 1;
    return currentPlaylist->songs.size();
}

void PlaylistDetailPage::applyDrop(const juce::String& desc, int rowSlot)
{
    if (currentPlaylist == nullptr) return;

    const int from  = desc.fromFirstOccurrenceOf(":", false, false).getIntValue();
    const int index = toPlaylistIndex(rowSlot);

    // in a sorted list the slot means nothing so the song goes on the end of the custom order
    if (desc.startsWith("library:") && from >= 0 && from < librarySongs.size())
        insertSong(librarySongs[from], isCustomOrder() ? index : currentPlaylist->songs.size()); // copy of the library song
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
    content->readSongDetails = readSongDetails;
    content->onCreated = [this](Song s)
    {
        if (! addSongToLibrary)
        {
            handleAddSong(std::move(s));
            return;
        }

        // goes into the library too so the file gets copied and saved
        // a duplicate asks first so this can finish later or not at all
        juce::Component::SafePointer<PlaylistDetailPage> safeThis(this);
        addSongToLibrary(std::move(s), [safeThis](const Song& stored)
        {
            if (safeThis == nullptr || safeThis->currentPlaylist == nullptr) return;

            // replacing a duplicate already swapped it in where the old song was
            for (const auto& existing : safeThis->currentPlaylist->songs)
                if (songsHaveSameIdentity(existing, stored))
                    return;

            safeThis->handleAddSong(stored);
        });
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Add Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}
