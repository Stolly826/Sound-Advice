#include "MixingPage.h"

static constexpr int kPad       = 16;
static constexpr int kChartH    = 165;
static constexpr int kLabelW    = 70;
static constexpr int kSliderH   = 26;
static constexpr int kSliderGap = 4;
static constexpr int kColGap    = 20;
static constexpr int kSepGap    = 10;

static int songSeconds(const Playlist* playlist, int index)
{
    return playlist != nullptr && juce::isPositiveAndBelow(index, playlist->songs.size())
         ? playlist->songs[index].durationSeconds : 0;
}

// with a fade in start but no end, the fade in lasts as many seconds as the fade out
static float fadeInEndOrMatch(float inStart, float inEnd, float outStart, float outEnd,
                              int outSeconds, int inSeconds)
{
    if (inStart < 0.0f || inEnd > inStart || outStart < 0.0f || outEnd <= outStart)
        return inEnd;

    const double ratio = outSeconds > 0 && inSeconds > 0 ? (double)outSeconds / inSeconds : 1.0;
    return (float)juce::jmin(1.0, inStart + (outEnd - outStart) * ratio);
}

// ── Construction ──────────────────────────────────────────────────────────────

MixingPage::MixingPage()
{
    titleLabel.setText("Mixing", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    playButton.setButtonText("");
    playButton.getProperties().set("iconShape", "play");
    addAndMakeVisible(playButton);

    // FRONTEND/BACKEND INTEGRATION: one button plays, pauses and resumes
    playButton.onClick = [this]
        {
            // Make sure a valid song is selected from a playlist
            const auto* song = getCurrentSong();
            if (song == nullptr)
                return;

            const bool loaded = currentSongIsLoaded();
            if (loaded && isPlaybackActive && isPlaybackActive())
            {
                if (onPause) onPause();
            }
            else if (loaded && isPlaybackPaused && isPlaybackPaused())
            {
                if (onResume) onResume(); // also brings back a transition that was paused
            }
            else if (!playCurrentSong()) // Ask MainComponent to play the selected audio file
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Unable to play song",
                    "This song does not have a playable MP3 or WAV file.");
            }

            updatePlayButtonIcon();
        };

    stopButton.onClick = [this]
        {
            if (onStop) onStop();
            updatePlayButtonIcon();
        };
    addAndMakeVisible(stopButton);

    recordButton.setButtonText("");
    recordButton.getProperties().set("iconShape", "record");
    addAndMakeVisible(recordButton);

    addAndMakeVisible(recordingsButton);
    chart2.setIsNextSong(true);
    chart1.onJumpTo = [this](float t) { jumpTo(t); };
    chart1.onSkip   = [this] { skipToNext(); };
    // a fade placed on one song matches the length of the other song's fade
    chart1.getPartnerFadeSeconds = [this] { return chart2.getFadeSeconds(); };
    chart2.getPartnerFadeSeconds = [this] { return chart1.getFadeSeconds(); };
    // changing either fade changes the other, so the fade out and fade in always match
    chart1.onFadeChanged = [this] { chart2.setFadeSeconds(chart1.getFadeSeconds()); };
    chart2.onFadeChanged = [this] { chart1.setFadeSeconds(chart2.getFadeSeconds()); };
    chart1.onPrevious    = [this] { skipToPrevious(); };
    // markers are remembered per pair of songs, so switching away and back keeps them
    chart1.onMarkersChanged = [this] { saveMixMarkers(); };
    chart2.onMarkersChanged = [this] { saveMixMarkers(); };
    addAndMakeVisible(chart1);
    addAndMakeVisible(chart2);
    transitionPresetsButton.onClick = [this] { showTransitionPresets(); };
    addAndMakeVisible(transitionPresetsButton);

    for (auto* l : { &controlsHeader, &eqHeader })
    {
        l->setFont(juce::FontOptions(11.0f));
        l->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.4f));
        addAndMakeVisible(l);
    }

    // pick which song the sliders change
    controllingLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(controllingLabel);
    for (auto* b : { &deck1Button, &deck2Button })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1);
        addAndMakeVisible(b);
    }
    deck1Button.onClick = [this] { if (deck1Button.getToggleState()) setControlledDeck(0); };
    deck2Button.onClick = [this] { if (deck2Button.getToggleState()) setControlledDeck(1); };
    deck1Button.setTooltip("The sliders change the song that is playing now");
    deck2Button.setTooltip("The sliders change the next song, heard while it fades in");

    for (auto* l : { &volumeLabel, &gainLabel, &pitchLabel, &speedLabel,
                     &trebleLabel, &midsLabel, &bassLabel })
    {
        l->setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(l);
    }

    const SongSettings defaults;
    const double maxGain = SongSettings::maxGainDb;
    const double maxEq   = SongSettings::maxEqDb;

    setupSlider(volumeSlider,    0.0, 100.0, defaults.volume * 100.0, "%", 0);
    setupSlider(gainSlider, -maxGain, maxGain, defaults.gainDb, " dB", 1);
    setupSlider(pitchSlider, -SongSettings::maxPitch, SongSettings::maxPitch,
                defaults.pitch, " st", 1);
    setupSlider(speedSlider, SongSettings::minSpeed * 100.0, SongSettings::maxSpeed * 100.0,
                defaults.speed * 100.0, "%", 0);
    setupSlider(trebleSlider, -maxEq, maxEq, defaults.trebleDb, " dB", 1);
    setupSlider(midsSlider,   -maxEq, maxEq, defaults.midsDb,   " dB", 1);
    setupSlider(bassSlider,   -maxEq, maxEq, defaults.bassDb,   " dB", 1);

    // FRONTEND/BACKEND INTEGRATION: Moving a slider updates the current song only
    songSliders = { &volumeSlider, &gainSlider, &pitchSlider, &speedSlider,
                    &trebleSlider, &midsSlider, &bassSlider };

    for (auto* s : songSliders)
        s->onValueChange = [this]
        {
            const auto* song = getControlledSong();
            if (song != nullptr && onSongSettingsChanged)
                onSongSettingsChanged(*song, getSliderSettings(), controlledDeck == 1);
        };

    //scratch slider: grab the record, the song follows the hand, let go and it carries on
    scratchSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    scratchSlider.setRange(-2.0, 2.0, 0.001); // fine steps so the movement is smooth
    scratchSlider.setValue(0.0, juce::dontSendNotification);
    scratchSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, true, 80, 24);
    scratchSlider.setNumDecimalPlacesToDisplay(2);
    scratchSlider.setTextValueSuffix(" sec");
    scratchSlider.setTooltip("Scratch the playing song: drag left or right");
    addAndMakeVisible(scratchSlider);

    scratchSlider.onDragStart = [this]
        {
            scratching = true;
            if (onScratchStart) onScratchStart();
        };

    scratchSlider.onValueChange = [this]
        {
            if (scratching && onScratchMove)
                onScratchMove(scratchSlider.getValue());
        };

    scratchSlider.onDragEnd = [this]
        {
            scratching = false;
            if (onScratchEnd) onScratchEnd();
            scratchSlider.setValue(0.0, juce::dontSendNotification);
        };
}

