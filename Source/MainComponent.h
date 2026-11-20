#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundAdviceLookAndFeel.h"
#include "AudioEngine.h"
#include "LocalTranscriber.h"
#include "Sidebar.h"
#include "Pages/MainMenuPage.h"
#include "Pages/PlaylistsPage.h"
#include "Pages/PlaylistDetailPage.h"
#include "Pages/SongsPage.h"
#include "Pages/TransitionsPage.h"
#include "Pages/MixingPage.h"
#include <memory>

// Root UI component. Owns the sidebar and all pages.
// Exactly one page is visible at a time; showPage() handles switching
// and shows/hides the sidebar accordingly.
class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SoundAdviceLookAndFeel lookAndFeel; // destroyed last

    AudioEngine audioEngine; //the audio engine thats shared by the front end

    Sidebar            sidebar;
    MainMenuPage       mainMenuPage;
    PlaylistsPage      playlistsPage;
    PlaylistDetailPage playlistDetailPage;
    SongsPage          songsPage;
    TransitionsPage    transitionsPage;
    MixingPage         mixingPage;

    juce::Label miniTrackLabel;
    juce::TextButton miniPlaybackButton { "Play" };
    juce::Slider miniProgressSlider;
    juce::Label miniTimeLabel;
    juce::File modelPathStorageFile;
    std::shared_ptr<juce::FileChooser> modelChooser;
    Song currentSong;
    bool hasCurrentSong = false;

    void showPage(juce::Component& page);
    void showPlaylistDetail(Playlist* playlist);
    void showMixingPage(Playlist* playlist, int songIndex);
    bool playSong(const Song& song);
    void requestTranscription(const Song& song,
                              std::function<void(juce::Result, juce::String)> completion);
    void startTranscription(const Song& song,
                            const juce::File& modelFile,
                            std::function<void(juce::Result, juce::String)> completion);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
