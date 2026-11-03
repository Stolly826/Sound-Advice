#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Popup that lets the user name and save (or rename and update) a transition.
class SaveTransitionDialog : public juce::Component
{
public:
    std::function<void(juce::String)> onSave;

    explicit SaveTransitionDialog(const juce::String& existingName = {});
    void paint  (juce::Graphics& g) override;
    void resized()                  override;

    static constexpr int kWidth  = 340;
    static constexpr int kHeight = 150;

private:
    juce::TextEditor nameEditor;
    juce::TextButton saveButton   { "Save" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveTransitionDialog)
};
