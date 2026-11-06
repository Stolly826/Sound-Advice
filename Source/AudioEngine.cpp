#include "AudioEngine.h"

AudioEngine::AudioEngine()
{
    // FRONTEND/BACKEND INTEGRATION: Recognise WAV, MP3, and other JUCE formats.
    formatManager.registerBasicFormats();

    // Set up output to the default system speakers (stereo).
    deviceError = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (deviceError.isEmpty())
    {
        audioPlayer.setSource(&speedResampler); //sending audio through resampler scrum37
        deviceManager.addAudioCallback(&audioPlayer);
    }
}

AudioEngine::~AudioEngine()
{
    // Disconnect in reverse order before deleting the audio file reader.
    if (deviceError.isEmpty())
        deviceManager.removeAudioCallback(&audioPlayer);
    audioPlayer.setSource(nullptr);
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    deviceManager.closeAudioDevice();
}

bool AudioEngine::playSong(const juce::File& file)
{
    if (!isReady() || !file.existsAsFile())
        return false;

    if (loadedFilePath != file.getFullPathName())
    {
        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(file));
        if (!reader)
            return false; // Unsupported or damaged audio file.

        const double sampleRate = reader->sampleRate;
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            reader.release(), true);
        transport.setSource(readerSource.get(), 0, nullptr, sampleRate);
        loadedFilePath = file.getFullPathName();
        paused = false; // New song should never inherit paused state.
    }

    // A finished song should restart; a paused song resumes at its position.
    if (transport.hasStreamFinished())
        transport.setPosition(0.0);
    transport.start();
    paused = false; // Play also resumes if already paused.
    return true;
}

void AudioEngine::pause()
{
    if (readerSource && transport.isPlaying())
    {
        transport.stop(); // Do not reset the position.
        paused = true;   // Remember this was a pause, not a normal Stop.
    }
}

// FRONTEND/BACKEND INTEGRATION: Resume only a previously paused song.
void AudioEngine::resume()
{
    if (readerSource && paused)
    {
        transport.start();
        paused = false;
    }
}

void AudioEngine::stop()
{
    transport.stop();
    transport.setPosition(0.0); // Stop always starts over next time.
    paused = false;
}

void AudioEngine::setVolume(float gain)
{
    transport.setGain(juce::jlimit(0.0f, 1.0f, gain));
}

void AudioEngine::setPlaybackSpeed(double speedMultiplier)
{
    //parameters for the speed so it stays between 0.5 and 1.5
    speedMultiplier = juce::jlimit(0.5, 1.5, speedMultiplier);
    speedResampler.setResamplingRatio(speedMultiplier);
}
