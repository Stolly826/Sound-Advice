#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

struct Playlist
{
    juce::String      name;
    juce::String      description;
    juce::String      group;        // playlist group empty shows under other
    juce::Image       image;        // Image::isNull() == true when none was provided
    juce::File        imageFile;    // copy of the image in the library folder
    juce::Time        lastUpdated;
    juce::Array<Song> songs;        // ordered list; newest additions go to the back
};
