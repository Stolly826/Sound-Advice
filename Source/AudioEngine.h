#pragma once

// FRONTEND/BACKEND INTEGRATION: Reusable JUCE playback engine for the main app.
// This replaces the separate Win32-specific AudioEngine in deployment-prototype.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include "SongSettings.h"

// Speed and EQ stages that sit between the transport and the speakers.
// The setters are safe to call from the UI while audio is playing.
class SongEffectsSource : public juce::AudioSource
{
public:
    explicit SongEffectsSource(juce::AudioSource& input);

    void setSpeed(double newSpeed);
    void setEq(double newBassDb, double newMidsDb, double newTrebleDb);

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    enum Band { bass, mids, treble, numBands };
    static constexpr int kNumChannels = 2;

    juce::ResamplingAudioSource resampler;
    juce::IIRFilter filters[kNumChannels][numBands];

    std::atomic<double> speed { 1.0 };
    std::atomic<double> bassDb { 0.0 };
    std::atomic<double> midsDb { 0.0 };
    std::atomic<double> trebleDb { 0.0 };
    std::atomic<bool> eqChanged { true };
    double currentSampleRate = 0.0;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongEffectsSource)
};

class FadeEnvelopeAudioSource final : public juce::AudioSource
{
public:
    FadeEnvelopeAudioSource(juce::AudioSource& inputSource,
                            juce::AudioTransportSource& audioTransport)
        : input(inputSource), transport(audioTransport) {}

    void setFadePoints(float inStart, float inEnd, float outStart, float outEnd);
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    juce::AudioSource& input;
    juce::AudioTransportSource& transport;
    std::atomic<float> fadeInStart { -1.0f };
    std::atomic<float> fadeInEnd   { -1.0f };
    std::atomic<float> fadeOutStart { -1.0f };
    std::atomic<float> fadeOutEnd   { -1.0f };
};

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    bool isReady() const { return deviceError.isEmpty(); }
    const juce::String& getError() const { return deviceError; }

    // Load a different file when necessary, then play. Pressing Play after
    // Pause resumes because stopping the transport preserves its position.
    bool playSong(const juce::File& file,
                  float fadeInStart = -1.0f, float fadeInEnd = -1.0f,
                  float fadeOutStart = -1.0f, float fadeOutEnd = -1.0f);
    void pause();
    void resume(); // FRONTEND/BACKEND INTEGRATION: Dedicated Resume button
    void stop();
    void scratch(double offsetSeconds);

    // FRONTEND/BACKEND INTEGRATION: Speed, volume, gain and EQ for the current song
    void applySettings(const SongSettings& settings);

    juce::AudioFormatManager& getFormats() { return formatManager; }

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
    SongEffectsSource effects { transport };
    juce::AudioSourcePlayer audioPlayer;
    juce::IIRFilterAudioSource bassFilter{ &speedResampler, false };
    juce::IIRFilterAudioSource midsFilter{ &bassFilter, false };
    juce::IIRFilterAudioSource trebleFilter{ &midsFilter, false };
    juce::ResamplingAudioSource speedResampler{ &transport, false, 2 };
    FadeEnvelopeAudioSource fadeSource;
    juce::AudioDeviceManager deviceManager;
    juce::String deviceError;
    juce::String loadedFilePath;
    bool paused = false; // Tracks if Resume should be allowed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
