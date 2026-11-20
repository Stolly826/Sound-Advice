#include "MainComponent.h"

static constexpr int kSidebarWidth = 220;
static constexpr int kMiniPlayerHeight = 64;

namespace
{
    juce::String formatPlaybackTime(double seconds)
    {
        const int wholeSeconds = juce::jmax(0, juce::roundToInt(seconds));
        return juce::String(wholeSeconds / 60) + ":"
             + juce::String(wholeSeconds % 60).paddedLeft('0', 2);
    }

    class TranscriptionJob final : public juce::ThreadPoolJob
    {
    public:
        TranscriptionJob(const Song& song,
                        const juce::File& modelFile,
                        std::function<void(juce::Result, juce::String)> completion)
            : ThreadPoolJob("Local song transcription"),
              audioFile(song.filePath),
              model(modelFile),
              callback(std::move(completion))
        {
        }

        JobStatus runJob() override
        {
            juce::String transcript;
            auto result = LocalTranscriber::transcribe(audioFile, model, transcript);
            juce::MessageManager::callAsync(
                [callback = std::move(callback), result, transcript = std::move(transcript)]() mutable
                {
                    if (callback)
                        callback(result, std::move(transcript));
                });
            return jobHasFinished;
        }

    private:
        juce::File audioFile;
        juce::File model;
        std::function<void(juce::Result, juce::String)> callback;
    };

    juce::ThreadPool& getTranscriptionThreadPool()
    {
        static juce::ThreadPool pool(1);
        return pool;
    }
}

