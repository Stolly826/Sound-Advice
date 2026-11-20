#include "MainComponent.h"

static constexpr int kSidebarWidth = 220;
static constexpr int kMiniPlayerHeight = 64;
static constexpr int kSaveDelayMs  = 500;

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
            playSong(currentSong, mixingPage.getSongSettings(currentSong));
        updateMiniPlayer();
    };

    modelPathStorageFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SoundAdvice").getChildFile("whisper-model-path.txt");

    mainMenuPage.onPlaylistsClicked   = [this] { showPage(playlistsPage); };
    mainMenuPage.onSongsClicked       = [this] { showPage(songsPage); };
    mainMenuPage.onTransitionsClicked = [this] { showPage(transitionsPage); };

    playlistsPage.onPlaylistClicked = [this](Playlist* p) { showPlaylistDetail(p); };

    playlistDetailPage.onSongClicked = [this](Playlist* p, int idx) { showMixingPage(p, idx); };
    playlistDetailPage.getAllSongs   = [this]() { return songsPage.getSongs(); };
    // order changes and added songs get saved
    playlistDetailPage.onPlaylistChanged = [this] { playlistsPage.playlistChanged(); };
    playlistDetailPage.addSongToLibrary  = [this](Song& s) { return songsPage.addSongToLibrary(s); };
    playlistDetailPage.onEditClicked     = [this](Playlist* p)
    {
        playlistsPage.showEditDialog(p, &playlistDetailPage, [this] { playlistDetailPage.refreshHeader(); });
    };
    transitionsPage.getAllSongs           = [this]()                              { return songsPage.getSongs(); };
    transitionsPage.getTransitionsForPair = [this](juce::String s1, juce::String s2) { return songsPage.getTransitionsForPair(s1, s2); };
    transitionsPage.addTransitionToSong   = [this](juce::String s1, Transition t)    { songsPage.addTransitionToSong(s1, t); };
    transitionsPage.updateTransitionInSong= [this](juce::String s1, int idx, Transition t) { songsPage.updateTransitionInSong(s1, idx, t); };

    mixingPage.getTransitionsForPair      = [this](juce::String s1, juce::String s2) { return songsPage.getTransitionsForPair(s1, s2); };

    // FRONTEND/BACKEND INTEGRATION: Connect the Mixing page controls to JUCE vibe coded 

    mixingPage.onPlaySong = [this](const Song& song, const SongSettings& settings,
                                   float fadeInStart, float fadeInEnd,
                                   float fadeOutStart, float fadeOutEnd)
        {
            return playSong(song, settings, fadeInStart, fadeInEnd, fadeOutStart, fadeOutEnd);
        };

    //start the next song quietly under the fade out, sounding how its sliders were set
    mixingPage.onCueNext = [this](const Song& song, float fadeInStart, float fadeInEnd)
        {
            return song.filePath.isNotEmpty()
                && audioEngine.cueNext(juce::File(song.filePath), fadeInStart, fadeInEnd,
                                       mixingPage.getSongSettings(song));
        };

    //pause the song without losing its current position
    mixingPage.onPause = [this] { audioEngine.pause(); };

    //resume the previously paused song
    mixingPage.onResume = [this] { audioEngine.resume(); };

    //stop the song and reset its playback position
    mixingPage.onStop = [this] { audioEngine.stop(); };

    //each song remembers its own speed, volume, gain and eq
    songSettings = songsPage.getLibrary().loadSongSettings();

    mixingPage.getSongSettings = [this](const Song& song)
        {
            const auto found = songSettings.find(songsPage.getLibrary().getSettingsKey(song));
            return found != songSettings.end() ? found->second : SongSettings{};
        };

    //changes are heard straight away, the file gets written once the sliders settle
    //the next song's sliders go to the incoming deck so they're heard while it fades in
    mixingPage.onSongSettingsChanged = [this](const Song& song, const SongSettings& settings,
                                              bool isNextSong)
        {
            if (isNextSong)
                audioEngine.applyIncomingSettings(settings);
            else
                audioEngine.applySettings(settings);

            const auto key = songsPage.getLibrary().getSettingsKey(song);
            if (key.isNotEmpty())
            {
                songSettings[key] = settings;
                startTimer(kSaveDelayMs);
            }
        };

    //fades, loop and start markers come back when you return to a pair of songs, even after a restart
    mixMarkers = songsPage.getLibrary().loadMixMarkers();

    mixingPage.getMixMarkers = [this](const Song& from, const Song& to)
        {
            const auto found = mixMarkers.find(songsPage.getLibrary().getMixKey(from, to));
            return found != mixMarkers.end() ? found->second : Transition{};
        };

    mixingPage.onMixMarkersChanged = [this](const Song& from, const Song& to, const Transition& t)
        {
            mixMarkers[songsPage.getLibrary().getMixKey(from, to)] = t;
            if (! songsPage.getLibrary().saveMixMarkers(mixMarkers))
            {
                DBG("couldnt save mix markers to " + songsPage.getLibrary().getMixesFile().getFullPathName());
            }
        };

    //a removed songs file can be reused by the next upload so its settings and mixes go with it
    songsPage.onSongRemoved = [this](const Song& song)
        {
            const auto key = songsPage.getLibrary().getSettingsKey(song);
            if (songSettings.erase(key) > 0)
                songsPage.getLibrary().saveSongSettings(songSettings);

            const auto before = mixMarkers.size();
            const auto ownKey = key.isNotEmpty() ? key : "title:" + song.title;
            for (auto it = mixMarkers.begin(); it != mixMarkers.end();)
            {
                if (it->first.startsWith(ownKey + " -> ") || it->first.endsWith(" -> " + ownKey))
                    it = mixMarkers.erase(it);
                else
                    ++it;
            }
            if (mixMarkers.size() != before)
                songsPage.getLibrary().saveMixMarkers(mixMarkers);
        };

    // FRONTEND/BACKEND INTEGRATION: fill in the add song dialog from the files own tags
    auto readSongDetails = [this](const juce::File& file)
        {
            const auto info = SongData::readDetails(audioEngine.getFormats(), file);

            Song s;
            s.title           = info.title;
            s.artist          = info.artist;
            s.album           = info.album;
            s.durationSeconds = juce::roundToInt(info.lengthSeconds);
            return s;
        };
    songsPage.readSongDetails          = readSongDetails;
    playlistDetailPage.readSongDetails = readSongDetails;

    //scratching: grab the record, move it, let go
    mixingPage.onScratchStart = [this] { audioEngine.beginScratch(); };
    mixingPage.onScratchMove  = [this](double offsetSeconds) { audioEngine.scratchTo(offsetSeconds); };
    mixingPage.onScratchEnd   = [this] { audioEngine.endScratch(); };

    // playhead jump to and skip read from the same engine
    mixingPage.getPlaybackPosition = [this] { return audioEngine.getPosition(); };
    mixingPage.getPlaybackLength   = [this] { return audioEngine.getLength(); };
    mixingPage.isPlaybackActive    = [this] { return audioEngine.isPlaying(); };
    mixingPage.isPlaybackPaused    = [this] { return audioEngine.isPaused(); };
    mixingPage.getLoadedFilePath   = [this] { return audioEngine.getLoadedFilePath(); };
    mixingPage.onSeek              = [this](double seconds) { audioEngine.setPosition(seconds); };

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
    miniPlayerTimer.onTick = [this] { updateMiniPlayer(); };
    miniPlayerTimer.startTimerHz(4);
}

MainComponent::~MainComponent()
{
    miniPlayerTimer.stopTimer();

    if (isTimerRunning())
        timerCallback(); //dont lose a slider change made right before closing

    sidebar.setPageContent(nullptr);
    setLookAndFeel(nullptr);
}

void MainComponent::timerCallback()
{
    stopTimer();

    if (! songsPage.getLibrary().saveSongSettings(songSettings))
        DBG("couldnt save song settings to " + songsPage.getLibrary().getSettingsFile().getFullPathName());
}

bool MainComponent::playSong(const Song& song, const SongSettings& settings,
                             float fadeInStart, float fadeInEnd,
                             float fadeOutStart, float fadeOutEnd)
{
    //only play songs with the right files attached mp3 and wav
    if (song.filePath.isEmpty())
        return false;

    audioEngine.applySettings(settings);
    if (!audioEngine.playSong(juce::File(song.filePath),
                              fadeInStart, fadeInEnd, fadeOutStart, fadeOutEnd))
        return false;

    currentSong = song;
    hasCurrentSong = true;
    updateMiniPlayer();
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

void MainComponent::updateMiniPlayer()
{
    const double duration = audioEngine.getLength();
    const double position = audioEngine.getPosition();
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
