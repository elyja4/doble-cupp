#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <vector>
#include "Parameters.h"

class DoblecuppProcessor : public juce::AudioProcessor
{
public:
    DoblecuppProcessor();
    ~DoblecuppProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "2far doblecupp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // presets de fábrica (programas del host + selector en la interfaz)
    static int numPresets();
    static juce::String presetName(int i);
    int getNumPrograms() override { return numPresets(); }
    int getCurrentProgram() override { return (int) apvts.state.getProperty("program", 0); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override { return presetName(index); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // valores para la GUI (escritos por el hilo de audio)
    std::atomic<float> visSpill { 0.f };      // 0..1 posición suavizada (inclinación del vaso)
    std::atomic<float> visSpilled { 0.f };    // 0..1 fracción de líquido derramado (spill * amount)
    std::atomic<float> visIntensity { 0.f };  // 0..1 fuerza real aplicada al audio (derramado * mix)
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };

private:
    juce::dsp::StateVariableTPTFilter<float> svf;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm;
    std::array<std::vector<float>, 2> delay;
    int delaySize = 1, writePos = 0, centreDelay = 1;
    float lfoPhase = 0.f, spillSmooth = 0.f;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoblecuppProcessor)
};
