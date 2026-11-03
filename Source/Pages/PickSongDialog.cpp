#include "PickSongDialog.h"

static constexpr int kPad = 16;

PickSongDialog::PickSongDialog(juce::Array<Song> librarySongs)
    : allSongs(std::move(librarySongs)),
      filtered(allSongs)
{
    addNewRow.onClick = [this]
    {
        if (onAddNewSong) onAddNewSong();
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(addNewRow);

    searchEditor.setTextToShowWhenEmpty("Search songs...", juce::Colours::grey);
    searchEditor.addListener(this);
    addAndMakeVisible(searchEditor);

    listViewport.setViewedComponent(&listContainer, false);
    listViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(listViewport);

    rebuildRows();

    setSize(kWidth, kHeight);
}

void PickSongDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Add Song", kPad, kPad, getWidth() - kPad * 2, 38, juce::Justification::centredLeft);
}

void PickSongDialog::resized()
{
    auto area = getLocalBounds().reduced(kPad);
    area.removeFromTop(42); // title

    addNewRow.setBounds(area.removeFromTop(NewPlaylistRow::kHeight));
    area.removeFromTop(8);
    searchEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    listViewport.setBounds(area);

    // Layout rows inside the viewport container
    const int viewW = juce::jmax(1, listViewport.getMaximumVisibleWidth());
    const int rowW  = viewW - 8;
    const int pad   = 8;

    int totalH = pad;
    for (int i = 0; i < rows.size(); ++i)
        totalH += SongRow::kHeight + pad;

    listContainer.setSize(viewW, juce::jmax(totalH, listViewport.getHeight()));

    int y = pad;
    for (auto* row : rows)
    {
        row->setBounds(4, y, rowW, SongRow::kHeight);
        y += SongRow::kHeight + pad;
    }
}

void PickSongDialog::textEditorTextChanged(juce::TextEditor&)
{
    applyFilter();
}

void PickSongDialog::applyFilter()
{
    const auto query = searchEditor.getText().trim().toLowerCase();
    filtered.clear();

    for (const auto& s : allSongs)
    {
        if (query.isEmpty()
            || s.title.toLowerCase().contains(query)
            || s.artist.toLowerCase().contains(query)
            || s.album.toLowerCase().contains(query))
        {
            filtered.add(s);
        }
    }

    rebuildRows();
}

void PickSongDialog::rebuildRows()
{
    rows.clear();

    for (int i = 0; i < filtered.size(); ++i)
    {
        auto* row = rows.add(new SongRow(filtered[i]));
        row->onClicked = [this, i]
        {
            if (onSongPicked) onSongPicked(filtered[i]);
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        // No onDeleteClicked — trash icon is visible but inert in picker context
        listContainer.addAndMakeVisible(row);
    }

    resized();
}
