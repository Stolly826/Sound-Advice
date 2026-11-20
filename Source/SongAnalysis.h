// SongAnalysis.h : works out how a song's volume, tempo and pitch change over time for the charts

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

// one value per point across the song, so the chart can draw it at any width
struct SongAnalysis
{
    static constexpr int kPoints = 240;

    std::vector<float> volumeDb;  // loudness of that part of the song, -100 = silent
    std::vector<float> bpm;       // local tempo, 0 = no steady beat there
    std::vector<float> pitchNote; // strongest note as a MIDI number (69 = A4), 0 = no clear note
    double lengthSeconds = 0.0;
    float  songBpm = 0.0f;        // tempo of the whole song, the bpm line is drawn around this
    float  lowNote = 0.0f, highNote = 0.0f; // usual pitch range, the pitch line is scaled to fit it
};

namespace SongAnalyser
{
    // tempo from how regularly the volume spikes (beats) repeat, same idea as SongData::estimateBpm
    inline float tempoOf(const float* onsets, size_t count, double framesPerSecond)
    {
        const double minBpm = 70.0, maxBpm = 180.0;
        const auto minLag = (size_t)std::floor(framesPerSecond * 60.0 / maxBpm);
        const auto maxLag = (size_t)std::ceil(framesPerSecond * 60.0 / minBpm);
        if (minLag < 2 || count < maxLag * 3)
            return 0.0f;

        std::vector<double> scores(maxLag + 2, 0.0);
        for (size_t lag = minLag - 1; lag <= maxLag + 1; ++lag)
        {
            double sum = 0.0;
            for (size_t i = 0; i + lag < count; ++i)
                sum += onsets[i] * onsets[i + lag];
            scores[lag] = sum / (double)(count - lag);
        }

        size_t best = minLag;
        for (size_t lag = minLag; lag <= maxLag; ++lag)
            if (scores[lag] > scores[best])
                best = lag;

        if (scores[best] <= 0.0)
            return 0.0f;

        const double a = scores[best - 1], b = scores[best], c = scores[best + 1];
        const double denom = a - 2.0 * b + c;
        const double offset = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
        return (float)(60.0 * framesPerSecond / ((double)best + offset));
    }

    // a tempo twice or half the song's tempo is almost always the same beat counted differently
    inline float matchOctave(float local, float song)
    {
        if (local <= 0.0f || song <= 0.0f)
            return local;
        for (float candidate : { local * 2.0f, local * 0.5f })
            if (std::abs(candidate - song) < std::abs(local - song))
                local = candidate;
        return local;
    }

