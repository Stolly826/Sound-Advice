#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Transition.h"

struct Song
{
    juce::String title;
    juce::String artist;
    juce::String album;
    int          releaseYear     = 0; // 0 = unknown
    juce::Time              dateAdded;
    int                     durationSeconds = 0; // 0 = unknown
    juce::Array<Transition> transitions;
    juce::File              audioFile; // stored copy in the library folder empty if no audio
};

inline juce::String formatSongDuration(int secs)
{
    if (secs <= 0) return "--:--";
    return juce::String(secs / 60) + ":" + juce::String(secs % 60).paddedLeft('0', 2);
}

inline int parseSongDuration(const juce::String& s)
{
    auto parts = juce::StringArray::fromTokens(s.trim(), ":", "");
    if (parts.size() == 2)  return parts[0].getIntValue() * 60 + parts[1].getIntValue();
    if (parts.size() == 1)  return parts[0].getIntValue();
    return 0;
}
