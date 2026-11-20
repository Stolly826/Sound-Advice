#include "AddSongDialog.h"

static constexpr int kPad   = 20;
static constexpr int kLineH = 22;

AddSongDialog::AddSongDialog()
{
    fileNameLabel.setText("No file selected", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
    addAndMakeVisible(fileNameLabel);

    chooseFileButton.onClick = [this] { onChooseFile(); };
    addAndMakeVisible(chooseFileButton);

    for (auto* l : { &titleLabel, &artistLabel, &albumLabel })
        addAndMakeVisible(l);

    for (auto* e : { &titleEditor, &artistEditor, &albumEditor })
    {
        e->setReturnKeyStartsNewLine(false);
        addAndMakeVisible(e);
    }

    addButton.onClick    = [this] { onAddClicked(); };
    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(addButton);
    addAndMakeVisible(cancelButton);

    setSize(kWidth, kHeight);
}

void AddSongDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    const int w = getWidth() - kPad * 2;

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Add Song", kPad, kPad, w, 38, juce::Justification::centredLeft);

    // Section header
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("AUDIO FILE", kPad, kPad + 46, w, 16, juce::Justification::centredLeft);

    // Separator between file section and manual fields
    // y = kPad + title(42) + header(22) + button(40) + gap(6) + fileLabel(kLineH) + gap(10)
    const int sepY = kPad + 42 + 22 + 40 + 6 + kLineH + 10;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(sepY, (float)kPad, (float)(getWidth() - kPad));
}

void AddSongDialog::resized()
{
    auto area = getLocalBounds().reduced(kPad);

    area.removeFromTop(42); // title
    area.removeFromTop(22); // "AUDIO FILE" header

    chooseFileButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(6);
    fileNameLabel.setBounds(area.removeFromTop(kLineH));
    area.removeFromTop(10 + 1 + 10); // separator space

    auto field = [&](juce::Label& lbl, juce::TextEditor& ed)
    {
        lbl.setBounds(area.removeFromTop(18));
        ed.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
    };

    field(titleLabel,  titleEditor);
    field(artistLabel, artistEditor);
    field(albumLabel,  albumEditor);

    area.removeFromTop(4);
    auto btnRow = area.removeFromTop(36);
    cancelButton.setBounds(btnRow.removeFromRight(90));
    btnRow.removeFromRight(8);
    addButton.setBounds(btnRow.removeFromRight(90));
}

void AddSongDialog::onChooseFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an audio file",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mp3;*.wav;*.flac;*.aiff;*.m4a");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto result = fc.getResult();
            if (result.existsAsFile())
                onFileChosen(result);
        });
}

void AddSongDialog::onFileChosen(const juce::File& file)
{
    selectedFile = file;
    fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId,
                            juce::Colours::white.withAlpha(0.85f));

    // FRONTEND/BACKEND INTEGRATION: Pre-fill the form from the file's own details
    if (readSongDetails)
    {
        const auto details = readSongDetails(file);
        titleEditor.setText(details.title);
        artistEditor.setText(details.artist);
        albumEditor.setText(details.album);
        selectedDuration = details.durationSeconds;
    }

    // TODO: analyse key and energy for transition recommendations
}

void AddSongDialog::onAddClicked()
{
    const auto name = titleEditor.getText().trim();
    if (name.isEmpty() && !selectedFile.existsAsFile())
    {
        titleEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::red);
        titleEditor.repaint();
        return;
    }

    Song s;
    s.title     = name.isNotEmpty() ? name : selectedFile.getFileNameWithoutExtension();
    s.artist    = artistEditor.getText().trim();
    s.album     = albumEditor.getText().trim();
    s.filePath = selectedFile.existsAsFile() ? selectedFile.getFullPathName() : juce::String{}; //saves file full location jasen edit
    s.dateAdded = juce::Time::getCurrentTime();
    s.durationSeconds = selectedDuration;

    if (onCreated) onCreated(std::move(s));

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(1);
}
