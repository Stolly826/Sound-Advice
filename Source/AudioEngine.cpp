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

void SongEffectsSource::setPitch(double semitones)
{
    pitchSemitones = semitones;
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
    pitchShifter.prepare(sampleRate);

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

    // pitch shifting colours the sound a little, so it is skipped entirely at 0
    const double semitones = pitchSemitones.load();
    if (std::abs(semitones) > 0.01)
    {
        float* channels[kNumChannels] = {};
        const int numShifted = juce::jmin(kNumChannels, info.buffer->getNumChannels());
        for (int ch = 0; ch < numShifted; ++ch)
            channels[ch] = info.buffer->getWritePointer(ch, info.startSample);

        pitchShifter.process(channels, numShifted, info.numSamples,
                             std::pow(2.0, semitones / 12.0));
        pitchWasOn = true;
    }
    else if (pitchWasOn)
    {
        pitchShifter.reset(); // so old audio isnt heard the next time it is turned on
        pitchWasOn = false;
    }

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

// ── Pitch shifter ─────────────────────────────────────────────────────────────

void PitchShifter::prepare(double sampleRate)
{
    windowSize = kWindowSeconds * sampleRate;
    bufferSize = (int)windowSize + 8;
    for (auto& line : delayLine)
        line.assign((size_t)bufferSize, 0.0f);
    reset();
}

void PitchShifter::reset()
{
    for (auto& line : delayLine)
        std::fill(line.begin(), line.end(), 0.0f);
    writePos = 0;
    phase = 0.0;
}

float PitchShifter::readDelayed(int channel, double delaySamples) const
{
    double pos = writePos - delaySamples;
    if (pos < 0.0)
        pos += bufferSize;

    const int i0 = (int)pos;
    const int i1 = i0 + 1 < bufferSize ? i0 + 1 : 0;
    const float frac = (float)(pos - i0);
    const auto& line = delayLine[channel];
    return line[(size_t)i0] + (line[(size_t)i1] - line[(size_t)i0]) * frac;
}

void PitchShifter::process(float* const* channels, int numChannels, int numSamples, double ratio)
{
    if (bufferSize == 0)
        return;

    numChannels = juce::jmin(numChannels, kChannels);

    // the delay grows or shrinks by (1 - ratio) every sample, which is what changes the pitch
    const double step = (1.0 - ratio) / windowSize;

    for (int s = 0; s < numSamples; ++s)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            delayLine[ch][(size_t)writePos] = channels[ch][s];

        // the second head is half a window behind, and their fades always add up to 1
        double otherPhase = phase + 0.5;
        if (otherPhase >= 1.0)
            otherPhase -= 1.0;

        const float fade = (float)std::sin(juce::MathConstants<double>::pi * phase);
        const float gain1 = fade * fade;
        const float gain2 = 1.0f - gain1;
        const double delay1 = 1.0 + phase * windowSize;
        const double delay2 = 1.0 + otherPhase * windowSize;

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][s] = gain1 * readDelayed(ch, delay1) + gain2 * readDelayed(ch, delay2);

        phase += step;
        if (phase >= 1.0)     phase -= 1.0;
        else if (phase < 0.0) phase += 1.0;

        if (++writePos >= bufferSize)
            writePos = 0;
    }
}

// ── Scratching ────────────────────────────────────────────────────────────────

// how quickly the record catches up with the hand, and how much the speed is smoothed
static constexpr double kScratchFollowSeconds = 0.025;
static constexpr double kScratchSmoothSeconds = 0.004;
static constexpr double kScratchMaxSpeed      = 4.0; // times normal speed

