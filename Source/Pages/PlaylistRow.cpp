#include "PlaylistRow.h"

// ─── TrashButton ──────────────────────────────────────────────────────────────

void PlaylistRow::TrashButton::paintButton(juce::Graphics& g, bool highlighted, bool /*down*/)
{
    auto b = getLocalBounds().toFloat().reduced(5.0f);
    g.setColour(highlighted ? juce::Colours::red.brighter(0.3f) : juce::Colours::grey);

    const float cx  = b.getCentreX();
    const float w   = b.getWidth();
    const float top = b.getY();
    const float h   = b.getHeight();

    // Handle (small bar above lid)
    g.fillRoundedRectangle(cx - w * 0.15f, top, w * 0.3f, h * 0.14f, 1.5f);
    // Lid
    g.fillRoundedRectangle(cx - w * 0.42f, top + h * 0.16f, w * 0.84f, h * 0.16f, 1.5f);
    // Body
    g.fillRoundedRectangle(cx - w * 0.35f, top + h * 0.34f, w * 0.70f, h * 0.60f, 2.0f);
}

// ─── PlaylistRow ──────────────────────────────────────────────────────────────

static juce::String formatDate(juce::Time t)
{
    return t.toString(true, false, false, false); // date only
}

PlaylistRow::PlaylistRow(const Playlist& data) : playlist(data)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    nameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    nameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(nameLabel);

    dateLabel.setFont(juce::FontOptions(12.0f));
    dateLabel.setJustificationType(juce::Justification::centredRight);
    dateLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(dateLabel);

    trashButton.onClick = [this] { if (onDeleteClicked) onDeleteClicked(); };
    addAndMakeVisible(trashButton);

    update(data);
}

void PlaylistRow::update(const Playlist& data)
{
    playlist = data;
    nameLabel.setText(data.name, juce::dontSendNotification);
    dateLabel.setText(formatDate(data.lastUpdated), juce::dontSendNotification);
}

void PlaylistRow::paint(juce::Graphics& g)
{
    // Rounded rect background + border
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.1f);
    if (hovered) bg = bg.brighter(0.12f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Icon area (left)
    const int iconSize = 44;
    const int iconX    = 12;
    const int iconY    = (getHeight() - iconSize) / 2;

    if (!playlist.image.isNull())
    {
        // Clip to a rounded rect before drawing
        juce::Path clip;
        clip.addRoundedRectangle((float)iconX, (float)iconY, (float)iconSize, (float)iconSize, 4.0f);
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImageWithin(playlist.image, iconX, iconY, iconSize, iconSize,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        g.restoreState();
    }
    else
    {
        // Music note placeholder
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.fillRoundedRectangle((float)iconX, (float)iconY, (float)iconSize, (float)iconSize, 4.0f);
        drawMusicNote(g, { (float)iconX, (float)iconY, (float)iconSize, (float)iconSize },
                      juce::Colours::white.withAlpha(0.6f));
    }
}

void PlaylistRow::resized()
{
    const int iconSize = 44;
    const int iconX    = 12;
    const int trashW   = 32;
    const int dateW    = 100;
    const int pad      = 8;

    auto area = getLocalBounds().reduced(pad);
    area.removeFromLeft(iconX + iconSize + pad); // skip icon column

    trashButton.setBounds(area.removeFromRight(trashW).withSizeKeepingCentre(trashW, trashW));
    area.removeFromRight(pad);
    dateLabel.setBounds(area.removeFromRight(dateW));
    area.removeFromRight(pad);
    nameLabel.setBounds(area);
}

void PlaylistRow::mouseUp(const juce::MouseEvent& e)
{
    // Ignore clicks that land on the trash button (it handles those itself).
    if (!trashButton.getBounds().contains(e.getPosition()))
        if (onClicked) onClicked();
}

void PlaylistRow::mouseEnter(const juce::MouseEvent&) { hovered = true;  repaint(); }
void PlaylistRow::mouseExit(const juce::MouseEvent&)  { hovered = false; repaint(); }

void PlaylistRow::drawMusicNote(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);

    const float stemX  = area.getCentreX() + area.getWidth() * 0.08f;
    const float top    = area.getY() + area.getHeight() * 0.12f;
    const float stemH  = area.getHeight() * 0.52f;
    const float noteRW = area.getWidth()  * 0.22f;
    const float noteRH = area.getHeight() * 0.14f;
    const float noteY  = top + stemH;

    g.fillRect(stemX - 1.5f, top, 3.0f, stemH);                               // stem
    g.fillEllipse(stemX - noteRW, noteY - noteRH * 0.7f, noteRW * 2.0f, noteRH * 1.8f); // head
    g.fillRect(stemX - 1.5f, top, area.getWidth() * 0.28f, 3.0f);            // flag
}
