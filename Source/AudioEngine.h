#pragma once

// FRONTEND/BACKEND INTEGRATION: Reusable JUCE playback engine for the main app.
// This replaces the separate Win32-specific AudioEngine in deployment-prototype.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    bool isReady() const { return deviceError.isEmpty(); }
    const juce::String& getError() const { return deviceError; }

    // Load a different file when necessary, then play. Pressing Play after
    // Pause resumes because stopping the transport preserves its position.
    bool playSong(const juce::File& file);
    void pause();
    void resume(); // FRONTEND/BACKEND INTEGRATION: Dedicated Resume button
    void stop();
    void setVolume(float gain);
    void setPlaybackSpeed(double speedMultiplier);
    void setBass(float decibels);
    void setMids(float decibels);
    void setTreble(float decibels);
    void scratch(double offsetSeconds);

    // playback info for the mixing page playhead jump to and skip
    double getPosition() const { return transport.getCurrentPosition(); }  // seconds
    double getLength() const   { return transport.getLengthInSeconds(); }  // seconds
    bool   isPlaying() const   { return transport.isPlaying(); }
    const juce::String& getLoadedFilePath() const { return loadedFilePath; }
    void   setPosition(double seconds); // jump to a time in the song

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer audioPlayer;
    juce::IIRFilterAudioSource bassFilter{ &speedResampler, false };
    juce::IIRFilterAudioSource midsFilter{ &bassFilter, false };
    juce::IIRFilterAudioSource trebleFilter{ &midsFilter, false };
    juce::ResamplingAudioSource speedResampler{ &transport, false, 2 };
    juce::AudioDeviceManager deviceManager;
    juce::String deviceError;
    juce::String loadedFilePath;
    bool paused = false; // Tracks if Resume should be allowed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
