#include "MainComponent.h"

static constexpr int kSidebarWidth = 220;

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

    songsPage.getPlaylistsForSong = [this](const Song& s)
    {
        return playlistsPage.getPlaylistNamesContaining(s);
    };

    showPage(mainMenuPage);

    setSize(1000, 620);
}

MainComponent::~MainComponent()
{
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
