#pragma once

#include <JuceHeader.h>
#include <memory>
#include <functional>
#include <array>

// new modular components
#include "PadComponent.h"
#include "SidebarBrowser.h"
#include "YouTubeHandler.h"
#include "RackManager.h"

class MainComponent : public juce::AudioAppComponent,
    public juce::MidiInputCallback,
    public juce::DragAndDropContainer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void loadSoundIntoPad(int padIndex, juce::File soundFile);
    void clearSoundFromPad(int padIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Audio Engine components
    juce::AudioFormatManager formatManager;
    juce::Synthesiser sampler;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::AudioFormatReader> currentAudioReader;

    juce::TimeSliceThread backgroundThread{ "Audio Buffering Thread" };
    juce::MidiMessageCollector midiCollector;

    // UI Components
    std::unique_ptr<SidebarBrowser> sidebarBrowser;
    std::unique_ptr<RackManager> rackManager;
    std::unique_ptr<YouTubeHandler> youtubeHandler;
    std::array<std::unique_ptr<PadComponent>, 8> padComponents;
    std::array<juce::File, 8> padSoundFiles;

    // Buttons
    juce::TextEditor youtubeLinkBox;
    juce::TextButton playYoutubeButton{ "Play YouTube" };

    bool isLoadingRack = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
