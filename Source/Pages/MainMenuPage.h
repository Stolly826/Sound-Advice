#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Landing / home screen. MainComponent owns all navigation state;
// these callbacks let the buttons here trigger page switches.
class MainMenuPage : public juce::Component {
public:
    std::function<void()> onPlaylistsClicked;
    std::function<void()> onSongsClicked;
    std::function<void()> onTransitionsClicked;

    MainMenuPage();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label      titleLabel;
    juce::TextButton playlistsButton{ "Playlists" };
    juce::TextButton songsButton{ "Songs" };
    juce::TextButton transitionsButton{ "Transitions" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainMenuPage)
};
