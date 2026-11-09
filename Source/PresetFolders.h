#pragma once
#include <juce_data_structures/juce_data_structures.h>

// Where an instrument opens and saves presets. Shared by every instance of that product on this computer,
// stored in the user's application settings (Application Support / AppData) under "Calabi Yau".
namespace pg
{
class PresetFolders
{
public:
    PresetFolders (const juce::String& productName, const juce::File& defaultFolder) : def (defaultFolder)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = productName;
        o.folderName = "Calabi Yau";
        o.filenameSuffix = "settings";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
    }

    juce::File defaultFolder() const { def.createDirectory(); return def; }
    juce::File openFolder() const { return get ("openFolder"); }
    juce::File saveFolder() const { return get ("saveFolder"); }
    void setOpenFolder (const juce::File& f) { set ("openFolder", f); }
    void setSaveFolder (const juce::File& f) { set ("saveFolder", f); }
    void useDefaults() { props->removeValue ("openFolder"); props->removeValue ("saveFolder"); props->saveIfNeeded(); }
    bool isDefault (const juce::File& f) const { return f == def; }

private:
    juce::File get (const char* key) const
    {
        const juce::File f (props->getValue (key, def.getFullPathName()));
        if (f.isDirectory()) return f;
        def.createDirectory();
        return def;   // a folder that vanished (unplugged drive...) falls back to the default
    }
    void set (const char* key, const juce::File& f)
    {
        if (! f.isDirectory()) return;
        props->setValue (key, f.getFullPathName());
        props->saveIfNeeded();
    }
    juce::File def;
    std::unique_ptr<juce::PropertiesFile> props;
};
} // namespace pg
