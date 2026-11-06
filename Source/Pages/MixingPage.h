#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "Transition.h"
#include "SongChartComponent.h"
#include "SongsSidebarContent.h"
#include "../AudioPlayer.h"

class MixingPage : public juce::Component,
                   private juce::Timer // moves the playhead
{
public:
    std::function<juce::Array<Transition>(juce::String, juce::String)> getTransitionsForPair;

    MixingPage();

    void setContent(Playlist* playlist, int songIndex);

    // shared player owned by main component
    void setAudioPlayer(AudioPlayer* p) { player = p; }

    void paint  (juce::Graphics& g) override;
    void resized()                  override;
    void visibilityChanged()        override;

    juce::Component& getSidebarContent() { return sidebarContent; }

private:
    Playlist* currentPlaylist = nullptr;
    int       currentIndex    = 0;

    AudioPlayer* player     = nullptr;
    bool         songLoaded = false; // current song has audio ready to play

    // Header
    juce::Label      titleLabel;
    juce::TextButton playButton;
    juce::TextButton recordButton;
    juce::TextButton recordingsButton  { "Recordings" };

    // Charts
    SongChartComponent chart1;
    SongChartComponent chart2;
    juce::TextButton   transitionPresetsButton { "Transition Presets" };

    // Controls
    juce::Label  controlsHeader { {}, "CONTROLS" };
    juce::Label  volumeLabel    { {}, "Volume" };
    juce::Slider volumeSlider;
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

    SongsSidebarContent sidebarContent;
    int sepY = 0;

    void setupSlider(juce::Slider& s, double min, double max, double defaultVal);
    void showTransitionPresets();

    // playback
    void loadCurrentSong();
    void togglePlay();
    void jumpTo(float position);
    void skipToNext();
    void updatePlayButton();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixingPage)
};
