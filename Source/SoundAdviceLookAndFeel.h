#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

// App-wide look and feel. Handles custom fonts:
//
//   1. Drop .ttf / .otf files into Resources/Fonts/ and re-run CMake.
//      They get embedded into the exe, so teammates don't need them installed.
//   2. Pick a font per component by its family name:
//        SoundAdviceLookAndFeel::useFont (myButton, "Bungee");
//        SoundAdviceLookAndFeel::useFont (myLabel,  "Bungee", 28.0f);
//
// Names that aren't embedded fall back to fonts installed on the system,
// and then to JUCE's default font.
class SoundAdviceLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SoundAdviceLookAndFeel();

    // Tag a component with a font. height <= 0 means "pick a size automatically".
    static void useFont(juce::Component&, const juce::String& family, float height = 0.0f);

    // Build a font by name, e.g. "Bungee" or "Bungee Bold".
    juce::Font getFont(const juce::String& family, float height) const;

    // Names of every embedded font. Handy for checking what loaded.
    juce::StringArray getEmbeddedFontNames() const;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool isMouseOver, bool isButtonDown) override;

private:
    std::map<juce::String, juce::Typeface::Ptr> typefaces;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundAdviceLookAndFeel)
};
