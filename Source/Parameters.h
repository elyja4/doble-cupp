#pragma once
#include <JuceHeader.h>

namespace Param
{
    inline constexpr const char* spill  = "spill";   // posición del control principal (0..100)
    inline constexpr const char* amount = "amount";  // cuánto del líquido puede salir (0..100 %)
    inline constexpr const char* speed  = "speed";   // velocidad de respuesta/animación (0.25..4x)
    inline constexpr const char* fill   = "fill";    // líquido inicial en el vaso (1..16 oz)
    inline constexpr const char* mix    = "mix";     // dry/wet del efecto (0..100 %)
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto F = [&](const char* id, const char* name, float lo, float hi, float def, float skew,
                 std::function<String(float, int)> toText)
    {
        p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ id, 1 }, name,
            NormalisableRange<float>(lo, hi, 0.001f, skew), def,
            AudioParameterFloatAttributes().withStringFromValueFunction(std::move(toText))));
    };

    F(Param::spill,  "Spill",  0.f, 100.f, 0.f,  1.f, [](float v, int) { return String(roundToInt(v)); });
    F(Param::amount, "Amount", 0.f, 100.f, 100.f, 1.f, [](float v, int) { return String(roundToInt(v)) + "%"; });
    F(Param::speed,  "Speed",  0.25f, 4.f, 1.f,  0.5f, [](float v, int) { return String(v, 2) + "x"; });
    F(Param::fill,   "Fill",   1.f, 16.f, 12.f,  1.f, [](float v, int) { return String(v, 1) + " oz"; });
    F(Param::mix,    "Mix",    0.f, 100.f, 100.f, 1.f, [](float v, int) { return String(roundToInt(v)) + "%"; });

    return { p.begin(), p.end() };
}
