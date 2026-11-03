#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

class AddSongDialog : public juce::Component
{
public:
    std::function<void(Song)> onCreated;

    AddSongDialog();
    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth  = 420;
    static constexpr int kHeight = 400;

private:
    juce::TextButton chooseFileButton { "Choose Audio File" };
    juce::Label      fileNameLabel;

    juce::Label      titleLabel  { {}, "Title *" };
    juce::TextEditor titleEditor;
    juce::Label      artistLabel { {}, "Artist" };
    juce::TextEditor artistEditor;
    juce::Label      albumLabel  { {}, "Album" };
    juce::TextEditor albumEditor;

    juce::TextButton addButton    { "Add Song" };
    juce::TextButton cancelButton { "Cancel" };

    juce::File                         selectedFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void onChooseFile();
    void onAddClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddSongDialog)
};
