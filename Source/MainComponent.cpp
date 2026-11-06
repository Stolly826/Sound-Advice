#include "MainComponent.h"

static constexpr int kSidebarWidth = 220;
static constexpr int kSaveDelayMs  = 500;

static Song toSong(const SongInfo& info)
{
    Song s;
    s.title           = info.title;
    s.artist          = info.artist;
    s.album           = info.album;
    s.filePath        = info.path;
    s.dateAdded       = juce::Time::fromISO8601(info.addedAt);
    s.durationSeconds = juce::roundToInt(info.lengthSeconds);
    return s;
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

    //each song remembers its own speed, volume, gain and eq in the library
    mixingPage.getSongSettings = [this](const Song& song)
        {
            const auto* info = songLibrary.findByPath(song.filePath);
            return info != nullptr ? info->settings : SongSettings{};
        };

    //changes are heard straight away, the file gets written once the sliders settle
    mixingPage.onSongSettingsChanged = [this](const Song& song, const SongSettings& settings)
        {
            audioEngine.applySettings(settings);
            songLibrary.setSettings(song.filePath, settings);
            startTimer(kSaveDelayMs);
        };

    // FRONTEND/BACKEND INTEGRATION: Connect the song pages to the song library

    //reads the song details from a file chosen in the add song dialog
    auto readSongDetails = [this](const juce::File& file)
        {
            return toSong(SongData::readDetails(audioEngine.getFormats(), file));
        };
    songsPage.readSongDetails          = readSongDetails;
    playlistDetailPage.readSongDetails = readSongDetails;

    songsPage.onNewSong          = [this](const Song& s) { addUploadedSong(s); };
    playlistDetailPage.onNewSong = [this](const Song& s) { addUploadedSong(s); };
    songsPage.onSongRemoved      = [this](const Song& s) { songLibrary.remove(s.filePath); };

    //brings back everything uploaded in earlier runs
    juce::Array<Song> savedSongs;
    for (const auto& info : songLibrary.getSongs())
        savedSongs.add(toSong(info));
    songsPage.addSongs(savedSongs);

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

void MainComponent::addUploadedSong(const Song& song)
{
    //songs typed in without a file have no audio to save settings for
    if (song.filePath.isNotEmpty())
    {
        //reading the whole song for bpm and loudness can take a second so show the loading cursor
        juce::MouseCursor::showWaitCursor();
        auto info = SongData::readSong(audioEngine.getFormats(), juce::File(song.filePath));
        juce::MouseCursor::hideWaitCursor();

        //whatever was typed in the dialog wins over the tags
        info.title   = song.title;
        info.artist  = song.artist;
        info.album   = song.album;
        info.addedAt = song.dateAdded.toISO8601(true);

        if (!songLibrary.save(info))
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Unable to save song",
                "Could not write to " + songLibrary.getFile().getFullPathName());
    }

    songsPage.addSongs({ song });
}

void MainComponent::timerCallback()
{
    stopTimer();
    songLibrary.writeToDisk();
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
