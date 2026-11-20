#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Transition.h"

inline const juce::StringArray& getExplicitWordList()
{
    static const juce::StringArray words =
        juce::StringArray::fromTokens("asshole bastard bitch crap damn fuck hell shit", " ", "");
    return words;
}

struct Song
{
    juce::String title;
    juce::String artist;
    juce::String album;
    juce::String genre;
    juce::String filePath;
    juce::String transcript;
    int          releaseYear     = 0; // 0 = unknown
    juce::Time              dateAdded;
    int                     durationSeconds = 0; // 0 = unknown
    juce::Array<Transition> transitions;
    juce::Image             image;     // isNull() when the song has no picture
    juce::File              imageFile; // copy of the picture in the library images folder

    juce::StringArray getExplicitWords() const
    {
        const auto text = transcript.toLowerCase();
        juce::StringArray found;
        juce::String token;

        auto addToken = [&]
        {
            if (getExplicitWordList().contains(token) && !found.contains(token))
                found.add(token);
            token.clear();
        };

        for (auto character = text.getCharPointer(); !character.isEmpty(); ++character)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(*character))
                token += *character;
            else
                addToken();
        }
        addToken();
        return found;
    }

    bool isExplicit() const { return !getExplicitWords().isEmpty(); }
};

inline bool songsHaveSameIdentity(const Song& first, const Song& second)
{
    if (first.filePath.isNotEmpty() && second.filePath.isNotEmpty())
        return juce::File(first.filePath).getFullPathName()
            .equalsIgnoreCase(juce::File(second.filePath).getFullPathName());

    return first.title.equalsIgnoreCase(second.title)
        && first.artist.equalsIgnoreCase(second.artist)
        && first.album.equalsIgnoreCase(second.album);
}

inline juce::String getExplicitWordListText()
{
    return getExplicitWordList().joinIntoString(", ");
}

inline juce::String formatSongDuration(int secs)
{
    if (secs <= 0) return "--:--";
    return juce::String(secs / 60) + ":" + juce::String(secs % 60).paddedLeft('0', 2);
}

inline int parseSongDuration(const juce::String& s)
{
    auto parts = juce::StringArray::fromTokens(s.trim(), ":", "");
    if (parts.size() == 2)  return parts[0].getIntValue() * 60 + parts[1].getIntValue();
    if (parts.size() == 1)  return parts[0].getIntValue();
    return 0;
}
