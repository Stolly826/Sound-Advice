#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Playlist.h"

// also used to edit a playlist pass the playlist to fill the fields in
class NewPlaylistDialog : public juce::Component
{
public:
    std::function<void(Playlist)> onCreated; // edit mode gets the changed fields only no songs

    // existingGroups fills the group dropdown
    explicit NewPlaylistDialog(const juce::StringArray& existingGroups = {},
                               const Playlist* toEdit = nullptr);
    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth  = 420;
    static constexpr int kHeight = 444;

private:
    juce::Label      nameLabel  { {}, "Name *" };
    juce::TextEditor nameEditor;

    juce::Label      groupLabel { {}, "Group" };
    juce::ComboBox   groupBox;   // pick a group or type a new one

    juce::TextButton imageButton { "Choose Image" };
    juce::Label      imageFilename;

    juce::Label      descLabel  { {}, "Description" };
    juce::TextEditor descEditor;

    juce::TextButton createButton { "Create" };
    juce::TextButton cancelButton { "Cancel" };

    bool                               editing = false;
    juce::Image                        selectedImage;
    juce::File                         selectedImageFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void onImageClicked();
    void onCreateClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewPlaylistDialog)
};