void MixingPage::setupSlider(juce::Slider& s, double min, double max, double def,
                             const juce::String& suffix, int decimals)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    s.setRange(min, max);
    s.setNumDecimalPlacesToDisplay(decimals);
    s.setTextValueSuffix(suffix);
    s.setValue(def, juce::dontSendNotification);
    s.setDoubleClickReturnValue(true, def);
    addAndMakeVisible(s);
}

// ── Content ───────────────────────────────────────────────────────────────────

void MixingPage::setContent(Playlist* playlist, int songIndex)
{
    const bool advancingToNext = playlist != nullptr && playlist == currentPlaylist
                             && songIndex == currentIndex + 1;
    const float nextFadeInStart = chart2.getFadeStart();
    const float nextFadeInEnd = fadeInEndOrMatch(nextFadeInStart, chart2.getFadeEnd(),
                                                 chart1.getFadeStart(), chart1.getFadeEnd(),
                                                 songSeconds(currentPlaylist, currentIndex),
                                                 songSeconds(currentPlaylist, currentIndex + 1));

    if (onStop) onStop();
    currentPlaylist = playlist;
    currentIndex    = songIndex;
    controlledDeck  = 0; // after a handover the song that was next is now song 1
    loadSongSettings();
    incomingFadeInStart = -1.0f;
    incomingFadeInEnd   = -1.0f;

    // Reset editable state when loading new content
    chart1.setEditable(true);
    chart2.setEditable(true);

    if (playlist != nullptr)
    {
        const auto& songs  = playlist->songs;
        const bool hasCurr = songIndex >= 0 && songIndex < songs.size();
        const bool hasNext = songIndex + 1 < songs.size();

        if (hasCurr && advancingToNext && nextFadeInStart >= 0.0f
            && nextFadeInEnd > nextFadeInStart)
        {
            incomingFadeInStart = nextFadeInStart;
            incomingFadeInEnd = nextFadeInEnd;
        }
        else if (hasCurr && songIndex > 0)
        {
            // the fade in the user placed for the previous song into this one, else the first preset
            Transition t;
            bool found = false;
            if (getMixMarkers)
            {
                t = getMixMarkers(songs[songIndex - 1], songs[songIndex]);
                found = t.fadeInStart >= 0.0f;
            }
            if (!found && getTransitionsForPair)
            {
                const auto presets = getTransitionsForPair(songs[songIndex - 1].title, songs[songIndex].title);
                if (!presets.isEmpty())
                {
                    t = presets.getReference(0);
                    found = true;
                }
            }

            if (found)
            {
                incomingFadeInStart = t.fadeInStart;
                incomingFadeInEnd = fadeInEndOrMatch(t.fadeInStart, t.fadeInEnd,
                                                     t.fadeOutStart, t.fadeOutEnd,
                                                     songs[songIndex - 1].durationSeconds,
                                                     songs[songIndex].durationSeconds);
            }
        }

        chart1.setSongName    (hasCurr ? songs[songIndex].title             : juce::String{});
        chart2.setSongName    (hasNext ? songs[songIndex + 1].title         : juce::String{});
        chart1.setSongDuration(hasCurr ? songs[songIndex].durationSeconds   : 0);
        chart2.setSongDuration(hasNext ? songs[songIndex + 1].durationSeconds : 0);
        chart1.setSongFile    (hasCurr ? songs[songIndex].filePath          : juce::String{});
        chart2.setSongFile    (hasNext ? songs[songIndex + 1].filePath      : juce::String{});

        // bring back the markers last placed for this pair, or clear the old pair's
        const Transition saved = hasCurr && hasNext && getMixMarkers
                               ? getMixMarkers(songs[songIndex], songs[songIndex + 1]) : Transition{};
        chart1.applyTransition(saved);
        chart2.applyTransition(saved);

        juce::Array<Song> arr;
        for (const auto& s : songs) arr.add(s);
        sidebarContent.updateSongs(arr);

        sidebarContent.onItemClicked = [this](int idx)
        {
            if (currentPlaylist && idx >= 0 && idx < currentPlaylist->songs.size())
                setContent(currentPlaylist, idx);
        };
    }

    updateDeckButtons();
    updatePlayButtonIcon();
    resetPlayhead();
}

