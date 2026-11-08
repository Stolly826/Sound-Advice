#include "SongChartComponent.h"

// ── Wave data ─────────────────────────────────────────────────────────────────
namespace {
    struct WaveSpec {
        juce::Colour colour;
        float        freq;
        float        amp;
        float        phase;
        const char*  label;
    };

    const WaveSpec kWaves[3] = {
        { juce::Colour(0xff4c9be8), 4.2f, 1.00f, 0.0f, "Pitch"  },
        { juce::Colour(0xff4ecb71), 6.1f, 0.65f, 1.3f, "BPM"    },
        { juce::Colour(0xffe8a14c), 2.7f, 0.80f, 2.5f, "Volume" },
    };

    static constexpr int kBPad  = 8;   // inner padding
    static constexpr int kNameH = 18;  // song name strip
    static constexpr int kLegH  = 18;  // legend + time strip

    juce::String fmtTime(int secs)
    {
        return juce::String(secs / 60) + ":"
             + juce::String(secs % 60).paddedLeft('0', 2);
    }
}

// ── OverlayButton ─────────────────────────────────────────────────────────────
void SongChartComponent::OverlayButton::paintButton(juce::Graphics& g,
                                                     bool highlighted,
                                                     bool /*down*/)
{
    auto b  = getLocalBounds().toFloat().reduced(2.0f);
    auto col = highlighted ? juce::Colours::white : juce::Colours::white.withAlpha(0.55f);
    g.setColour(col);

    if (shape == Shape::LoopReturn)
    {
        // Nearly-full circular arc with an arrowhead at the end
        float cx = b.getCentreX(), cy = b.getCentreY();
        float r  = juce::jmin(b.getWidth(), b.getHeight()) * 0.36f;

        float startA = juce::degreesToRadians(50.0f);
        float endA   = juce::degreesToRadians(355.0f);

        juce::Path arc;
        arc.addCentredArc(cx, cy, r, r, 0.0f, startA, endA, true);
        g.strokePath(arc, juce::PathStrokeType(1.4f));

        // Arrowhead at endA, pointing counter-clockwise (loop/return feel)
        float ex = cx + r * std::cos(endA);
        float ey = cy + r * std::sin(endA);
        float tx =  std::sin(endA);   // counter-clockwise tangent
        float ty = -std::cos(endA);
        float px = -ty, py = tx;
        float as = 3.2f;

        juce::Path head;
        head.startNewSubPath(ex + tx * as,                     ey + ty * as);
        head.lineTo         (ex - tx * as * 0.3f + px * as,   ey - ty * as * 0.3f + py * as);
        head.lineTo         (ex - tx * as * 0.3f - px * as,   ey - ty * as * 0.3f - py * as);
        head.closeSubPath();
        g.fillPath(head);
    }
    else // Skip
    {
        // Filled right-pointing triangle + vertical bar
        float cy   = b.getCentreY();
        float barW = 2.2f;
        float barH = b.getHeight() * 0.62f;
        float barX = b.getX();             // bar on the LEFT
        float barY = cy - barH * 0.5f;
        g.fillRect(barX, barY, barW, barH);

        float triLeft = barX + barW + 1.5f; // triangle just right of bar
        float triH    = barH;
        juce::Path tri;
        tri.startNewSubPath(triLeft,               cy - triH * 0.5f); // top-left
        tri.lineTo         (triLeft,               cy + triH * 0.5f); // bottom-left
        tri.lineTo         (triLeft + triH * 0.6f, cy);               // tip pointing RIGHT
        tri.closeSubPath();
        g.fillPath(tri);
    }
}

// ── Construction ──────────────────────────────────────────────────────────────
SongChartComponent::SongChartComponent()
{
    // loop return jumps to the loop marker if one is set
    loopReturnButton.onClick = [this] { if (loopPoint >= 0.0f && onJumpTo) onJumpTo(loopPoint); };
    skipButton.onClick       = [this] { if (onSkip) onSkip(); };
    addAndMakeVisible(loopReturnButton);
    addAndMakeVisible(skipButton);

    selectSongButton.onClick = [this] { if (onSelectSong) onSelectSong(); };
    addAndMakeVisible(selectSongButton);
}

// ── Public setters ─────────────────────────────────────────────────────────────
void SongChartComponent::setSongName(const juce::String& name)
{
    if (name != songName)
    {
        loopPoint = startPoint = fadeStart = fadeEnd = -1.0f;
        isEditable = true;
    }
    songName = name;
    selectSongButton.setVisible(name.isEmpty());
    repaint();
}

void SongChartComponent::setSongDuration(int seconds)
{
    durationSeconds = seconds;
    repaint();
}

void SongChartComponent::setPlayhead(float position)
{
    // only repaint when it moved
    if (! juce::exactlyEqual(position, playhead)) { playhead = position; repaint(); }
}

void SongChartComponent::setIsNextSong(bool isNext)
{
    isNextSong = isNext;
}

