#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <vector>
#include <array>
#include <algorithm>
#include "PluginProcessor.h"
#include "UI.h"

// Escena animada: vaso de plástico con líquido morado e hielos, calle con ladrillos y
// graffiti, póster con TU imagen. El vaso se inclina (resorte con rebote), el nivel del
// líquido se calcula geométricamente (área real dentro del vaso), sale un chorro, se forma
// un charco que ondula con la señal de audio y salen gotas con los transientes.
class SceneView : public juce::Component, private juce::Timer
{
public:
    explicit SceneView(DoblecuppProcessor& p) : proc(p)
    {
        character = juce::ImageCache::getFromMemory(BinaryData::character_png, BinaryData::character_pngSize);
        startTimerHz(60);
    }
    ~SceneView() override { stopTimer(); }

    void resized() override { rebuildBackground(); }

    void paint(juce::Graphics& g) override
    {
        if (bg.isValid()) g.drawImageAt(bg, 0, 0);
        drawShadow(g); drawPuddle(g); drawCup(g); drawStream(g); drawDrops(g);
    }

private:
    using Pt = juce::Point<float>;
    using Path = juce::Path;
    using Poly = std::vector<Pt>;
    struct Drop { float x, y, vx, vy, life, size; };

    float floorY() const { return (float) getHeight() * 0.80f; }
    Pt pivot() const { return { (float) getWidth() * 0.34f, floorY() }; }
    static constexpr float maxTheta = 1.466f;   // ~84 grados: el vaso queda casi tumbado

    //------------------------------------------------------------ geometría
    static float area(const Poly& p)
    {
        float a = 0.f;
        for (size_t i = 0; i < p.size(); ++i) { const auto& u = p[i]; const auto& v = p[(i + 1) % p.size()]; a += u.x * v.y - v.x * u.y; }
        return std::abs(a) * 0.5f;
    }
    static Poly clipHalf(const Poly& in, float ySurf, float slope, float cx)
    {
        Poly out;
        auto d = [&](const Pt& p) { return (p.y - ySurf) - slope * (p.x - cx); };   // >= 0: por debajo de la superficie
        for (size_t i = 0; i < in.size(); ++i)
        {
            const Pt a = in[i], b = in[(i + 1) % in.size()];
            const float da = d(a), db = d(b);
            if (da >= 0.f) out.push_back(a);
            if ((da >= 0.f) != (db >= 0.f)) out.push_back(a + (b - a) * (da / (da - db)));
        }
        return out;
    }
    Pt toWorld(Pt p) const
    {
        const float c = std::cos(angle), s = std::sin(angle); const auto pv = pivot();
        return { pv.x + p.x * c - p.y * s, pv.y + p.x * s + p.y * c };
    }
    static float sstep(float t) { t = juce::jlimit(0.f, 1.f, t); return t * t * (3.f - 2.f * t); }

    //------------------------------------------------------------ simulación
    void timerCallback() override
    {
        const float dt = 1.f / 60.f; time += dt;
        const float s = proc.visSpill.load(), eff = proc.visSpilled.load(), in = proc.inLevel.load();
        lvl += (in - lvl) * 0.35f;

        angVel += (s * maxTheta - angle) * 0.20f; angVel *= 0.76f; angle += angVel;   // resorte con rebote
        if (angle > maxTheta) { angle = maxTheta; angVel = 0.f; }
        if (angle < -0.05f) { angle = -0.05f; angVel *= -0.3f; }
        const float ds = (s - prevS) / dt; prevS = s;
        sNow = s;
        puddleP = juce::jmax(eff, puddleP - 0.05f * dt);                              // el charco se evapora despacio
        slosh = juce::jlimit(-0.5f, 0.5f, std::sin(time * 8.5f) * (0.03f + lvl * 0.25f) - angVel * 0.9f);

        computeCup(eff, ds);

        // gotas al caer el chorro
        if (streamFlow > 0.05f && rng.nextFloat() < streamFlow)
            spawnDrops(1 + (int) (streamFlow * 2.f), rimPoint.x + 10.f + streamFlow * 14.f, floorY() - 8.f, 3.f, 6.f);
        // gotas saltando del vaso con los transientes del audio
        cooldown -= dt;
        if (in > prevIn * 1.6f + 0.03f && in > 0.06f && cooldown <= 0.f && hasLiquid && angle < 0.8f)
        { spawnDrops(3 + (int) (in * 6.f), liqCentre.x, liqCentre.y, 5.f, 9.f); cooldown = 0.08f; }
        prevIn = in;

        for (auto& d : drops) { d.x += d.vx; d.y += d.vy; d.vy += 0.45f; d.life -= 0.012f; if (d.y > floorY() - 2.f) d.life = 0.f; }
        drops.erase(std::remove_if(drops.begin(), drops.end(), [](const Drop& d) { return d.life <= 0.f; }), drops.end());
        if (drops.size() > 220) drops.erase(drops.begin(), drops.begin() + 40);
        repaint();
    }

