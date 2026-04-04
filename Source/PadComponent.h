#pragma once

#include <JuceHeader.h>
#include <functional>


class PadComponent : public juce::Component,
    public juce::FileDragAndDropTarget,
    public juce::DragAndDropTarget
{
public:
    PadComponent(int padIndex,
        std::function<void(int, juce::File)> onFileDropped,
        std::function<void(int)> onPadClicked)
        : padIndex(padIndex),
        onFileDropped(onFileDropped),
        onPadClicked(onPadClicked),
        fileName(""),
        isHovering(false),
        isMouseDown(false)
    {
        setInterceptsMouseClicks(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4);

        // Glow Shadow
        if (isHovering || isMouseDown)
        {
            auto glowColour = isMouseDown ? juce::Colour(0xff00d4ff) : juce::Colour(0xff00d4ff).withAlpha(0.2f);
            g.setColour(glowColour);
            g.fillRoundedRectangle(bounds.translated(0, isMouseDown ? 0 : 2), 12);
        }

        // Background
        juce::Colour topCol = isMouseDown ? juce::Colour(0xff2a2a3e) : juce::Colour(0xff1e1e2e);
        juce::Colour botCol = isMouseDown ? juce::Colour(0xff1a1a25) : juce::Colour(0xff12121d);

        juce::ColourGradient grad(topCol, bounds.getX(), bounds.getY(),
            botCol, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 12);

        // Inner Glow and Highlight
        g.setColour(juce::Colours::white.withAlpha(isMouseDown ? 0.1f : 0.05f));
        g.fillRoundedRectangle(bounds.reduced(1), 12);

        // Premium Border
        g.setColour(isHovering || isMouseDown ? juce::Colour(0xff00d4ff) : juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, 12, (isHovering || isMouseDown) ? 2.0f : 1.0f);

        // Pad Text
        g.setColour(isHovering || isMouseDown ? juce::Colours::white : juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        juce::String displayText = fileName.isEmpty()
            ? ("PAD " + juce::String(padIndex + 1))
            : fileName.substring(0, 12).toUpperCase();

        // Offset text slightly when pressed for "3D" feel
        auto textBounds = bounds.reduced(5);
        if (isMouseDown) textBounds.translate(0, 1);

        g.drawText(displayText, textBounds, juce::Justification::centred, true);
    }

    void mouseEnter(const juce::MouseEvent&) override { isHovering = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { isHovering = false; repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        isMouseDown = true;
        onPadClicked(padIndex);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isMouseDown = false;
        repaint();
    }

    // External Drag and Drop
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        if (files.size() > 0)
        {
            juce::File f(files[0]);
            fileName = f.getFileNameWithoutExtension();
            onFileDropped(padIndex, f);
            repaint();
        }
    }

    // Internal Drag and Drop(from Sidebar)
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails&) override { return true; }
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override
    {
        juce::String filePath = details.description.toString();
        if (filePath.isNotEmpty())
        {
            juce::File f(filePath);
            if (f.existsAsFile())
            {
                fileName = f.getFileNameWithoutExtension();
                onFileDropped(padIndex, f);
                repaint();
            }
        }
    }

    void setFileName(const juce::String& name)
    {
        fileName = name;
        repaint();
    }

private:
    int padIndex;
    juce::String fileName;
    bool isHovering;
    bool isMouseDown;
    std::function<void(int, juce::File)> onFileDropped;
    std::function<void(int)> onPadClicked;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PadComponent)
};
