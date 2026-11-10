#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Transition.h"

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

    // where playback is 0 to 1 or -1 to hide the line
    void setPlayhead(float position);

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
        enum class Shape { LoopReturn, Skip };
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

    OverlayButton      loopReturnButton { OverlayButton::Shape::LoopReturn };
    OverlayButton      skipButton       { OverlayButton::Shape::Skip };
    juce::TextButton   selectSongButton { "Select Song" };

    // Chart sub-area (excludes name header and legend strip)
    juce::Rectangle<int> getChartArea() const;

    float        waveValueAt  (int specIndex, float t)        const;
    juce::String formatValue  (int specIndex, float rawValue) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongChartComponent)
};
