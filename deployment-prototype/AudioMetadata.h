// AudioMetadata.h : Thin wrapper around TagLib for reading audio file tags
// and properties. Not used by the app yet, ready for when we need it.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct AudioMetadata
{
    // Tag info (empty / 0 if the file has no tag for it)
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring genre;
    std::wstring comment;
    unsigned int year = 0;
    unsigned int track = 0;

    // Audio properties (0 if TagLib couldn't read them)
    int lengthMs = 0;
    int bitrateKbps = 0;
    int sampleRate = 0;
    int channels = 0;
};

// Returns std::nullopt if the file can't be opened or isn't a format TagLib supports.
std::optional<AudioMetadata> readAudioMetadata(const std::filesystem::path& file);
