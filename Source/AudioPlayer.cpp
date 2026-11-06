#include "AudioPlayer.h"

// mp3 needs JUCE_USE_MP3AUDIOFORMAT=1 already set in cmakelists

AudioPlayer::AudioPlayer()
{
    // register wav aiff flac ogg and mp3
    // mp3 is included here so dont register it again
    formatManager.registerBasicFormats();

    // open default output 0 inputs 2 outputs
    // empty string means it worked
    const juce::String error = deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceReady = error.isEmpty();

    if (! deviceReady)
    {
        DBG ("audio device error " + error);
        return;
    }

    // connect device -> source player -> transport
    // audio thread now pulls samples from the transport
    sourcePlayer.setSource (&transportSource);
    deviceManager.addAudioCallback (&sourcePlayer);
}

AudioPlayer::~AudioPlayer()
{
    // disconnect in reverse so the audio thread never hits a deleted object
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    transportSource.stop();
    transportSource.setSource (nullptr);
    deviceManager.closeAudioDevice();
}

bool AudioPlayer::isAudioDeviceReady() const { return deviceReady; }

bool AudioPlayer::loadFile (const juce::File& file)
{
    // get a decoder for the file null means bad path or unsupported format
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    const double fileSampleRate = reader->sampleRate;

    // wrap reader as an audio source
    // true means the source owns and deletes the reader so release it
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    // swap the new file into the transport
    // file sample rate lets it resample so speed and pitch stay right
    transportSource.stop();
    transportSource.setSource (newSource.get(), 0, nullptr, fileSampleRate);

    // keep new source alive old one gets deleted here safely
    readerSource = std::move (newSource);
    return true;
}

bool AudioPlayer::hasFileLoaded() const { return readerSource != nullptr; }

bool AudioPlayer::play()
{
    if (! hasFileLoaded())
        return false;

    // restart if song already ended
    if (transportSource.hasStreamFinished())
        transportSource.setPosition (0.0);

    transportSource.start();
    return true;
}

bool AudioPlayer::playFrom (double seconds)
{
    if (! hasFileLoaded())
        return false;

    setPosition (seconds);
    transportSource.start();
    return true;
}

void AudioPlayer::pause()           { transportSource.stop(); }
bool AudioPlayer::isPlaying() const   { return transportSource.isPlaying(); }
bool AudioPlayer::hasFinished() const { return transportSource.hasStreamFinished(); }

void AudioPlayer::setPosition (double seconds)
{
    // clamp to 0 through song length
    transportSource.setPosition (juce::jlimit (0.0, getLengthInSeconds(), seconds));
}

double AudioPlayer::getPosition() const        { return transportSource.getCurrentPosition(); }
double AudioPlayer::getLengthInSeconds() const { return transportSource.getLengthInSeconds(); }

void AudioPlayer::setVolume (float newGain)
{
    // clamp to 0 through 1
    transportSource.setGain (juce::jlimit (0.0f, 1.0f, newGain));
}

float AudioPlayer::getVolume() const { return transportSource.getGain(); }

bool AudioPlayer::convertToWav (const juce::File& sourceFile, const juce::File& destinationWavFile)
{
    // open input
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (sourceFile));
    if (reader == nullptr)
        return false;

    // overwrite old output and open a stream
    destinationWavFile.deleteFile();
    std::unique_ptr<juce::OutputStream> outStream = destinationWavFile.createOutputStream();
    if (outStream == nullptr)
        return false; // bad folder or no permission

    // same sample rate and channels as input 16 bit
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (reader->sampleRate)
                             .withNumChannels ((int) reader->numChannels)
                             .withBitsPerSample (16);

    // writer takes the stream if it succeeds otherwise we still own it so no leak
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer = wavFormat.createWriterFor (outStream, options);
    if (writer == nullptr)
        return false;

    // copy all samples in chunks so big songs dont fill memory
    // writer closes the file when it goes out of scope
    return writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples);
}

juce::AudioFormatManager& AudioPlayer::getFormats() { return formatManager; }
