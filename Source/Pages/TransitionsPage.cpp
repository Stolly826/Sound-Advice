#include "TransitionsPage.h"
#include "PickSongDialog.h"
#include "SaveTransitionDialog.h"

static constexpr int kPad       = 16;
static constexpr int kChartH    = 165;
static constexpr int kLabelW    = 70;
static constexpr int kSliderH   = 32;
static constexpr int kSliderGap = 6;
static constexpr int kColGap    = 20;
static constexpr int kSepGap    = 10;

// ── Construction ──────────────────────────────────────────────────────────────

TransitionsPage::TransitionsPage()
{
    titleLabel.setText("Transitions", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    playButton.setButtonText("");
    playButton.getProperties().set("iconShape", "play");
    addAndMakeVisible(playButton);

    saveTransitionButton.onClick = [this] { onSaveClicked(); };
    addAndMakeVisible(saveTransitionButton);

    chart2.setIsNextSong(true);
    chart1.onSelectSong = [this] { showPickSong(chart1); };
    chart2.onSelectSong = [this] { showPickSong(chart2); };
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
    setupSlider(pitchSlider,  -12.0,  12.0,   0.0);
    setupSlider(speedSlider,   50.0, 150.0, 100.0);
    setupSlider(trebleSlider, -12.0,  12.0,   0.0);
    setupSlider(midsSlider,   -12.0,  12.0,   0.0);
    setupSlider(bassSlider,   -12.0,  12.0,   0.0);
}

void TransitionsPage::setupSlider(juce::Slider& s, double min, double max, double def)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(min, max);
    s.setValue(def, juce::dontSendNotification);
    addAndMakeVisible(s);
}

// ── Song selection ─────────────────────────────────────────────────────────────

void TransitionsPage::showPickSong(SongChartComponent& chart)
{
    auto songs    = getAllSongs ? getAllSongs() : juce::Array<Song>{};
    auto* content = new PickSongDialog(std::move(songs));

    content->onSongPicked = [&chart, this](Song s)
    {
        chart.setSongName(s.title);
        chart.setSongDuration(s.durationSeconds);
        // A new song means the loaded preset no longer applies
        loadedTransitionIndex = -1;
        loadedTransitionName  = {};
        saveTransitionButton.setButtonText("Save Transition");
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content);
    opts.dialogTitle             = "Select Song";
    opts.useNativeTitleBar       = false;
    opts.resizable               = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
}

// ── Transition presets ────────────────────────────────────────────────────────

void TransitionsPage::showTransitionPresets()
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
            loadedTransitionIndex = result - 1;
            loadedTransitionName  = t.name;
            saveTransitionButton.setButtonText("Save Changes");
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
    t.savedAt      = juce::Time::getCurrentTime();

    if (loadedTransitionIndex >= 0 && updateTransitionInSong)
    {
        updateTransitionInSong(s1, loadedTransitionIndex, t);
        loadedTransitionName = name;
    }
    else if (addTransitionToSong)
    {
        addTransitionToSong(s1, t);
        loadedTransitionIndex = -1; // new save; index unknown without re-fetching
        loadedTransitionName  = name;
        saveTransitionButton.setButtonText("Save Changes");
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

    // Button row: [▶] ........... [Save Transition]
    {
        auto row = area.removeFromTop(30);
        playButton.setBounds(row.removeFromLeft(32).withSizeKeepingCentre(32, 30));
        saveTransitionButton.setBounds(row.removeFromRight(130).withSizeKeepingCentre(130, 28));
    }
    area.removeFromTop(8);

    // Chart 1
    chart1.setBounds(area.removeFromTop(kChartH));
    area.removeFromTop(6);

    // Transition Presets button
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
