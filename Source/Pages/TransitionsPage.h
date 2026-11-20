#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "Transition.h"
#include "SongChartComponent.h"
#include "../SongSettings.h"

// Two songs picked by hand, played through one transition.
// Unlike the mixing page the charts never swap: song 1 plays, fades into song 2,
// song 2 carries on in the bottom chart, and when it ends playback simply stops.
class TransitionsPage : public juce::Component,
                        private juce::Timer // moves the playheads and runs the transition
{
public:
    std::function<juce::Array<Song>()>                               getAllSongs;
    std::function<juce::Array<Transition>(juce::String, juce::String)> getTransitionsForPair;
    std::function<void(juce::String, Transition)>                    addTransitionToSong;
    std::function<void(juce::String, int, Transition)>               updateTransitionInSong;

    // song, its settings, fade in start/end and fade out start/end (0 to 1, -1 for none),
    // then the fade in curve and the fade out curve
    std::function<bool(const Song&, const SongSettings&, float, float, float, float,
                       FadeCurve, FadeCurve)> onPlaySong;
    // song 2, its fade in start/end and fade in curve
    std::function<bool(const Song&, float, float, FadeCurve)> onCueNext;
    std::function<void()> onPause;
    std::function<void()> onResume;
    std::function<void()> onStop;

    // scratching: grab the record, move it (seconds from where it was grabbed), let go
    std::function<void()>       onScratchStart;
    std::function<void(double)> onScratchMove;
    std::function<void()>       onScratchEnd;

    // each song keeps its own slider positions
    std::function<SongSettings(const Song&)> getSongSettings;
    // last argument is true when the song isn't the one on the main deck (it is fading in)
    std::function<void(const Song&, const SongSettings&, bool)> onSongSettingsChanged;

    // playback info for the playhead, jump to and the transition
    std::function<double()>       getPlaybackPosition; // seconds
    std::function<double()>       getPlaybackLength;   // seconds
    std::function<bool()>         isPlaybackActive;
    std::function<bool()>         isPlaybackPaused;
    std::function<juce::String()> getLoadedFilePath;
    std::function<void(double)>   onSeek;              // seconds

    TransitionsPage();
    void paint  (juce::Graphics& g) override;
    void resized()                  override;
    void visibilityChanged()        override;

private:
    // the songs picked into each chart
    Song song1, song2;
    bool hasSong1 = false, hasSong2 = false;

    // which chart is on the main deck: 0 = song 1, 1 = song 2 (after the handover)
    int activeDeck = 0;

    // Header
    juce::Label      titleLabel;
    juce::TextButton playButton; // play and pause in one, the icon shows what a click will do
    juce::TextButton stopButton { "Stop" };
    juce::TextButton saveTransitionButton { "Save Transition" };

    // Charts
    SongChartComponent chart1;
    SongChartComponent chart2;
    juce::TextButton   transitionPresetsButton { "Transition Presets" };
    juce::Label        presetNameLabel; // name of the preset in use, left of the button

    // Which song the sliders below change: 0 = song 1 (chart 1), 1 = song 2 (chart 2)
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
    juce::Label  eqHeader    { {}, "EQ" };
    juce::Label  trebleLabel { {}, "Treble" };
    juce::Slider trebleSlider;
    juce::Label  midsLabel   { {}, "Mids" };
    juce::Slider midsSlider;
    juce::Label  bassLabel   { {}, "Bass" };
    juce::Slider bassSlider;
    juce::Slider scratchSlider;
    bool scratching = false; // the scratch slider is being dragged

    // The sliders saved per song.
    juce::Array<juce::Slider*> songSliders;

    int          sepY                   = 0;
    int          loadedTransitionIndex  = -1;  // -1 = none loaded
    juce::String loadedTransitionName;

    void setupSlider(juce::Slider& s, double min, double max, double defaultVal,
                     const juce::String& suffix, int decimals);
    void showPickSong(int chartIndex);
    void showTransitionPresets();
    void onSaveClicked();
    void doSaveTransition(const juce::String& name);

    // playback
    const Song*  getSong(int deck) const;      // nullptr if that chart is empty
    const Song*  getControlledSong() const { return getSong(controlledDeck); }
    bool         activeSongIsLoaded() const;   // engine has the active chart's song loaded
    bool         isPlayingHere() const;        // playing one of this page's songs right now
    bool         playSong1();                  // song 1 with its fade out
    bool         playSong2(bool withFadeIn);   // song 2, takes over the deck cued under the fade out
    float        song2FadeInEnd() const;       // chart 2's fade in end, matched to the fade out
    void         jumpTo(int deck, float position);
    void         stopPlayback();               // back to the start of song 1
    void         resetPlayheads();
    void         updatePlayButtonIcon();
    void         timerCallback() override;

    // per-song sliders
    void         setControlledDeck(int deck);
    void         updateDeckButtons();
    SongSettings getSliderSettings() const;
    void         loadSongSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransitionsPage)
};
