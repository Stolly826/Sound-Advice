// AudioMetadata.cpp : TagLib implementation of readAudioMetadata.

#include "AudioMetadata.h"

#include <fileref.h>
#include <tag.h>
#include <audioproperties.h>

std::optional<AudioMetadata> readAudioMetadata(const std::filesystem::path& file)
{
    // path::c_str() is wchar_t on Windows and char elsewhere, which matches TagLib::FileName
    TagLib::FileRef ref(file.c_str());
    if (ref.isNull())
        return std::nullopt;

    AudioMetadata meta;

    if (const TagLib::Tag* tag = ref.tag())
    {
        meta.title = tag->title().toWString();
        meta.artist = tag->artist().toWString();
        meta.album = tag->album().toWString();
        meta.genre = tag->genre().toWString();
        meta.comment = tag->comment().toWString();
        meta.year = tag->year();
        meta.track = tag->track();
    }

    if (const TagLib::AudioProperties* props = ref.audioProperties())
    {
        meta.lengthMs = props->lengthInMilliseconds();
        meta.bitrateKbps = props->bitrate();
        meta.sampleRate = props->sampleRate();
        meta.channels = props->channels();
    }

    return meta;
}
