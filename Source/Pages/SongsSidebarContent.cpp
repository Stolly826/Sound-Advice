#include "SongsSidebarContent.h"

void SongsSidebarContent::updateSongs(const juce::Array<Song>& songs)
{
    items = songs;
    hoveredIndex = -1;
    repaint();
}

int SongsSidebarContent::visibleCount() const
{
    const int maxH = (int)(getHeight() * kFillRatio);
    return juce::jmin(maxH / kItemHeight, items.size());
}

int SongsSidebarContent::getItemIndexAt(juce::Point<int> pos) const
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        if (juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight).contains(pos))
            return i;
        y += kItemHeight + 4; // matches paint loop
    }
    return -1;
}

void SongsSidebarContent::paint(juce::Graphics& g)
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        auto row = juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight);

        g.setColour(juce::Colours::white.withAlpha(i == hoveredIndex ? 0.14f : 0.05f));
        g.fillRoundedRectangle(row.toFloat(), 4.0f);

        const int textX     = row.getX() + 8;
        const int textW     = row.getWidth() - 16;
        const int titleY    = y + 5;
        const int titleH    = 16;
        const int artistGap = 2;

        g.setColour(juce::Colours::white.withAlpha(i == hoveredIndex ? 1.0f : 0.80f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(items[i].title,
                   juce::Rectangle<int>(textX, titleY, textW, titleH),
                   juce::Justification::centredLeft, true);

        if (items[i].artist.isNotEmpty())
        {
            g.setColour(juce::Colours::white.withAlpha(i == hoveredIndex ? 0.65f : 0.45f));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(items[i].artist,
                       juce::Rectangle<int>(textX, titleY + titleH + artistGap, textW, 13),
                       juce::Justification::centredLeft, true);
        }

        y += kItemHeight + 4;
    }
}

void SongsSidebarContent::mouseMove(const juce::MouseEvent& e)
{
    const int idx = getItemIndexAt(e.getPosition());
    setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
    if (idx != hoveredIndex) { hoveredIndex = idx; repaint(); }
}

void SongsSidebarContent::mouseExit(const juce::MouseEvent&)
{
    hoveredIndex = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void SongsSidebarContent::mouseUp(const juce::MouseEvent& e)
{
    // a drag isnt a click
    if (e.mouseWasDraggedSinceMouseDown()) return;

    const int idx = getItemIndexAt(e.getPosition());
    if (idx >= 0 && onItemClicked)
        onItemClicked(idx);
}

void SongsSidebarContent::mouseDrag(const juce::MouseEvent& e)
{
    if (! dragEnabled || e.getDistanceFromDragStart() < 6) return;

    // item where the press started
    const int idx = getItemIndexAt(e.getMouseDownPosition());
    if (idx < 0) return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (container == nullptr || container->isDragAndDropActive()) return;

    // picture of just that item follows the mouse
    const auto itemArea = juce::Rectangle<int>(4, idx * (kItemHeight + 4), getWidth() - 8, kItemHeight);
    const auto offset   = itemArea.getPosition() - e.getMouseDownPosition();
    container->startDragging("library:" + juce::String(idx), this,
                             juce::ScaledImage(createComponentSnapshot(itemArea)), false, &offset);
}
