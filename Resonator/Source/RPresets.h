#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace cyr
{
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // parameter id -> plain value (on top of Init)
};
const std::vector<FactoryPreset>& factoryPresets();
} // namespace cyr
