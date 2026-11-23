#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song.h"

class AddSongDialog : public juce::Component
{
public:
    std::function<void(Song)> onCreated;

    // Reads the title, artist, album, duration and tagged bpm from the chosen audio file.
    std::function<Song(const juce::File&)> readSongDetails;

    AddSongDialog();
    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth  = 420;
    static constexpr int kHeight = 614;

private:
    juce::TextButton chooseFileButton { "Choose Audio File" };
    juce::Label      fileNameLabel;

    juce::Label      titleLabel  { {}, "Title *" };
    juce::TextEditor titleEditor;
    juce::Label      artistLabel { {}, "Artist" };
    juce::TextEditor artistEditor;
    juce::Label      albumLabel  { {}, "Album" };
    juce::TextEditor albumEditor;
    juce::Label      genreLabel  { {}, "Genre" };
    juce::TextEditor genreEditor;
    juce::Label      bpmLabel    { {}, "BPM" };
    juce::TextEditor bpmEditor;

    juce::TextButton imageButton { "Choose Image" };
    juce::Label      imageFilename;

    juce::TextButton addButton    { "Add Song" };
    juce::TextButton cancelButton { "Cancel" };

    juce::File                         selectedFile;
    int                                selectedDuration = 0;
    juce::Image                        selectedImage;
    juce::File                         selectedImageFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // separator lines worked out in resized so paint matches the layout
    int separator1Y = 0, separator2Y = 0, imageHeaderY = 0;

    void onChooseFile();
    void onChooseImage();
    void onFileChosen(const juce::File& file);
    void onAddClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddSongDialog)
};
