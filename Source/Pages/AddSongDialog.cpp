#include "AddSongDialog.h"
#include "../SongAnalysis.h"

static constexpr int kPad   = 20;
static constexpr int kLineH = 22;

AddSongDialog::AddSongDialog()
{
    fileNameLabel.setText("No file selected", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
    addAndMakeVisible(fileNameLabel);

    chooseFileButton.onClick = [this] { onChooseFile(); };
    addAndMakeVisible(chooseFileButton);

    imageFilename.setText("No image selected", juce::dontSendNotification);
    imageFilename.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
    addAndMakeVisible(imageFilename);

    imageButton.onClick = [this] { onChooseImage(); };
    addAndMakeVisible(imageButton);

    for (auto* l : { &titleLabel, &artistLabel, &albumLabel, &genreLabel, &bpmLabel })
        addAndMakeVisible(l);

    bpmEditor.setInputRestrictions(6, "0123456789.");
    bpmEditor.setTextToShowWhenEmpty("Detected from the file, or type it in", juce::Colours::grey);

    for (auto* e : { &titleEditor, &artistEditor, &albumEditor, &genreEditor, &bpmEditor })
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

    // separators and the IMAGE header line up with resized()
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(separator1Y, (float)kPad, (float)(getWidth() - kPad));
    g.drawHorizontalLine(separator2Y, (float)kPad, (float)(getWidth() - kPad));

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("IMAGE", kPad, imageHeaderY, w, 16, juce::Justification::centredLeft);
}

void AddSongDialog::resized()
{
    auto area = getLocalBounds().reduced(kPad);

    area.removeFromTop(42); // title
    area.removeFromTop(22); // "AUDIO FILE" header

    chooseFileButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(6);
    fileNameLabel.setBounds(area.removeFromTop(kLineH));
    area.removeFromTop(10);
    separator1Y = area.getY();
    area.removeFromTop(1 + 10);

    imageHeaderY = area.getY();
    area.removeFromTop(22); // "IMAGE" header
    imageButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(6);
    imageFilename.setBounds(area.removeFromTop(kLineH));
    area.removeFromTop(10);
    separator2Y = area.getY();
    area.removeFromTop(1 + 10);

    auto field = [&](juce::Label& lbl, juce::TextEditor& ed)
    {
        lbl.setBounds(area.removeFromTop(18));
        ed.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
    };

    field(titleLabel,  titleEditor);
    field(artistLabel, artistEditor);
    field(albumLabel,  albumEditor);
    field(genreLabel,  genreEditor);
    field(bpmLabel,    bpmEditor);

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

void AddSongDialog::onChooseImage()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a song image",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto result = fc.getResult();
            if (! result.existsAsFile())
                return;

            const auto image = juce::ImageFileFormat::loadFrom(result);
            if (image.isNull())
            {
                imageFilename.setText("That file isn't an image we can open", juce::dontSendNotification);
                imageFilename.setColour(juce::Label::textColourId, juce::Colours::red.brighter(0.3f));
                return;
            }

            selectedImage     = image;
            selectedImageFile = result; // library copies it when the song is added
            imageFilename.setText(result.getFileName(), juce::dontSendNotification);
            imageFilename.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
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
        bpmEditor.setText(formatSongBpm(details.bpm));
    }

    // no bpm tag so work it out from the audio, takes about a second so it runs in the background
    if (bpmEditor.isEmpty())
    {
        bpmEditor.setTextToShowWhenEmpty("Detecting...", juce::Colours::grey);
        juce::Component::SafePointer<AddSongDialog> safeThis(this);
        SongAnalyser::Service::getInstance()->request(file.getFullPathName(),
            [safeThis, file](std::shared_ptr<const SongAnalysis> result)
            {
                // dialog closed or a different file was picked meanwhile
                if (safeThis == nullptr || safeThis->selectedFile != file)
                    return;

                safeThis->bpmEditor.setTextToShowWhenEmpty("Detected from the file, or type it in",
                                                           juce::Colours::grey);
                // dont overwrite something the user typed while waiting
                if (result != nullptr && safeThis->bpmEditor.isEmpty())
                    safeThis->bpmEditor.setText(formatSongBpm(result->songBpm));
                safeThis->bpmEditor.repaint();
            });
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
    s.genre     = genreEditor.getText().trim();
    s.image     = selectedImage;
    s.imageFile = selectedImageFile;
    s.filePath = selectedFile.existsAsFile() ? selectedFile.getFullPathName() : juce::String{}; //saves file full location jasen edit
    s.dateAdded = juce::Time::getCurrentTime();
    s.durationSeconds = selectedDuration;
    s.bpm       = parseSongBpm(bpmEditor.getText());

    if (onCreated) onCreated(std::move(s));

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(1);
}