void SongChartComponent::applyTransition(const Transition& t)
{
    if (isNextSong)
    {
        startPoint = t.startPoint;
        fadeStart  = t.fadeInStart;
        fadeEnd    = t.fadeInEnd;
    }
    else
    {
        loopPoint  = t.loopPoint;
        fadeStart  = t.fadeOutStart;
        fadeEnd    = t.fadeOutEnd;
    }
    repaint();
}

// ── Geometry ──────────────────────────────────────────────────────────────────
juce::Rectangle<int> SongChartComponent::getChartArea() const
{
    return getLocalBounds()
        .reduced(kBPad)
        .withTrimmedTop  (kNameH + 2)
        .withTrimmedBottom(kLegH  + 2);
}

void SongChartComponent::resized()
{
    // Position overlay buttons in the top-right of the name header strip
    constexpr int btnSize = 16;
    constexpr int gap     = 3;
    auto inner = getLocalBounds().reduced(kBPad);
    int btnY   = inner.getY() + (kNameH - btnSize) / 2;
    int right  = inner.getRight();

    skipButton      .setBounds(right - btnSize,              btnY, btnSize, btnSize);
    loopReturnButton.setBounds(right - btnSize * 2 - gap,    btnY, btnSize, btnSize);

    // "Select Song" centred in the chart data area
    auto ca = getChartArea();
    selectSongButton.setBounds(ca.withSizeKeepingCentre(120, 30));
}

// ── Wave helpers ──────────────────────────────────────────────────────────────
float SongChartComponent::waveValueAt(int i, float t) const
{
    return kWaves[i].amp
         * std::sin(t * juce::MathConstants<float>::twoPi * kWaves[i].freq
                    + kWaves[i].phase);
}

