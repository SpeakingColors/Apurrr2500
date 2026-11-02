#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Presets use the units of the mockup: choices as indices, Threshold in dBu, Make-up in dB, Var in ms, Mix in percent.
// Unknown keys are ignored; an out of range value refuses the preset (same rules as the mockup).
struct Preset
{
    juce::String name, type = "Mix";
    juce::StringArray tags;
    bool factory = false;
    double v[16] { 4, 1, 5, 2, 150, 1, 0, 1, 0, 6, 0, 0, 0, 0, 100, 1 };
};

namespace presets
{
inline constexpr int kCount = 16;
inline const char* const keys[kCount] = { "thr", "ratio", "attack", "release", "relVar", "knee", "tilt", "type", "extSc",
                                          "link", "shape", "muMode", "gain", "blend", "mix", "inSw" };
inline const double lo[kCount] = { -20, 0, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
inline const double hi[kCount] = { 10, 6, 6, 6, 3000, 2, 2, 1, 1, 6, 2, 1, 24, 2, 100, 1 };
inline const char* const types[] = { "Drum", "Bass", "Vocal", "Synth", "Mix", "Master", "FX" };
inline constexpr int kNumTypes = 7;
inline const char* const ratioLabels[7] = { "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "Inf:1" };
inline const char* const attackLabels[7] = { "0.03 ms", "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" };

const std::vector<Preset>& factory();
juce::File userFolder();

class Store
{
public:
    Store();
    void reload();                                    // read the user folder
    std::vector<Preset> all() const;                  // factory first, then yours
    const std::vector<Preset>& user() const { return mine; }
    const Preset* find (const juce::String& name) const;
    bool save (const Preset& p, const juce::String& replaceName = {}); // writes the file, replaces a same-named one
    bool remove (const juce::String& name);
    juce::String exportJson (const std::vector<Preset>& list) const;
    int importJson (const juce::String& text);        // returns count, or -1 when invalid
private:
    std::vector<Preset> mine;
};

bool valid (const juce::var& presetObject);
Preset fromVar (const juce::var& o);
juce::var toVar (const Preset& p);
} // namespace presets