// ── Playback ──────────────────────────────────────────────────────────────────
// play pause resume stop go through the teams audio engine
// these add the playhead jump to skip and loop return on top

bool MixingPage::currentSongIsLoaded() const
{
    if (currentPlaylist == nullptr || ! juce::isPositiveAndBelow(currentIndex, currentPlaylist->songs.size()))
        return false;

    // engine still has the last song until play is pressed
    const auto& path = currentPlaylist->songs[currentIndex].filePath;
    return path.isNotEmpty() && getLoadedFilePath && getLoadedFilePath() == path;
}

void MixingPage::resetPlayhead()
{
    // line at the start for songs with audio hidden for demo songs
    const bool hasAudio = currentPlaylist != nullptr
                       && juce::isPositiveAndBelow(currentIndex, currentPlaylist->songs.size())
                       && currentPlaylist->songs[currentIndex].filePath.isNotEmpty();
    chart1.setPlayhead(hasAudio ? 0.0f : -1.0f);
}

void MixingPage::jumpTo(float position)
{
    if (currentPlaylist == nullptr || ! juce::isPositiveAndBelow(currentIndex, currentPlaylist->songs.size()))
        return;

    // start the song if it isnt playing yet then move to the spot
    // position is 0 to 1 across the chart
    if (! playCurrentSong())
        return;

    if (onSeek && getPlaybackLength)
        onSeek(position * getPlaybackLength());
}

void MixingPage::skipToPrevious()
{
    if (currentPlaylist == nullptr || currentIndex <= 0) return;

    // keep playing if it was already playing, same as skip
    const bool wasPlaying = currentSongIsLoaded() && isPlaybackActive && isPlaybackActive();
    setContent(currentPlaylist, currentIndex - 1); // this stops the current song

    if (wasPlaying)
        playCurrentSong();
}

