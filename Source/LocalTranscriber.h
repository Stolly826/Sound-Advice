#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

class LocalTranscriber
{
public:
    static juce::Result transcribe(const juce::File& audioFile,
                                   const juce::File& modelFile,
                                   juce::String& transcript);
};