    void spawnDrops(int n, float x, float y, float vyMin, float vyMax)
    {
        for (int i = 0; i < n; ++i)
            drops.push_back({ x + (rng.nextFloat() - 0.5f) * 20.f, y, (rng.nextFloat() - 0.4f) * 4.f,
                              -(vyMin + rng.nextFloat() * (vyMax - vyMin)), 1.f, 3.f + rng.nextFloat() * 4.f });
    }

    void computeCup(float eff, float ds)
    {
        static const Pt outerL[4] = { { -72.f, 0.f }, { 0.f, 0.f }, { 12.f, -150.f }, { -84.f, -150.f } };
        static const Pt innerL[4] = { { -67.f, -6.f }, { -5.f, -6.f }, { 8.f, -146.f }, { -80.f, -146.f } };
        static const float aFull = area(Poly(innerL, innerL + 4));

        for (int i = 0; i < 4; ++i) { outer[(size_t) i] = toWorld(outerL[i]); inner[(size_t) i] = toWorld(innerL[i]); }
        const Poly in(inner.begin(), inner.end());

        const float fillOz = proc.apvts.getRawParameterValue(Param::fill)->load();
        const float V = (fillOz / 16.f) * 0.95f * (1.f - eff) * aFull;
        const float ySmin = juce::jmax(in[2].y, in[3].y);
        float ySmax = in[0].y; for (auto& p : in) ySmax = juce::jmax(ySmax, p.y);
        const float cxP = (in[0].x + in[1].x + in[2].x + in[3].x) * 0.25f, slope = std::tan(slosh);

        float overflow = 0.f;
        const float aMax = area(clipHalf(in, ySmin, slope, cxP));
        if (V >= aMax) { ySurf = ySmin; overflow = (V - aMax) / aFull; }
        else
        {
            float lo = ySmin, hi = ySmax;
            for (int i = 0; i < 20; ++i) { const float mid = 0.5f * (lo + hi); if (area(clipHalf(in, mid, slope, cxP)) > V) lo = mid; else hi = mid; }
            ySurf = 0.5f * (lo + hi);
        }
        hasLiquid = V > 0.01f * aFull;
        liquid = hasLiquid ? clipHalf(in, ySurf, slope, cxP) : Poly();
        if (liquid.size() < 3) { liquid.clear(); hasLiquid = false; }
        float mx = 0.f; for (auto& p : liquid) mx += p.x;
        liqCentre = { liquid.empty() ? cxP : mx / (float) liquid.size(), ySurf };

        rimPoint = outer[2].y > outer[3].y ? outer[2] : outer[3];
        const bool tilted = angle > 0.5f;
        streamFlow = (tilted && (hasLiquid || overflow > 0.f)) ? juce::jlimit(0.f, 1.f, overflow * 5.f + juce::jmax(0.f, ds) * 0.7f) : 0.f;
        if (streamFlow < 0.03f) streamFlow = 0.f;
    }

    //------------------------------------------------------------ dibujo dinámico
    static Path polyPath(const Poly& p)
    {
        Path path; if (p.empty()) return path;
        path.startNewSubPath(p[0]); for (size_t i = 1; i < p.size(); ++i) path.lineTo(p[i]); path.closeSubPath(); return path;
    }
    static void halftone(juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, float spacing, float rad)
    {
        g.setColour(c);
        int row = 0;
        for (float y = b.getY(); y < b.getBottom(); y += spacing * 0.5f, ++row)
            for (float x = b.getX() + ((row & 1) ? spacing * 0.5f : 0.f); x < b.getRight(); x += spacing)
                g.fillEllipse(x - rad, y - rad, rad * 2.f, rad * 2.f);
    }

    void drawShadow(juce::Graphics& g)
    {
        const auto pv = pivot();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(pv.x - 100.f, pv.y - 6.f, 140.f + angle * 70.f, 18.f);
    }

