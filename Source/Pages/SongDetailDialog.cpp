#include "SongDetailDialog.h"

static constexpr int kPad      = 20;
static constexpr int kLineH    = 22;
static constexpr int kSepH     = 10; // gap above/below separator lines
static constexpr int kHeaderH  = 20;
static constexpr int kItemH    = 20;

int SongDetailDialog::calcHeight(int numInfoLines, int numPlaylists, int numTransitions)
{
    // Title row + labeled info rows + separator
    const int topSection = 42 + (numInfoLines * kLineH) + kSepH * 2 + 1;
    // "In Playlists:" header + at least one item line
    const int plSection  = kHeaderH + (juce::jmax(1, numPlaylists) * kItemH) + kSepH * 2 + 1;
    // "Transitions:" header + at least one item line
    const int trSection  = kHeaderH + (juce::jmax(1, numTransitions) * kItemH) + kSepH * 2 + 1;
    // Close button + bottom padding
    const int footer     = 36 + kPad;

    return kPad + topSection + plSection + trSection + footer;
}

SongDetailDialog::SongDetailDialog(const Song& s,
                                   juce::StringArray inPlaylists,
                                   juce::StringArray inTransitions)
    : song(s),
      playlistNames(std::move(inPlaylists)),
      transitionNames(std::move(inTransitions))
{
    addAndMakeVisible(closeButton);
    closeButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };

    int numInfo = 0;
    if (s.artist.isNotEmpty())   ++numInfo;
    if (s.album.isNotEmpty())    ++numInfo;
    if (s.releaseYear > 0)       ++numInfo;
    if (s.durationSeconds > 0)   ++numInfo;

    setSize(kWidth, calcHeight(numInfo, playlistNames.size(), transitionNames.size()));
}

void SongDetailDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    const int w = getWidth() - kPad * 2;
    int y = kPad;

    // ── Song name ──────────────────────────────────────────────────────────
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(song.title, kPad, y, w, 38, juce::Justification::centredLeft, true);
    y += 42;

    // ── Labeled info rows ──────────────────────────────────────────────────
    g.setFont(juce::FontOptions(13.0f));
    auto drawInfoRow = [&](const juce::String& label, const juce::String& value)
    {
        const int labelW = 80;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawText(label, kPad, y, labelW, kLineH, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawText(value, kPad + labelW, y, w - labelW, kLineH, juce::Justification::centredLeft, true);
        y += kLineH;
    };

    if (song.artist.isNotEmpty())   drawInfoRow("Artist:",   song.artist);
    if (song.album.isNotEmpty())    drawInfoRow("Album:",    song.album);
    if (song.releaseYear > 0)       drawInfoRow("Year:",     juce::String(song.releaseYear));
    if (song.durationSeconds > 0)   drawInfoRow("Duration:", formatSongDuration(song.durationSeconds));

    // ── Separator ──────────────────────────────────────────────────────────
    y += kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(y, (float)kPad, (float)(getWidth() - kPad));
    y += 1 + kSepH;
    infoBottom = y;

    // ── In Playlists ───────────────────────────────────────────────────────
    playlistsTop = y;
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("In Playlists", kPad, y, w, kHeaderH, juce::Justification::centredLeft);
    y += kHeaderH;

    g.setFont(juce::FontOptions(13.0f));
    if (playlistNames.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawText("(none)", kPad + 12, y, w - 12, kItemH, juce::Justification::centredLeft);
        y += kItemH;
    }
    else
    {
        for (const auto& name : playlistNames)
        {
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x80\xa2")) + "  " + name,
                       kPad + 8, y, w - 8, kItemH, juce::Justification::centredLeft);
            y += kItemH;
        }
    }

    y += kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(y, (float)kPad, (float)(getWidth() - kPad));
    y += 1 + kSepH;

    // ── Transitions ────────────────────────────────────────────────────────
    transTop = y;
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Transitions", kPad, y, w, kHeaderH, juce::Justification::centredLeft);
    y += kHeaderH;

    g.setFont(juce::FontOptions(13.0f));
    if (transitionNames.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawText("(none saved)", kPad + 12, y, w - 12, kItemH, juce::Justification::centredLeft);
    }
    else
    {
        for (const auto& name : transitionNames)
        {
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x80\xa2")) + "  " + name,
                       kPad + 8, y, w - 8, kItemH, juce::Justification::centredLeft);
            y += kItemH;
        }
    }
}

void SongDetailDialog::resized()
{
    auto btnBounds = getLocalBounds().reduced(kPad);
    closeButton.setBounds(btnBounds.removeFromBottom(36).removeFromRight(100));
}