bool ScratchAudioSource::begin(juce::AudioFormatReader& reader, double grabSeconds, double rangeSeconds)
{
    const double rate = reader.sampleRate;
    const juce::int64 total = reader.lengthInSamples;
    if (rate <= 0.0 || total <= 0)
        return false;

    const auto range = (juce::int64)(rangeSeconds * rate);
    const juce::int64 centre = std::clamp((juce::int64)(grabSeconds * rate), (juce::int64)0, total);
    const juce::int64 start = std::max((juce::int64)0, centre - range);
    const juce::int64 finish = std::min(total, centre + range);
    const int length = (int)(finish - start);
    if (length < 8)
        return false;

    juce::AudioBuffer<float> newWindow((int)juce::jmax(1u, juce::jmin(2u, reader.numChannels)), length);
    reader.read(&newWindow, 0, length, start, true, true);

    const juce::SpinLock::ScopedLockType sl(lock);
    window = std::move(newWindow);
    windowStartSeconds = (double)start / rate;
    fileRate = rate;
    grabPos = (double)(centre - start);
    readPos = grabPos;
    velocity = 0.0;
    handPos = grabPos;
    positionSeconds = grabSeconds;
    active = true;
    return true;
}

void ScratchAudioSource::setHandOffset(double seconds)
{
    handPos = grabPos + seconds * fileRate;
}

double ScratchAudioSource::end()
{
    const juce::SpinLock::ScopedLockType sl(lock);
    active = false;
    return windowStartSeconds + readPos / fileRate;
}

void ScratchAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    deviceRate = sampleRate;
    input.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void ScratchAudioSource::releaseResources()
{
    input.releaseResources();
}

float ScratchAudioSource::sampleAt(int channel, double pos) const
{
    // 4 point Hermite interpolation keeps slow scratches from sounding gritty
    const int i = (int)pos;
    const float f = (float)(pos - i);
    const float* d = window.getReadPointer(channel);
    const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

void ScratchAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (!active.load())
    {
        input.getNextAudioBlock(info);
        return;
    }

    const juce::SpinLock::ScopedTryLockType tryLock(lock);
    if (!tryLock.isLocked() || !active.load() || window.getNumSamples() < 8)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const double normalStep = fileRate / deviceRate; // window samples per output sample at 1x
    const double maxStep = normalStep * kScratchMaxSpeed;
    const double follow = 1.0 / (kScratchFollowSeconds * deviceRate);
    const double smoothing = 1.0 - std::exp(-1.0 / (kScratchSmoothSeconds * deviceRate));
    const double target = handPos.load();
    const double lastPos = window.getNumSamples() - 3.0;
    const int sourceChannels = window.getNumChannels();

    for (int s = 0; s < info.numSamples; ++s)
    {
        // the record chases the hand, so a quick flick speeds up and slows down like vinyl
        const double wanted = juce::jlimit(-maxStep, maxStep, (target - readPos) * follow);
        velocity += (wanted - velocity) * smoothing;
        readPos = juce::jlimit(1.0, lastPos, readPos + velocity);

        // a record held still is silent and gets louder as it picks up speed
        const float gain = (float)juce::jmin(1.0, std::abs(velocity) / (normalStep * 0.3));

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
            info.buffer->getWritePointer(ch, info.startSample)[s]
                = sampleAt(juce::jmin(ch, sourceChannels - 1), readPos) * gain;
    }

    positionSeconds = windowStartSeconds + readPos / fileRate;
}

