#include "SongDetailDialog.h"

static constexpr int kPad      = 20;
static constexpr int kLineH    = 22;
static constexpr int kSepH     = 10; // gap above/below separator lines
static constexpr int kHeaderH  = 20;
static constexpr int kItemH    = 20;
static constexpr int kTranscriptEditorHeight = 100;

int SongDetailDialog::calcHeight(int headerHeight, int numInfoLines, int numPlaylists, int numTransitions)
{
    // Title row + labeled info rows + separator
    const int topSection = headerHeight + (numInfoLines * kLineH) + kSepH * 2 + 1;
    // "In Playlists:" header + at least one item line
    const int plSection  = kHeaderH + (juce::jmax(1, numPlaylists) * kItemH) + kSepH * 2 + 1;
    // "Transitions:" header + at least one item line
    const int trSection  = kHeaderH + (juce::jmax(1, numTransitions) * kItemH) + kSepH * 2 + 1;
    // Transcription header, action, explicit-word status, and transcript editor
    const int transcriptSection = kHeaderH + 30 + 6 + kItemH + 6
                                + kTranscriptEditorHeight + kSepH + 1;
    // Close button + bottom padding
    const int footer     = 36 + kPad;

    return kPad + topSection + plSection + trSection + transcriptSection + footer;
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

    transcribeButton.onClick = [this]
    {
        if (!onTranscribeRequested)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Transcription unavailable", "The local transcription engine is not available.");
            return;
        }

        transcribeButton.setEnabled(false);
        transcribeButton.setButtonText("Transcribing...");
        const juce::Component::SafePointer<SongDetailDialog> safeThis(this);
        onTranscribeRequested(song, [safeThis](juce::Result result, juce::String transcript)
        {
            if (safeThis == nullptr)
                return;

            safeThis->transcribeButton.setEnabled(true);
            safeThis->transcribeButton.setButtonText("Transcribe audio");
            if (result.failed())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Transcription failed", result.getErrorMessage());
                return;
            }

            safeThis->song.transcript = std::move(transcript);
            safeThis->updateTranscriptDetails();
            if (safeThis->onSongUpdated)
                safeThis->onSongUpdated(safeThis->song);
        });
    };
    addAndMakeVisible(transcribeButton);

    bpmEditor.setInputRestrictions(6, "0123456789.");
    bpmEditor.setTextToShowWhenEmpty("Unknown", juce::Colours::grey);
    bpmEditor.setText(formatSongBpm(song.bpm), juce::dontSendNotification);
    bpmEditor.onTextChange = [this]
    {
        const auto bpm = parseSongBpm(bpmEditor.getText());
        if (bpm == song.bpm) return;
        song.bpm = bpm;
        if (onBpmChanged) onBpmChanged(song);
    };
    bpmEditor.onReturnKey = [this] { unfocusAllComponents(); };
    addAndMakeVisible(bpmEditor);

    explicitWordsLabel.setFont(juce::FontOptions(12.0f));
    explicitWordsLabel.setColour(juce::Label::textColourId,
                                 juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(explicitWordsLabel);

    transcriptEditor.setMultiLine(true);
    transcriptEditor.setReadOnly(true);
    transcriptEditor.setScrollbarsShown(true);
    transcriptEditor.setTextToShowWhenEmpty("No transcript yet.", juce::Colours::grey);
    addAndMakeVisible(transcriptEditor);

    setSize(kWidth, calcHeight(getHeaderHeight(), getNumInfoLines(), playlistNames.size(), transitionNames.size()));
    updateTranscriptDetails();
}

void SongDetailDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    const int w = getWidth() - kPad * 2;
    int y = kPad;

    // ── Picture and song name ──────────────────────────────────────────────
    if (! song.image.isNull())
    {
        juce::Path clip;
        clip.addRoundedRectangle((float)kPad, (float)y, (float)kImageSize, (float)kImageSize, 6.0f);
        g.saveState();
        g.reduceClipRegion(clip);
        g.setOpacity(1.0f);
        g.drawImageWithin(song.image, kPad, y, kImageSize, kImageSize,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        g.restoreState();

        const int textX = kPad + kImageSize + 14;
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawFittedText(song.title, textX, y, getWidth() - kPad - textX, kImageSize,
                         juce::Justification::centredLeft, 3);
    }
    else
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawText(song.title, kPad, y, w, 38, juce::Justification::centredLeft, true);
    }
    y += getHeaderHeight();

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
    if (song.genre.isNotEmpty())    drawInfoRow("Genre:",    song.genre);
    if (song.releaseYear > 0)       drawInfoRow("Year:",     juce::String(song.releaseYear));
    if (song.durationSeconds > 0)   drawInfoRow("Duration:", formatSongDuration(song.durationSeconds));
    drawInfoRow("BPM:", {}); // the value is bpmEditor, placed over this row in resized

    // ── Separator ──────────────────────────────────────────────────────────
    y += kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(y, (float)kPad, (float)(getWidth() - kPad));
    y += 1 + kSepH;
    // ── In Playlists ───────────────────────────────────────────────────────
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
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Transitions", kPad, y, w, kHeaderH, juce::Justification::centredLeft);
    y += kHeaderH;

    g.setFont(juce::FontOptions(13.0f));
    if (transitionNames.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawText("(none saved)", kPad + 12, y, w - 12, kItemH, juce::Justification::centredLeft);
        y += kItemH;
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

    y += kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(y, (float)kPad, (float)(getWidth() - kPad));
    y += 1 + kSepH;

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Transcription", kPad, y, w, kHeaderH, juce::Justification::centredLeft);
}

void SongDetailDialog::resized()
{
    // bpm is always the last info row
    const int bpmRowY = kPad + getHeaderHeight() + (getNumInfoLines() - 1) * kLineH;
    bpmEditor.setBounds(kPad + 80, bpmRowY, 90, kLineH);

    int y = kPad + getHeaderHeight() + getNumInfoLines() * kLineH;
    y += kSepH * 2 + 1;
    y += kHeaderH + juce::jmax(1, playlistNames.size()) * kItemH + kSepH * 2 + 1;
    y += kHeaderH + juce::jmax(1, transitionNames.size()) * kItemH + kSepH * 2 + 1;
    y += kHeaderH;

    transcribeButton.setBounds(kPad, y, 150, 30);
    y += 36;
    explicitWordsLabel.setBounds(kPad, y, getWidth() - kPad * 2, kItemH);
    y += kItemH + 6;
    transcriptEditor.setBounds(kPad, y, getWidth() - kPad * 2, kTranscriptEditorHeight);

    auto btnBounds = getLocalBounds().reduced(kPad);
    closeButton.setBounds(btnBounds.removeFromBottom(36).removeFromRight(100));
}

int SongDetailDialog::getHeaderHeight() const
{
    return song.image.isNull() ? 42 : kImageSize + 12;
}

int SongDetailDialog::getNumInfoLines() const
{
    return (song.artist.isNotEmpty() ? 1 : 0)
         + (song.album.isNotEmpty() ? 1 : 0)
         + (song.genre.isNotEmpty() ? 1 : 0)
         + (song.releaseYear > 0 ? 1 : 0)
         + (song.durationSeconds > 0 ? 1 : 0)
         + 1; // bpm, always shown so it can be filled in
}

void SongDetailDialog::updateTranscriptDetails()
{
    const auto words = song.getExplicitWords();
    explicitWordsLabel.setText(words.isEmpty()
        ? "No explicit words detected"
        : juce::String("Explicit words detected: ") + words.joinIntoString(", "),
        juce::dontSendNotification);
    transcriptEditor.setText(song.transcript, juce::dontSendNotification);
}