void MixingPage::skipToNext()
{
    if (currentPlaylist == nullptr || currentIndex + 1 >= currentPlaylist->songs.size()) return;

    // keep playing if it was already playing
    const bool wasPlaying = currentSongIsLoaded() && isPlaybackActive && isPlaybackActive();
    setContent(currentPlaylist, currentIndex + 1); // this stops the old song

    if (wasPlaying)
        playCurrentSong(); // the next songs own settings and its incoming fade
}

bool MixingPage::playCurrentSong()
{
    const auto* song = getCurrentSong();
    if (song == nullptr || ! onPlaySong)
        return false;

    return onPlaySong(*song, getSliderSettings(),
                      incomingFadeInStart, incomingFadeInEnd,
                      chart1.getFadeStart(), chart1.getFadeEnd());
}

void MixingPage::updatePlayButtonIcon()
{
    const bool playing = currentSongIsLoaded() && isPlaybackActive && isPlaybackActive();
    const juce::String shape = playing ? "pause" : "play";
    if (playButton.getProperties()["iconShape"].toString() != shape)
    {
        playButton.getProperties().set("iconShape", shape);
        playButton.setTooltip(playing ? "Pause" : "Play");
        playButton.repaint();
    }
}

void MixingPage::timerCallback()
{
    if (! scratching)
        updatePlayButtonIcon(); // the record is held still while scratching, keep showing pause

    if (! currentSongIsLoaded() || ! getPlaybackPosition || ! getPlaybackLength) return;

    const double length = getPlaybackLength();
    if (length <= 0.0) return;

    const double progress = getPlaybackPosition() / length;
    chart1.setPlayhead((float)progress);

    // move on once the fade out has finished or the file has run out
    const bool playing  = isPlaybackActive && isPlaybackActive();
    const float fadeEnd = chart1.getFadeEnd();
    const bool fadeDone = playing && chart1.getFadeStart() >= 0.0f && fadeEnd > 0.0f && progress >= fadeEnd;
    const bool songDone = ! playing && progress >= 0.999; // not a pause in the middle
    const bool hasNext  = currentIndex + 1 < currentPlaylist->songs.size();

    // once the fade out starts, the next song comes in quietly underneath it
    const float nextInStart = chart2.getFadeStart();
    const float nextInEnd   = fadeInEndOrMatch(nextInStart, chart2.getFadeEnd(),
                                               chart1.getFadeStart(), chart1.getFadeEnd(),
                                               juce::roundToInt(length),
                                               songSeconds(currentPlaylist, currentIndex + 1));
    if (playing && hasNext && onCueNext && chart1.getFadeStart() >= 0.0f
        && progress >= chart1.getFadeStart() && ! fadeDone
        && nextInStart >= 0.0f && nextInEnd > nextInStart)
        onCueNext(currentPlaylist->songs[currentIndex + 1], nextInStart, nextInEnd);

    if (fadeDone || songDone)
    {
        if (hasNext)
        {
            // picks up chart2's fade in, and the engine takes over the incoming deck
            // setContent loads this song's own saved settings, the same ones the incoming
            // deck was already playing with, so nothing jumps at the handover
            setContent(currentPlaylist, currentIndex + 1);
            playCurrentSong();
        }
        else if (onStop)
        {
            onStop(); // end of the playlist
        }
        return;
    }

    // use the real length if the song didnt have one
    if (currentPlaylist->songs[currentIndex].durationSeconds <= 0)
        chart1.setSongDuration(juce::roundToInt(length));
}

// ── Per-song settings ─────────────────────────────────────────────────────────

const Song* MixingPage::getCurrentSong() const
{
    if (currentPlaylist == nullptr ||
        !juce::isPositiveAndBelow(currentIndex, currentPlaylist->songs.size()))
        return nullptr;

    return &currentPlaylist->songs.getReference(currentIndex);
}

const Song* MixingPage::getNextSong() const
{
    if (currentPlaylist == nullptr ||
        !juce::isPositiveAndBelow(currentIndex + 1, currentPlaylist->songs.size()))
        return nullptr;

    return &currentPlaylist->songs.getReference(currentIndex + 1);
}

