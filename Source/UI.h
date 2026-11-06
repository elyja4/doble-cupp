#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace Pal
{
    inline const juce::Colour bg(0xff07040d), purple(0xff7b2fff), deep(0xff3b1b6e), light(0xffb58cff),
        silver(0xffc8ccd4), silverDark(0xff7d8190), white(0xffffffff), black(0xff000000);
}
inline juce::Font F(float h, int style = juce::Font::plain) { return juce::Font(juce::FontOptions(h, style)); }
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
struct DoblecuppLnF : public juce::LookAndFeel_V4
{
    DoblecuppLnF()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff150b26));
        setColour(juce::ComboBox::textColourId, Pal::white);
        setColour(juce::ComboBox::outlineColourId, Pal::silverDark);
        setColour(juce::ComboBox::arrowColourId, Pal::light);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff150b26));
        setColour(juce::PopupMenu::textColourId, Pal::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Pal::purple);
    }

    juce::Label* createSliderTextBox(juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox(s);
        const bool big = (bool) s.getProperties()["big"];
        l->setFont(F(big ? 24.f : 13.f, juce::Font::bold));
        return l;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        auto b = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f);
        const float r = jmin(b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);

        // anillo exterior plateado
        g.setColour(Colours::black.withAlpha(0.6f)); g.fillEllipse(c.x - r - 1, c.y - r + 2, 2 * r + 2, 2 * r + 2);
        g.setGradientFill(ColourGradient(Pal::silver.brighter(0.4f), c.x - r * 0.5f, c.y - r * 0.7f,
                                         Pal::silverDark.darker(0.4f), c.x + r * 0.6f, c.y + r * 0.9f, true));
        g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour(Colours::black); g.drawEllipse(c.x - r, c.y - r, 2 * r, 2 * r, 2.f);

        // surco + arco de valor
        const float rr = r * 0.82f;
        Path track; track.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, a1, true);
        g.setColour(Colour(0xff10081c)); g.strokePath(track, PathStrokeType(r * 0.12f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, jmax(a0 + 0.001f, ang), true);
        g.setColour(Pal::purple); g.strokePath(val, PathStrokeType(r * 0.12f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(Pal::light.withAlpha(0.7f)); g.strokePath(val, PathStrokeType(r * 0.04f, PathStrokeType::curved, PathStrokeType::rounded));

        // esfera morada con destello
        const float orb = r * 0.64f;
        g.setColour(Colours::black); g.fillEllipse(c.x - orb - 2, c.y - orb - 2, 2 * orb + 4, 2 * orb + 4);
        g.setGradientFill(ColourGradient(Pal::light, c.x - orb * 0.4f, c.y - orb * 0.5f, Colour(0xff2a1058), c.x + orb * 0.7f, c.y + orb * 0.9f, true));
        g.fillEllipse(c.x - orb, c.y - orb, 2 * orb, 2 * orb);
        g.setColour(Colours::white.withAlpha(0.35f)); g.fillEllipse(c.x - orb * 0.6f, c.y - orb * 0.85f, orb * 0.7f, orb * 0.35f);
        Path star; star.addStar(c, 4, orb * 0.07f, orb * 0.55f, ang * 0.6f);
        g.setColour(Colours::white); g.fillPath(star);

        // punto indicador
        const float ix = c.x + std::sin(ang) * rr, iy = c.y - std::cos(ang) * rr;
        g.setColour(Pal::purple.withAlpha(0.5f)); g.fillEllipse(ix - r * 0.14f, iy - r * 0.14f, r * 0.28f, r * 0.28f);
        g.setColour(Colours::white); g.fillEllipse(ix - r * 0.06f, iy - r * 0.06f, r * 0.12f, r * 0.12f);
        ignoreUnused(s);
    }
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob(APVTS& apvts, const char* id, const juce::String& name, bool big = false)
    {
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(F(big ? 17.f : 12.f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, big ? Pal::white : Pal::light);
        s.getProperties().set("big", big);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters(1.2f * juce::MathConstants<float>::pi, 2.8f * juce::MathConstants<float>::pi, true);
        s.setColour(juce::Slider::textBoxTextColourId, Pal::white);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, big ? 100 : 76, big ? 30 : 16);
        addAndMakeVisible(s); addAndMakeVisible(label);
        att = std::make_unique<APVTS::SliderAttachment>(apvts, id, s);
        if (auto* p = apvts.getParameter(id))
            s.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        bigKnob = big;
    }
    void resized() override { auto r = getLocalBounds(); label.setBounds(r.removeFromTop(bigKnob ? 22 : 16)); s.setBounds(r); }
private:
    bool bigKnob = false;
    juce::Slider s; juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

//==============================================================================
class IntensityBar : public juce::Component, private juce::Timer
{
public:
    explicit IntensityBar(std::atomic<float>& src) : source(src) { startTimerHz(30); }
    ~IntensityBar() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(Colour(0xff0a0614)); g.fillRoundedRectangle(r, 4.f);
        g.setColour(Pal::silverDark); g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.f);
        const int segs = 30;
        const float lit = shown * (float) segs, sw = (r.getWidth() - 8.f) / segs;
        for (int i = 0; i < segs; ++i)
        {
            const float f = (float) i / (float) segs;
            const Colour c = f < 0.6f ? Pal::purple : (f < 0.85f ? Pal::light : Pal::white);
            g.setColour((float) i < lit ? c : c.withAlpha(0.13f));
            g.fillRect(r.getX() + 4.f + i * sw, r.getY() + 4.f, sw - 2.f, r.getHeight() - 8.f);
        }
    }
private:
    void timerCallback() override
    {
        const float v = juce::jlimit(0.f, 1.f, source.load());
        shown += (v - shown) * 0.3f; repaint();
    }
    std::atomic<float>& source; float shown = 0.f;
};

