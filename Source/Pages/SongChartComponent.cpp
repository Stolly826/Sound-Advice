#include "SongChartComponent.h"

// ── Wave data ─────────────────────────────────────────────────────────────────
namespace {
    struct WaveSpec {
        juce::Colour colour;
        const char*  label;
    };

    enum Wave { pitchWave, bpmWave, volumeWave };
    const WaveSpec kWaves[3] = {
        { juce::Colour(0xff4c9be8), "Pitch"  },
        { juce::Colour(0xff4ecb71), "BPM"    },
        { juce::Colour(0xffe8a14c), "Volume" },
    };

    // how far the bpm line can move from the song's tempo before it hits the edge
    static constexpr float kBpmSpread    = 12.0f;
    // volume is drawn from this quiet level (bottom) up to full scale (top)
    static constexpr float kQuietestDb   = -48.0f;

    static constexpr int kBPad  = 8;   // inner padding
    static constexpr int kNameH = 18;  // song name strip
    static constexpr int kLegH  = 18;  // legend + time strip

    static constexpr int kCurveIdBase = 100; // menu result for a fade curve: 100 + curve number

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
    else if (shape == Shape::Previous)
    {
        // mirror of skip: triangle pointing LEFT with the bar on its right
        float cy   = b.getCentreY();
        float barW = 2.2f;
        float barH = b.getHeight() * 0.62f;
        float barX = b.getRight() - barW;
        g.fillRect(barX, cy - barH * 0.5f, barW, barH);

        float triRight = barX - 1.5f;
        juce::Path tri;
        tri.startNewSubPath(triRight,               cy - barH * 0.5f);
        tri.lineTo         (triRight,               cy + barH * 0.5f);
        tri.lineTo         (triRight - barH * 0.6f, cy);
        tri.closeSubPath();
        g.fillPath(tri);
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
    previousButton.onClick   = [this] { if (onPrevious) onPrevious(); };
    previousButton.setTooltip("Previous song");
    skipButton.setTooltip("Next song");
    loopReturnButton.setTooltip("Back to the loop marker");
    addAndMakeVisible(previousButton);
    addAndMakeVisible(loopReturnButton);
    addAndMakeVisible(skipButton);

    selectSongButton.onClick = [this] { if (onSelectSong) onSelectSong(); };
    addAndMakeVisible(selectSongButton);

    changeSongButton.onClick = [this] { if (onSelectSong) onSelectSong(); };
    addChildComponent(changeSongButton); // shown by setSongName
}

// ── Public setters ─────────────────────────────────────────────────────────────
void SongChartComponent::setSongName(const juce::String& name)
{
    if (name != songName)
    {
        loopPoint = startPoint = fadeStart = fadeEnd = -1.0f;
        fadeCurve  = FadeCurve::defaultCurve;
        isEditable = true;
    }
    songName = name;
    // only pages that can pick a song (transitions) get the button, the rest show a message
    selectSongButton.setVisible(name.isEmpty() && onSelectSong != nullptr);
    changeSongButton.setVisible(name.isNotEmpty() && onSelectSong != nullptr);
    repaint();
}

void SongChartComponent::setSongFile(const juce::String& path)
{
    if (path == songFile && graphState != GraphState::failed)
        return;

    songFile = path;
    analysis.reset();

    if (path.isEmpty() || !juce::File(path).existsAsFile())
    {
        graphState = path.isEmpty() ? GraphState::noFile : GraphState::failed;
        repaint();
        return;
    }

    graphState = GraphState::loading;
    repaint();

    juce::Component::SafePointer<SongChartComponent> safeThis(this);
    SongAnalyser::Service::getInstance()->request(path,
        [safeThis, path](std::shared_ptr<const SongAnalysis> result)
        {
            // the chart may have moved on to another song while this one was being read
            if (safeThis == nullptr || safeThis->songFile != path)
                return;

            safeThis->analysis   = result;
            safeThis->graphState = result != nullptr ? GraphState::ready : GraphState::failed;
            safeThis->repaint();
        });
}

double SongChartComponent::getFadeSeconds() const
{
    if (fadeStart < 0.0f || fadeEnd <= fadeStart || durationSeconds <= 0)
        return 0.0;
    return (double)(fadeEnd - fadeStart) * durationSeconds;
}

void SongChartComponent::setFadeSeconds(double seconds)
{
    if (seconds <= 0.0 || durationSeconds <= 0)
        return;

    const float length = (float)(seconds / durationSeconds);
    if (fadeStart >= 0.0f)
        fadeEnd = juce::jmin(1.0f, fadeStart + length);
    else if (fadeEnd >= 0.0f)
        fadeStart = juce::jmax(0.0f, fadeEnd - length);
    else
        return; // no fade placed here yet, it will match when the user places one

    repaint();
}

void SongChartComponent::completeFade(bool placedStart)
{
    // already a sensible fade, leave the user's other marker alone
    if (fadeStart >= 0.0f && fadeEnd > fadeStart)
        return;

    // match the other song's fade so the two line up, otherwise a few seconds
    double seconds = getPartnerFadeSeconds ? getPartnerFadeSeconds() : 0.0;
    if (seconds <= 0.0)
        seconds = kDefaultFadeSeconds;

    const float length = durationSeconds > 0 ? (float)(seconds / durationSeconds) : 0.05f;

    if (placedStart)
        fadeEnd = juce::jmin(1.0f, fadeStart + length);
    else
        fadeStart = juce::jmax(0.0f, fadeEnd - length);
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
        fadeCurve  = t.fadeInCurve;
    }
    else
    {
        loopPoint  = t.loopPoint;
        fadeStart  = t.fadeOutStart;
        fadeEnd    = t.fadeOutEnd;
        fadeCurve  = t.fadeOutCurve;
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
    previousButton  .setBounds(right - btnSize * 3 - gap * 2, btnY, btnSize, btnSize);
    previousButton  .setVisible(onPrevious != nullptr); // only the mixing page's first chart has one

    constexpr int selectW = 48;
    changeSongButton.setBounds(right - btnSize * 3 - gap * 4 - selectW, btnY - 1, selectW, btnSize + 2);

    // "Select Song" centred in the chart data area
    auto ca = getChartArea();
    selectSongButton.setBounds(ca.withSizeKeepingCentre(120, 30));
    selectSongButton.setVisible(songName.isEmpty() && onSelectSong != nullptr);
}

// ── Wave helpers ──────────────────────────────────────────────────────────────
bool SongChartComponent::waveValueAt(int i, float t, float& out) const
{
    if (graphState != GraphState::ready || analysis == nullptr)
        return false;

    const int n   = SongAnalysis::kPoints;
    const int idx = juce::jlimit(0, n - 1, (int)(t * (float)n));

    switch (i)
    {
        case pitchWave:
        {
            const float note = analysis->pitchNote[(size_t)idx];
            const float range = analysis->highNote - analysis->lowNote;
            if (note <= 0.0f || range <= 0.0f) return false;
            out = (note - analysis->lowNote) / range * 2.0f - 1.0f;
            break;
        }
        case bpmWave:
        {
            const float bpm = analysis->bpm[(size_t)idx];
            if (bpm <= 0.0f || analysis->songBpm <= 0.0f) return false;
            out = (bpm - analysis->songBpm) / kBpmSpread;
            break;
        }
        case volumeWave:
            out = (analysis->volumeDb[(size_t)idx] - kQuietestDb) / -kQuietestDb * 2.0f - 1.0f;
            break;
        default:
            return false;
    }

    out = juce::jlimit(-1.0f, 1.0f, out);
    return true;
}

juce::String SongChartComponent::formatValue(int i, float t) const
{
    if (graphState != GraphState::ready || analysis == nullptr)
        return "--";

    const int idx = juce::jlimit(0, SongAnalysis::kPoints - 1, (int)(t * (float)SongAnalysis::kPoints));
    switch (i)
    {
        case pitchWave:
        {
            const float note = analysis->pitchNote[(size_t)idx];
            return note > 0.0f ? juce::MidiMessage::getMidiNoteName(juce::roundToInt(note), true, true, 4)
                               : juce::String("--");
        }
        case bpmWave:
        {
            const float bpm = analysis->bpm[(size_t)idx];
            return bpm > 0.0f ? juce::String(juce::roundToInt(bpm)) + " bpm" : juce::String("--");
        }
        case volumeWave:
        {
            const float db = analysis->volumeDb[(size_t)idx];
            return db > -99.0f ? juce::String(db, 1) + " dB" : juce::String("silent");
        }
        default:
            return {};
    }
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

    // hovering Start Fade In/Out opens the curve list to its right,
    // clicking it straight away keeps the curve the fade already has
    juce::PopupMenu curveMenu;
    for (int c = 0; c < (int)FadeCurve::numCurves; ++c)
        curveMenu.addItem(kCurveIdBase + c, getFadeCurveName((FadeCurve)c),
                          true, fadeCurve == (FadeCurve)c);

    juce::PopupMenu menu;
    if (isNextSong)
    {
        menu.addItem(1, "Set Start");
        menu.addItem(2, "Set Loop");
        menu.addSubMenu("Start Fade In", curveMenu, true, std::unique_ptr<juce::Drawable>{}, false, 3);
        menu.addItem(4, "End Fade In");
    }
    else
    {
        menu.addItem(1, "Jump To");
        menu.addItem(2, "Set Loop",       isEditable);
        menu.addSubMenu("Start Fade Out", curveMenu, isEditable, std::unique_ptr<juce::Drawable>{}, false, 3);
        menu.addItem(4, "End Fade Out",   isEditable);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ e.getScreenX(), e.getScreenY(), 1, 1 }),
        [this, t](int result)
        {
            // picking a curve also starts the fade here, with that shape
            if (result >= kCurveIdBase)
            {
                fadeCurve = fadeCurveFromInt(result - kCurveIdBase);
                result = 3;
            }

            switch (result)
            {
                case 1:
                    if (isNextSong) startPoint = t;
                    else if (onJumpTo) onJumpTo(t); // start playing from here
                    break;
                case 2: loopPoint = t; break;
                // placing one end of a fade puts the other end in for you if it is missing
                case 3: fadeStart = t; completeFade(true);  break;
                case 4: fadeEnd   = t; completeFade(false); break;
                default: return;
            }
            repaint();

            // the other song's fade follows so the fade out and fade in always last as long
            if ((result == 3 || result == 4) && onFadeChanged)
                onFadeChanged();

            // jump to on the first chart is playback, not a marker
            if ((result != 1 || isNextSong) && onMarkersChanged)
                onMarkersChanged();
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

    if (songName.isEmpty())
    {
        // no way to pick one here (e.g. the end of a playlist on the mixing page)
        if (onSelectSong == nullptr)
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(juce::FontOptions(13.0f));
            g.drawText("No Song Selected", ca, juce::Justification::centred);
        }
        return;
    }

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
    if (graphState == GraphState::ready)
    {
        // one point per analysed slice, with a gap wherever a line has no reading
        for (int wi = 0; wi < 3; ++wi)
        {
            juce::Path path;
            bool drawing = false;
            for (int i = 0; i < SongAnalysis::kPoints; ++i)
            {
                const float t = ((float)i + 0.5f) / (float)SongAnalysis::kPoints;
                float value = 0.0f;
                if (!waveValueAt(wi, t, value)) { drawing = false; continue; }

                const float x = cx + t * cw;
                const float y = midY - ampH * value;
                if (drawing) path.lineTo(x, y);
                else         path.startNewSubPath(x, y);
                drawing = true;
            }
            g.setColour(kWaves[wi].colour.withAlpha(0.9f));
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }
    else
    {
        const char* message = graphState == GraphState::loading ? "Analysing song..."
                            : graphState == GraphState::failed  ? "Couldn't read this song's audio file"
                                                                : "No audio file for this song";
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(message, ca, juce::Justification::centred);
    }

    // ── Fade shape: the volume curve drawn between the fade markers ───────
    if (fadeStart >= 0.0f && fadeEnd > fadeStart)
    {
        const float x0     = cx + fadeStart * cw;
        const float x1     = cx + fadeEnd   * cw;
        const float top    = cy + 4.0f;
        const float bottom = cy + ch - 2.0f;
        constexpr int steps = 48;

        juce::Path curve;
        for (int i = 0; i <= steps; ++i)
        {
            const float p     = (float)i / (float)steps;
            const float level = isNextSong ? fadeInLevel(p, fadeCurve) : fadeOutLevel(p, fadeCurve);
            const float x     = x0 + (x1 - x0) * p;
            const float y     = bottom - (bottom - top) * level;
            if (i == 0) curve.startNewSubPath(x, y);
            else        curve.lineTo(x, y);
        }

        const juce::Colour fadeColour(0xffFF8C00);
        juce::Path area(curve);
        area.lineTo(x1, bottom);
        area.lineTo(x0, bottom);
        area.closeSubPath();
        g.setColour(fadeColour.withAlpha(0.12f));
        g.fillPath(area);
        g.setColour(fadeColour.withAlpha(0.8f));
        g.strokePath(curve, juce::PathStrokeType(1.5f));

        // name the curve when the fade is wide enough to fit it
        if (x1 - x0 > 70.0f)
        {
            g.setFont(juce::FontOptions(9.5f));
            g.setColour(fadeColour.withAlpha(0.9f));
            g.drawText(getFadeCurveName(fadeCurve), (int)x0 + 5, (int)top + 2,
                       (int)(x1 - x0) - 8, 12,
                       isNextSong ? juce::Justification::centredRight
                                  : juce::Justification::centredLeft);
        }
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
            float value = 0.0f;
            if (!waveValueAt(wi, t, value)) continue;
            const float dotY = midY - ampH * value;
            g.setColour(kWaves[wi].colour);
            g.fillEllipse((float)hoverX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
        }
    }

    g.restoreState(); // end chart clip

    // ── Hover value callout (drawn outside clip so it can overlap name) ───
    if (hoverX >= 0 && graphState == GraphState::ready)
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
            const juce::String txt = juce::String(kWaves[wi].label) + ":  "
                                   + formatValue(wi, t);
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
