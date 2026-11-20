#include "NewPlaylistDialog.h"

static constexpr int kPad  = 20;
static constexpr int kSepH = 10; // gap on each side of a separator line

NewPlaylistDialog::NewPlaylistDialog(const juce::StringArray& existingGroups, const Playlist* toEdit)
    : editing(toEdit != nullptr)
{
    nameEditor.setReturnKeyStartsNewLine(false);
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(nameEditor);

    // editable so a new group can be typed in
    groupBox.setEditableText(true);
    groupBox.addItemList(existingGroups, 1);
    groupBox.setTextWhenNothingSelected("No group");
    addAndMakeVisible(groupLabel);
    addAndMakeVisible(groupBox);

    imageFilename.setText("No image selected", juce::dontSendNotification);
    imageFilename.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
    imageButton.onClick = [this] { onImageClicked(); };
    addAndMakeVisible(imageButton);
    addAndMakeVisible(imageFilename);

    descEditor.setMultiLine(true, true);
    descEditor.setReturnKeyStartsNewLine(true);
    addAndMakeVisible(descLabel);
    addAndMakeVisible(descEditor);

    createButton.onClick = [this] { onCreateClicked(); };
    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(createButton);
    addAndMakeVisible(cancelButton);

    // edit mode fill in whats already there
    if (toEdit != nullptr)
    {
        nameEditor.setText(toEdit->name, false);
        groupBox.setText(toEdit->group, juce::dontSendNotification);
        descEditor.setText(toEdit->description, false);
        selectedImage     = toEdit->image;
        selectedImageFile = toEdit->imageFile;
        if (! selectedImage.isNull())
            imageFilename.setText("Current image", juce::dontSendNotification);
        createButton.setButtonText("Save");
    }

    setSize(kWidth, kHeight);
}

void NewPlaylistDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    const int w = getWidth() - kPad * 2;

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(editing ? "Edit Playlist" : "New Playlist", kPad, kPad, w, 38, juce::Justification::centredLeft);

    // Separator after name and group fields
    // kPad + title(42) + 2 x (label(18) + editor(28) + gap(8)) + sepGap(kSepH)
    const int sep1Y = kPad + 42 + (18 + 28 + 8) * 2 + kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(sep1Y, (float)kPad, (float)(getWidth() - kPad));

    // IMAGE section header sep1Y + 1 + kSepH lines up with resized()
    const int imageHeaderY = sep1Y + 1 + kSepH;
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("IMAGE", kPad, imageHeaderY, w, 16, juce::Justification::centredLeft);

    // Separator after image section
    // imageHeaderY + header(16) + gap(6) + button(40) + gap(6) + filename(22) + sepGap
    const int sep2Y = imageHeaderY + 16 + 6 + 40 + 6 + 22 + kSepH;
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(sep2Y, (float)kPad, (float)(getWidth() - kPad));
}

void NewPlaylistDialog::resized()
{
    auto area = getLocalBounds().reduced(kPad);

    area.removeFromTop(42); // title

    // Name field
    nameLabel.setBounds(area.removeFromTop(18));
    nameEditor.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    // Group field
    groupLabel.setBounds(area.removeFromTop(18));
    groupBox.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    // Separator 1 space
    area.removeFromTop(kSepH + 1 + kSepH);

    // IMAGE section header (drawn in paint, just skip the space)
    area.removeFromTop(16);
    area.removeFromTop(6);

    // Image button + filename
    imageButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(6);
    imageFilename.setBounds(area.removeFromTop(22));

    // Separator 2 space
    area.removeFromTop(kSepH + 1 + kSepH);

    // Description
    descLabel.setBounds(area.removeFromTop(18));
    descEditor.setBounds(area.removeFromTop(52));
    area.removeFromTop(8);

    // Buttons
    area.removeFromTop(4);
    auto btnRow = area.removeFromTop(36);
    cancelButton.setBounds(btnRow.removeFromRight(90));
    btnRow.removeFromRight(8);
    createButton.setBounds(btnRow.removeFromRight(90));
}

void NewPlaylistDialog::onImageClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a playlist image",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto result = fc.getResult();
            if (result.existsAsFile())
            {
                selectedImage     = juce::ImageFileFormat::loadFrom(result);
                selectedImageFile = result; // library copies it when saved
                imageFilename.setText(result.getFileName(), juce::dontSendNotification);
                imageFilename.setColour(juce::Label::textColourId,
                                        juce::Colours::white.withAlpha(0.85f));
            }
        });
}

void NewPlaylistDialog::onCreateClicked()
{
    const auto name = nameEditor.getText().trim();
    if (name.isEmpty())
    {
        nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::red);
        nameEditor.repaint();
        return;
    }

    Playlist p;
    p.name        = name;
    p.description = descEditor.getText().trim();
    p.group       = groupBox.getText().trim();
    p.image       = selectedImage;
    p.imageFile   = selectedImageFile;
    p.lastUpdated = juce::Time::getCurrentTime();

    if (onCreated) onCreated(std::move(p));

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(1);
}
