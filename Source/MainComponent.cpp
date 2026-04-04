#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Audio Permissions and Basics
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio) && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels(2, 2);
    }

    // Initialize Sub-Handlers
    youtubeHandler = std::make_unique<YouTubeHandler>(formatManager);
    sidebarBrowser = std::make_unique<SidebarBrowser>(nullptr);
    addAndMakeVisible(sidebarBrowser.get());

    rackManager = std::make_unique<RackManager>([this](const RackPreset& r) {
        isLoadingRack = true;
        for (int i = 0; i < 8; ++i)
        {
            if (r.padPaths[i].isNotEmpty())
                loadSoundIntoPad(i, juce::File(r.padPaths[i]));
            else
                clearSoundFromPad(i);
        }
        isLoadingRack = false;
        });

    rackManager->onRequestCurrentSounds = [this](int row) {
        rackManager->updateRackPads(row, padSoundFiles);
        };

    addAndMakeVisible(rackManager.get());

    auto initialRack = RackPreset{ "DEFAULT RACK" }; 


    // MIDI Setup
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    if (midiInputs.size() > 0)
    {
        deviceManager.addMidiInputCallback(midiInputs[0].identifier, this);
        deviceManager.setMidiInputEnabled(midiInputs[0].identifier, true);
    }

    // Sampler Setup
    formatManager.registerBasicFormats();
#if JUCE_WINDOWS
    formatManager.registerFormat(new juce::WindowsMediaAudioFormat(), false);
#endif

    sampler.clearVoices();
    for (int i = 0; i < 5; ++i)
        sampler.addVoice(new juce::SamplerVoice());

    // Pad Setup
    for (int i = 0; i < 8; ++i)
    {
        padComponents[i] = std::make_unique<PadComponent>(i,
            [this](int padIndex, juce::File f) { loadSoundIntoPad(padIndex, f); },
            [this](int padIndex) {
                // Trigger sampler note (MIDI pads 0-7 -> notes 36-43)
                sampler.noteOn(1, 36 + padIndex, 1.0f);

                // Immediately send note off to simulate a trigger
                juce::Timer::callAfterDelay(100, [this, padIndex] {
                    sampler.noteOff(1, 36 + padIndex, 0.0f, true);
                    });
            });
        addAndMakeVisible(padComponents[i].get());
    }



    // UI Main Controls
    addAndMakeVisible(youtubeLinkBox);
    youtubeLinkBox.setTextToShowWhenEmpty("Paste YouTube Link Here", juce::Colours::grey);

    addAndMakeVisible(playYoutubeButton);
    playYoutubeButton.onClick = [this] {
        youtubeHandler->startStream(youtubeLinkBox.getText(), [this](std::unique_ptr<juce::AudioFormatReader> reader) {
            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.get(), false);
            currentAudioReader = std::move(reader);
            transportSource.setSource(readerSource.get());
            transportSource.start();
            });
        };

    backgroundThread.startThread();

    // Theme
    getLookAndFeel().setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a2e));
    getLookAndFeel().setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00d4ff));
    getLookAndFeel().setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f0f1a));
    getLookAndFeel().setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

    setSize(1200, 650);
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    midiCollector.reset(sampleRate);
    sampler.setCurrentPlaybackSampleRate(sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);
    sampler.renderNextBlock(*bufferToFill.buffer, incomingMidi, 0, bufferToFill.numSamples);
    bufferToFill.buffer->applyGain(10.0f);

    if (readerSource != nullptr && transportSource.isPlaying())
    {
        juce::AudioBuffer<float> tempBuffer(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        juce::AudioSourceChannelInfo tempInfo(&tempBuffer, 0, bufferToFill.numSamples);
        transportSource.getNextAudioBlock(tempInfo);
        for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
            bufferToFill.buffer->addFrom(channel, 0, tempBuffer, channel, 0, bufferToFill.numSamples);
    }
}

void MainComponent::releaseResources() {}

