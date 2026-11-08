// SongData.h : reads song details (TagLib + JUCE) and saves them to a library file

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>
#include <algorithm>
#include <cmath>
#include <vector>

//everything we know about one uploaded song
struct SongInfo
{
    juce::String path;
    juce::String title;
    juce::String artist;
    juce::String album;
    double lengthSeconds = 0.0;
    double bpm = 0.0;           //0 means we couldnt figure it out
    bool bpmFromTag = false;    //true if the bpm came from the file tags, false if we estimated it
    double peakDb = -100.0;     //loudest sample in dBFS (0 = max before clipping)
    double rmsDb = -100.0;      //average loudness in dBFS
    int sampleRate = 0;
    int channels = 0;
    int bitrate = 0;            //kbps
    juce::String addedAt;

    juce::String formatLength() const
    {
        int total = (int)std::lround(lengthSeconds);
        return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2);
    }

    //multi line summary for showing in the window
    juce::String describe() const
    {
        juce::String bpmText = bpm > 0.0
            ? juce::String(bpm, 1) + (bpmFromTag ? "" : " (estimated)")
            : juce::String("unknown");

        return "Title: " + title + "\n"
            + "Artist: " + artist + "\n"
            + "Album: " + (album.isEmpty() ? juce::String("-") : album) + "\n"
            + "Length: " + formatLength() + "\n"
            + "BPM: " + bpmText + "\n"
            + "Volume: " + juce::String(rmsDb, 1) + " dB RMS, " + juce::String(peakDb, 1) + " dB peak\n"
            + "Format: " + juce::String(sampleRate) + " Hz, " + juce::String(channels) + " ch, "
            + juce::String(bitrate) + " kbps";
    }

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("path", path);
        obj->setProperty("title", title);
        obj->setProperty("artist", artist);
        obj->setProperty("album", album);
        obj->setProperty("lengthSeconds", lengthSeconds);
        obj->setProperty("bpm", bpm);
        obj->setProperty("bpmFromTag", bpmFromTag);
        obj->setProperty("peakDb", peakDb);
        obj->setProperty("rmsDb", rmsDb);
        obj->setProperty("sampleRate", sampleRate);
        obj->setProperty("channels", channels);
        obj->setProperty("bitrate", bitrate);
        obj->setProperty("addedAt", addedAt);
        return juce::var(obj);
    }

    //opposite of toVar, turns a saved JSON entry back into a SongInfo
    static SongInfo fromVar(const juce::var& v)
    {
        SongInfo info;
        info.path = v.getProperty("path", "").toString();
        info.title = v.getProperty("title", "").toString();
        info.artist = v.getProperty("artist", "").toString();
        info.album = v.getProperty("album", "").toString();
        info.lengthSeconds = (double)v.getProperty("lengthSeconds", 0.0);
        info.bpm = (double)v.getProperty("bpm", 0.0);
        info.bpmFromTag = (bool)v.getProperty("bpmFromTag", false);
        info.peakDb = (double)v.getProperty("peakDb", -100.0);
        info.rmsDb = (double)v.getProperty("rmsDb", -100.0);
        info.sampleRate = (int)v.getProperty("sampleRate", 0);
        info.channels = (int)v.getProperty("channels", 0);
        info.bitrate = (int)v.getProperty("bitrate", 0);
        info.addedAt = v.getProperty("addedAt", "").toString();
        return info;
    }
};

namespace SongData
{
    inline juce::String fromTagLib(const TagLib::String& s)
    {
        return juce::String::fromUTF8(s.toCString(true));
    }

    //guesses the tempo by looking for the most regular spacing between volume spikes (beats)
    inline double estimateBpm(const std::vector<float>& onsets, double framesPerSecond)
    {
        const double minBpm = 70.0, maxBpm = 180.0;
        int minLag = (int)std::floor(framesPerSecond * 60.0 / maxBpm);
        int maxLag = (int)std::ceil(framesPerSecond * 60.0 / minBpm);

        if (minLag < 1 || (int)onsets.size() < maxLag * 4)
            return 0.0; //song too short to say anything useful

        //autocorrelation: how well does the onset pattern line up with itself shifted by "lag" frames
        std::vector<double> scores(maxLag + 2, 0.0);
        for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
        {
            double sum = 0.0;
            for (size_t i = 0; i + lag < onsets.size(); ++i)
                sum += onsets[i] * onsets[i + lag];
            scores[lag] = sum / (double)(onsets.size() - lag);
        }

        int best = minLag;
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (scores[lag] > scores[best])
                best = lag;

        if (scores[best] <= 0.0)
            return 0.0;

        //parabolic interpolation so we arent stuck to whole frame lags
        double a = scores[best - 1], b = scores[best], c = scores[best + 1];
        double denom = a - 2.0 * b + c;
        double offset = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;

        return 60.0 * framesPerSecond / (best + offset);
    }