juce::String SongChartComponent::formatValue(int i, float rawValue) const
{
    // Normalise to [-1, 1] then map to a human-readable unit
    const float norm = rawValue / juce::jmax(kWaves[i].amp, 0.001f);
    switch (i)
    {
        case 0: // Pitch → semitones
        {
            const float st = norm * 12.0f;
            return (st >= 0.0f ? "+" : "") + juce::String(st, 1) + " st";
        }
        case 1: // BPM
            return juce::String(juce::roundToInt(128.0f + norm * 20.0f)) + " bpm";
        case 2: // Volume
            return juce::String(
                       juce::jlimit(0, 100, juce::roundToInt(50.0f + norm * 50.0f)))
                   + "%";
    }
    return {};
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void SongChartComponent::mouseMove(const juce::MouseEvent& e)
{
    const int nx = getChartArea().contains(e.x, e.y) ? e.x : -1;
    if (nx != hoverX) { hoverX = nx; repaint(); }
}

void SongChartComponent::mouseExit(const juce::MouseEvent&)
{
    if (hoverX != -1) { hoverX = -1; repaint(); }
}

void SongChartComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!getChartArea().contains(e.x, e.y)) return;
    // jump to stays available after a preset locks the markers
    if (!isEditable && isNextSong) return;

    const auto  ca = getChartArea();
    const float t  = juce::jlimit(0.0f, 1.0f,
        ((float)e.x - (float)ca.getX()) / (float)ca.getWidth());

    juce::PopupMenu menu;
    if (isNextSong)
    {
        menu.addItem(1, "Set Start");
        menu.addItem(2, "Set Loop");
        menu.addItem(3, "Start Fade In");
        menu.addItem(4, "End Fade In");
    }
    else
    {
        menu.addItem(1, "Jump To");
        menu.addItem(2, "Set Loop",       isEditable);
        menu.addItem(3, "Start Fade Out", isEditable);
        menu.addItem(4, "End Fade Out",   isEditable);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ e.getScreenX(), e.getScreenY(), 1, 1 }),
        [this, t](int result)
        {
            switch (result)
            {
                case 1:
                    if (isNextSong) startPoint = t;
                    else if (onJumpTo) onJumpTo(t); // start playing from here
                    break;
                case 2: loopPoint = t; break;
                case 3: fadeStart = t; break;
                case 4: fadeEnd   = t; break;
                default: return;
            }
            repaint();
        });
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void SongChartComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto inner  = bounds.reduced(kBPad);
    const auto ca     = getChartArea();

    // Background + border
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);

    // Song name
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(songName.isNotEmpty() ? songName : "(none)",
               inner.getX(), inner.getY(), inner.getWidth(), kNameH,
               juce::Justification::centredLeft);

    if (songName.isEmpty()) return;

    const float cx   = (float)ca.getX();
    const float cy   = (float)ca.getY();
    const float cw   = (float)ca.getWidth();
    const float ch   = (float)ca.getHeight();
    const float midY = cy + ch * 0.5f;
    const float ampH = ch * 0.38f;

    // ── Grid ──────────────────────────────────────────────────────────────
    g.saveState();
    g.reduceClipRegion(ca);

    // Vertical lines (8 time divisions)
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine((int)(cx + (float)i / 8.0f * cw), cy, cy + ch);

    // Horizontal lines (6 value divisions, skip centre)
    for (int i = 1; i < 6; ++i)
    {
        if (i == 3) continue;
        g.drawHorizontalLine((int)(cy + (float)i / 6.0f * ch), cx, cx + cw);
    }

    // Centre "0" line — brighter
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.drawHorizontalLine((int)midY, cx, cx + cw);

    // ── Waveforms ─────────────────────────────────────────────────────────
    for (int wi = 0; wi < 3; ++wi)
    {
        juce::Path path;
        for (int i = 0; i <= 200; ++i)
        {
            const float t = (float)i / 200.0f;
            const float x = cx + t * cw;
            const float y = midY - ampH * waveValueAt(wi, t);
            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }
        g.setColour(kWaves[wi].colour.withAlpha(0.9f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    // ── Set-point markers ─────────────────────────────────────────────────
    auto drawMarker = [&](float pos, juce::Colour colour)
    {
        if (pos < 0.0f) return;
        const float mx = cx + pos * cw;
        g.setColour(colour.withAlpha(0.85f));
        g.drawVerticalLine((int)mx, cy, cy + ch);
        g.fillEllipse(mx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    };
    drawMarker(loopPoint,  juce::Colour(0xffFFD700)); // gold  — loop
    drawMarker(startPoint, juce::Colour(0xff00CCFF)); // cyan  — set start
    drawMarker(fadeStart,  juce::Colour(0xffFF8C00)); // orange — fade start
    drawMarker(fadeEnd,    juce::Colour(0xffFF4444)); // red   — fade end

    // playhead
    if (playhead >= 0.0f)
    {
        const float px = cx + playhead * cw;
        g.setColour(juce::Colours::white);
        g.fillRect(px - 1.0f, cy, 2.0f, ch);
    }

    // ── Hover vertical line + wave dots ───────────────────────────────────
    if (hoverX >= 0)
    {
        const float t = juce::jlimit(0.0f, 1.0f, ((float)hoverX - cx) / cw);

        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawVerticalLine(hoverX, cy, cy + ch);

        for (int wi = 0; wi < 3; ++wi)
        {
            const float dotY = midY - ampH * waveValueAt(wi, t);
            g.setColour(kWaves[wi].colour);
            g.fillEllipse((float)hoverX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
        }
    }

    g.restoreState(); // end chart clip

    // ── Hover value callout (drawn outside clip so it can overlap name) ───
    if (hoverX >= 0)
    {
        const float t    = juce::jlimit(0.0f, 1.0f, ((float)hoverX - cx) / cw);
        const int   boxW = 122;
        const int   boxH = 3 * 18 + 10;
        const int   boxX = (t > 0.65f) ? hoverX - boxW - 8 : hoverX + 8;
        const int   boxY = ca.getY() + 4;

        g.setColour(juce::Colours::black.withAlpha(0.80f));
        g.fillRoundedRectangle((float)boxX, (float)boxY,
                               (float)boxW, (float)boxH, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle((float)boxX, (float)boxY,
                               (float)boxW, (float)boxH, 4.0f, 1.0f);

        g.setFont(juce::FontOptions(11.0f));
        for (int wi = 0; wi < 3; ++wi)
        {
            const float      val  = waveValueAt(wi, t);
            const juce::String txt = juce::String(kWaves[wi].label) + ":  "
                                   + formatValue(wi, val);
            const int rowY = boxY + 5 + wi * 18;

            g.setColour(kWaves[wi].colour.withAlpha(0.9f));
            g.fillEllipse((float)(boxX + 7), (float)rowY + 4.0f, 7.0f, 7.0f);

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawText(txt, boxX + 18, rowY, boxW - 22, 15,
                       juce::Justification::centredLeft);
        }
    }

    // ── Legend (left) + time display (right) ──────────────────────────────
    const auto leg = inner.withTop(inner.getBottom() - kLegH);
    g.setFont(juce::FontOptions(10.0f));

    float lx = (float)leg.getX();
    for (int wi = 0; wi < 3; ++wi)
    {
        g.setColour(kWaves[wi].colour.withAlpha(0.8f));
        g.drawText(juce::String("-- ") + kWaves[wi].label,
                   (int)lx, leg.getY(), 70, kLegH,
                   juce::Justification::centredLeft);
        lx += 72.0f;
    }

    // Time: "m:ss / m:ss" (or just "-- / --" if no duration)
    if (durationSeconds > 0)
    {
        const juce::String total = fmtTime(durationSeconds);
        juce::String timeStr;
        if (hoverX >= 0)
        {
            const float t = juce::jlimit(0.0f, 1.0f, ((float)hoverX - cx) / cw);
            timeStr = fmtTime(juce::roundToInt(t * (float)durationSeconds))
                    + " / " + total;
        }
        else
        {
            // show playback time when theres a playhead
            const int nowSecs = playhead >= 0.0f ? juce::roundToInt(playhead * (float)durationSeconds) : 0;
            timeStr = fmtTime(nowSecs) + " / " + total;
        }

        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawText(timeStr, leg, juce::Justification::centredRight);
    }
}
