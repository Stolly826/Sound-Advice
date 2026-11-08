#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"
#include "Transition.h"
#include "SongChartComponent.h"
#include "SongsSidebarContent.h"

class MixingPage : public juce::Component,
                   private juce::Timer // moves the playhead
{
public:
    std::function<juce::Array<Transition>(juce::String, juce::String)> getTransitionsForPair;

    std::function<bool(const Song&)> onPlaySong;
    std::function<void()> onPause;
    std::function<void()> onResume;
    std::function<void()> onStop;
    std::function<void(float)> onVolumeChanged;
    std::function<void(double)> onSpeedChanged;
    std::function<void(float)> onBassChanged;
    std::function<void(float)> onMidsChanged;
    std::function<void(float)> onTrebleChanged;
    std::function<void(double)> onScratch;

    // playback info for the playhead jump to and skip
    std::function<double()>       getPlaybackPosition; // seconds
    std::function<double()>       getPlaybackLength;   // seconds
    std::function<bool()>         isPlaybackActive;
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

    // Header
    juce::Label      titleLabel;
    juce::TextButton playButton;
    juce::TextButton pauseButton{ "Pause" };
    juce::TextButton resumeButton{ "Resume" };
    juce::TextButton stopButton{ "Stop" };
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
    juce::Slider scratchSlider;
    //how far slider moved
    double previousScratchValue = 0.0;

    SongsSidebarContent sidebarContent;
    int sepY = 0;

    void setupSlider(juce::Slider& s, double min, double max, double defaultVal);
    void showTransitionPresets();

    // playback extras on top of the audio engine
    bool currentSongIsLoaded() const; // engine has this pages song loaded
    void resetPlayhead();
    void jumpTo(float position);
    void skipToNext();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixingPage)
};
