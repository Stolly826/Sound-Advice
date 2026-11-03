#include "MainMenuPage.h"

MainMenuPage::MainMenuPage() {
    titleLabel.setText("MixForge", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(48.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);


    playlistsButton.onClick = [this] { if (onPlaylistsClicked)   onPlaylistsClicked(); };
    songsButton.onClick = [this] { if (onSongsClicked)       onSongsClicked(); };
    transitionsButton.onClick = [this] { if (onTransitionsClicked) onTransitionsClicked(); };

    addAndMakeVisible(playlistsButton);
    addAndMakeVisible(songsButton);
    addAndMakeVisible(transitionsButton);
}

void MainMenuPage::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainMenuPage::resized() {
    auto area = getLocalBounds().withSizeKeepingCentre(320, 300);

    titleLabel.setBounds(area.removeFromTop(60));
    area.removeFromTop(40);

    playlistsButton.setBounds(area.removeFromTop(55));
    area.removeFromTop(10);
    songsButton.setBounds(area.removeFromTop(55));
    area.removeFromTop(10);
    transitionsButton.setBounds(area.removeFromTop(55));
}
