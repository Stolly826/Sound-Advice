#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundAdviceLookAndFeel.h"
#include "AudioEngine.h"
#include "LocalTranscriber.h"
#include "SongData.h"
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
                      public juce::DragAndDropContainer, // lets songs be dragged between parts of the window
                      private juce::Timer                // saves song settings once the sliders settle
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

    //every songs speed, volume, gain and eq, kept in settings.json
    std::map<juce::String, SongSettings> songSettings;

    //markers last placed on the mixing page for each pair of songs, kept in mixes.json
    std::map<juce::String, Transition> mixMarkers;

    // mini player along the bottom of the window
    juce::Label miniTrackLabel;
    juce::TextButton miniPlaybackButton { "Play" };
    juce::Slider miniProgressSlider;
    juce::Label miniTimeLabel;
    Song currentSong;
    bool hasCurrentSong = false;

    // the mini player refreshes on its own timer, the inherited Timer is the settings save
    struct MiniPlayerTimer : public juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { if (onTick) onTick(); }
    };
    MiniPlayerTimer miniPlayerTimer;

    // local whisper transcription
    juce::File modelPathStorageFile;
    std::shared_ptr<juce::FileChooser> modelChooser;

    void showPage(juce::Component& page);
    void showPlaylistDetail(Playlist* playlist);
    void showMixingPage(Playlist* playlist, int songIndex);
    bool playSong(const Song& song, const SongSettings& settings,
                  float fadeInStart = -1.0f, float fadeInEnd = -1.0f,
                  float fadeOutStart = -1.0f, float fadeOutEnd = -1.0f);
    void updateMiniPlayer();
    void requestTranscription(const Song& song,
                              std::function<void(juce::Result, juce::String)> completion);
    void startTranscription(const Song& song,
                            const juce::File& modelFile,
                            std::function<void(juce::Result, juce::String)> completion);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
