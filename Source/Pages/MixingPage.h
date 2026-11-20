#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "Transition.h"
#include "SongChartComponent.h"
#include "SongsSidebarContent.h"
#include "../SongSettings.h"

class MixingPage : public juce::Component,
                   private juce::Timer // moves the playhead
{
public:
    std::function<juce::Array<Transition>(juce::String, juce::String)> getTransitionsForPair;

    // markers last placed for a pair of songs (all -1 if none), and saving them when they change
    std::function<Transition(const Song&, const Song&)>                     getMixMarkers;
    std::function<void(const Song&, const Song&, const Transition&)>        onMixMarkersChanged;

    // song, its settings, then fade in start/end and fade out start/end (0 to 1, -1 for none)
    std::function<bool(const Song&, const SongSettings&, float, float, float, float)> onPlaySong;
    std::function<bool(const Song&, float, float)> onCueNext; // next song, its fade in start/end
    std::function<void()> onPause;
    std::function<void()> onResume;
    std::function<void()> onStop;

    // scratching: grab the record, move it (seconds from where it was grabbed), let go
    std::function<void()>       onScratchStart;
    std::function<void(double)> onScratchMove;
    std::function<void()>       onScratchEnd;

    // FRONTEND/BACKEND INTEGRATION: Each song keeps its own slider positions
    std::function<SongSettings(const Song&)> getSongSettings;
    // last argument is true when the slider was moved for the next song (chart 2)
    std::function<void(const Song&, const SongSettings&, bool)> onSongSettingsChanged;

    // playback info for the playhead jump to and skip
    std::function<double()>       getPlaybackPosition; // seconds
    std::function<double()>       getPlaybackLength;   // seconds
    std::function<bool()>         isPlaybackActive;
    std::function<bool()>         isPlaybackPaused;
    std::function<juce::String()> getLoadedFilePath;
    std::function<void(double)>   onSeek;              // seconds

    MixingPage();

    void setContent(Playlist* playlist, int songIndex);

    void paint  (juce::Graphics& g) override;
    void resized()                  override;
    void visibilityChanged()        override;

    juce::Component& getSidebarContent() { return sidebarContent; }

private:
    Playlist* currentPlaylist = nullptr;
    int       currentIndex    = 0;
    float     incomingFadeInStart = -1.0f;
    float     incomingFadeInEnd   = -1.0f;

    // Header
    juce::Label      titleLabel;
    juce::TextButton playButton; // play and pause in one, the icon shows what a click will do
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton recordButton;
    juce::TextButton recordingsButton  { "Recordings" };

    // Charts
    SongChartComponent chart1;
    SongChartComponent chart2;
    juce::TextButton   transitionPresetsButton { "Transition Presets" };

    // Which song the sliders below change: 0 = current song (chart 1), 1 = next song (chart 2)
    int              controlledDeck = 0;
    juce::Label      controllingLabel { {}, "Controlling:" };
    juce::TextButton deck1Button;
    juce::TextButton deck2Button;

    // Controls
    juce::Label  controlsHeader { {}, "CONTROLS" };
    juce::Label  volumeLabel    { {}, "Volume" };
    juce::Slider volumeSlider;
    juce::Label  gainLabel      { {}, "Gain" };
    juce::Slider gainSlider;
    juce::Label  pitchLabel     { {}, "Pitch" };
    juce::Slider pitchSlider;
    juce::Label  speedLabel     { {}, "Speed" };
    juce::Slider speedSlider;

    // EQ
    juce::Label  eqHeader       { {}, "EQ" };
    juce::Label  trebleLabel    { {}, "Treble" };
    juce::Slider trebleSlider;
    juce::Label  midsLabel      { {}, "Mids" };
    juce::Slider midsSlider;
    juce::Label  bassLabel      { {}, "Bass" };
    juce::Slider bassSlider;
    juce::Slider scratchSlider;
    bool scratching = false; // the scratch slider is being dragged

    // The sliders saved per song.
    juce::Array<juce::Slider*> songSliders;

    SongsSidebarContent sidebarContent;
    int sepY = 0;

    void setupSlider(juce::Slider& s, double min, double max, double defaultVal,
                     const juce::String& suffix, int decimals);
    void showTransitionPresets();
    void saveMixMarkers(); // remembers both charts' markers for this pair of songs

    const Song*  getCurrentSong() const;
    const Song*  getNextSong() const;
    const Song*  getControlledSong() const; // whichever song the sliders are set to
    void         setControlledDeck(int deck);
    void         updateDeckButtons();
    void         updatePlayButtonIcon();
    SongSettings getSliderSettings() const;
    void         loadSongSettings();
    bool         playCurrentSong(); // plays with this songs settings and transition fades

    // playback extras on top of the audio engine
    bool currentSongIsLoaded() const; // engine has this pages song loaded
    void resetPlayhead();
    void jumpTo(float position);
    void skipToNext();
    void skipToPrevious();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixingPage)
};
