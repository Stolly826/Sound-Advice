#pragma once

#include <juce_audio_formats/juce_audio_formats.h> // reading and writing audio files
#include <juce_audio_devices/juce_audio_devices.h> // sound card and transport

// plays one audio file at a time through the default output
// file -> reader -> reader source -> transport -> source player -> device -> speakers
// keep one instance alive while you want sound eg as a member of maincomponent
class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();

    // true if the sound card opened
    bool isAudioDeviceReady() const;

    // load a wav or mp3 false if it cant be read
    bool loadFile (const juce::File& file);
    bool hasFileLoaded() const;

    // playback
    bool play();                        // start from current position false if nothing loaded
    bool playFrom (double seconds);     // jump to a time then start
    void pause();                       // stop but keep position
    bool isPlaying() const;
    bool hasFinished() const;           // reached the end of the song

    // position in seconds
    void   setPosition (double seconds);
    double getPosition() const;
    double getLengthInSeconds() const;

    // volume 0 silent 1 full
    void  setVolume (float newGain);
    float getVolume() const;

    // decode any readable file and save as 16 bit wav
    bool convertToWav (const juce::File& sourceFile, const juce::File& destinationWavFile);

    // shared formats eg for reading song info
    juce::AudioFormatManager& getFormats();

private:
    // members get destroyed bottom to top so device manager goes last
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::AudioSourcePlayer    sourcePlayer;
    juce::AudioDeviceManager   deviceManager;

    bool deviceReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPlayer)
};
