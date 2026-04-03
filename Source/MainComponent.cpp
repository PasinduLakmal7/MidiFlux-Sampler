#include "MainComponent.h"

MainComponent::MainComponent()
{
    // 1. Audio Permissions and Basics
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio) && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels(2, 2);
    }

    // 2. Initialize Sub-Handlers
    youtubeHandler = std::make_unique<YouTubeHandler>(formatManager);
    sidebarBrowser = std::make_unique<SidebarBrowser>([this](juce::File f) { loadSoundIntoPad(0, f); });
    addAndMakeVisible(sidebarBrowser.get());

    // 3. MIDI Setup
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    if (midiInputs.size() > 0)
    {
        deviceManager.addMidiInputCallback(midiInputs[0].identifier, this);
        deviceManager.setMidiInputEnabled(midiInputs[0].identifier, true);
    }

    // 4. Sampler Setup
    formatManager.registerBasicFormats();
#if JUCE_WINDOWS
    formatManager.registerFormat(new juce::WindowsMediaAudioFormat(), false);
#endif

    sampler.clearVoices();
    for (int i = 0; i < 5; ++i)
        sampler.addVoice(new juce::SamplerVoice());

    // 5. Pad Setup
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

    // Load default sound
    juce::File defaultDrum("C:\\Windows\\Media\\chimes.wav");
    if (defaultDrum.existsAsFile()) loadSoundIntoPad(0, defaultDrum);

    // 6. UI Main Controls
    addAndMakeVisible(testButton);
    testButton.onClick = [this] {
        auto msg = juce::MidiMessage::noteOn(1, 36, (juce::uint8)120);
        msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        handleIncomingMidiMessage(nullptr, msg);
        };

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

    // 7. Theme
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
    g.fillAll(juce::Colour(0xff0b0b14));

    juce::ColourGradient backgroundGlow(juce::Colour(0xff161625), w * 0.7f, h * 0.5f,
        juce::Colour(0xff0b0b14), w, h, true);
    g.setGradientFill(backgroundGlow);
    g.fillAll();

    int sidebarWidth = 380;
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRect(0, 0, sidebarWidth + 10, (int)h);

    juce::ColourGradient lineGrad(juce::Colour(0x0000d4ff), (float)sidebarWidth + 10.0f, 20.0f,
        juce::Colour(0xff00d4ff), (float)sidebarWidth + 10.0f, h / 2.0f, false);
    lineGrad.addColour(1.0, juce::Colour(0x0000d4ff).withAlpha(0.0f));
    g.setGradientFill(lineGrad);
    g.drawLine((float)sidebarWidth + 10.0f, 20.0f, (float)sidebarWidth + 10.0f, h - 20.0f, 1.5f);
}

void MainComponent::resized()
{
    int sidebarWidth = 380;
    sidebarBrowser->setBounds(0, 0, sidebarWidth + 10, getHeight());

    int mainX = sidebarWidth + 20;
    int mainWidth = getWidth() - sidebarWidth - 30;

    testButton.setBounds(mainX, 10, 150, 35);
    youtubeLinkBox.setBounds(mainX, 55, mainWidth - 160, 35);
    playYoutubeButton.setBounds(mainX + mainWidth - 150, 55, 140, 35);

    int padWidth = (mainWidth - 30) / 4;
    int padHeight = juce::jmin(70, (getHeight() - 150) / 2); // basic responsive height
    int startX = mainX + 10;
    int startY = 110;
    int padSpacing = 5;

    for (int i = 0; i < 8; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        padComponents[i]->setBounds(startX + col * (padWidth + padSpacing),
            startY + row * (padHeight + padSpacing),
            padWidth - 5, padHeight - 5);
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
    juce::AudioFormatReader* reader = formatManager.createReaderFor(soundFile);
    if (reader == nullptr) return;

    int midiNote = 36 + padIndex;
    juce::BigInteger noteRange;
    noteRange.setRange(midiNote, 1, true);

    sampler.addSound(new juce::SamplerSound("Pad" + juce::String(padIndex), *reader, noteRange, midiNote, 0.1, 0.1, 10.0));
    padSoundFiles[padIndex] = soundFile;
    padComponents[padIndex]->setFileName(soundFile.getFileNameWithoutExtension());
}