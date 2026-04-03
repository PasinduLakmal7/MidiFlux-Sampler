#pragma once

#include <JuceHeader.h>

/**
    A modern sidebar component for browsing sound files and folders.
*/
class SidebarBrowser : public juce::Component,
    public juce::ListBoxModel
{
public:
    SidebarBrowser(std::function<void(juce::File)> onFileSelected)
        : onFileSelected(onFileSelected)
    {
        // Setup Path Label
        addAndMakeVisible(folderPathLabel);
        folderPathLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
        folderPathLabel.setFont(juce::Font(12.0f, juce::Font::italic));

        // Setup List Box
        soundListBox.setRowHeight(35);
        soundListBox.setModel(this);
        soundListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(soundListBox);

        // Setup Browse Button
        addAndMakeVisible(browseFolderButton);
        browseFolderButton.onClick = [this] { browseForFolder(); };

        // Initial scan
        currentSoundFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        scanSoundFolder();
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        browseFolderButton.setBounds(bounds.removeFromTop(35));
        bounds.removeFromTop(5);
        folderPathLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);
        soundListBox.setBounds(bounds);
    }

    // ListBoxModel Overrides
    int getNumRows() override { return soundList.size(); }

    juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsToDrag) override
    {
        if (rowsToDrag.size() > 0)
        {
            int row = rowsToDrag[0];
            juce::String name = soundList[row];
            if (!name.startsWith("[DIR] "))
                return currentSoundFolder.getChildFile(name).getFullPathName();
        }
        return {};
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

        if (rowIsSelected)
        {
            g.setColour(juce::Colour(0xff00d4ff).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
            g.setColour(juce::Colour(0xff00d4ff).withAlpha(0.4f));
            g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);
        }

        g.setColour(juce::Colours::white.withAlpha(rowIsSelected ? 1.0f : 0.7f));
        g.setFont(juce::Font(14.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));

        juce::String name = soundList[rowNumber];
        bool isFolder = name.startsWith("[DIR] ");

        if (isFolder)
        {
            name = name.substring(6);
            g.setColour(juce::Colours::orange.withAlpha(0.8f));
            g.drawText(">> ", 10, 0, 20, height, juce::Justification::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
        }
        else
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.6f));
            g.drawText("-- ", 10, 0, 20, height, juce::Justification::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
        }

        g.drawText(name, 35, 0, width - 45, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        juce::String name = soundList[row];

        if (name.startsWith("[DIR] "))
        {
            juce::String dirName = name.substring(6);
            if (dirName == ".. (Go Up)")
                currentSoundFolder = currentSoundFolder.getParentDirectory();
            else
                currentSoundFolder = currentSoundFolder.getChildFile(dirName);

            scanSoundFolder();
        }
        else if (onFileSelected != nullptr)
        {
            onFileSelected(currentSoundFolder.getChildFile(name));
        }
    }

    void scanSoundFolder()
    {
        soundList.clear();
        folderPathLabel.setText("DIR: " + currentSoundFolder.getFullPathName(), juce::dontSendNotification);

        if (currentSoundFolder.exists() && currentSoundFolder.isDirectory())
        {
            if (currentSoundFolder.getParentDirectory() != currentSoundFolder)
                soundList.add("[DIR] .. (Go Up)");

            auto dirs = currentSoundFolder.findChildFiles(juce::File::findDirectories, false);
            for (auto& d : dirs) soundList.add("[DIR] " + d.getFileName());

            auto files = currentSoundFolder.findChildFiles(juce::File::findFiles, false, "*.wav;*.mp3;*.aiff;*.flac");
            for (auto& f : files) soundList.add(f.getFileName());
        }

        soundListBox.updateContent();
        soundListBox.repaint();
    }

    void browseForFolder()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select Sound Folder", currentSoundFolder, "*");
        auto chooserFlags = juce::FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc)
            {
                if (fc.getResult().exists())
                {
                    currentSoundFolder = fc.getResult();
                    scanSoundFolder();
                }
            });
    }

private:
    juce::TextButton browseFolderButton{ "Browse Sounds" };
    juce::Label folderPathLabel;
    juce::ListBox soundListBox;
    juce::StringArray soundList;
    juce::File currentSoundFolder;
    std::function<void(juce::File)> onFileSelected;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarBrowser)
};
