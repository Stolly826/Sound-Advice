#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"
#include <algorithm>
#include <vector>

// how a song list is shown
// custom is the order the songs are stored in, the user changes it by dragging rows
enum class SongSortMode { custom, title, artist, genre, newest, oldest, bpmSlowest, bpmFastest };

// saved to json as these words so the file stays readable
inline juce::String songSortModeToId(SongSortMode mode)
{
    switch (mode)
    {
        case SongSortMode::title:  return "title";
        case SongSortMode::artist: return "artist";
        case SongSortMode::genre:  return "genre";
        case SongSortMode::newest: return "newest";
        case SongSortMode::oldest: return "oldest";
        case SongSortMode::bpmSlowest: return "bpmSlowest";
        case SongSortMode::bpmFastest: return "bpmFastest";
        case SongSortMode::custom: break;
    }
    return "custom";
}

inline SongSortMode songSortModeFromId(const juce::String& id)
{
    for (auto mode : { SongSortMode::title, SongSortMode::artist, SongSortMode::genre,
                       SongSortMode::newest, SongSortMode::oldest,
                       SongSortMode::bpmSlowest, SongSortMode::bpmFastest })
        if (id == songSortModeToId(mode))
            return mode;

    return SongSortMode::custom; // empty or unknown
}

// fills a dropdown with every sort option, item ids are the mode + 1
inline void fillSongSortBox(juce::ComboBox& box)
{
    box.clear(juce::dontSendNotification);
    box.addItem("Custom order", (int) SongSortMode::custom + 1);
    box.addItem("Title (A-Z)",  (int) SongSortMode::title  + 1);
    box.addItem("Artist (A-Z)", (int) SongSortMode::artist + 1);
    box.addItem("Genre (A-Z)",  (int) SongSortMode::genre  + 1);
    box.addItem("Newest first", (int) SongSortMode::newest + 1);
    box.addItem("Oldest first", (int) SongSortMode::oldest + 1);
    box.addItem("BPM (slowest first)", (int) SongSortMode::bpmSlowest + 1);
    box.addItem("BPM (fastest first)", (int) SongSortMode::bpmFastest + 1);
}

inline void selectSongSortMode(juce::ComboBox& box, SongSortMode mode)
{
    box.setSelectedId((int) mode + 1, juce::dontSendNotification);
}

inline SongSortMode getSelectedSongSortMode(const juce::ComboBox& box)
{
    return (SongSortMode) juce::jlimit(0, (int) SongSortMode::bpmFastest, box.getSelectedId() - 1);
}

// indexes into songs in the order they should be shown
// alphabetical sorts ignore case and put songs with that field blank at the end
// bpm sorts put songs with no bpm at the end either way
// ties keep their custom order
inline juce::Array<int> getSortedSongOrder(const juce::Array<Song>& songs, SongSortMode mode)
{
    std::vector<int> order;
    for (int i = 0; i < songs.size(); ++i)
        order.push_back(i);

    auto alphabetical = [&songs](auto field)
    {
        return [&songs, field](int a, int b)
        {
            const auto& x = field(songs.getReference(a));
            const auto& y = field(songs.getReference(b));
            if (x.isEmpty() != y.isEmpty())
                return y.isEmpty(); // blanks last
            return x.compareNatural(y, false) < 0;
        };
    };

    switch (mode)
    {
        case SongSortMode::title:
            std::stable_sort(order.begin(), order.end(), alphabetical([](const Song& s) -> const juce::String& { return s.title; }));
            break;
        case SongSortMode::artist:
            std::stable_sort(order.begin(), order.end(), alphabetical([](const Song& s) -> const juce::String& { return s.artist; }));
            break;
        case SongSortMode::genre:
            std::stable_sort(order.begin(), order.end(), alphabetical([](const Song& s) -> const juce::String& { return s.genre; }));
            break;
        case SongSortMode::newest:
            std::stable_sort(order.begin(), order.end(), [&songs](int a, int b)
                { return songs.getReference(a).dateAdded > songs.getReference(b).dateAdded; });
            break;
        case SongSortMode::oldest:
            std::stable_sort(order.begin(), order.end(), [&songs](int a, int b)
                { return songs.getReference(a).dateAdded < songs.getReference(b).dateAdded; });
            break;
        case SongSortMode::bpmSlowest:
        case SongSortMode::bpmFastest:
        {
            const bool slowestFirst = mode == SongSortMode::bpmSlowest;
            std::stable_sort(order.begin(), order.end(), [&songs, slowestFirst](int a, int b)
            {
                const auto x = songs.getReference(a).bpm;
                const auto y = songs.getReference(b).bpm;
                if ((x > 0.0) != (y > 0.0))
                    return x > 0.0; // unknown last
                return slowestFirst ? x < y : x > y;
            });
            break;
        }
        case SongSortMode::custom:
            break;
    }

    juce::Array<int> result;
    for (int i : order)
        result.add(i);
    return result;
}
