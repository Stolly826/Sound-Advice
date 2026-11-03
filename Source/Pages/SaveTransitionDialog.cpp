#include "SaveTransitionDialog.h"

static constexpr int kPad = 20;

SaveTransitionDialog::SaveTransitionDialog(const juce::String& existingName)
{
    nameEditor.setText(existingName);
    nameEditor.setTextToShowWhenEmpty("Transition name...", juce::Colours::grey);
    addAndMakeVisible(nameEditor);

    saveButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();
        if (name.isEmpty()) return;
        if (onSave) onSave(name);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(saveButton);

    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(cancelButton);

    setSize(kWidth, kHeight);
}

void SaveTransitionDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("Name this transition", kPad, kPad, getWidth() - kPad * 2, 32,
               juce::Justification::centredLeft);
}

void SaveTransitionDialog::resized()
{
    auto area = getLocalBounds().reduced(kPad);
    area.removeFromTop(40);

    nameEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    auto btnRow = area.removeFromTop(32);
    cancelButton.setBounds(btnRow.removeFromRight(80));
    btnRow.removeFromRight(8);
    saveButton.setBounds(btnRow.removeFromRight(80));
}
