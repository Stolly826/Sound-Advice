#pragma once

// FRONTEND/BACKEND INTEGRATION: Reusable JUCE playback engine for the main app.
// This replaces the separate Win32-specific AudioEngine in deployment-prototype.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

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
    void setVolume(float gain);
    void setPlaybackSpeed(double speedMultiplier);
    void setBass(float decibels);
    void setMids(float decibels);
    void setTreble(float decibels);
    void scratch(double offsetSeconds);

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
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
