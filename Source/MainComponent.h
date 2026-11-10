#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundAdviceLookAndFeel.h"
#include "AudioEngine.h"
#include "SongData.h"
#include "Sidebar.h"
#include "Pages/MainMenuPage.h"
#include "Pages/PlaylistsPage.h"
#include "Pages/PlaylistDetailPage.h"
#include "Pages/SongsPage.h"
#include "Pages/TransitionsPage.h"
#include "Pages/MixingPage.h"

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

    void showPage(juce::Component& page);
    void showPlaylistDetail(Playlist* playlist);
    void showMixingPage(Playlist* playlist, int songIndex);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
