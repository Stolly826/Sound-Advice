#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// All points are normalised 0–1 (position within the song), or -1 if unset.
struct Transition
{
    juce::String name;
    juce::String toSongTitle;   // the "next" song

    float loopPoint    = -1.0f; // current song: return-to-loop marker
    float fadeOutStart = -1.0f; // current song: start of fade-out
    float fadeOutEnd   = -1.0f; // current song: end of fade-out

    float startPoint   = -1.0f; // next song: set-start marker
    float fadeInStart  = -1.0f; // next song: start of fade-in
    float fadeInEnd    = -1.0f; // next song: end of fade-in

    juce::Time savedAt;
};
