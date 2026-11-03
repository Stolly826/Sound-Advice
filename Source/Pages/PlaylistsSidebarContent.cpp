#include "PlaylistsSidebarContent.h"

void PlaylistsSidebarContent::updatePlaylists(const juce::Array<Playlist>& all)
{
    items = all;
    hoveredIndex = -1;
    repaint();
}

int PlaylistsSidebarContent::visibleCount() const
{
    const int maxH = (int)(getHeight() * kFillRatio);
    return juce::jmin(maxH / kItemHeight, items.size());
}

int PlaylistsSidebarContent::getItemIndexAt(juce::Point<int> pos) const
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        if (juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight - 4).contains(pos))
            return i;
        y += kItemHeight;
    }
    return -1;
}

void PlaylistsSidebarContent::paint(juce::Graphics& g)
{
    int y = 0;
    for (int i = 0; i < visibleCount(); ++i)
    {
        auto row = juce::Rectangle<int>(4, y, getWidth() - 8, kItemHeight - 4);

        g.setColour(juce::Colours::white.withAlpha(i == hoveredIndex ? 0.14f : 0.05f));
        g.fillRoundedRectangle(row.toFloat(), 4.0f);

        g.setColour(juce::Colours::white.withAlpha(i == hoveredIndex ? 1.0f : 0.80f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(items[i].name, row.reduced(8, 0), juce::Justification::centredLeft, true);

        y += kItemHeight;
    }
}

void PlaylistsSidebarContent::mouseMove(const juce::MouseEvent& e)
{
    const int idx = getItemIndexAt(e.getPosition());
    setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
    if (idx != hoveredIndex) { hoveredIndex = idx; repaint(); }
}

void PlaylistsSidebarContent::mouseExit(const juce::MouseEvent&)
{
    hoveredIndex = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void PlaylistsSidebarContent::mouseUp(const juce::MouseEvent& e)
{
    const int idx = getItemIndexAt(e.getPosition());
    if (idx >= 0 && onItemClicked)
        onItemClicked(idx);
}
