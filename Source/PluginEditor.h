#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI.h"
#include "SceneView.h"

class DoblecuppEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DoblecuppEditor(DoblecuppProcessor&);
    ~DoblecuppEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DoblecuppProcessor& proc;
    DoblecuppLnF lnf;
    HeaderView header { proc };
    SceneView scene { proc };
    juce::ComboBox presets;
    Knob kSpill { proc.apvts, Param::spill, "SPILL", true };
    Knob kAmount { proc.apvts, Param::amount, "AMOUNT" }, kSpeed { proc.apvts, Param::speed, "SPEED" },
         kFill { proc.apvts, Param::fill, "CUP FILL" }, kMix { proc.apvts, Param::mix, "MIX" };
    IntensityBar bar { proc.visIntensity };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoblecuppEditor)
};
