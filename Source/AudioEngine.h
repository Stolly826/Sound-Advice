#pragma once

// FRONTEND/BACKEND INTEGRATION: Reusable JUCE playback engine for the main app.
// This replaces the separate Win32-specific AudioEngine in deployment-prototype.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>
#include "SongSettings.h"

// Moves the key up or down without changing the speed.
// Two read heads sweep through a short delay line at a different rate than it is
// written, and each one fades out before it has to jump back, so the jumps can't be heard.
class PitchShifter
{
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* const* channels, int numChannels, int numSamples, double ratio);

private:
    static constexpr int kChannels = 2;
    static constexpr double kWindowSeconds = 0.06; // longer is smoother, shorter is less echoey

    std::vector<float> delayLine[kChannels];
    int bufferSize = 0;
    int writePos = 0;
    double windowSize = 0.0;
    double phase = 0.0;

    float readDelayed(int channel, double delaySamples) const;
};

// Speed, pitch and EQ stages that sit between the transport and the speakers.
// The setters are safe to call from the UI while audio is playing.
class SongEffectsSource : public juce::AudioSource
{
public:
    explicit SongEffectsSource(juce::AudioSource& input);

    void setSpeed(double newSpeed);
    void setPitch(double semitones);
    void setEq(double newBassDb, double newMidsDb, double newTrebleDb);

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    enum Band { bass, mids, treble, numBands };
    static constexpr int kNumChannels = 2;

    juce::ResamplingAudioSource resampler;
    juce::IIRFilter filters[kNumChannels][numBands];
    PitchShifter pitchShifter;

    std::atomic<double> speed { 1.0 };
    std::atomic<double> pitchSemitones { 0.0 };
    std::atomic<double> bassDb { 0.0 };
    std::atomic<double> midsDb { 0.0 };
    std::atomic<double> trebleDb { 0.0 };
    std::atomic<bool> eqChanged { true };
    bool pitchWasOn = false; // audio thread only
    double currentSampleRate = 0.0;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongEffectsSource)
};

// Turntable style scratching. While the user drags, the song plays from a few seconds
// of audio held in memory at whatever speed and direction the hand is moving, so it
// sounds like a record being pushed back and forth instead of jumping between spots.
// When not scratching it just passes the transport straight through.
class ScratchAudioSource final : public juce::AudioSource
{
public:
    explicit ScratchAudioSource(juce::AudioSource& inputSource) : input(inputSource) {}

    // message thread: grab the audio around the playhead and take over from the transport
    bool begin(juce::AudioFormatReader& reader, double positionSeconds, double rangeSeconds);
    void setHandOffset(double seconds); // where the hand is, relative to where it grabbed
    double end();                       // stop scratching, returns where the record ended up
    bool isActive() const { return active.load(); }
    double getPositionSeconds() const { return positionSeconds.load(); }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    juce::AudioSource& input;
    juce::SpinLock lock; // guards the window while it is swapped
    juce::AudioBuffer<float> window;
    double windowStartSeconds = 0.0;
    double fileRate = 44100.0;
    double deviceRate = 44100.0;

    std::atomic<bool> active { false };
    std::atomic<double> handPos { 0.0 };          // in window samples
    std::atomic<double> positionSeconds { 0.0 };  // for the playhead
    double grabPos = 0.0;  // in window samples, where the hand first grabbed
    double readPos = 0.0;  // in window samples
    double velocity = 0.0; // window samples per output sample

    float sampleAt(int channel, double pos) const;
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

    // scratching: grab the record, move it around, let go
    void beginScratch();
    void scratchTo(double offsetSeconds); // offset from where it was grabbed
    void endScratch();

    // start the next song quietly on a second deck from its fade in point
    // playSong with the same file later takes over from where this deck got to
    bool cueNext(const juce::File& file, float fadeInStart, float fadeInEnd,
                 const SongSettings& settings);

    // FRONTEND/BACKEND INTEGRATION: Speed, volume, gain, pitch and EQ for the current song
    void applySettings(const SongSettings& settings);
    // the same for the next song, heard while it fades in
    void applyIncomingSettings(const SongSettings& settings);

    juce::AudioFormatManager& getFormats() { return formatManager; }

    // playback info for the mixing page playhead jump to and skip
    double getPosition() const; // seconds, follows the record while scratching
    double getLength() const   { return transport.getLengthInSeconds(); }  // seconds
    bool   isPlaying() const   { return transport.isPlaying(); }
    bool   isPaused() const    { return paused; }
    const juce::String& getLoadedFilePath() const { return loadedFilePath; }
    void   setPosition(double seconds); // jump to a time in the song

private:
    static constexpr double kScratchRangeSeconds = 3.0; // a bit more than the slider reaches

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::AudioFormatReader> scratchReader; // own reader so it never races the transport
    juce::AudioTransportSource transport;
    ScratchAudioSource scratchSource { transport };
    SongEffectsSource effects { scratchSource };
    FadeEnvelopeAudioSource fadeSource { effects, transport }; // fades sit after speed and EQ
    bool playingBeforeScratch = false;

    // incoming deck that plays the next song during a transition
    std::unique_ptr<juce::AudioFormatReaderSource> incomingReader;
    juce::AudioTransportSource incomingTransport;
    SongEffectsSource incomingEffects { incomingTransport };
    FadeEnvelopeAudioSource incomingFade { incomingEffects, incomingTransport };
    juce::String incomingFilePath;

    juce::MixerAudioSource mixer; // current deck + incoming deck
    juce::AudioSourcePlayer audioPlayer;
    juce::AudioDeviceManager deviceManager;
    juce::String deviceError;
    juce::String loadedFilePath;
    bool paused = false; // Tracks if Resume should be allowed

    void openScratchReader(const juce::File& file);
    static void applyTo(juce::AudioTransportSource& deck, SongEffectsSource& fx,
                        const SongSettings& settings);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