    //reads the whole file once with JUCE to get loudness and an onset envelope for bpm
    inline void analyseAudio(juce::AudioFormatManager& formats, const juce::File& file, SongInfo& info)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader || reader->sampleRate <= 0.0)
            return;

        if (info.lengthSeconds <= 0.0)
            info.lengthSeconds = (double)reader->lengthInSamples / reader->sampleRate;
        if (info.sampleRate == 0)
            info.sampleRate = (int)reader->sampleRate;
        if (info.channels == 0)
            info.channels = (int)reader->numChannels;

        const int hop = 512; //samples per envelope frame (~86 frames a second at 44.1kHz)
        const int framesPerBlock = 128;
        const int numChannels = (int)reader->numChannels;
        juce::AudioBuffer<float> buffer(numChannels, hop * framesPerBlock);

        std::vector<float> energy;
        energy.reserve((size_t)(reader->lengthInSamples / hop) + 1);
        double sumSquares = 0.0;
        float peak = 0.0f;

        for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += buffer.getNumSamples())
        {
            int n = (int)std::min<juce::int64>(buffer.getNumSamples(), reader->lengthInSamples - pos);
            buffer.clear();
            reader->read(&buffer, 0, n, pos, true, true);

            for (int start = 0; start < n; start += hop)
            {
                int len = std::min(hop, n - start);
                double frameSum = 0.0;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* data = buffer.getReadPointer(ch, start);
                    for (int i = 0; i < len; ++i)
                    {
                        frameSum += (double)data[i] * data[i];
                        peak = std::max(peak, std::abs(data[i]));
                    }
                }
                sumSquares += frameSum;
                energy.push_back((float)std::sqrt(frameSum / (len * numChannels)));
            }
        }

        double totalSamples = (double)reader->lengthInSamples * numChannels;
        if (totalSamples > 0.0)
            info.rmsDb = juce::Decibels::gainToDecibels(std::sqrt(sumSquares / totalSamples), -100.0);
        info.peakDb = juce::Decibels::gainToDecibels((double)peak, -100.0);

        if (info.bpm <= 0.0)
        {
            //onset = how much louder this frame got compared to the last one
            std::vector<float> onsets(energy.size(), 0.0f);
            for (size_t i = 1; i < energy.size(); ++i)
                onsets[i] = std::max(0.0f, energy[i] - energy[i - 1]);

            info.bpm = estimateBpm(onsets, reader->sampleRate / hop);
            info.bpmFromTag = false;
        }
    }

    //TagLib gives us the tags (title, artist, bpm) and JUCE fills in the rest
    inline SongInfo readSong(juce::AudioFormatManager& formats, const juce::File& file)
    {
        SongInfo info;
        info.path = file.getFullPathName();
        info.addedAt = juce::Time::getCurrentTime().toISO8601(true);

#ifdef _WIN32
        TagLib::FileRef ref(file.getFullPathName().toWideCharPointer());
#else
        TagLib::FileRef ref(file.getFullPathName().toRawUTF8());
#endif

        if (!ref.isNull() && ref.tag())
        {
            info.title = fromTagLib(ref.tag()->title()).trim();
            info.artist = fromTagLib(ref.tag()->artist()).trim();
            info.album = fromTagLib(ref.tag()->album()).trim();

            //"BPM" is the same key for ID3 (TBPM), FLAC/Vorbis comments and MP4
            TagLib::PropertyMap props = ref.file()->properties();
            if (props.contains("BPM") && !props["BPM"].isEmpty())
            {
                double tagged = fromTagLib(props["BPM"].front()).getDoubleValue();
                if (tagged > 0.0)
                {
                    info.bpm = tagged;
                    info.bpmFromTag = true;
                }
            }
        }

        if (!ref.isNull() && ref.audioProperties())
        {
            auto* props = ref.audioProperties();
            info.lengthSeconds = props->lengthInMilliseconds() / 1000.0;
            info.sampleRate = props->sampleRate();
            info.channels = props->channels();
            info.bitrate = props->bitrate();
        }

        if (info.title.isEmpty())
            info.title = file.getFileNameWithoutExtension();
        if (info.artist.isEmpty())
            info.artist = "Unknown Artist";

        analyseAudio(formats, file, info);
        return info;
    }
}

//keeps every uploaded song in memory and in a JSON file so the details stick around between runs
//loads library.json when created, then any page can ask for the songs and use whichever details it needs:
//  song list / playlist row -> song.title, song.formatLength()
//  song details page        -> everything (or song.describe() for a ready made summary)
class SongLibrary
{
public:
    SongLibrary()
        : libraryFile(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("SoundAdvice")
                          .getChildFile("library.json"))
    {
        load();
    }

    const juce::File& getFile() const
    {
        return libraryFile;
    }

    //every song in upload order (oldest first)
    const std::vector<SongInfo>& getSongs() const
    {
        return songs;
    }

    int size() const
    {
        return (int)songs.size();
    }

    //nullptr if this file was never uploaded
    const SongInfo* findByPath(const juce::String& path) const
    {
        for (const auto& song : songs)
            if (song.path == path)
                return &song;
        return nullptr;
    }

    //re-reads library.json, throwing away whatever is in memory
    void load()
    {
        songs.clear();

        juce::var root = juce::JSON::parse(libraryFile); //empty if the file doesnt exist yet
        if (auto* list = root.getProperty("songs", juce::var()).getArray())
            for (const auto& entry : *list)
                songs.push_back(SongInfo::fromVar(entry));
    }

    //adds the song, or replaces it if the same file was uploaded before, then writes the file
    bool save(const SongInfo& song)
    {
        songs.erase(std::remove_if(songs.begin(), songs.end(),
                                   [&](const SongInfo& s) { return s.path == song.path; }),
                    songs.end());
        songs.push_back(song);

        return writeToDisk();
    }

private:
    bool writeToDisk() const
    {
        juce::Array<juce::var> list;
        for (const auto& song : songs)
            list.add(song.toVar());

        auto* root = new juce::DynamicObject();
        root->setProperty("songs", list);

        if (!libraryFile.getParentDirectory().createDirectory())
            return false;
        return libraryFile.replaceWithText(juce::JSON::toString(juce::var(root)));
    }

    juce::File libraryFile;
    std::vector<SongInfo> songs;
};