    void drawPuddle(juce::Graphics& g)
    {
        using namespace juce;
        const float p = puddleP; if (p < 0.004f) return;
        const auto pv = pivot();
        const float w = 50.f + p * 330.f, h = 14.f + p * 34.f, cx = pv.x - 40.f + w * 0.5f, cy = floorY() - 2.f + h * 0.12f;
        Path path;
        for (int k = 0; k <= 56; ++k)
        {
            const float a = (float) k * MathConstants<float>::twoPi / 56.f;
            const float wob = 1.f + 0.06f * std::sin(3.f * a + time * 1.6f) + 0.04f * std::sin(5.f * a - time * 2.1f) + 0.10f * lvl * std::sin(9.f * a + time * 14.f);
            const float x = cx + std::cos(a) * w * 0.5f * wob, y = cy + std::sin(a) * h * 0.5f * wob;
            if (k == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        path.closeSubPath();
        g.setGradientFill(ColourGradient(Colour(0xff9a5cff), cx, cy - h * 0.5f, Colour(0xff4a1cb0), cx, cy + h * 0.5f, false));
        g.fillPath(path);
        { Graphics::ScopedSaveState ss(g); g.reduceClipRegion(path, AffineTransform());
          halftone(g, path.getBounds(), Colour(0xff2a0f6e).withAlpha(0.55f), 7.f, 1.5f);
          g.setColour(Colours::white.withAlpha(0.22f)); g.fillEllipse(cx - w * 0.38f, cy - h * 0.34f, w * 0.34f, h * 0.2f); }
        g.setColour(Colours::black); g.strokePath(path, PathStrokeType(3.f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void drawCup(juce::Graphics& g)
    {
        using namespace juce;
        Poly o(outer.begin(), outer.end());
        auto outerPath = polyPath(o);
        g.setColour(Colours::white.withAlpha(0.13f)); g.fillPath(outerPath);

        // líquido (área real calculada) con trama de puntos
        if (hasLiquid)
        {
            auto lp = polyPath(liquid); const auto lb = lp.getBounds();
            g.setGradientFill(ColourGradient(Colour(0xff9a5cff), 0, lb.getY(), Colour(0xff4a1cb0), 0, lb.getBottom() + 1.f, false));
            g.fillPath(lp);
            { Graphics::ScopedSaveState ss(g); g.reduceClipRegion(lp, AffineTransform());
              halftone(g, lb, Colour(0xff2a0f6e).withAlpha(0.5f), 6.f, 1.3f); }
            g.setColour(Pal::light.withAlpha(0.8f)); g.strokePath(lp, PathStrokeType(1.5f));
        }

        // hielos: flotan en la superficie y salen al charco cuando el vaso se vuelca
        static const float th[4] = { 0.55f, 0.68f, 0.80f, 0.90f }, dx[4] = { -14.f, 8.f, -2.f, 20.f };
        const auto pv = pivot();
        for (int i = 0; i < 4; ++i)
        {
            Pt inCup = hasLiquid ? Pt(liqCentre.x + dx[i], ySurf - 6.f + std::sin(time * 2.f + (float) i) * 1.5f)
                                 : toWorld({ -36.f + dx[i], -12.f });
            const Pt out(pv.x + 30.f + (float) i * 34.f, floorY() - 9.f + (float) (i % 2) * 5.f);
            const float t = sstep((sNow - th[i]) / 0.12f);
            Pt pos = inCup + (out - inCup) * t; pos.y -= std::sin(t * MathConstants<float>::pi) * 30.f;
            Graphics::ScopedSaveState ss(g);
            g.addTransform(AffineTransform::rotation((float) i * 0.5f + t * 3.f, pos.x, pos.y));
            g.setColour(Colours::white.withAlpha(0.55f)); g.fillRoundedRectangle(pos.x - 8.5f, pos.y - 8.5f, 17.f, 17.f, 3.f);
            g.setColour(Colours::white.withAlpha(0.95f)); g.drawRoundedRectangle(pos.x - 8.5f, pos.y - 8.5f, 17.f, 17.f, 3.f, 1.5f);
            g.setColour(Colours::white.withAlpha(0.7f)); g.fillRect(pos.x - 5.f, pos.y - 5.f, 6.f, 3.f);
        }

        // brillo lateral, labio y contorno
        { Path hl; hl.startNewSubPath(toWorld({ -76.f, -18.f })); hl.lineTo(toWorld({ -72.f, -18.f })); hl.lineTo(toWorld({ -78.f, -132.f })); hl.lineTo(toWorld({ -82.f, -132.f })); hl.closeSubPath();
          g.setColour(Colours::white.withAlpha(0.4f)); g.fillPath(hl); }
        { Path lip; lip.startNewSubPath(outer[3]); lip.lineTo(outer[2]); lip.lineTo(inner[2]); lip.lineTo(inner[3]); lip.closeSubPath();
          g.setColour(Colours::white.withAlpha(0.55f)); g.fillPath(lip); }
        g.setColour(Colours::black); g.strokePath(outerPath, PathStrokeType(4.f, PathStrokeType::mitered, PathStrokeType::rounded));
    }

    void drawStream(juce::Graphics& g)
    {
        using namespace juce;
        if (streamFlow <= 0.f) return;
        const Pt a = rimPoint, b(a.x + 10.f + streamFlow * 14.f, floorY() - 5.f);
        const float w = 3.f + streamFlow * 13.f;
        Path p; p.startNewSubPath(a); p.quadraticTo(a.x + 16.f + streamFlow * 10.f, a.y + (b.y - a.y) * 0.25f, b.x, b.y);
        g.setColour(Colours::black); g.strokePath(p, PathStrokeType(w + 4.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(Colour(0xff7b2fff)); g.strokePath(p, PathStrokeType(w, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(Pal::light.withAlpha(0.6f)); g.strokePath(p, PathStrokeType(w * 0.3f), AffineTransform::translation(-w * 0.2f, 0.f));
    }

    void drawDrops(juce::Graphics& g)
    {
        for (const auto& d : drops)
        {
            g.setColour(juce::Colour(0xff7b2fff).withAlpha(juce::jlimit(0.f, 1.f, d.life * 1.5f)));
            g.fillEllipse(d.x - d.size * 0.5f, d.y - d.size * 0.6f, d.size, d.size * 1.2f);
            g.setColour(juce::Colours::white.withAlpha(0.6f * juce::jlimit(0.f, 1.f, d.life)));
            g.fillEllipse(d.x - d.size * 0.25f, d.y - d.size * 0.4f, d.size * 0.3f, d.size * 0.3f);
        }
    }

    //------------------------------------------------------------ fondo estático (cacheado)
    void rebuildBackground()
    {
        using namespace juce;
        const int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) { bg = Image(); return; }
        bg = Image(Image::ARGB, w, h, true);
        Graphics g(bg);
        const float W = (float) w, H = (float) h, wallB = H * 0.57f;

        // ladrillos
        g.fillAll(Colour(0xff08050f));
        const Colour tones[4] = { Colour(0xff2d1850), Colour(0xff3a2166), Colour(0xff24133f), Colour(0xff432a78) };
        const float bw = 62.f, bh = 28.f;
        for (int row = 0; row * bh < wallB; ++row)
            for (int col = -1; col * bw < W; ++col)
            {
                uint32 hs = (uint32) (row * 73856093) ^ (uint32) ((col + 3) * 19349663); hs *= 2654435761u;
                const float x = col * bw - ((row & 1) ? bw * 0.5f : 0.f), y = row * bh;
                Rectangle<float> r(x + 1.5f, y + 1.5f, bw - 3.f, bh - 3.f);
                g.setColour(tones[(hs >> 24) & 3]); g.fillRoundedRectangle(r, 2.f);
                g.setColour(Colours::white.withAlpha(0.08f)); g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 2, r.getRight() - 2);
                g.setColour(Colours::black.withAlpha(0.3f)); g.drawHorizontalLine((int) r.getBottom() - 1, r.getX() + 2, r.getRight() - 2);
            }
        g.setGradientFill(ColourGradient(Colours::transparentBlack, W * 0.5f, wallB * 0.5f, Colours::black.withAlpha(0.6f), 0.f, 0.f, true));
        g.fillRect(0.f, 0.f, W, wallB);

        // graffiti DOBLECUPP con chorreones
        {
            Graphics::ScopedSaveState ss(g);
            g.addTransform(AffineTransform::rotation(-0.07f, 200.f, 80.f));
            g.setFont(F(70.f, Font::bold | Font::italic));
            const Rectangle<float> r(14.f, 20.f, 420.f, 96.f);
            g.setColour(Colours::black.withAlpha(0.7f)); g.drawText("DOBLECUPP", r.translated(8.f, 9.f), Justification::centredLeft);
            g.setColour(Pal::purple);
            for (int k = 0; k < 16; ++k)
            { const float a = (float) k * MathConstants<float>::twoPi / 16.f; g.drawText("DOBLECUPP", r.translated(std::cos(a) * 5.f, std::sin(a) * 5.f), Justification::centredLeft); }
            g.setColour(Colours::white); g.drawText("DOBLECUPP", r, Justification::centredLeft);
            const float dxs[] = { 40.f, 96.f, 150.f, 228.f, 300.f, 372.f }, dls[] = { 38.f, 20.f, 54.f, 28.f, 44.f, 24.f };
            for (int i = 0; i < 6; ++i)
            {
                g.setColour(Colours::black); g.drawLine(dxs[i], 100.f, dxs[i], 100.f + dls[i], 9.f);
                g.setColour(Pal::purple); g.drawLine(dxs[i], 100.f, dxs[i], 100.f + dls[i], 6.f);
                g.fillEllipse(dxs[i] - 4.f, 100.f + dls[i] - 3.f, 8.f, 10.f);
            }
        }

        // póster con la imagen del usuario (sin alterar), pegada con cinta plateada
        {
            const float pw = 216.f, ph = 174.f, px = W * 0.665f, py = H * 0.10f;
            Graphics::ScopedSaveState ss(g);
            g.addTransform(AffineTransform::rotation(0.04f, px + pw * 0.5f, py + ph * 0.5f));
            g.setColour(Colours::black.withAlpha(0.45f)); g.fillRect(px + 6.f, py + 8.f, pw, ph);
            g.setColour(Colour(0xfff4f0ff)); g.fillRect(px, py, pw, ph);
            const Rectangle<float> inner(px + 8.f, py + 8.f, pw - 16.f, ph - 16.f);
            if (character.isValid()) g.drawImage(character, inner, RectanglePlacement::centred);
            else { g.setColour(Pal::deep); g.fillRect(inner); }
            g.setColour(Colours::black); g.drawRect(inner, 2.f);
            for (float tx : { px - 10.f, px + pw - 44.f })
            {
                Graphics::ScopedSaveState s2(g);
                g.addTransform(AffineTransform::rotation(tx < px + 20.f ? -0.6f : 0.6f, tx + 27.f, py + 6.f));
                g.setGradientFill(ColourGradient(Pal::silver, tx, py, Pal::silverDark, tx, py + 16.f, false));
                g.fillRect(tx, py - 2.f, 54.f, 16.f);
                g.setColour(Colours::black.withAlpha(0.5f)); g.drawRect(tx, py - 2.f, 54.f, 16.f, 1.f);
            }
        }

        // acera: bordillo plateado, losas en perspectiva y grietas
        g.setGradientFill(ColourGradient(Colour(0xff4b4763), 0.f, wallB, Colour(0xff241f38), 0.f, H, false));
        g.fillRect(0.f, wallB, W, H - wallB);
        g.setGradientFill(ColourGradient(Pal::silver, 0.f, wallB, Pal::silverDark, 0.f, wallB + 12.f, false));
        g.fillRect(0.f, wallB, W, 12.f);
        g.setColour(Colours::black); g.drawHorizontalLine((int) (wallB + 12.f), 0.f, W);
        const float top = wallB + 12.f;
        g.setColour(Colours::black.withAlpha(0.4f));
        for (int k = 1; k <= 5; ++k) g.drawHorizontalLine((int) (top + (H - top) * std::pow((float) k / 5.f, 1.6f)), 0.f, W);
        const Pt vp(W * 0.5f, wallB - 140.f);
        for (int xi = -6; xi <= 6; ++xi)
        {
            const float xb = W * 0.5f + (float) xi * 120.f, ratio = (top - vp.y) / (H - vp.y), xt = vp.x + (xb - vp.x) * ratio;
            g.drawLine(xt, top, xb, H, 2.f);
        }
        g.setColour(Colours::black.withAlpha(0.55f));
        { Path c; c.startNewSubPath(W * 0.12f, H * 0.88f); c.lineTo(W * 0.15f, H * 0.93f); c.lineTo(W * 0.14f, H * 0.97f);
          c.startNewSubPath(W * 0.80f, H * 0.64f); c.lineTo(W * 0.84f, H * 0.69f); c.lineTo(W * 0.83f, H * 0.74f); c.lineTo(W * 0.87f, H * 0.78f);
          g.strokePath(c, PathStrokeType(1.6f)); }
    }

    //------------------------------------------------------------ estado
    DoblecuppProcessor& proc;
    juce::Image bg, character;
    std::array<Pt, 4> outer {}, inner {};
    Poly liquid;
    Pt liqCentre, rimPoint;
    std::vector<Drop> drops;
    juce::Random rng;
    float time = 0.f, angle = 0.f, angVel = 0.f, prevS = 0.f, sNow = 0.f, puddleP = 0.f, slosh = 0.f,
          lvl = 0.f, prevIn = 0.f, cooldown = 0.f, ySurf = 0.f, streamFlow = 0.f;
    bool hasLiquid = false;
};
