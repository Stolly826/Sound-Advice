#include "Sidebar.h"

Sidebar::Sidebar() {
    appNameLabel.setText("MixForge", juce::dontSendNotification);
    appNameLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    appNameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(appNameLabel);

    backButton.onClick = [this] { if (onBackClicked) onBackClicked(); };
    addAndMakeVisible(backButton);
}

void Sidebar::setPageContent(juce::Component* content) {
    if (pageContent != nullptr)
        removeChildComponent(pageContent);

    pageContent = content;

    if (pageContent != nullptr) {
        addAndMakeVisible(pageContent);
        resized();
    }
}

void Sidebar::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.25f));

    // Right-edge separator
    g.setColour(juce::Colours::grey.withAlpha(0.4f));
    g.drawVerticalLine(getWidth() - 1, 0.0f, (float)getHeight());
}

void Sidebar::resized() {
    auto area = getLocalBounds().reduced(12, 16);

    appNameLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(8);
    backButton.setBounds(area.removeFromTop(36));
    area.removeFromTop(12);

    if (pageContent != nullptr)
        pageContent->setBounds(area);
}
