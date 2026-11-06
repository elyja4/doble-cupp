#include "PluginEditor.h"

DoblecuppEditor::DoblecuppEditor(DoblecuppProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    for (int i = 0; i < DoblecuppProcessor::numPresets(); ++i) presets.addItem(DoblecuppProcessor::presetName(i), i + 1);
    presets.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
    presets.onChange = [this] { const int i = presets.getSelectedItemIndex(); if (i >= 0) proc.setCurrentProgram(i); };

    for (juce::Component* c : std::initializer_list<juce::Component*>{ &header, &scene, &presets, &kSpill, &kAmount, &kSpeed, &kFill, &kMix, &bar })
        addAndMakeVisible(c);
    setSize(680, 780);
    startTimerHz(5);   // mantiene el selector de presets sincronizado si el host cambia de programa
}

DoblecuppEditor::~DoblecuppEditor() { stopTimer(); setLookAndFeel(nullptr); }

void DoblecuppEditor::timerCallback()
{
    const int cur = proc.getCurrentProgram();
    if (presets.getSelectedItemIndex() != cur && !presets.isPopupActive())
        presets.setSelectedItemIndex(cur, juce::dontSendNotification);
}

void DoblecuppEditor::paint(juce::Graphics& g)
{
    using namespace juce;
    const float y0 = 548.f, W = (float) getWidth(), H = (float) getHeight();
    g.setGradientFill(ColourGradient(Colour(0xff1c0d36), 0.f, y0, Pal::bg, 0.f, H, false));
    g.fillAll();
    g.setGradientFill(ColourGradient(Pal::silver, 0.f, y0, Pal::silverDark, 0.f, y0 + 6.f, false));
    g.fillRect(0.f, y0, W, 6.f);
    g.setColour(Colours::black); g.drawHorizontalLine((int) y0 + 6, 0.f, W);

    // tag de graffiti de fondo
    { Graphics::ScopedSaveState ss(g); g.addTransform(AffineTransform::rotation(-0.12f, 340.f, 680.f));
      g.setFont(F(120.f, Font::bold | Font::italic)); g.setColour(Pal::purple.withAlpha(0.07f));
      g.drawText("2FAR", 90, 600, 500, 140, Justification::centred); }

    for (auto p : { Point<float>(14.f, y0 + 20.f), Point<float>(W - 14.f, y0 + 20.f), Point<float>(14.f, H - 14.f), Point<float>(W - 14.f, H - 14.f) })
    { g.setColour(Colours::black); g.fillEllipse(p.x - 5, p.y - 5, 10, 10); g.setColour(Pal::silver); g.fillEllipse(p.x - 4, p.y - 4, 8, 8); }

    g.setColour(Pal::silver); g.setFont(F(11.f, Font::bold));
    g.drawText("INTENSITY", 40, 750, 96, 18, Justification::centredRight);
}

void DoblecuppEditor::resized()
{
    header.setBounds(0, 0, 680, 72);
    presets.setBounds(16, 46, 190, 22);
    scene.setBounds(0, 72, 680, 476);
    kSpill.setBounds(240, 556, 200, 190);
    kAmount.setBounds(60, 572, 100, 128);  kSpeed.setBounds(160, 572, 100, 128);
    kFill.setBounds(420, 572, 100, 128);   kMix.setBounds(520, 572, 100, 128);
    bar.setBounds(144, 750, 496, 18);
}
