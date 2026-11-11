#pragma once
// Offline licensing. A key file is a short text block holding the buyer's details and an RSA signature
// over their SHA-256 hash, made with Calabi Yau's private key. The plugin only carries the public key,
// so a key file can be read and checked without any network and cannot be forged by editing it.
// Without a valid key file the plugin runs in demo mode: two oscillators at most, and no preset load or save.
#include <juce_cryptography/juce_cryptography.h>
#include <juce_data_structures/juce_data_structures.h>

namespace wc
{
struct License
{
    enum class Status { Demo, Licensed };

    Status status = Status::Demo;
    juce::String name, email, order, issued;

    bool licensed() const noexcept { return status == Status::Licensed; }
    juce::String summary() const;

    // Reads the first valid key file found in the usual places (and the extra path, when given).
    static License findAndValidate (const juce::File& extra = {});
    // Checks one key file's text. Returns a Demo license when anything at all does not add up.
    static License validate (const juce::String& blob);
    // Where the plugin looks for "WestCoastModular.key"
    static juce::Array<juce::File> searchPaths();
    static juce::String keyFileName() { return "WestCoastModular.key"; }

    // Signs a payload with a private key: used only by the key generator, never by the plugin.
    static juce::String sign (const juce::String& payload, const juce::String& privateKey);
    static juce::String makeKeyFile (const juce::String& name, const juce::String& email, const juce::String& order, const juce::String& privateKey);

    static constexpr const char* kProduct = "Calabi Yau West Coast Modular";
    static constexpr int kDemoMaxOscillators = 2;
};
} // namespace wc
