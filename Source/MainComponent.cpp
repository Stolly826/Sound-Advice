#include "MainComponent.h"

static constexpr int kSidebarWidth = 220;
static constexpr int kSaveDelayMs  = 500;

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

    mixingPage.onPlaySong = [this](const Song& song, const SongSettings& settings)
        {
            //only play songs with the right files attached mp3 and wav
            if (song.filePath.isEmpty())
                return false;

            audioEngine.applySettings(settings);
            return audioEngine.playSong(juce::File(song.filePath));
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
    mixingPage.onSongSettingsChanged = [this](const Song& song, const SongSettings& settings)
        {
            audioEngine.applySettings(settings);

            const auto key = songsPage.getLibrary().getSettingsKey(song);
            if (key.isNotEmpty())
            {
                songSettings[key] = settings;
                startTimer(kSaveDelayMs);
            }
        };

    //a removed songs file can be reused by the next upload so its settings go with it
    songsPage.onSongRemoved = [this](const Song& song)
        {
            if (songSettings.erase(songsPage.getLibrary().getSettingsKey(song)) > 0)
                songsPage.getLibrary().saveSongSettings(songSettings);
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

    mixingPage.onScratch = [this](double offsetSeconds)
        {
            audioEngine.scratch(offsetSeconds);
        };

    // playhead jump to and skip read from the same engine
    mixingPage.getPlaybackPosition = [this] { return audioEngine.getPosition(); };
    mixingPage.getPlaybackLength   = [this] { return audioEngine.getLength(); };
    mixingPage.isPlaybackActive    = [this] { return audioEngine.isPlaying(); };
    mixingPage.getLoadedFilePath   = [this] { return audioEngine.getLoadedFilePath(); };
    mixingPage.onSeek              = [this](double seconds) { audioEngine.setPosition(seconds); };

    songsPage.getPlaylistsForSong = [this](const Song& s)
    {
        return playlistsPage.getPlaylistNamesContaining(s);
    };

    showPage(mainMenuPage);

    setSize(1000, 620);
}

MainComponent::~MainComponent()
{
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
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    if (sidebar.isVisible())
        sidebar.setBounds(area.removeFromLeft(kSidebarWidth));

    for (auto* p : { (juce::Component*)&mainMenuPage,
                     (juce::Component*)&playlistsPage,
                     (juce::Component*)&playlistDetailPage,
                     (juce::Component*)&songsPage,
                     (juce::Component*)&transitionsPage,
                     (juce::Component*)&mixingPage })
        p->setBounds(area);
}