MainComponent::MainComponent()
{
    setLookAndFeel(&lookAndFeel);

    addChildComponent(sidebar);

    addChildComponent(mainMenuPage);
    addChildComponent(playlistsPage);
    addChildComponent(playlistDetailPage);
    addChildComponent(songsPage);
    addChildComponent(transitionsPage);
    addChildComponent(mixingPage);

    miniTrackLabel.setText("Nothing playing", juce::dontSendNotification);
    miniTrackLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(miniTrackLabel);
    addAndMakeVisible(miniPlaybackButton);
    miniProgressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    miniProgressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    miniProgressSlider.setRange(0.0, 1.0, 0.1);
    miniProgressSlider.onValueChange = [this]
    {
        if (hasCurrentSong)
            audioEngine.setPosition(miniProgressSlider.getValue());
    };
    addAndMakeVisible(miniProgressSlider);
    miniTimeLabel.setJustificationType(juce::Justification::centredRight);
    miniTimeLabel.setFont(juce::FontOptions(12.0f));
    miniTimeLabel.setText("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible(miniTimeLabel);

    miniPlaybackButton.onClick = [this]
    {
        if (!hasCurrentSong)
            return;

        if (audioEngine.isPlaying())
            audioEngine.pause();
        else if (audioEngine.isPaused())
            audioEngine.resume();
        else
            playSong(currentSong);
        timerCallback();
    };

    modelPathStorageFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SoundAdvice").getChildFile("whisper-model-path.txt");

    mainMenuPage.onPlaylistsClicked   = [this] { showPage(playlistsPage); };
    mainMenuPage.onSongsClicked       = [this] { showPage(songsPage); };
    mainMenuPage.onTransitionsClicked = [this] { showPage(transitionsPage); };

    playlistsPage.onPlaylistClicked = [this](Playlist* p) { showPlaylistDetail(p); };

    playlistDetailPage.onSongClicked = [this](Playlist* p, int idx) { showMixingPage(p, idx); };
    playlistDetailPage.getAllSongs   = [this]() { return songsPage.getSongs(); };
    transitionsPage.getAllSongs           = [this]()                              { return songsPage.getSongs(); };
    transitionsPage.getTransitionsForPair = [this](juce::String s1, juce::String s2) { return songsPage.getTransitionsForPair(s1, s2); };
    transitionsPage.addTransitionToSong   = [this](juce::String s1, Transition t)    { songsPage.addTransitionToSong(s1, t); };
    transitionsPage.updateTransitionInSong= [this](juce::String s1, int idx, Transition t) { songsPage.updateTransitionInSong(s1, idx, t); };

    mixingPage.getTransitionsForPair      = [this](juce::String s1, juce::String s2) { return songsPage.getTransitionsForPair(s1, s2); };

    // FRONTEND/BACKEND INTEGRATION: Connect the Mixing page controls to JUCE vibe coded 

    mixingPage.onPlaySong = [this](const Song& song)
        {
            return playSong(song);
        };

    //pause the song without losing its current position
    mixingPage.onPause = [this] { audioEngine.pause(); };

    //resume the previously paused song
    mixingPage.onResume = [this] { audioEngine.resume(); };

    //stop the song and reset its playback position
    mixingPage.onStop = [this] { audioEngine.stop(); };

    //changes JUCE volume with the slider
    mixingPage.onVolumeChanged = [this](float gain)
        {
            audioEngine.setVolume(gain);
        };
    mixingPage.onSpeedChanged = [this](double speed)
        {
            audioEngine.setPlaybackSpeed(speed);
        };
    mixingPage.onBassChanged = [this](float db)
        {
            audioEngine.setBass(db);
        };

    mixingPage.onMidsChanged = [this](float db)
        {
            audioEngine.setMids(db);
        };

    mixingPage.onTrebleChanged = [this](float db)
        {
            audioEngine.setTreble(db);
        };

    mixingPage.onScratch = [this](double offsetSeconds)
        {
            audioEngine.scratch(offsetSeconds);
        };

    //starting volume is set at 75%
    audioEngine.setVolume(0.75f);

    songsPage.getPlaylistsForSong = [this](const Song& s)
    {
        return playlistsPage.getPlaylistNamesContaining(s);
    };
    songsPage.onTranscribeSong = [this](
        const Song& song,
        std::function<void(juce::Result, juce::String)> completion)
    {
        requestTranscription(song, std::move(completion));
    };
    songsPage.onSongTranscriptChanged = [this](const Song& song)
    {
        playlistsPage.updateSongTranscript(song);
        playlistDetailPage.refreshSongs();
    };

    showPage(mainMenuPage);

    setSize(1000, 700);
    startTimerHz(4);
}

MainComponent::~MainComponent()
{
    stopTimer();
    sidebar.setPageContent(nullptr);
    setLookAndFeel(nullptr);
}

bool MainComponent::playSong(const Song& song)
{
    if (song.filePath.isEmpty())
        return false;

    if (!audioEngine.playSong(juce::File(song.filePath)))
        return false;

    currentSong = song;
    hasCurrentSong = true;
    timerCallback();
    return true;
}

void MainComponent::requestTranscription(
    const Song& song,
    std::function<void(juce::Result, juce::String)> completion)
{
    if (!completion)
        return;

    if (song.filePath.isEmpty())
    {
        completion(juce::Result::fail("This song does not have an audio file to transcribe."), {});
        return;
    }

    juce::File modelFile;
    if (modelPathStorageFile.existsAsFile())
        modelFile = juce::File(modelPathStorageFile.loadFileAsString().trim());

    if (modelFile.existsAsFile())
    {
        startTranscription(song, modelFile, std::move(completion));
        return;
    }

    const juce::Component::SafePointer<MainComponent> safeThis(this);
    modelChooser = std::make_shared<juce::FileChooser>(
        "Select a local Whisper GGML model",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.bin");
    const auto chooser = modelChooser;
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser, song, completion = std::move(completion)](
            const juce::FileChooser& fileChooser) mutable
        {
            if (safeThis == nullptr)
                return;

            const auto selectedModel = fileChooser.getResult();
            if (!selectedModel.existsAsFile())
            {
                completion(juce::Result::fail("No Whisper model was selected."), {});
                return;
            }

            const auto createDirectory = safeThis->modelPathStorageFile.getParentDirectory()
                .createDirectory();
            if (createDirectory.failed()
                || !safeThis->modelPathStorageFile.replaceWithText(selectedModel.getFullPathName()))
            {
                completion(juce::Result::fail("The Whisper model selection could not be saved."), {});
                return;
            }

            safeThis->startTranscription(song, selectedModel, std::move(completion));
        });
}

void MainComponent::startTranscription(
    const Song& song,
    const juce::File& modelFile,
    std::function<void(juce::Result, juce::String)> completion)
{
    getTranscriptionThreadPool().addJob(
        new TranscriptionJob(song, modelFile, std::move(completion)), true);
}

