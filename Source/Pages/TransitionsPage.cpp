#include "TransitionsPage.h"
#include "PickSongDialog.h"
#include "SaveTransitionDialog.h"

static constexpr int kPad       = 16;
static constexpr int kChartH    = 165;
static constexpr int kLabelW    = 70;
static constexpr int kSliderH   = 26;
static constexpr int kSliderGap = 4;
static constexpr int kColGap    = 20;
static constexpr int kSepGap    = 10;

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

TransitionsPage::TransitionsPage()
{
    titleLabel.setText("Transitions", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    playButton.setButtonText("");
    playButton.getProperties().set("iconShape", "play");
    addAndMakeVisible(playButton);

    // one button plays, pauses and resumes
    playButton.onClick = [this]
        {
            if (getSong(0) == nullptr && getSong(1) == nullptr)
                return;

            const bool loaded = activeSongIsLoaded();
            if (loaded && isPlaybackActive && isPlaybackActive())
            {
                if (onPause) onPause();
            }
            else if (loaded && isPlaybackPaused && isPlaybackPaused())
            {
                if (onResume) onResume(); // also brings back a transition that was paused
            }
            else
            {
                // always starts from song 1, or song 2 alone when only it is picked
                activeDeck = getSong(0) != nullptr ? 0 : 1;
                const bool ok = activeDeck == 0 ? playSong1() : playSong2(true);
                if (! ok)
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Unable to play song",
                        "This song does not have a playable MP3 or WAV file.");
            }

            updatePlayButtonIcon();
        };

    stopButton.onClick = [this] { stopPlayback(); };
    addAndMakeVisible(stopButton);

    saveTransitionButton.onClick = [this] { onSaveClicked(); };
    addAndMakeVisible(saveTransitionButton);

    chart2.setIsNextSong(true);
    chart1.onSelectSong = [this] { showPickSong(0); };
    chart2.onSelectSong = [this] { showPickSong(1); };
    chart1.onJumpTo = [this](float t) { jumpTo(0, t); };
    chart2.onJumpTo = [this](float t) { jumpTo(1, t); }; // loop return on song 2
    // skip on song 1 goes straight into song 2, skip on song 2 ends playback
    chart1.onSkip = [this]
        {
            if (getSong(1) != nullptr && getSong(1)->filePath.isNotEmpty())
                playSong2(true);
            updatePlayButtonIcon();
        };
    chart2.onSkip = [this] { stopPlayback(); };
    // a fade placed on one song matches the length of the other song's fade
    chart1.getPartnerFadeSeconds = [this] { return chart2.getFadeSeconds(); };
    chart2.getPartnerFadeSeconds = [this] { return chart1.getFadeSeconds(); };
    // changing either fade changes the other, so the fade out and fade in always match
    chart1.onFadeChanged = [this] { chart2.setFadeSeconds(chart1.getFadeSeconds()); };
    chart2.onFadeChanged = [this] { chart1.setFadeSeconds(chart2.getFadeSeconds()); };
    addAndMakeVisible(chart1);
    addAndMakeVisible(chart2);

    transitionPresetsButton.onClick = [this] { showTransitionPresets(); };
    addAndMakeVisible(transitionPresetsButton);
    presetNameLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    presetNameLabel.setJustificationType(juce::Justification::centredRight);
    presetNameLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(presetNameLabel);

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
    deck1Button.setTooltip("The sliders change song 1");
    deck2Button.setTooltip("The sliders change song 2, heard while it fades in and after");

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

    songSliders = { &volumeSlider, &gainSlider, &pitchSlider, &speedSlider,
                    &trebleSlider, &midsSlider, &bassSlider };

    for (auto* s : songSliders)
        s->onValueChange = [this]
        {
            const auto* song = getControlledSong();
            if (song == nullptr || ! onSongSettingsChanged)
                return;

            // heard straight away on the main deck only if that song is the one playing there,
            // otherwise it goes to the incoming deck (song 2 fading in, or nothing)
            const bool onMainDeck = controlledDeck == activeDeck && activeSongIsLoaded();
            onSongSettingsChanged(*song, getSliderSettings(), ! onMainDeck);
        };

    // scratch slider: grab the record, the song follows the hand, let go and it carries on
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

    updateDeckButtons();
    loadSongSettings();
}

