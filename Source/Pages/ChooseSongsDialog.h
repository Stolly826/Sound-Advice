#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Popup shown when TransitionsPage first opens.
// User types (or will later pick) the two songs for the transition.
class ChooseSongsDialog : public juce::Component
{
public:
    // Fired when the user confirms. Args: top-song name, bottom-song name.
    std::function<void(juce::String, juce::String)> onChosen;

    ChooseSongsDialog();
    void paint  (juce::Graphics& g) override;
    void resized()                  override;

    static constexpr int kWidth  = 420;
    static constexpr int kHeight = 265;

private:
    juce::TextEditor topSongEditor;
    juce::TextEditor bottomSongEditor;
    juce::TextButton okButton     { "Choose" };
    juce::TextButton cancelButton { "Cancel" };

    void onOkClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChooseSongsDialog)
};
