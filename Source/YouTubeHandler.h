#pragma once

#include <JuceHeader.h>

/**
    A helper class to handle YouTube downloading and streaming via yt-dlp and ffmpeg.
*/
class YouTubeHandler
{
public:
    YouTubeHandler(juce::AudioFormatManager& formatManager)
        : formatManager(formatManager) {}

    void startStream(const juce::String& url, std::function<void(std::unique_ptr<juce::AudioFormatReader>)> onComplete)
    {
        juce::Thread::launch([this, url, onComplete = std::move(onComplete)]()
            {
                juce::Logger::writeToLog("--- Starting High-Compatibility YouTube Stream ---");

                auto appFolder = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
                auto ytdlpFile = appFolder.getChildFile("yt-dlp.exe");

                if (!ytdlpFile.existsAsFile())
                {
                    juce::Logger::writeToLog("ERROR: yt-dlp.exe not found at: " + ytdlpFile.getFullPathName());
                    return;
                }

                juce::File ffmpegFile, ffprobeFile;
                auto candidateDirs = { appFolder, appFolder.getChildFile("output"),
                                       appFolder.getParentDirectory().getChildFile("output"),
                                       juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory) };

                for (auto& dir : candidateDirs)
                {
                    if (!ffmpegFile.existsAsFile())
                    {
                        juce::File candidate = dir.getChildFile("ffmpeg.exe");
                        if (candidate.existsAsFile()) ffmpegFile = candidate;
                    }
                    if (!ffprobeFile.existsAsFile())
                    {
                        juce::File candidate = dir.getChildFile("ffprobe.exe");
                        if (candidate.existsAsFile()) ffprobeFile = candidate;
                    }
                }

                if (!ffmpegFile.existsAsFile()) { juce::Logger::writeToLog("ERROR: ffmpeg.exe not found."); return; }

                auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("midi_player_temp_audio.wav");
                tempFile.deleteFile();

                juce::String command = "\"" + ytdlpFile.getFullPathName() + "\" "
                    + "--ffmpeg-location \"" + ffmpegFile.getFullPathName() + "\" "
                    + "--no-playlist --no-warnings --no-progress -f bestaudio "
                    + "--extract-audio --audio-format wav -o \"" + tempFile.getFullPathName() + "\" \"" + url + "\"";

                juce::ChildProcess cp;
                if (cp.start(command))
                {
                    while (cp.isRunning()) juce::Thread::sleep(50);
                    
                    if (tempFile.existsAsFile() && tempFile.getSize() > 44)
                    {
                        auto reader = formatManager.createReaderFor(tempFile);
                        if (reader != nullptr)
                        {
                            juce::MessageManager::callAsync([reader, onComplete]() {
                                onComplete(std::unique_ptr<juce::AudioFormatReader>(reader));
                            });
                        }
                    }
                }
            });
    }

private:
    juce::AudioFormatManager& formatManager;
};