void TransitionsPage::setupSlider(juce::Slider& s, double min, double max, double def,
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

// ── Song selection ─────────────────────────────────────────────────────────────

void TransitionsPage::showPickSong(int chartIndex)
{
    auto songs    = getAllSongs ? getAllSongs() : juce::Array<Song>{};
    auto* content = new PickSongDialog(std::move(songs));

    content->onSongPicked = [chartIndex, this](Song s)
    {
        // swapping out a song that is playing here stops it
        if (activeSongIsLoaded())
            stopPlayback();

        auto& chart = chartIndex == 0 ? chart1 : chart2;
        chart.setSongName(s.title);
        chart.setSongDuration(s.durationSeconds);
        chart.setSongFile(s.filePath);

        if (chartIndex == 0) { song1 = s; hasSong1 = true; }
        else                 { song2 = s; hasSong2 = true; }

        // A new song means the loaded preset no longer applies
        loadedTransitionIndex = -1;
        loadedTransitionName  = {};
        saveTransitionButton.setButtonText("Save Transition");
        presetNameLabel.setText({}, juce::dontSendNotification);

        updateDeckButtons();
        loadSongSettings();
        resetPlayheads();
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Select Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

// ── Playback ──────────────────────────────────────────────────────────────────
// Song 1 plays with its fade out. Once the fade out starts, song 2 comes in quietly
// on the incoming deck, then takes over the main deck when song 1 is done.
// The charts never swap: song 2 keeps playing in chart 2, and when it ends it just stops.

const Song* TransitionsPage::getSong(int deck) const
{
    if (deck == 0) return hasSong1 ? &song1 : nullptr;
    return hasSong2 ? &song2 : nullptr;
}

bool TransitionsPage::activeSongIsLoaded() const
{
    const auto* song = getSong(activeDeck);
    return song != nullptr && song->filePath.isNotEmpty()
        && getLoadedFilePath && getLoadedFilePath() == song->filePath;
}

bool TransitionsPage::isPlayingHere() const
{
    return activeSongIsLoaded() && isPlaybackActive && isPlaybackActive();
}

float TransitionsPage::song2FadeInEnd() const
{
    return fadeInEndOrMatch(chart2.getFadeStart(), chart2.getFadeEnd(),
                            chart1.getFadeStart(), chart1.getFadeEnd(),
                            hasSong1 ? song1.durationSeconds : 0,
                            hasSong2 ? song2.durationSeconds : 0);
}

bool TransitionsPage::playSong1()
{
    if (! hasSong1 || ! onPlaySong)
        return false;

    activeDeck = 0;
    const auto settings = getSongSettings ? getSongSettings(song1) : SongSettings{};
    return onPlaySong(song1, settings, -1.0f, -1.0f,
                      chart1.getFadeStart(), chart1.getFadeEnd(),
                      FadeCurve::defaultCurve, chart1.getFadeCurve());
}

bool TransitionsPage::playSong2(bool withFadeIn)
{
    if (! hasSong2 || ! onPlaySong)
        return false;

    activeDeck = 1;
    const auto settings = getSongSettings ? getSongSettings(song2) : SongSettings{};

    // a set start beats the fade in: song 2 just starts there at full volume
    const float startPoint = withFadeIn ? chart2.getStartPoint() : -1.0f;
    const bool  useStart   = startPoint >= 0.0f;
    const float inStart = withFadeIn && ! useStart ? chart2.getFadeStart() : -1.0f;
    const float inEnd   = withFadeIn && ! useStart ? song2FadeInEnd()      : -1.0f;

    // if song 2 was cued under the fade out, the engine carries it on from where it got to
    if (! onPlaySong(song2, settings, inStart, inEnd, -1.0f, -1.0f,
                     chart2.getFadeCurve(), FadeCurve::defaultCurve))
        return false;

    if (useStart && onSeek && getPlaybackLength)
        onSeek(startPoint * getPlaybackLength());
    return true;
}

void TransitionsPage::jumpTo(int deck, float position)
{
    if (getSong(deck) == nullptr)
        return;

    // start clean so a song 2 cued under an earlier fade out doesn't keep playing
    if (onStop) onStop();
    resetPlayheads();

    // jumping into song 2 plays it at full volume from that spot, without its fade in
    const bool ok = deck == 0 ? playSong1() : playSong2(false);
    if (ok && onSeek && getPlaybackLength)
        onSeek(position * getPlaybackLength());

    updatePlayButtonIcon();
}

void TransitionsPage::stopPlayback()
{
    if (onStop) onStop();
    activeDeck = 0;
    resetPlayheads();
    updatePlayButtonIcon();
}

void TransitionsPage::resetPlayheads()
{
    // line at the start of song 1 when it has audio, song 2's appears once it takes over
    chart1.setPlayhead(hasSong1 && song1.filePath.isNotEmpty() ? 0.0f : -1.0f);
    chart2.setPlayhead(-1.0f);
}

void TransitionsPage::updatePlayButtonIcon()
{
    const bool playing = isPlayingHere();
    const juce::String shape = playing ? "pause" : "play";
    if (playButton.getProperties()["iconShape"].toString() != shape)
    {
        playButton.getProperties().set("iconShape", shape);
        playButton.setTooltip(playing ? "Pause" : "Play");
        playButton.repaint();
    }
}

void TransitionsPage::timerCallback()
{
    if (! scratching)
        updatePlayButtonIcon(); // the record is held still while scratching, keep showing pause

    if (! activeSongIsLoaded() || ! getPlaybackPosition || ! getPlaybackLength) return;

    const double length = getPlaybackLength();
    if (length <= 0.0) return;

    const double progress = getPlaybackPosition() / length;
    const bool   playing  = isPlaybackActive && isPlaybackActive();
    const bool   songDone = ! playing && progress >= 0.999; // not a pause in the middle
    auto&        chart    = activeDeck == 0 ? chart1 : chart2;
    chart.setPlayhead((float)progress);

    // use the real length if the song didnt have one
    const auto* song = getSong(activeDeck);
    if (song != nullptr && song->durationSeconds <= 0)
        chart.setSongDuration(juce::roundToInt(length));

    if (activeDeck == 1)
    {
        // song 2 is the last song: when it runs out, everything just goes quiet
        if (songDone)
        {
            if (onStop) onStop();
            activeDeck = 0;
            updatePlayButtonIcon();
        }
        return;
    }

    // song 1: once the fade out starts, song 2 comes in quietly underneath it
    const bool  song2Playable = hasSong2 && song2.filePath.isNotEmpty();
    const float outStart      = chart1.getFadeStart();
    const float outEnd        = chart1.getFadeEnd();
    const bool  fadeDone      = playing && outStart >= 0.0f && outEnd > 0.0f && progress >= outEnd;

    const float inStart = chart2.getFadeStart();
    const float inEnd   = song2FadeInEnd();
    // (a set start on song 2 means it doesn't fade in, it just starts when song 1 ends)
    if (playing && song2Playable && onCueNext && chart2.getStartPoint() < 0.0f
        && outStart >= 0.0f
        && progress >= outStart && ! fadeDone
        && inStart >= 0.0f && inEnd > inStart)
        onCueNext(song2, inStart, inEnd, chart2.getFadeCurve());

    if (fadeDone || songDone)
    {
        // song 2 carries on in its own chart, the charts stay where they are
        if (song2Playable && playSong2(true))
            chart1.setPlayhead(outEnd > 0.0f && fadeDone ? outEnd : 1.0f);
        else
            stopPlayback(); // no song 2 to go to
    }
}

void TransitionsPage::visibilityChanged()
{
    // only run the playheads and transition while the page is showing
    if (isVisible()) startTimerHz(30);
    else             stopTimer();
}

// ── Per-song settings ─────────────────────────────────────────────────────────

void TransitionsPage::setControlledDeck(int deck)
{
    controlledDeck = (deck == 1 && getSong(1) != nullptr) ? 1 : 0;
    loadSongSettings();
    updateDeckButtons();
}

void TransitionsPage::updateDeckButtons()
{
    if (controlledDeck == 1 && getSong(1) == nullptr)
        controlledDeck = 0;

    const auto* s1 = getSong(0);
    const auto* s2 = getSong(1);
    deck1Button.setButtonText("1: " + (s1 != nullptr ? s1->title : juce::String("(none)")));
    deck2Button.setButtonText("2: " + (s2 != nullptr ? s2->title : juce::String("(none)")));
    deck2Button.setEnabled(s2 != nullptr);
    deck1Button.setToggleState(controlledDeck == 0, juce::dontSendNotification);
    deck2Button.setToggleState(controlledDeck == 1, juce::dontSendNotification);
}

SongSettings TransitionsPage::getSliderSettings() const
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

void TransitionsPage::loadSongSettings()
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

// ── Transition presets ────────────────────────────────────────────────────────

void TransitionsPage::showTransitionPresets()
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
            loadedTransitionIndex = result - 1;
            loadedTransitionName  = t.name;
            saveTransitionButton.setButtonText("Save Changes");
            presetNameLabel.setText(t.name, juce::dontSendNotification);
        });
}

