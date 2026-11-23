#include "LocalTranscriber.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <whisper.h>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    constexpr double kWhisperSampleRate = 16000.0;
}

juce::Result LocalTranscriber::transcribe(const juce::File& audioFile,
                                          const juce::File& modelFile,
                                          juce::String& transcript)
{
    if (!audioFile.existsAsFile())
        return juce::Result::fail("The song's audio file could not be found.");

    if (!modelFile.existsAsFile())
        return juce::Result::fail("The selected Whisper model file could not be found.");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (!reader)
        return juce::Result::fail("The audio file format is not supported.");

    if (reader->sampleRate <= 0.0 || reader->numChannels == 0
        || reader->lengthInSamples <= 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        return juce::Result::fail("The audio file has invalid or unsupported dimensions.");

    const int numSamples = static_cast<int>(reader->lengthInSamples);
    const int numChannels = static_cast<int>(reader->numChannels);
    juce::AudioBuffer<float> source(numChannels, numSamples);
    if (!reader->read(source.getArrayOfWritePointers(), numChannels, 0, numSamples))
        return juce::Result::fail("The audio file could not be decoded.");

    std::vector<float> mono(static_cast<size_t>(numSamples) + 4, 0.0f);
    const float channelScale = 1.0f / static_cast<float>(numChannels);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* samples = source.getReadPointer(channel);
        for (int i = 0; i < numSamples; ++i)
            mono[static_cast<size_t>(i)] += samples[i] * channelScale;
    }

    const auto outputSamples64 = static_cast<juce::int64>(std::llround(
        static_cast<double>(numSamples) * kWhisperSampleRate / reader->sampleRate));
    if (outputSamples64 <= 0 || outputSamples64 > std::numeric_limits<int>::max())
        return juce::Result::fail("The audio file is too long to transcribe.");

    std::vector<float> resampled(static_cast<size_t>(outputSamples64));
    juce::LagrangeInterpolator resampler;
    resampler.process(reader->sampleRate / kWhisperSampleRate,
                      mono.data(), resampled.data(), static_cast<int>(outputSamples64));

    auto contextParams = whisper_context_default_params();
    contextParams.use_gpu = false;
    std::unique_ptr<whisper_context, decltype(&whisper_free)> context(
        whisper_init_from_file_with_params(modelFile.getFullPathName().toRawUTF8(), contextParams),
        &whisper_free);
    if (!context)
        return juce::Result::fail("Whisper could not load this model. Choose a valid GGML model file.");

    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = juce::jlimit(1, 8, juce::SystemStats::getNumCpus());
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.language = "auto";

    if (whisper_full(context.get(), params, resampled.data(),
                     static_cast<int>(resampled.size())) != 0)
        return juce::Result::fail("Whisper could not transcribe this audio file.");

    transcript.clear();
    for (int i = 0; i < whisper_full_n_segments(context.get()); ++i)
    {
        const auto* segment = whisper_full_get_segment_text(context.get(), i);
        if (segment != nullptr)
        {
            const auto text = juce::String::fromUTF8(segment).trim();
            if (text.isNotEmpty())
            {
                if (transcript.isNotEmpty())
                    transcript << "\n";
                transcript << text;
            }
        }
    }

    if (transcript.isEmpty())
        return juce::Result::fail("Whisper did not detect any speech in this song.");

    return juce::Result::ok();
}
