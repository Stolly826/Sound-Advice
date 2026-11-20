#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "Transition.h"
#include "../SongAnalysis.h"

// Interactive waveform chart used in MixingPage.
// Hover shows a crosshair with live values; click shows a context menu.
class SongChartComponent : public juce::Component
{
public:
    SongChartComponent();

    // Fired when the user clicks the "Select Song" button on an empty chart.
    std::function<void()> onSelectSong;

    // playback hooks set by mixing page
    std::function<void(float)> onJumpTo;   // 0 to 1 position picked with jump to or loop return
    std::function<void()>      onSkip;     // skip button
    std::function<void()>      onPrevious; // back to the previous song in the playlist

    // the user moved this chart's fade, so the other chart can match its length
    std::function<void()>      onFadeChanged;

    // the user placed any marker (fade, loop or start), so the page can save them
    std::function<void()>      onMarkersChanged;

    // resize this chart's fade to last this many seconds, keeping the end the song is
    // anchored on (fade out keeps its start, fade in keeps its start too)
    void setFadeSeconds(double seconds);

    // how long the other chart's fade is, so a fade placed here can match it (0 if none)
    std::function<double()> getPartnerFadeSeconds;

    // where playback is 0 to 1 or -1 to hide the line
    void setPlayhead(float position);

    // the audio file to graph, analysed in the background (empty = no file)
    void setSongFile(const juce::String& path);

    void setSongName    (const juce::String& name);
    void setSongDuration(int seconds);
    juce::String getSongName() const { return songName; }

    // Pass true for the second (next-song) chart to get the "next song" menu.
    void setIsNextSong(bool isNext);

    // Apply a saved transition's markers to this chart.
    void applyTransition(const Transition& t);

    // Read back the points the user has placed.
    float getLoopPoint()  const { return loopPoint; }
    float getStartPoint() const { return startPoint; }
    float getFadeStart()  const { return fadeStart; }
    float getFadeEnd()    const { return fadeEnd; }
    double getFadeSeconds() const; // length of this chart's fade, 0 if it isnt fully placed

    // When false the right-click menu is suppressed (used in MixingPage after preset applied).
    void setEditable(bool editable) { isEditable = editable; }

    void paint    (juce::Graphics&          g) override;
    void resized  ()                            override;
    void mouseMove(const juce::MouseEvent&  e) override;
    void mouseExit(const juce::MouseEvent&  e) override;
    void mouseDown(const juce::MouseEvent&  e) override;

private:
    // ── Small icon buttons drawn inside the chart header ──────────────────
    class OverlayButton : public juce::Button
    {
    public:
        enum class Shape { LoopReturn, Skip, Previous };
        explicit OverlayButton(Shape s) : juce::Button(""), shape(s) {}
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    private:
        Shape shape;
    };

    juce::String   songName;
    int            durationSeconds = 0;
    int            hoverX          = -1;
    bool           isNextSong      = false;
    bool           isEditable      = true;
    float          playhead        = -1.0f;

    float          loopPoint  = -1.0f;
    float          startPoint = -1.0f;
    float          fadeStart  = -1.0f;
    float          fadeEnd    = -1.0f;

    // real data for the graph
    enum class GraphState { noFile, loading, ready, failed };
    juce::String   songFile;
    GraphState     graphState = GraphState::noFile;
    std::shared_ptr<const SongAnalysis> analysis;

    static constexpr double kDefaultFadeSeconds = 8.0;
    void completeFade(bool placedStart); // fills in the other end of a fade

    OverlayButton      loopReturnButton { OverlayButton::Shape::LoopReturn };
    OverlayButton      skipButton       { OverlayButton::Shape::Skip };
    OverlayButton      previousButton   { OverlayButton::Shape::Previous };
    juce::TextButton   selectSongButton { "Select Song" };
    juce::TextButton   changeSongButton { "Select" }; // small header button once a song is picked

    // Chart sub-area (excludes name header and legend strip)
    juce::Rectangle<int> getChartArea() const;

    // each line's value at t (0 to 1 across the song), scaled to -1..1 for drawing
    // returns false where that line has no reading (silence, no clear note or beat)
    bool         waveValueAt  (int specIndex, float t, float& out) const;
    juce::String formatValue  (int specIndex, float t)            const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongChartComponent)
};
