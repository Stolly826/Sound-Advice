#include "AudioEngine.h"

// Where each EQ band sits. Bass and treble are shelves, mids is a wide bell.
static constexpr double kBassHz   = 200.0;
static constexpr double kMidsHz   = 1000.0;
static constexpr double kTrebleHz = 4000.0;
static constexpr double kEqQ      = 0.7071;

SongEffectsSource::SongEffectsSource(juce::AudioSource& input)
    : resampler(&input, false, kNumChannels)
{
}

void SongEffectsSource::setSpeed(double newSpeed)
{
    speed = newSpeed;
    resampler.setResamplingRatio(newSpeed);
}

void SongEffectsSource::setEq(double newBassDb, double newMidsDb, double newTrebleDb)
{
    bassDb = newBassDb;
    midsDb = newMidsDb;
    trebleDb = newTrebleDb;
    eqChanged = true; // The audio thread rebuilds the filters on its next block.
}

void SongEffectsSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;

    // The resampler tells its input the sample rate is scaled by its ratio, and the
    // transport would then correct for that and cancel the speed out. Preparing at
    // normal speed avoids that if the device restarts mid-song, and the bigger block
    // stops the resampler reallocating on the audio thread at top speed.
    resampler.setResamplingRatio(1.0);
    resampler.prepareToPlay(juce::roundToInt(samplesPerBlockExpected * SongSettings::maxSpeed),
                            sampleRate);
    resampler.setResamplingRatio(speed);

    for (auto& channel : filters)
        for (auto& filter : channel)
            filter.reset();

    eqChanged = true;
}

void SongEffectsSource::releaseResources()
{
    resampler.releaseResources();
}

void SongEffectsSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    resampler.getNextAudioBlock(info);

    if (eqChanged.exchange(false))
        updateFilters();

    const int numChannels = juce::jmin(kNumChannels, info.buffer->getNumChannels());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = info.buffer->getWritePointer(ch, info.startSample);

        for (auto& filter : filters[ch])
            filter.processSamples(samples, info.numSamples);

        // Gain and EQ can both boost, so stop peaks from going past full scale.
        juce::FloatVectorOperations::clip(samples, samples, -1.0f, 1.0f, info.numSamples);
    }
}

void SongEffectsSource::updateFilters()
{
    auto toGain = [](double db) { return (float)juce::Decibels::decibelsToGain(db); };

    const auto bassBand = juce::IIRCoefficients::makeLowShelf(currentSampleRate, kBassHz, kEqQ, toGain(bassDb));
    const auto midsBand = juce::IIRCoefficients::makePeakFilter(currentSampleRate, kMidsHz, kEqQ, toGain(midsDb));
    const auto trebleBand = juce::IIRCoefficients::makeHighShelf(currentSampleRate, kTrebleHz, kEqQ, toGain(trebleDb));

    for (auto& channel : filters)
    {
        channel[bass].setCoefficients(bassBand);
        channel[mids].setCoefficients(midsBand);
        channel[treble].setCoefficients(trebleBand);
    }
}

AudioEngine::AudioEngine()
{
    // FRONTEND/BACKEND INTEGRATION: Recognise WAV, MP3, and other JUCE formats.
    formatManager.registerBasicFormats();

    // Set up output to the default system speakers (stereo).
    deviceError = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (deviceError.isEmpty())
    {
        // transport -> speed and EQ -> transition fades -> mixer -> speakers
        mixer.addInputSource(&fadeSource, false);
        mixer.addInputSource(&incomingFade, false);
        audioPlayer.setSource(&mixer);
        deviceManager.addAudioCallback(&audioPlayer);
    }
}

AudioEngine::~AudioEngine()
{
    // Disconnect in reverse order before deleting the audio file reader.
    if (deviceError.isEmpty())
        deviceManager.removeAudioCallback(&audioPlayer);
    audioPlayer.setSource(nullptr);
    mixer.removeAllInputs();
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    incomingTransport.stop();
    incomingTransport.setSource(nullptr);
    incomingReader.reset();
    deviceManager.closeAudioDevice();
}

bool AudioEngine::playSong(const juce::File& file,
                           float fadeInStart, float fadeInEnd,
                           float fadeOutStart, float fadeOutEnd)
{
    if (!isReady() || !file.existsAsFile())
        return false;

    fadeSource.setFadePoints(fadeInStart, fadeInEnd, fadeOutStart, fadeOutEnd);

    if (incomingReader && incomingFilePath == file.getFullPathName())
    {
        // the transition is over: carry on the incoming deck's song from where it got to
        const double position = incomingTransport.getCurrentPosition();
        incomingTransport.stop();
        incomingTransport.setSource(nullptr);

        transport.stop();
        transport.setSource(nullptr);
        readerSource = std::move(incomingReader);
        transport.setSource(readerSource.get(), 0, nullptr,
                            readerSource->getAudioFormatReader()->sampleRate);
        transport.setPosition(position);
        loadedFilePath = incomingFilePath;
        incomingFilePath.clear();
        paused = false;
    }
    else if (loadedFilePath != file.getFullPathName())
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

    // a song with an incoming transition starts at its fade in point, not silently at 0
    const double length = transport.getLengthInSeconds();
    if (fadeInStart >= 0.0f && fadeInEnd > fadeInStart
        && transport.getCurrentPosition() < fadeInStart * length)
        transport.setPosition(fadeInStart * length);

    transport.start();
    paused = false; // Play also resumes if already paused.
    return true;
}