// ── Engine ────────────────────────────────────────────────────────────────────

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
                           float fadeOutStart, float fadeOutEnd,
                           FadeCurve fadeInCurve, FadeCurve fadeOutCurve)
{
    if (!isReady() || !file.existsAsFile())
        return false;

    if (scratchSource.isActive())
        endScratch();

    fadeSource.setFadeCurves(fadeInCurve, fadeOutCurve);
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
        openScratchReader(file);
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
        openScratchReader(file);
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

bool AudioEngine::cueNext(const juce::File& file, float fadeInStart, float fadeInEnd,
                          const SongSettings& settings, FadeCurve fadeInCurve)
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
    applyIncomingSettings(settings); // the next song comes in sounding how the user set it
    incomingTransport.stop();
    incomingTransport.setSource(nullptr);
    incomingReader = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    incomingTransport.setSource(incomingReader.get(), 0, nullptr, sampleRate);
    incomingFilePath = file.getFullPathName();

    // starts silent at the fade in point and ramps up to full volume at the fade in end
    incomingFade.setFadeCurves(fadeInCurve, FadeCurve::defaultCurve);
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

void FadeEnvelopeAudioSource::setFadeCurves(FadeCurve inCurve, FadeCurve outCurve)
{
    fadeInCurve.store((int)inCurve, std::memory_order_relaxed);
    fadeOutCurve.store((int)outCurve, std::memory_order_relaxed);
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

    const auto inCurve  = fadeCurveFromInt(fadeInCurve.load(std::memory_order_relaxed));
    const auto outCurve = fadeCurveFromInt(fadeOutCurve.load(std::memory_order_relaxed));

    const double positionDelta = positionAtEnd - positionAtStart;
    const int lastSample = juce::jmax(1, info.numSamples - 1);
    for (int sample = 0; sample < info.numSamples; ++sample)
    {
        const float songProgress = static_cast<float>(
            juce::jlimit(0.0, 1.0,
                (positionAtStart + positionDelta * sample / lastSample) / duration));

        float gain = 1.0f;
        // each fade follows its own curve (default, linear, exponential, log or constant power)
        // silent before a fade in starts and after a fade out ends, whatever the curve
        if (hasFadeIn)
            gain *= fadeInLevel((songProgress - inStart) / (inEnd - inStart), inCurve);
        if (hasFadeOut)
            gain *= fadeOutLevel((songProgress - outStart) / (outEnd - outStart), outCurve);

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
    if (scratchSource.isActive())
        scratchSource.end();
    playingBeforeScratch = false;

    transport.stop();
    transport.setPosition(0.0); // Stop always starts over next time.
    incomingTransport.stop();   // kept loaded so moving to the next song can take it over
    paused = false;
}

void AudioEngine::applyTo(juce::AudioTransportSource& deck, SongEffectsSource& fx,
                          const SongSettings& settings)
{
    // Volume and gain are both level changes, so the transport's smoothed gain does both.
    deck.setGain((float)(settings.volume * juce::Decibels::decibelsToGain(settings.gainDb)));
    fx.setSpeed(settings.speed);
    fx.setPitch(settings.pitch);
    fx.setEq(settings.bassDb, settings.midsDb, settings.trebleDb);
}

void AudioEngine::applySettings(const SongSettings& settings)
{
    applyTo(transport, effects, settings);
}

void AudioEngine::applyIncomingSettings(const SongSettings& settings)
{
    applyTo(incomingTransport, incomingEffects, settings);
}

// scratching: the transport is held still while the scratch source plays the record
void AudioEngine::openScratchReader(const juce::File& file)
{
    scratchReader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));
}

void AudioEngine::beginScratch()
{
    if (!readerSource || !scratchReader || scratchSource.isActive())
        return;

    playingBeforeScratch = transport.isPlaying();
    const double grabbedAt = transport.getCurrentPosition();
    transport.stop();

    if (!scratchSource.begin(*scratchReader, grabbedAt, kScratchRangeSeconds) && playingBeforeScratch)
        transport.start(); // couldnt read the audio, just carry on
}

void AudioEngine::scratchTo(double offsetSeconds)
{
    if (scratchSource.isActive())
        scratchSource.setHandOffset(offsetSeconds);
}

void AudioEngine::endScratch()
{
    if (!scratchSource.isActive())
        return;

    // carry on from wherever the record was let go
    transport.setPosition(scratchSource.end());
    if (playingBeforeScratch)
        transport.start();
    playingBeforeScratch = false;
}

double AudioEngine::getPosition() const
{
    return scratchSource.isActive() ? scratchSource.getPositionSeconds()
                                    : transport.getCurrentPosition();
}

// jump to a time used by jump to, loop return and the mini player
void AudioEngine::setPosition(double seconds)
{
    if (!readerSource || scratchSource.isActive())
        return;

    const double songLength = transport.getLengthInSeconds();
    if (songLength <= 0.0)
        return;

    transport.setPosition(juce::jlimit(0.0, songLength, seconds));
}