void MainComponent::paint(juce::Graphics& g)
{
    auto w = (float)getWidth();
    auto h = (float)getHeight();
    int browserWidth = 300;
    int rackWidth = 260;

    // Background Gradient Overlay
    juce::ColourGradient backgroundGlow(juce::Colour(0xff0b0b14), w * 0.5f, h * 0.5f,
        juce::Colour(0xff0a0a14), w, h, true);
    backgroundGlow.addColour(0.4, juce::Colour(0xff121220));
    g.setGradientFill(backgroundGlow);
    g.fillAll();

    // Sidebar Panels (Glass Effect)
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRect(0, 0, browserWidth + rackWidth + 20, (int)h);

    // Glowing Divider Lines
    auto lineX1 = (float)browserWidth + 5.0f;
    auto lineX2 = (float)browserWidth + rackWidth + 15.0f;

    for (float x : { lineX1, lineX2 })
    {
        juce::ColourGradient lineGrad(juce::Colour(0x0000d4ff), x, 20.0f,
            juce::Colour(0xff00d4ff), x, h / 2.0f, false);
        lineGrad.addColour(1.0, juce::Colour(0x0000d4ff).withAlpha(0.0f));
        g.setGradientFill(lineGrad);
        g.drawLine(x, 20.0f, x, h - 20.0f, 1.2f);
    }
}

void MainComponent::resized()
{
    int browserWidth = 300;
    int rackWidth = 260;

    sidebarBrowser->setBounds(0, 0, browserWidth, getHeight());
    rackManager->setBounds(browserWidth + 10, 0, rackWidth, getHeight());

    int mainX = browserWidth + rackWidth + 40;
    int mainWidth = getWidth() - mainX - 20;

    youtubeLinkBox.setBounds(mainX, 20, mainWidth - 140, 38);
    playYoutubeButton.setBounds(mainX + mainWidth - 130, 20, 120, 38);

    int padAreaWidth = mainWidth - 20;
    int padWidth = (padAreaWidth - 20) / 4;
    int padHeight = juce::jmin(110, (getHeight() - 150) / 2);
    int startX = mainX + 10;
    int startY = 100;
    int padSpacing = 8;

    for (int i = 0; i < 8; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        padComponents[i]->setBounds(startX + col * (padWidth + padSpacing),
            startY + row * (padHeight + padSpacing),
            padWidth, padHeight);
    }
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (message.isNoteOn()) juce::Logger::writeToLog("Pad Hit! Note: " + juce::String(message.getNoteNumber()));
    midiCollector.addMessageToQueue(message);
}

void MainComponent::loadSoundIntoPad(int padIndex, juce::File soundFile)
{
    if (!soundFile.existsAsFile()) return;
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(soundFile));
    if (reader == nullptr) return;

    int midiNote = 36 + padIndex;
    juce::BigInteger noteRange;
    noteRange.setRange(midiNote, 1, true);

    // Remove previous sound for this note if it exists
    for (int i = sampler.getNumSounds(); --i >= 0;)
    {
        if (auto* sound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(i).get()))
        {
            if (sound->appliesToNote(midiNote))
                sampler.removeSound(i);
        }
    }

    sampler.addSound(new juce::SamplerSound("Pad" + juce::String(padIndex), *reader, noteRange, midiNote, 0.1, 0.1, 10.0));
    padSoundFiles[padIndex] = soundFile;
    padComponents[padIndex]->setFileName(soundFile.getFileNameWithoutExtension());

    // Auto-save back to current rack
    if (!isLoadingRack && rackManager != nullptr)
    {
        int row = rackManager->getCurrentSelectedIndex();
        if (row >= 0) rackManager->updateRackPads(row, padSoundFiles);
    }
}

void MainComponent::clearSoundFromPad(int padIndex)
{
    int midiNote = 36 + padIndex;

    for (int i = sampler.getNumSounds(); --i >= 0;)
    {
        if (auto* sound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(i).get()))
        {
            if (sound->appliesToNote(midiNote))
                sampler.removeSound(i);
        }
    }

    padSoundFiles[padIndex] = juce::File();
    if (padComponents[padIndex] != nullptr)
        padComponents[padIndex]->clear();

    // Auto-save update
    if (!isLoadingRack && rackManager != nullptr)
    {
        int row = rackManager->getCurrentSelectedIndex();
        if (row >= 0) rackManager->updateRackPads(row, padSoundFiles);
    }
}