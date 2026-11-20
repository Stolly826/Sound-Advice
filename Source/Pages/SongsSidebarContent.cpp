#include "SongsSidebarContent.h"

void SongsSidebarContent::updateSongs(const juce::Array<Song>& songs)
{
    items = songs;
    hoveredIndex = -1;
    firstVisibleIndex = 0;
    repaint();
}

int SongsSidebarContent::visibleCount() const
{
    return juce::jmin(getHeight() / (kItemHeight + 4), items.size());
}

int SongsSidebarContent::getItemIndexAt(juce::Point<int> pos) const
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        if (juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight).contains(pos))
            return firstVisibleIndex + i;
        y += kItemHeight + 4; // matches paint loop
    }
    return -1;
}

void SongsSidebarContent::paint(juce::Graphics& g)
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        const int index = firstVisibleIndex + i;
        auto row = juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight);

        g.setColour(juce::Colours::white.withAlpha(index == hoveredIndex ? 0.14f : 0.05f));
        g.fillRoundedRectangle(row.toFloat(), 4.0f);

        const int textX     = row.getX() + 8;
        const int textW     = row.getWidth() - 16;
        const int titleY    = y + 5;
        const int titleH    = 16;
        const int artistGap = 2;

        g.setColour(juce::Colours::white.withAlpha(index == hoveredIndex ? 1.0f : 0.80f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(items[index].title,
                   juce::Rectangle<int>(textX, titleY, textW, titleH),
                   juce::Justification::centredLeft, true);

        if (items[index].artist.isNotEmpty())
        {
            g.setColour(juce::Colours::white.withAlpha(index == hoveredIndex ? 0.65f : 0.45f));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(items[index].artist,
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

void SongsSidebarContent::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) return; // sideways scroll, nothing to do

    const int maxFirstIndex = juce::jmax(0, items.size() - visibleCount());
    const int steps = juce::jmax(1, juce::roundToInt(juce::jmax(wheel.deltaY, -wheel.deltaY) * 3.0f));
    const int direction = wheel.deltaY > 0.0f ? -1 : 1;
    firstVisibleIndex = juce::jlimit(0, maxFirstIndex,
        firstVisibleIndex + direction * steps);
    repaint();
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
    const int  rowY     = (idx - firstVisibleIndex) * (kItemHeight + 4); // scrolled position
    const auto itemArea = juce::Rectangle<int>(4, rowY, getWidth() - 8, kItemHeight);
    const auto offset   = itemArea.getPosition() - e.getMouseDownPosition();
    container->startDragging("library:" + juce::String(idx), this,
                             juce::ScaledImage(createComponentSnapshot(itemArea)), false, &offset);
}