//==============================================================================
class HeaderView : public juce::Component, private juce::Timer
{
public:
    explicit HeaderView(DoblecuppProcessor& p) : proc(p) { startTimerHz(20); }
    ~HeaderView() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff1a0c30), 0, 0, Pal::bg, 0, r.getBottom(), false));
        g.fillAll();
        g.setColour(Pal::purple); g.fillRect(0.f, r.getBottom() - 4.f, r.getWidth(), 4.f);
        for (float x = 0; x < r.getWidth(); x += 24.f)   // franja plateada tipo cinta
        { g.setColour(Pal::silver.withAlpha(0.55f)); Path p; p.addQuadrilateral(x, r.getBottom() - 4, x + 10, r.getBottom() - 4, x + 6, r.getBottom(), x - 4, r.getBottom()); g.fillPath(p); }

        // título estilo graffiti
        g.setFont(F(31.f, Font::bold | Font::italic));
        const Rectangle<int> t(16, 2, 300, 42);
        g.setColour(Colours::black); g.drawText("2FAR DOBLECUPP", t.translated(3, 4), Justification::centredLeft);
        g.setColour(Pal::purple);
        for (auto o : { Point<int>(-2, 0), Point<int>(2, 0), Point<int>(0, -2), Point<int>(0, 2), Point<int>(-2, -2), Point<int>(2, 2), Point<int>(-2, 2), Point<int>(2, -2) })
            g.drawText("2FAR DOBLECUPP", t.translated(o.x, o.y), Justification::centredLeft);
        g.setColour(Colours::white); g.drawText("2FAR DOBLECUPP", t, Justification::centredLeft);

        // insignia de formato
        Rectangle<float> badge(216.f, 46.f, 84.f, 20.f);
        g.setGradientFill(ColourGradient(Pal::silver.brighter(0.2f), 0, badge.getY(), Pal::silverDark, 0, badge.getBottom(), false));
        g.fillRoundedRectangle(badge, 4.f);
        g.setColour(Colours::black); g.setFont(F(11.f, Font::bold));
        g.drawText(String::fromUTF8("VST3 \xE2\x80\xA2 AU"), badge, Justification::centred);

        // lecturas
        const float eff = proc.visSpilled.load(), inten = proc.visIntensity.load();
        const float fillOz = proc.apvts.getRawParameterValue(Param::fill)->load();
        drawReadout(g, 330, "IN THE CUP", String(fillOz * (1.f - eff), 1) + " oz");
        drawReadout(g, 440, "SPILLED", String(roundToInt(eff * 100.f)) + "%");
        drawReadout(g, 550, "INTENSITY", String(roundToInt(inten * 100.f)) + "%");
    }
private:
    void drawReadout(juce::Graphics& g, int x, const juce::String& cap, const juce::String& val)
    {
        using namespace juce;
        g.setColour(Pal::silver); g.setFont(F(10.f, Font::bold));
        g.drawText(cap, x, 10, 104, 14, Justification::centredLeft);
        g.setColour(Pal::light); g.setFont(F(26.f, Font::bold));
        g.drawText(val, x, 24, 110, 34, Justification::centredLeft);
    }
    void timerCallback() override { repaint(); }
    DoblecuppProcessor& proc;
};
