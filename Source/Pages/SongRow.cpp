#include "SongRow.h"

// ─── TrashButton ─────────────────────────────────────────────────────────────

void SongRow::TrashButton::paintButton(juce::Graphics& g, bool highlighted, bool /*down*/)
{
    auto b = getLocalBounds().toFloat().reduced(5.0f);
    g.setColour(highlighted ? juce::Colours::red.brighter(0.3f) : juce::Colours::grey);
    const float cx = b.getCentreX(), w = b.getWidth(), top = b.getY(), h = b.getHeight();
    g.fillRoundedRectangle(cx - w * 0.15f, top,            w * 0.30f, h * 0.14f, 1.5f); // handle
    g.fillRoundedRectangle(cx - w * 0.42f, top + h * 0.16f, w * 0.84f, h * 0.16f, 1.5f); // lid
    g.fillRoundedRectangle(cx - w * 0.35f, top + h * 0.34f, w * 0.70f, h * 0.60f, 2.0f); // body
}

// ─── SongRow ─────────────────────────────────────────────────────────────────

SongRow::SongRow(const Song& data) : song(data)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);

    explicitLabel.setText("EXPLICIT", juce::dontSendNotification);
    explicitLabel.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    explicitLabel.setJustificationType(juce::Justification::centred);
    explicitLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    explicitLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkred);
    explicitLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(explicitLabel);

    for (auto* l : { &artistLabel, &albumLabel, &dateLabel, &durationLabel })
    {
        l->setFont(juce::FontOptions(12.0f));
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    }

    durationLabel.setJustificationType(juce::Justification::centredRight);

    trashButton.onClick = [this] { if (onDeleteClicked) onDeleteClicked(); };
    addAndMakeVisible(trashButton);

    update(data);
}

void SongRow::update(const Song& data)
{
    song = data;
    titleLabel.setText(data.title, juce::dontSendNotification);
    explicitLabel.setVisible(data.isExplicit());
    artistLabel.setText(data.artist, juce::dontSendNotification);
    albumLabel.setText(data.album, juce::dontSendNotification);
    dateLabel.setText(data.dateAdded.toString(true, false, false, false),
                      juce::dontSendNotification);
    durationLabel.setText(formatSongDuration(data.durationSeconds),
                          juce::dontSendNotification);
}

void SongRow::setStackedLayout(bool shouldStack)
{
    stacked = shouldStack;

    titleLabel.setFont(juce::FontOptions(stacked ? 16.0f : 14.0f, juce::Font::bold));
    artistLabel.setFont(juce::FontOptions(stacked ? 14.0f : 12.0f));
    if (stacked)
        albumLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
    else
        albumLabel.removeColour(juce::Label::textColourId);

    // stacked labels sit tight together so drop the default label padding
    const juce::BorderSize<int> border = stacked ? juce::BorderSize<int>(0, 5, 0, 5) : juce::BorderSize<int>(1, 5, 1, 5);
    titleLabel.setBorderSize(border);
    albumLabel.setBorderSize(border);
    titleLabel.setJustificationType(stacked ? juce::Justification::bottomLeft : juce::Justification::centredLeft);
    albumLabel.setJustificationType(stacked ? juce::Justification::topLeft    : juce::Justification::centredLeft);

    trashButton.setVisible(! stacked); // the picker has nothing to delete
    resized();
}

void SongRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.1f);
    if (hovered) bg = bg.brighter(0.12f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    const int iconSize = 32, iconX = 10;
    const int iconY    = (getHeight() - iconSize) / 2;
    if (! song.image.isNull())
    {
        // images draw with the current colour's alpha so go back to fully opaque first
        juce::Path clip;
        clip.addRoundedRectangle((float)iconX, (float)iconY, (float)iconSize, (float)iconSize, 4.0f);
        g.saveState();
        g.reduceClipRegion(clip);
        g.setOpacity(1.0f);
        g.drawImageWithin(song.image, iconX, iconY, iconSize, iconSize,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        g.restoreState();
        return;
    }

    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    g.fillRoundedRectangle((float)iconX, (float)iconY, (float)iconSize, (float)iconSize, 4.0f);
    drawMusicNote(g, { (float)iconX, (float)iconY, (float)iconSize, (float)iconSize },
                  juce::Colours::white.withAlpha(0.5f));
}

void SongRow::resized()
{
    const int iconX = 10, iconSize = 32;
    const int pad = 6;
    const int trashW = 28, durationW = 52, dateW = 80, albumW = 100, artistW = 110;

    auto area = getLocalBounds().reduced(pad);
    area.removeFromLeft(iconX + iconSize + pad);

    if (stacked)
    {
        // [title over album | artist | date | duration] title gets the most room
        durationLabel.setBounds(area.removeFromRight(durationW));
        area.removeFromRight(pad);
        dateLabel.setBounds(area.removeFromRight(dateW));
        area.removeFromRight(pad);
        artistLabel.setBounds(area.removeFromRight(juce::jlimit(110, 200, area.getWidth() * 2 / 5)));
        area.removeFromRight(pad);

        auto titleArea = area;
        auto albumArea = titleArea.removeFromBottom(song.album.isNotEmpty() ? titleArea.getHeight() * 2 / 5 : 0);
        if (song.album.isEmpty())
            titleLabel.setJustificationType(juce::Justification::centredLeft);
        if (song.isExplicit())
        {
            explicitLabel.setBounds(titleArea.removeFromLeft(60).withSizeKeepingCentre(60, 16));
            titleArea.removeFromLeft(6);
        }
        titleLabel.setBounds(titleArea);
        albumLabel.setBounds(albumArea);
        return;
    }

    trashButton.setBounds(area.removeFromRight(trashW).withSizeKeepingCentre(trashW, trashW));
    area.removeFromRight(pad);
    durationLabel.setBounds(area.removeFromRight(durationW));
    area.removeFromRight(pad);
    dateLabel.setBounds(area.removeFromRight(dateW));
    area.removeFromRight(pad);
    albumLabel.setBounds(area.removeFromRight(albumW));
    area.removeFromRight(pad);
    artistLabel.setBounds(area.removeFromRight(artistW));
    area.removeFromRight(pad);
    if (song.isExplicit())
    {
        explicitLabel.setBounds(area.removeFromLeft(60));
        area.removeFromLeft(6);
    }
    titleLabel.setBounds(area); // flex — whatever remains
}

void SongRow::mouseUp(const juce::MouseEvent& e)
{
    // a drag isnt a click
    if (e.mouseWasDraggedSinceMouseDown()) return;

    if (!trashButton.getBounds().contains(e.getPosition()))
        if (onClicked) onClicked();
}

void SongRow::mouseDrag(const juce::MouseEvent& e)
{
    if (dragDescription.isVoid() || e.getDistanceFromDragStart() < 6) return;

    // main component is the drag container it shows a picture of the row under the mouse
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (! container->isDragAndDropActive())
            container->startDragging(dragDescription, this);
}

void SongRow::mouseEnter(const juce::MouseEvent&) { hovered = true;  repaint(); }
void SongRow::mouseExit(const juce::MouseEvent&)  { hovered = false; repaint(); }

void SongRow::drawMusicNote(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    const float stemX = area.getCentreX() + area.getWidth() * 0.08f;
    const float top   = area.getY() + area.getHeight() * 0.12f;
    const float stemH = area.getHeight() * 0.52f;
    const float nrw   = area.getWidth() * 0.22f;
    const float nrh   = area.getHeight() * 0.14f;
    g.fillRect(stemX - 1.5f, top, 3.0f, stemH);
    g.fillEllipse(stemX - nrw, top + stemH - nrh * 0.7f, nrw * 2.0f, nrh * 1.8f);
    g.fillRect(stemX - 1.5f, top, area.getWidth() * 0.28f, 3.0f);
}
