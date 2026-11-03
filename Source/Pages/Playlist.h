#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

struct Playlist
{
    juce::String      name;
    juce::String      description;
    juce::Image       image;        // Image::isNull() == true when none was provided
    juce::Time        lastUpdated;
    juce::Array<Song> songs;        // ordered list; newest additions go to the back
};
