#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"

class NewPlaylistDialog : public juce::Component
{
public:
    std::function<void(Playlist)> onCreated;

    NewPlaylistDialog();
    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth  = 420;
    static constexpr int kHeight = 390;

private:
    juce::Label      nameLabel  { {}, "Name *" };
    juce::TextEditor nameEditor;

    juce::TextButton imageButton { "Choose Image" };
    juce::Label      imageFilename;

    juce::Label      descLabel  { {}, "Description" };
    juce::TextEditor descEditor;

    juce::TextButton createButton { "Create" };
    juce::TextButton cancelButton { "Cancel" };

    juce::Image                        selectedImage;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void onImageClicked();
    void onCreateClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewPlaylistDialog)
};