bool AudioEngine::cueNext(const juce::File& file, float fadeInStart, float fadeInEnd)
{
    if (!isReady() || !file.existsAsFile())
        return false;

    // already coming in, called every timer tick during the fade out
    if (incomingFilePath == file.getFullPathName() && incomingTransport.isPlaying())
        return true;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader)
        return false;

    const double sampleRate = reader->sampleRate;
    incomingTransport.stop();
    incomingTransport.setSource(nullptr);
    incomingReader = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    incomingTransport.setSource(incomingReader.get(), 0, nullptr, sampleRate);
    incomingFilePath = file.getFullPathName();

    // starts silent at the fade in point and ramps up to full volume at the fade in end
    incomingFade.setFadePoints(fadeInStart, fadeInEnd, -1.0f, -1.0f);
    incomingTransport.setPosition(fadeInStart * incomingTransport.getLengthInSeconds());
    incomingTransport.start();
    return true;
}

void FadeEnvelopeAudioSource::setFadePoints(float inStart, float inEnd,
                                             float outStart, float outEnd)
{
    fadeInStart.store(inStart, std::memory_order_relaxed);
    fadeInEnd.store(inEnd, std::memory_order_relaxed);
    fadeOutStart.store(outStart, std::memory_order_relaxed);
    fadeOutEnd.store(outEnd, std::memory_order_relaxed);
}

void FadeEnvelopeAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    input.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void FadeEnvelopeAudioSource::releaseResources()
{
    input.releaseResources();
}

void FadeEnvelopeAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer == nullptr || info.numSamples <= 0)
        return;

    const double positionAtStart = transport.getCurrentPosition();
    input.getNextAudioBlock(info);
    const double positionAtEnd = transport.getCurrentPosition();
    const double duration = transport.getLengthInSeconds();
    if (duration <= 0.0)
        return;

    const float inStart = fadeInStart.load(std::memory_order_relaxed);
    const float inEnd = fadeInEnd.load(std::memory_order_relaxed);
    const float outStart = fadeOutStart.load(std::memory_order_relaxed);
    const float outEnd = fadeOutEnd.load(std::memory_order_relaxed);
    const bool hasFadeIn = inStart >= 0.0f && inEnd > inStart && inEnd <= 1.0f;
    const bool hasFadeOut = outStart >= 0.0f && outEnd > outStart && outEnd <= 1.0f;
    if (!hasFadeIn && !hasFadeOut)
        return;

    const double positionDelta = positionAtEnd - positionAtStart;
    const int lastSample = juce::jmax(1, info.numSamples - 1);
    for (int sample = 0; sample < info.numSamples; ++sample)
    {
        const float songProgress = static_cast<float>(
            juce::jlimit(0.0, 1.0,
                (positionAtStart + positionDelta * sample / lastSample) / duration));

        float gain = 1.0f;
        if (hasFadeIn)
            gain *= juce::jlimit(0.0f, 1.0f,
                (songProgress - inStart) / (inEnd - inStart));
        if (hasFadeOut)
            gain *= 1.0f - juce::jlimit(0.0f, 1.0f,
                (songProgress - outStart) / (outEnd - outStart));

        for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
            info.buffer->getWritePointer(channel, info.startSample)[sample] *= gain;
    }
}

void AudioEngine::pause()
{
    if (readerSource && transport.isPlaying())
    {
        transport.stop(); // Do not reset the position.
        incomingTransport.stop(); // pause a transition in progress too
        paused = true;   // Remember this was a pause, not a normal Stop.
    }
}

// FRONTEND/BACKEND INTEGRATION: Resume only a previously paused song.
void AudioEngine::resume()
{
    if (readerSource && paused)
    {
        transport.start();
        if (incomingReader)
            incomingTransport.start();
        paused = false;
    }
}

void AudioEngine::stop()
{
    transport.stop();
    transport.setPosition(0.0); // Stop always starts over next time.
    incomingTransport.stop();   // kept loaded so moving to the next song can take it over
    paused = false;
}

void AudioEngine::applySettings(const SongSettings& settings)
{
    // Volume and gain are both level changes, so the transport's smoothed gain does both.
    transport.setGain((float)(settings.volume * juce::Decibels::decibelsToGain(settings.gainDb)));
    effects.setSpeed(settings.speed);
    effects.setEq(settings.bassDb, settings.midsDb, settings.trebleDb);
}

//scratch stuff
void AudioEngine::scratch(double offsetSeconds)
{
    //make sure song is loaded
    if (!readerSource)
        return;

    double songLength = transport.getLengthInSeconds();
    if (songLength <= 0.0)
        return;

    //showing where to move
    double newPosition = transport.getCurrentPosition() + offsetSeconds;

    //stays within bounds of the song
    newPosition = juce::jlimit(0.0, songLength, newPosition);

    //move playback
    transport.setPosition(newPosition);
}
// jump to a time used by jump to and loop return
void AudioEngine::setPosition(double seconds)
{
    if (!readerSource)
        return;

    transport.setPosition(juce::jlimit(0.0, transport.getLengthInSeconds(), seconds));
}