    // smooths out one-off wrong guesses, ignoring the gaps (zeros)
    inline std::vector<float> medianSmooth(const std::vector<float>& values, int radius)
    {
        std::vector<float> out(values.size(), 0.0f), window;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (values[i] <= 0.0f)
                continue;
            window.clear();
            for (int d = -radius; d <= radius; ++d)
            {
                const auto j = (long)i + d;
                if (j >= 0 && j < (long)values.size() && values[(size_t)j] > 0.0f)
                    window.push_back(values[(size_t)j]);
            }
            std::nth_element(window.begin(), window.begin() + (long)window.size() / 2, window.end());
            out[i] = window[window.size() / 2];
        }
        return out;
    }

    inline float percentile(std::vector<float> values, float fraction)
    {
        values.erase(std::remove_if(values.begin(), values.end(), [](float v) { return v <= 0.0f; }),
                     values.end());
        if (values.empty())
            return 0.0f;
        const auto k = (size_t)juce::jlimit(0.0f, (float)values.size() - 1.0f, fraction * (float)(values.size() - 1));
        std::nth_element(values.begin(), values.begin() + (long)k, values.end());
        return values[k];
    }

    // reads the whole file once, slow (about a second for a normal song) so it runs off the UI thread
    // shouldStop lets the app quit in the middle
    inline std::shared_ptr<SongAnalysis> analyse(const juce::File& file, const std::function<bool()>& shouldStop)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return nullptr;

        const double sampleRate = reader->sampleRate;
        const int numChannels = (int)juce::jmax(1u, reader->numChannels);
        const juce::int64 totalSamples = reader->lengthInSamples;

        // volume and beats from short frames, pitch from longer FFT frames
        constexpr int hop = 512;
        constexpr int fftOrder = 13;
        constexpr int fftSize = 1 << fftOrder;   // 8192, about 0.19 s, enough detail for bass notes
        constexpr int fftEvery = fftSize / hop / 2; // a pitch reading every half FFT
        juce::dsp::FFT fft(fftOrder);
        juce::dsp::WindowingFunction<float> hann((size_t)fftSize, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> recent((size_t)fftSize, 0.0f);       // last fftSize mono samples
        std::vector<float> fftData((size_t)fftSize * 2, 0.0f);

        std::vector<float> frameEnergy; // mean square per hop
        std::vector<float> framePitch;  // note per FFT frame, aligned to the hop it was taken at
        frameEnergy.reserve((size_t)(totalSamples / hop) + 1);
        framePitch.reserve((size_t)(totalSamples / hop) + 1);

        const int minBin = (int)std::ceil(55.0 * fftSize / sampleRate);   // A1
        const int maxBin = (int)std::floor(1000.0 * fftSize / sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, hop * 64);
        int hopCount = 0;

        for (juce::int64 pos = 0; pos < totalSamples; pos += buffer.getNumSamples())
        {
            if (shouldStop())
                return nullptr;

            const int n = (int)std::min<juce::int64>(buffer.getNumSamples(), totalSamples - pos);
            buffer.clear();
            reader->read(&buffer, 0, n, pos, true, true);

            for (int start = 0; start + hop <= n || (start < n && pos + n >= totalSamples); start += hop)
            {
                const int len = std::min(hop, n - start);
                std::memmove(recent.data(), recent.data() + hop, sizeof(float) * (size_t)(fftSize - hop));
                float* tail = recent.data() + fftSize - hop;
                double sumSquares = 0.0;
                for (int i = 0; i < hop; ++i)
                {
                    float mono = 0.0f;
                    if (i < len)
                        for (int ch = 0; ch < numChannels; ++ch)
                            mono += buffer.getSample(ch, start + i);
                    mono /= (float)numChannels;
                    tail[i] = mono;
                    sumSquares += (double)mono * mono;
                }
                frameEnergy.push_back((float)(sumSquares / juce::jmax(1, len)));

                float note = 0.0f;
                if (++hopCount % fftEvery == 0 && hopCount * hop >= fftSize)
                {
                    std::copy(recent.begin(), recent.end(), fftData.begin());
                    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
                    hann.multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);
                    fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

                    // harmonic product: a real note also has energy at 2x and 3x its frequency,
                    // which stops a loud overtone being picked instead of the note itself
                    int bestBin = -1;
                    double bestScore = 0.0;
                    for (int k = minBin; k <= maxBin && k * 3 < fftSize / 2; ++k)
                    {
                        const double score = std::log1p(fftData[(size_t)k])
                                           + std::log1p(fftData[(size_t)k * 2])
                                           + std::log1p(fftData[(size_t)k * 3]);
                        if (score > bestScore) { bestScore = score; bestBin = k; }
                    }

                    if (bestBin > minBin && bestBin < maxBin)
                    {
                        const double a = fftData[(size_t)bestBin - 1], b = fftData[(size_t)bestBin], c = fftData[(size_t)bestBin + 1];
                        const double denom = a - 2.0 * b + c;
                        const double offset = denom != 0.0 ? juce::jlimit(-0.5, 0.5, 0.5 * (a - c) / denom) : 0.0;
                        const double hz = (bestBin + offset) * sampleRate / fftSize;
                        note = (float)(69.0 + 12.0 * std::log2(hz / 440.0));
                    }
                }
                framePitch.push_back(note);
            }
        }

        const size_t numFrames = frameEnergy.size();
        if (numFrames == 0)
            return nullptr;

        const double framesPerSecond = sampleRate / hop;
        auto result = std::make_shared<SongAnalysis>();
        result->lengthSeconds = (double)totalSamples / sampleRate;

        // onset strength: how much louder each frame got than the one before
        std::vector<float> onsets(numFrames, 0.0f);
        std::vector<float> level(numFrames);
        for (size_t i = 0; i < numFrames; ++i)
            level[i] = std::sqrt(frameEnergy[i]);
        for (size_t i = 1; i < numFrames; ++i)
            onsets[i] = std::max(0.0f, level[i] - level[i - 1]);

        result->songBpm = tempoOf(onsets.data(), numFrames, framesPerSecond);

        const int points = SongAnalysis::kPoints;
        const auto tempoHalfWindow = (size_t)(5.0 * framesPerSecond); // 10 s of beats per tempo reading
        std::vector<float> rawBpm((size_t)points), rawPitch((size_t)points);
        result->volumeDb.resize((size_t)points);

        for (int p = 0; p < points; ++p)
        {
            const auto from = (size_t)((double)p / points * numFrames);
            const auto to = std::max(from + 1, (size_t)((double)(p + 1) / points * numFrames));

            double energy = 0.0;
            std::vector<float> notes;
            for (size_t i = from; i < to && i < numFrames; ++i)
            {
                energy += frameEnergy[i];
                if (framePitch[i] > 0.0f)
                    notes.push_back(framePitch[i]);
            }
            energy /= (double)(to - from);
            const float db = (float)juce::Decibels::gainToDecibels(std::sqrt(energy), -100.0);
            result->volumeDb[(size_t)p] = db;

            const bool audible = db > -45.0f; // no pitch or tempo in near silence
            rawPitch[(size_t)p] = audible && !notes.empty() ? percentile(notes, 0.5f) : 0.0f;

            const size_t centre = (from + to) / 2;
            const size_t lo = centre > tempoHalfWindow ? centre - tempoHalfWindow : 0;
            const size_t hi = std::min(numFrames, centre + tempoHalfWindow);
            rawBpm[(size_t)p] = audible ? matchOctave(tempoOf(onsets.data() + lo, hi - lo, framesPerSecond),
                                                      result->songBpm)
                                        : 0.0f;
        }

        result->bpm = medianSmooth(rawBpm, 4);
        result->pitchNote = medianSmooth(rawPitch, 2);
        result->lowNote = percentile(result->pitchNote, 0.05f);
        result->highNote = percentile(result->pitchNote, 0.95f);
        if (result->highNote - result->lowNote < 6.0f) // dont blow small wobbles up to full height
        {
            const float mid = 0.5f * (result->lowNote + result->highNote);
            result->lowNote = mid - 3.0f;
            result->highNote = mid + 3.0f;
        }
        return result;
    }

    // runs analyses one at a time in the background and remembers them for the rest of the session
    class Service : private juce::DeletedAtShutdown
    {
    public:
        using Callback = std::function<void(std::shared_ptr<const SongAnalysis>)>;

        ~Service() override
        {
            pool.removeAllJobs(true, 4000);
            clearSingletonInstance();
        }

        // onDone runs on the message thread, straight away if this file was already analysed
        // nullptr means the file couldnt be read
        void request(const juce::String& path, Callback onDone)
        {
            const juce::ScopedLock sl(lock);
            if (auto found = done.find(path); found != done.end())
            {
                auto result = found->second;
                juce::MessageManager::callAsync([onDone, result] { onDone(result); });
                return;
            }

            auto& queue = waiting[path];
            queue.push_back(std::move(onDone));
            if (queue.size() == 1)
                pool.addJob(new Job(*this, path), true);
        }

        JUCE_DECLARE_SINGLETON_INLINE(Service, true)

    private:
        Service() = default;

        class Job : public juce::ThreadPoolJob
        {
        public:
            Job(Service& s, const juce::String& p) : juce::ThreadPoolJob("song analysis"), service(s), path(p) {}

            JobStatus runJob() override
            {
                std::shared_ptr<const SongAnalysis> result = analyse(juce::File(path), [this] { return shouldExit(); });
                if (!shouldExit())
                    service.finish(path, result);
                return jobHasFinished;
            }

        private:
            Service& service;
            juce::String path;
        };

        void finish(const juce::String& path, std::shared_ptr<const SongAnalysis> result)
        {
            std::vector<Callback> callbacks;
            {
                const juce::ScopedLock sl(lock);
                done[path] = result;
                callbacks = std::move(waiting[path]);
                waiting.erase(path);
            }
            juce::MessageManager::callAsync([callbacks, result]
            {
                for (const auto& callback : callbacks)
                    callback(result);
            });
        }

        juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads(1) };
        juce::CriticalSection lock;
        std::map<juce::String, std::shared_ptr<const SongAnalysis>> done;
        std::map<juce::String, std::vector<Callback>> waiting;
    };
}