const Song* MixingPage::getControlledSong() const
{
    return controlledDeck == 1 ? getNextSong() : getCurrentSong();
}

void MixingPage::setControlledDeck(int deck)
{
    controlledDeck = (deck == 1 && getNextSong() != nullptr) ? 1 : 0;
    loadSongSettings();
    updateDeckButtons();
}

void MixingPage::updateDeckButtons()
{
    if (controlledDeck == 1 && getNextSong() == nullptr)
        controlledDeck = 0;

    const auto* current = getCurrentSong();
    const auto* next    = getNextSong();
    deck1Button.setButtonText("1: " + (current != nullptr ? current->title : juce::String("(none)")));
    deck2Button.setButtonText("2: " + (next    != nullptr ? next->title    : juce::String("(no next song)")));
    deck2Button.setEnabled(next != nullptr);
    deck1Button.setToggleState(controlledDeck == 0, juce::dontSendNotification);
    deck2Button.setToggleState(controlledDeck == 1, juce::dontSendNotification);
}

SongSettings MixingPage::getSliderSettings() const
{
    SongSettings settings;
    settings.speed    = speedSlider.getValue() / 100.0;
    settings.volume   = volumeSlider.getValue() / 100.0;
    settings.gainDb   = gainSlider.getValue();
    settings.bassDb   = bassSlider.getValue();
    settings.midsDb   = midsSlider.getValue();
    settings.trebleDb = trebleSlider.getValue();
    settings.pitch    = pitchSlider.getValue();
    return settings;
}

void MixingPage::loadSongSettings()
{
    const auto* song    = getControlledSong();
    const bool hasAudio = song != nullptr && song->filePath.isNotEmpty();
    const auto settings = hasAudio && getSongSettings ? getSongSettings(*song) : SongSettings{};

    volumeSlider.setValue(settings.volume * 100.0, juce::dontSendNotification);
    gainSlider  .setValue(settings.gainDb,         juce::dontSendNotification);
    speedSlider .setValue(settings.speed * 100.0,  juce::dontSendNotification);
    trebleSlider.setValue(settings.trebleDb,       juce::dontSendNotification);
    midsSlider  .setValue(settings.midsDb,         juce::dontSendNotification);
    bassSlider  .setValue(settings.bassDb,         juce::dontSendNotification);
    pitchSlider .setValue(settings.pitch,          juce::dontSendNotification);

    // A song with no audio file has nothing to control.
    for (auto* s : songSliders)
        s->setEnabled(hasAudio);

    // name the song so it's clear which one the sliders change
    const juce::String name = song != nullptr ? song->title.toUpperCase() : juce::String();
    controlsHeader.setText(song == nullptr ? "CONTROLS"
                           : hasAudio      ? "CONTROLS - " + name
                                           : "CONTROLS - " + name + " (no audio file)",
                           juce::dontSendNotification);
    eqHeader.setText(song != nullptr ? "EQ - " + name : "EQ", juce::dontSendNotification);
}

void MixingPage::visibilityChanged()
{
    // only move the playhead while the page is showing
    if (isVisible()) startTimerHz(30);
    else             stopTimer();
}

// ── Transition presets ────────────────────────────────────────────────────────

void MixingPage::showTransitionPresets()
{
    const auto s1 = chart1.getSongName();
    const auto s2 = chart2.getSongName();

    // only presets made from song 1 into song 2, in this order
    auto presets = getTransitionsForPair && s1.isNotEmpty() && s2.isNotEmpty()
                 ? getTransitionsForPair(s1, s2) : juce::Array<Transition>{};

    juce::PopupMenu menu;
    if (presets.isEmpty())
    {
        menu.addItem(1, "No Presets Saved", false, false);
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(&transitionPresetsButton), [](int){});
        return;
    }

    for (int i = 0; i < presets.size(); ++i)
        menu.addItem(i + 1, presets[i].name);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&transitionPresetsButton),
        [this, presets](int result) mutable
        {
            if (result <= 0) return;
            const auto& t = presets[result - 1];
            chart1.applyTransition(t);
            chart2.applyTransition(t);
            saveMixMarkers(); // switching away and back keeps the preset's markers too
            // Read-only after applying — no editing or saving in MixingPage
            chart1.setEditable(false);
            chart2.setEditable(false);
        });
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void MixingPage::saveMixMarkers()
{
    const auto* current = getCurrentSong();
    const auto* next    = getNextSong();
    if (current == nullptr || next == nullptr || ! onMixMarkersChanged)
        return;

    Transition t;
    t.loopPoint    = chart1.getLoopPoint();
    t.fadeOutStart = chart1.getFadeStart();
    t.fadeOutEnd   = chart1.getFadeEnd();
    t.startPoint   = chart2.getStartPoint();
    t.fadeInStart  = chart2.getFadeStart();
    t.fadeInEnd    = chart2.getFadeEnd();
    onMixMarkersChanged(*current, *next, t);
}

void MixingPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    if (sepY > 0)
    {
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(sepY, (float)kPad, (float)(getWidth() - kPad));
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void MixingPage::resized()
{
    auto area = getLocalBounds().reduced(kPad);

    // Title
    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);

    // Button row: [▶] [●] ........... [Recordings]
    {
        auto row = area.removeFromTop(30);
        playButton.setBounds      (row.removeFromLeft(32).withSizeKeepingCentre(32, 30));
        row.removeFromLeft(6);
        stopButton.setBounds(row.removeFromLeft(50));
        row.removeFromLeft(6);
        recordButton.setBounds    (row.removeFromLeft(32).withSizeKeepingCentre(32, 30));
        recordingsButton.setBounds(row.removeFromRight(96).withSizeKeepingCentre(96, 28));
    }
    area.removeFromTop(8);

    // The charts give up height first so every slider row stays on screen
    const int controlsH = 24 + 8 + 18 + 8 + 4 * kSliderH + 3 * kSliderGap;
    const int gapsH     = 6 + 26 + 6 + kSepGap + 1 + kSepGap;
    const int chartH    = juce::jmin(kChartH, (area.getHeight() - controlsH - gapsH) / 2);

    // Chart 1
    chart1.setBounds(area.removeFromTop(chartH));
    area.removeFromTop(6);

    // Transition Presets button — right-aligned in the gap row
    {
        auto row = area.removeFromTop(26);

        transitionPresetsButton.setBounds(row.removeFromRight(162));

        row.removeFromRight(12); //spacing between controls

        scratchSlider.setBounds(row.removeFromLeft(
            juce::jmin(260, row.getWidth())));
    }
    area.removeFromTop(6);

    // Chart 2
    chart2.setBounds(area.removeFromTop(chartH));

    // Separator
    area.removeFromTop(kSepGap);
    sepY = area.getY();
    area.removeFromTop(1 + kSepGap);

    // Which song the sliders change
    {
        auto row = area.removeFromTop(24);
        controllingLabel.setBounds(row.removeFromLeft(kLabelW + 14));
        const int deckW = juce::jmin(240, (row.getWidth() - 8) / 2);
        deck1Button.setBounds(row.removeFromLeft(deckW));
        row.removeFromLeft(8);
        deck2Button.setBounds(row.removeFromLeft(deckW));
    }
    area.removeFromTop(8);

    // Two-column slider section
    const int colW = (area.getWidth() - kColGap) / 2;
    auto leftCol  = area.removeFromLeft(colW);
    area.removeFromLeft(kColGap);
    auto rightCol = area;

    auto placeRow = [&](juce::Label& lbl, juce::Slider& sl,
                        juce::Rectangle<int>& col)
    {
        auto row = col.removeFromTop(kSliderH);
        lbl.setBounds(row.removeFromLeft(kLabelW));
        sl.setBounds(row);
    };

    controlsHeader.setBounds(leftCol.removeFromTop(18));
    leftCol.removeFromTop(8);
    placeRow(volumeLabel, volumeSlider, leftCol);
    leftCol.removeFromTop(kSliderGap);
    placeRow(gainLabel,   gainSlider,   leftCol);
    leftCol.removeFromTop(kSliderGap);
    placeRow(pitchLabel,  pitchSlider,  leftCol);
    leftCol.removeFromTop(kSliderGap);
    placeRow(speedLabel,  speedSlider,  leftCol);

    eqHeader.setBounds(rightCol.removeFromTop(18));
    rightCol.removeFromTop(8);
    placeRow(trebleLabel, trebleSlider, rightCol);
    rightCol.removeFromTop(kSliderGap);
    placeRow(midsLabel,   midsSlider,   rightCol);
    rightCol.removeFromTop(kSliderGap);
    placeRow(bassLabel,   bassSlider,   rightCol);
}