// ── Save transition ───────────────────────────────────────────────────────────

void TransitionsPage::onSaveClicked()
{
    const juce::String existing = (loadedTransitionIndex >= 0) ? loadedTransitionName
                                                                : juce::String{};
    auto* dlg = new SaveTransitionDialog(existing);
    dlg->onSave = [this](juce::String name) { doSaveTransition(name); };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dlg);
    opts.dialogTitle             = loadedTransitionIndex >= 0 ? "Save Changes" : "Save Transition";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

void TransitionsPage::doSaveTransition(const juce::String& name)
{
    const auto s1 = chart1.getSongName();
    const auto s2 = chart2.getSongName();
    if (s1.isEmpty() || s2.isEmpty()) return;

    Transition t;
    t.name         = name;
    t.toSongTitle  = s2;
    t.loopPoint    = chart1.getLoopPoint();
    t.fadeOutStart = chart1.getFadeStart();
    t.fadeOutEnd   = chart1.getFadeEnd();
    t.startPoint   = chart2.getStartPoint();
    t.fadeInStart  = chart2.getFadeStart();
    t.fadeInEnd    = chart2.getFadeEnd();
    t.fadeOutCurve = chart1.getFadeCurve();
    t.fadeInCurve  = chart2.getFadeCurve();
    t.savedAt      = juce::Time::getCurrentTime();

    if (loadedTransitionIndex >= 0 && updateTransitionInSong)
    {
        updateTransitionInSong(s1, loadedTransitionIndex, t);
        loadedTransitionName = name;
        presetNameLabel.setText(name, juce::dontSendNotification); // may have been renamed
    }
    else if (addTransitionToSong)
    {
        addTransitionToSong(s1, t);
        // it was added last, so further saves update it instead of making copies
        loadedTransitionIndex = getTransitionsForPair ? getTransitionsForPair(s1, s2).size() - 1 : -1;
        loadedTransitionName  = name;
        saveTransitionButton.setButtonText("Save Changes");
        presetNameLabel.setText(name, juce::dontSendNotification); // it's the preset in use now
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void TransitionsPage::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    if (sepY > 0)
    {
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(sepY, (float)kPad, (float)(getWidth() - kPad));
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void TransitionsPage::resized()
{
    auto area = getLocalBounds().reduced(kPad);

    // Title
    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);

    // Button row: [▶] [Stop] ........... [Save Transition]
    {
        auto row = area.removeFromTop(30);
        playButton.setBounds(row.removeFromLeft(32).withSizeKeepingCentre(32, 30));
        row.removeFromLeft(6);
        stopButton.setBounds(row.removeFromLeft(50));
        saveTransitionButton.setBounds(row.removeFromRight(130).withSizeKeepingCentre(130, 28));
    }
    area.removeFromTop(8);

    // The charts give up height first so every slider row stays on screen
    const int controlsH = 24 + 8 + 18 + 8 + 4 * kSliderH + 3 * kSliderGap;
    const int gapsH     = 6 + 26 + 6 + kSepGap + 1 + kSepGap;
    const int chartH    = juce::jmin(kChartH, (area.getHeight() - controlsH - gapsH) / 2);

    // Chart 1
    chart1.setBounds(area.removeFromTop(chartH));
    area.removeFromTop(6);

    // Scratch slider on the left, Transition Presets on the right
    {
        auto row = area.removeFromTop(26);
        transitionPresetsButton.setBounds(row.removeFromRight(162));
        row.removeFromRight(12);
        scratchSlider.setBounds(row.removeFromLeft(juce::jmin(260, row.getWidth())));
        row.removeFromLeft(12);
        presetNameLabel.setBounds(row); // what's left, right next to the button
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

    // Two-column sliders
    const int colW = (area.getWidth() - kColGap) / 2;
    auto leftCol  = area.removeFromLeft(colW);
    area.removeFromLeft(kColGap);
    auto rightCol = area;

    auto placeRow = [&](juce::Label& lbl, juce::Slider& sl, juce::Rectangle<int>& col)
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
