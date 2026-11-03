#include "NewPlaylistRow.h"

NewPlaylistRow::NewPlaylistRow(const juce::String& label) : rowLabel(label)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NewPlaylistRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.05f);
    if (hovered) bg = bg.brighter(0.12f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Dashed-style border (drawn as a slightly transparent line)
    g.setColour(juce::Colours::grey.withAlpha(hovered ? 0.65f : 0.35f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

    // "+" icon centred in the icon column
    const float cx = 12.0f + 22.0f; // icon column centre-x
    const float cy = (float)getHeight() * 0.5f;
    const float r  = 10.0f;
    auto iconColour = juce::Colours::grey.brighter(hovered ? 0.8f : 0.4f);
    g.setColour(iconColour);
    g.fillRect(cx - r, cy - 2.0f, r * 2.0f, 4.0f);
    g.fillRect(cx - 2.0f, cy - r, 4.0f, r * 2.0f);

    // Label
    g.setFont(juce::FontOptions(15.0f));
    g.setColour(iconColour);
    g.drawText(rowLabel,
               (int)(cx + r + 14), 0,
               (int)(bounds.getRight() - cx - r - 14), getHeight(),
               juce::Justification::centredLeft);
}

void NewPlaylistRow::mouseUp(const juce::MouseEvent&)
{
    if (onClick) onClick();
}

void NewPlaylistRow::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void NewPlaylistRow::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}
