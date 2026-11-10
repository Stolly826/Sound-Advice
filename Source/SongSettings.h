// SongSettings.h : the playback controls that belong to one song

#pragma once

#include <juce_core/juce_core.h>

//every song keeps its own copy of these so it sounds the same each time it plays
struct SongSettings
{
    static constexpr double minSpeed = 0.5;
    static constexpr double maxSpeed = 1.5;
    static constexpr double maxGainDb = 12.0;
    static constexpr double maxEqDb = 12.0;

    double speed = 1.0;     //1 is normal, changes the pitch too like a turntable
    double volume = 0.75;   //0 to 1, turns the song down from its normal level
    double gainDb = 0.0;    //trim in dB, unlike volume it can also boost a quiet song
    double bassDb = 0.0;
    double midsDb = 0.0;
    double trebleDb = 0.0;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("speed", speed);
        obj->setProperty("volume", volume);
        obj->setProperty("gainDb", gainDb);
        obj->setProperty("bassDb", bassDb);
        obj->setProperty("midsDb", midsDb);
        obj->setProperty("trebleDb", trebleDb);
        return juce::var(obj);
    }

    //opposite of toVar, anything missing keeps its default and anything out of range gets pulled back in
    static SongSettings fromVar(const juce::var& v)
    {
        SongSettings s;
        s.speed = juce::jlimit(minSpeed, maxSpeed, (double)v.getProperty("speed", s.speed));
        s.volume = juce::jlimit(0.0, 1.0, (double)v.getProperty("volume", s.volume));
        s.gainDb = juce::jlimit(-maxGainDb, maxGainDb, (double)v.getProperty("gainDb", s.gainDb));
        s.bassDb = juce::jlimit(-maxEqDb, maxEqDb, (double)v.getProperty("bassDb", s.bassDb));
        s.midsDb = juce::jlimit(-maxEqDb, maxEqDb, (double)v.getProperty("midsDb", s.midsDb));
        s.trebleDb = juce::jlimit(-maxEqDb, maxEqDb, (double)v.getProperty("trebleDb", s.trebleDb));
        return s;
    }
};
