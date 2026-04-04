#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

struct RackPreset
{
    juce::String name;
    std::array<juce::String, 8> padPaths;
};

class RackManager : public juce::Component,
                    public juce::ListBoxModel
{
public:
    RackManager(std::function<void(const RackPreset &)> onRackLoad)
        : onRackLoad(onRackLoad)
    {
        // Setup ListBox
        rackList.setRowHeight(45);
        rackList.setModel(this);
        rackList.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        rackList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(rackList);

        // Setup Add Button
        addRackButton.setButtonText("+ ADD NEW RACK");
        addRackButton.onClick = [this]
        { addNewRack(); };
        addAndMakeVisible(addRackButton);

        loadRacksFromDisk();

        // add a default one
        if (racks.empty())
        {
            addNewRack("DEFAULT RACK");
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        addRackButton.setBounds(bounds.removeFromTop(35));
        bounds.removeFromTop(10);
        rackList.setBounds(bounds);
    }

    // ListBoxModel
    int getNumRows() override { return (int)racks.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(6, 4);

        // Background with Glassmorphism
        g.setColour(rowIsSelected ? juce::Colour(0xff00d4ff).withAlpha(0.15f) : juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

        if (rowIsSelected)
        {
            g.setColour(juce::Colour(0xff00d4ff).withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 1.5f);

            g.setColour(juce::Colour(0xff00d4ff).withAlpha(0.8f));
            g.fillRoundedRectangle((float)bounds.getX() + 8.0f, (float)bounds.getY() + bounds.getHeight() * 0.35f, 3.0f, bounds.getHeight() * 0.3f, 1.0f);
        }

        // Rack Name
        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::white.withAlpha(0.75f));
        g.setFont(juce::Font(15.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
        g.drawText(racks[rowNumber].name, 30, 0, width - 85, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showContextMenu(row);
        }
        else
        {
            rackList.selectRow(row);
            if (onRackLoad)
                onRackLoad(racks[row]);
        }
    }

    void addNewRack(juce::String name = "NEW RACK")
    {
        RackPreset r;
        r.name = name;
        for (auto &p : r.padPaths)
            p = "";
        racks.push_back(r);
        saveRacksToDisk();
        rackList.updateContent();
    }

    void updateRackPads(int row, const std::array<juce::File, 8> &files)
    {
        if (row >= 0 && row < racks.size())
        {
            for (int i = 0; i < 8; ++i)
                racks[row].padPaths[i] = files[i].getFullPathName();
            saveRacksToDisk();
        }
    }

    int getCurrentSelectedIndex() const { return rackList.getSelectedRow(); }

private:
    void showContextMenu(int row)
    {
        juce::PopupMenu m;
        m.addItem(1, "Rename");
        m.addItem(2, "Delete");
        m.addItem(3, "Save Current Sounds Here");

        m.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result)
                        {
                if (result == 1) renameRack(row);
                else if (result == 2) deleteRack(row);
                else if (result == 3) {
                    if (onRequestCurrentSounds) onRequestCurrentSounds(row);
                } });
    }

    void renameRack(int row)
    {
        auto *alert = new juce::AlertWindow("Rename Rack", "Enter new name:", juce::AlertWindow::NoIcon);
        alert->addTextEditor("name", racks[row].name);
        alert->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        alert->enterModalState(true, juce::ModalCallbackFunction::create([this, row, alert](int res)
                                                                         {
                if (res == 1)
                {
                    racks[row].name = alert->getTextEditorContents("name");
                    saveRacksToDisk();
                    rackList.updateContent();
                } }),
                               true);
    }

    void deleteRack(int row)
    {
        if (racks.size() <= 1)
            return;

        racks.erase(racks.begin() + row);
        saveRacksToDisk();
        rackList.updateContent();
    }

    void saveRacksToDisk()
    {
        auto file = getSettingsFile();
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        juce::Array<juce::var> rackArray;

        for (auto &r : racks)
        {
            juce::DynamicObject::Ptr rObj = new juce::DynamicObject();
            rObj->setProperty("name", r.name);
            juce::Array<juce::var> pathArray;
            for (auto &p : r.padPaths)
                pathArray.add(p);
            rObj->setProperty("pads", pathArray);
            rackArray.add(rObj.get());
        }

        obj->setProperty("racks", rackArray);

        juce::FileOutputStream fos(file);
        if (fos.openedOk())
        {
            fos.setPosition(0);
            fos.truncate();
            fos.writeText(juce::JSON::toString(obj.get()), false, false, nullptr);
        }
    }

    void loadRacksFromDisk()
    {
        auto file = getSettingsFile();
        if (!file.existsAsFile())
            return;

        auto json = juce::JSON::parse(file.loadFileAsString());
        if (json.isObject())
        {
            racks.clear();
            if (auto *rackArray = json["racks"].getArray())
            {
                for (auto &rVar : *rackArray)
                {
                    RackPreset r;
                    r.name = rVar["name"].toString();
                    if (r.name.isEmpty())
                        r.name = "UNNAMED";

                    if (auto *padArray = rVar["pads"].getArray())
                    {
                        for (int i = 0; i < 8 && i < padArray->size(); ++i)
                            r.padPaths[i] = (*padArray)[i].toString();
                    }
                    racks.push_back(r);
                }
            }
        }
    }

    juce::File getSettingsFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("GlassSampler-MidiMod");
        if (!dir.exists())
            dir.createDirectory();
        return dir.getChildFile("racks.json");
    }

public:
    std::function<void(int)> onRequestCurrentSounds;

private:
    class GlassyButton : public juce::TextButton
    {
    public:
        GlassyButton(const juce::String &name) : juce::TextButton(name) {}
        void paintButton(juce::Graphics &g, bool m, bool d) override
        {
            auto b = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(juce::Colour(0xff1a1a2e).withAlpha(d ? 0.9f : 0.6f));
            g.fillRoundedRectangle(b, 8.0f);
            g.setColour(juce::Colour(0xff00d4ff).withAlpha(m ? 0.8f : 0.4f));
            g.drawRoundedRectangle(b, 8.0f, 1.5f);
            g.setColour(juce::Colours::white.withAlpha(m ? 1.0f : 0.8f));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(getButtonText(), b, juce::Justification::centred);
        }
    };
    juce::ListBox rackList;
    GlassyButton addRackButton{"+ ADD NEW RACK"};
    std::vector<RackPreset> racks;
    std::function<void(const RackPreset &)> onRackLoad;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RackManager)
};
