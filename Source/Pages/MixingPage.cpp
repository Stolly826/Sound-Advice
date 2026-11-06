#include "MixingPage.h"

static constexpr int kPad       = 16;
static constexpr int kChartH    = 165;
static constexpr int kLabelW    = 70;
static constexpr int kSliderH   = 32;
static constexpr int kSliderGap = 6;
static constexpr int kColGap    = 20;
static constexpr int kSepGap    = 10;

// ── Construction ──────────────────────────────────────────────────────────────

MixingPage::MixingPage()
{
    titleLabel.setText("Mixing", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    playButton.setButtonText("");
    playButton.getProperties().set("iconShape", "play");
    addAndMakeVisible(playButton);

    // FRONTEND/BACKEND INTEGRATION: Play the selected song using JUCE
    playButton.onClick = [this]
        {
            // Make sure a valid song is selected from a playlist
            if (currentPlaylist == nullptr ||
                !juce::isPositiveAndBelow(currentIndex, currentPlaylist->songs.size()))
                return;

            const auto& song = currentPlaylist->songs[currentIndex];

            // Ask MainComponent to play the selected audio file
            if (!onPlaySong || !onPlaySong(song))
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Unable to play song",
                    "This song does not have a playable MP3 or WAV file.");
        };

    // FRONTEND/BACKEND INTEGRATION: Connect Pause, Resume, and Stop
    pauseButton.onClick = [this] { if (onPause) onPause(); };
    resumeButton.onClick = [this] { if (onResume) onResume(); };
    stopButton.onClick = [this] { if (onStop) onStop(); };

    // Make the new buttons visible in the frontend
    addAndMakeVisible(pauseButton);
    addAndMakeVisible(resumeButton);
    addAndMakeVisible(stopButton);

    recordButton.setButtonText("");
    recordButton.getProperties().set("iconShape", "record");
    addAndMakeVisible(recordButton);

    addAndMakeVisible(recordingsButton);
    chart2.setIsNextSong(true);
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

    for (auto* l : { &volumeLabel, &pitchLabel, &speedLabel,
                     &trebleLabel, &midsLabel,   &bassLabel })
    {
        l->setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(l);
    }

    setupSlider(volumeSlider,   0.0, 100.0,  75.0);
    volumeSlider.onValueChange = [this]
        {
            if (onVolumeChanged)
                onVolumeChanged(static_cast<float>(volumeSlider.getValue() / 100.0));
        };
    setupSlider(pitchSlider,  -12.0,  12.0,   0.0);
    setupSlider(speedSlider,   50.0, 150.0, 100.0);
    speedSlider.onValueChange = [this]
    {
        if (onSpeedChanged)
            onSpeedChanged(speedSlider.getValue() / 100.0);
    };
    setupSlider(trebleSlider, -12.0,  12.0,   0.0);
    setupSlider(midsSlider,   -12.0,  12.0,   0.0);
    setupSlider(bassSlider,   -12.0,  12.0,   0.0);
}

void MixingPage::setupSlider(juce::Slider& s, double min, double max, double def)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(min, max);
    s.setValue(def, juce::dontSendNotification);
    addAndMakeVisible(s);
}

// ── Content ───────────────────────────────────────────────────────────────────

void MixingPage::setContent(Playlist* playlist, int songIndex)
{
    if (onStop) onStop();
    currentPlaylist = playlist;
    currentIndex    = songIndex;

    // Reset editable state when loading new content
    chart1.setEditable(true);
    chart2.setEditable(true);

    if (playlist != nullptr)
    {
        const auto& songs  = playlist->songs;
        const bool hasCurr = songIndex >= 0 && songIndex < songs.size();
        const bool hasNext = songIndex + 1 < songs.size();

        chart1.setSongName    (hasCurr ? songs[songIndex].title             : juce::String{});
        chart2.setSongName    (hasNext ? songs[songIndex + 1].title         : juce::String{});
        chart1.setSongDuration(hasCurr ? songs[songIndex].durationSeconds   : 0);
        chart2.setSongDuration(hasNext ? songs[songIndex + 1].durationSeconds : 0);

        juce::Array<Song> arr;
        for (const auto& s : songs) arr.add(s);
        sidebarContent.updateSongs(arr);

        sidebarContent.onItemClicked = [this](int idx)
        {
            if (currentPlaylist && idx >= 0 && idx < currentPlaylist->songs.size())
                setContent(currentPlaylist, idx);
        };
    }
}

// ── Transition presets ────────────────────────────────────────────────────────

void MixingPage::showTransitionPresets()
{
    const auto s1 = chart1.getSongName();
    const auto s2 = chart2.getSongName();
    if (s1.isEmpty() || s2.isEmpty()) return;

    auto presets = getTransitionsForPair ? getTransitionsForPair(s1, s2)
                                         : juce::Array<Transition>{};

    juce::PopupMenu menu;
    if (presets.isEmpty())
    {
        menu.addItem(1, "(no saved transitions)", false, false);
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
            // Read-only after applying — no editing or saving in MixingPage
            chart1.setEditable(false);
            chart2.setEditable(false);
        });
}

// ── Paint ─────────────────────────────────────────────────────────────────────

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
        //position for new playback buttons
        pauseButton.setBounds(row.removeFromLeft(58));
        row.removeFromLeft(6);

        resumeButton.setBounds(row.removeFromLeft(66));
        row.removeFromLeft(6);

        stopButton.setBounds(row.removeFromLeft(50));
        row.removeFromLeft(6);
        recordButton.setBounds    (row.removeFromLeft(32).withSizeKeepingCentre(32, 30));
        recordingsButton.setBounds(row.removeFromRight(96).withSizeKeepingCentre(96, 28));
    }
    area.removeFromTop(8);

    // Chart 1
    chart1.setBounds(area.removeFromTop(kChartH));
    area.removeFromTop(6);

    // Transition Presets button — right-aligned in the gap row
    {
        auto row = area.removeFromTop(26);
        transitionPresetsButton.setBounds(row.removeFromRight(162));
    }
    area.removeFromTop(6);

    // Chart 2
    chart2.setBounds(area.removeFromTop(kChartH));

    // Separator
    area.removeFromTop(kSepGap);
    sepY = area.getY();
    area.removeFromTop(1 + kSepGap);

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
