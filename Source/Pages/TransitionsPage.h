#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include "Transition.h"
#include "SongChartComponent.h"

class TransitionsPage : public juce::Component
{
public:
    std::function<juce::Array<Song>()>                               getAllSongs;
    std::function<juce::Array<Transition>(juce::String, juce::String)> getTransitionsForPair;
    std::function<void(juce::String, Transition)>                    addTransitionToSong;
    std::function<void(juce::String, int, Transition)>               updateTransitionInSong;

    TransitionsPage();
    void paint  (juce::Graphics& g) override;
    void resized()                  override;

private:
    // Header
    juce::Label      titleLabel;
    juce::TextButton playButton;
    juce::TextButton saveTransitionButton { "Save Transition" };

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
    juce::Label  eqHeader    { {}, "EQ" };
    juce::Label  trebleLabel { {}, "Treble" };
    juce::Slider trebleSlider;
    juce::Label  midsLabel   { {}, "Mids" };
    juce::Slider midsSlider;
    juce::Label  bassLabel   { {}, "Bass" };
    juce::Slider bassSlider;

    int          sepY                   = 0;
    int          loadedTransitionIndex  = -1;  // -1 = none loaded
    juce::String loadedTransitionName;

    void setupSlider(juce::Slider& s, double min, double max, double def);
    void showPickSong(SongChartComponent& chart);
    void showTransitionPresets();
    void onSaveClicked();
    void doSaveTransition(const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransitionsPage)
};