void MainComponent::timerCallback()
{
    const double duration = audioEngine.getLengthInSeconds();
    const double position = audioEngine.getCurrentPosition();
    const bool ready = hasCurrentSong && audioEngine.hasLoadedSong();

    miniTrackLabel.setText(ready
        ? juce::String(currentSong.title)
            + (currentSong.artist.isNotEmpty() ? juce::String(" - ") + currentSong.artist : juce::String{})
        : "Nothing playing", juce::dontSendNotification);
    miniPlaybackButton.setEnabled(ready);
    miniPlaybackButton.setButtonText(audioEngine.isPlaying() ? "Pause" : "Play");
    miniProgressSlider.setEnabled(ready && duration > 0.0);
    miniProgressSlider.setRange(0.0, juce::jmax(1.0, duration), 0.1);
    if (!miniProgressSlider.isMouseButtonDown())
        miniProgressSlider.setValue(position, juce::dontSendNotification);
    miniTimeLabel.setText(formatPlaybackTime(position) + " / " + formatPlaybackTime(duration),
                          juce::dontSendNotification);
}

void MainComponent::showPlaylistDetail(Playlist* playlist)
{
    playlistDetailPage.setPlaylist(playlist);
    showPage(playlistDetailPage);
}

void MainComponent::showMixingPage(Playlist* playlist, int songIndex)
{
    mixingPage.setContent(playlist, songIndex);
    showPage(mixingPage);
}

void MainComponent::showPage(juce::Component& page)
{
    sidebar.setPageContent(nullptr);

    const bool isSubPage = (&page != &mainMenuPage);
    sidebar.setVisible(isSubPage);

    // Context-aware back button: each sub-page knows where "back" leads.
    if (&page == &playlistsPage)
        sidebar.onBackClicked = [this] { showPage(mainMenuPage); };
    else if (&page == &playlistDetailPage)
        sidebar.onBackClicked = [this] { showPage(playlistsPage); };
    else if (&page == &mixingPage)
        sidebar.onBackClicked = [this] { showPage(playlistDetailPage); };
    else
        sidebar.onBackClicked = [this] { showPage(mainMenuPage); };

    // Inject page-specific sidebar content.
    if (&page == &playlistsPage)
        sidebar.setPageContent(&playlistsPage.getSidebarContent());
    else if (&page == &playlistDetailPage)
        sidebar.setPageContent(&playlistDetailPage.getSidebarContent());
    else if (&page == &songsPage)
        sidebar.setPageContent(&songsPage.getSidebarContent());
    else if (&page == &mixingPage)
        sidebar.setPageContent(&mixingPage.getSidebarContent());

    for (auto* p : { (juce::Component*)&mainMenuPage,
                     (juce::Component*)&playlistsPage,
                     (juce::Component*)&playlistDetailPage,
                     (juce::Component*)&songsPage,
                     (juce::Component*)&transitionsPage,
                     (juce::Component*)&mixingPage })
        p->setVisible(p == &page);

    resized();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    const int playerTop = getHeight() - kMiniPlayerHeight;
    g.setColour(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.18f));
    g.fillRect(0, playerTop, getWidth(), kMiniPlayerHeight);
    g.setColour(juce::Colours::grey.withAlpha(0.35f));
    g.drawHorizontalLine(playerTop, 0.0f, static_cast<float>(getWidth()));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto playerArea = area.removeFromBottom(kMiniPlayerHeight).reduced(12, 8);

    if (sidebar.isVisible())
        sidebar.setBounds(area.removeFromLeft(kSidebarWidth));

    for (auto* p : { (juce::Component*)&mainMenuPage,
                     (juce::Component*)&playlistsPage,
                     (juce::Component*)&playlistDetailPage,
                     (juce::Component*)&songsPage,
                     (juce::Component*)&transitionsPage,
                     (juce::Component*)&mixingPage })
        p->setBounds(area);

    miniPlaybackButton.setBounds(playerArea.removeFromLeft(68));
    playerArea.removeFromLeft(10);
    miniTrackLabel.setBounds(playerArea.removeFromLeft(240));
    playerArea.removeFromLeft(12);
    miniTimeLabel.setBounds(playerArea.removeFromRight(100));
    playerArea.removeFromRight(10);
    miniProgressSlider.setBounds(playerArea);
}
