#pragma once
#include "Widgets.h"
#include "../core/Apurrr2500.h"

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
struct Live
{
    double fs = 48000.0;
    float inPk = 0, outPk = 0;           // linear, decaying peak hold
    float grL = 0, grR = 0;              // gain reduction now, dB, positive
    float grPk = 0;                      // largest reduction, with a short hold
    float matchDb = 0;
    float msIn[2] { 1.0e-9f, 1.0e-9f }, msOut[2] { 1.0e-9f, 1.0e-9f };
    apurr::Params prm;
    bool bypass = false;
    int meterMode = 0;                   // 0 GR, 1 Out, 2 In
    float histL[360] {}, histR[360] {};
    int hi = 0;
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }

// Base of the display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
protected:
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void label (Graphics& g, const String& s, float x, float y, Justification j, Colour c, float px = 9.0f)
    {
        g.setFont (theme::font (theme::FontKind::mono, px));
        g.setColour (c);
        g.drawSingleLineText (s, roundToInt (x), roundToInt (y), j);
    }
    static void dashed (Graphics& g, Line<float> ln, float a, float b, float w)
    {
        const float d[2] = { a, b };
        g.drawDashedLine (ln, d, 2, w);
    }
    static void dashedPath (Graphics& g, const Path& p, float a, float b, float w)
    {
        Path out; const float d[2] = { a, b };
        PathStrokeType (w).createDashedStroke (out, p, d, 2);
        g.fillPath (out);
    }
    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

// Transfer curve: input level across (-60 to 0 dB), output level up (-60 to +12 dB).
class CurveView : public CanvasView
{
public:
    CurveView (const Ctx& c, const Live& l) : CanvasView (c, l, 300, 124) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float x0 = -60, x1 = 0, y0 = -60, y1 = 12;
        auto X = [=] (float v) { return 26.0f + (v - x0) / (x1 - x0) * (w - 34.0f); };
        auto Y = [=] (float v) { return h - 14.0f - (v - y0) / (y1 - y0) * (h - 22.0f); };
        for (float v : { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.setColour (t.line); g.drawLine (X (v), 4, X (v), h - 14, 1.0f);
            label (g, String ((int) v), X (v), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float v : { -48.0f, -24.0f, 0.0f, 12.0f })
        {
            g.setColour (t.line); g.drawLine (26, Y (v), w - 6, Y (v), 1.0f);
            label (g, String ((int) v), 23, Y (v) + 3, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff4a4d55));
        dashed (g, { X (-60), Y (-60), X (0), Y (0) }, 3, 3, 1.0f);
        const auto d = apurr::derive (live.prm);
        // knee band and threshold marker
        g.setColour (theme::purple.withAlpha (0.18f));
        g.fillRect (X ((float) (d.thr - d.w / 2)), 4.0f, std::max (2.0f, X ((float) (d.thr + d.w / 2)) - X ((float) (d.thr - d.w / 2))), h - 18.0f);
        g.setColour (theme::pink);
        dashed (g, { X ((float) d.thr), 4, X ((float) d.thr), h - 14 }, 2, 3, 1.0f);
        Path curve;
        bool first = true;
        for (float v = x0; v <= x1 + 0.01f; v += 0.5f)
        {
            const float o = live.bypass ? v : (float) apurr::staticOutDb (v, live.prm);
            const Point<float> pt (X (v), Y (jlimit (y0, y1, o)));
            if (first) curve.startNewSubPath (pt); else curve.lineTo (pt);
            first = false;
        }
        g.setColour (t.accent);
        g.strokePath (curve, PathStrokeType (2.0f));
        const float li = l2d (live.inPk), lo = l2d (live.outPk);
        if (live.inPk > 1.0e-3f)
        {
            g.setColour (Colours::white);
            g.fillEllipse (X (jmin (x1, li)) - 4, Y (jlimit (y0, y1, lo)) - 4, 8, 8);
        }
    }
};

// What the detector hears: the tilt filter response, -20 to +20 dB over 20 Hz to 20 kHz.
class FiltView : public CanvasView
{
public:
    FiltView (const Ctx& c, const Live& l) : CanvasView (c, l, 260, 124) {}
    void setTilt (const apurr::Compressor& comp) { for (int i = 0; i <= 120; ++i) mag[i] = (float) comp.tiltMag (20.0 * std::pow (1000.0, i / 120.0)); }
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float f0 = 20, f1 = 20000;
        auto X = [=] (float f) { return 26.0f + std::log (f / f0) / std::log (f1 / f0) * (w - 34.0f); };
        auto Y = [=] (float d) { return h - 14.0f - (d + 20.0f) / 40.0f * (h - 22.0f); };
        for (float f : { 20.0f, 100.0f, 1000.0f, 10000.0f })
        {
            g.setColour (t.line); g.drawLine (X (f), 4, X (f), h - 14, 1.0f);
            label (g, f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), X (f), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float d : { -15.0f, 0.0f, 15.0f })
        {
            g.setColour (t.line); g.drawLine (26, Y (d), w - 6, Y (d), 1.0f);
            label (g, String (d > 0 ? "+" : "") + String ((int) d), 23, Y (d) + 3, Justification::right, t.dim);
        }
        Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float f = f0 * std::pow (f1 / f0, (float) i / 120.0f);
            const float d = jlimit (-20.0f, 20.0f, l2d (mag[i]));
            const Point<float> pt (X (f), Y (d));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (t.accent);
        g.strokePath (p, PathStrokeType (2.0f));
        static const char* nm[3] = { "Norm", "Med", "Loud" };
        label (g, String (nm[jlimit (0, 2, live.prm.tilt)]) + (live.prm.oldType == false && live.prm.extSc ? String::fromUTF8 (" \xc2\xb7 Ext SC") : String()),
               32, 14, Justification::left, t.ink, 11.0f);
    }
private:
    float mag[121] {};
};

// Gain reduction over the last six seconds: mint is left, pink dashed is right.
class HistView : public CanvasView
{
public:
    HistView (const Ctx& c, const Live& l) : CanvasView (c, l, 300, 124) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        auto Y = [=] (float d) { return 8.0f + d / 20.0f * (h - 24.0f); };
        for (int d = 0; d <= 20; d += 5)
        {
            g.setColour (t.line); g.drawLine (26, Y ((float) d), w - 4, Y ((float) d), 1.0f);
            label (g, d ? "-" + String (d) : String ("0"), 23, Y ((float) d) + 3, Justification::right, t.dim);
        }
        const int NH = 360;
        Path p;
        p.startNewSubPath (28, Y (0));
        for (int i = 0; i < NH; ++i)
            p.lineTo (28.0f + (float) i / (NH - 1) * (w - 32.0f), Y (jmin (20.0f, live.histL[(live.hi + i) % NH])));
        p.lineTo (w - 4, Y (0));
        p.closeSubPath();
        g.setColour (theme::green.withAlpha (0.28f));
        g.fillPath (p);
        g.setColour (theme::green);
        g.strokePath (p, PathStrokeType (1.6f));
        Path r;
        for (int i = 0; i < NH; ++i)
        {
            const Point<float> pt (28.0f + (float) i / (NH - 1) * (w - 32.0f), Y (jmin (20.0f, live.histR[(live.hi + i) % NH])));
            if (i == 0) r.startNewSubPath (pt); else r.lineTo (pt);
        }
        g.setColour (theme::pink);
        dashedPath (g, r, 4, 3, 1.2f);
        label (g, "GR " + String (live.grPk, 1) + " dB", 32, h - 6, Justification::left, t.ink, 11.0f);
    }
};

// Stereo needle meter with a dB scale. Gain reduction has 0 at the right; level modes read -20 to +3 VU.
class NeedleView : public CanvasView
{
public:
    NeedleView (const Ctx& c, const Live& l) : CanvasView (c, l, 228, 124) {}
    void advance (float dt)
    {
        const bool gr = live.meterMode == 0;
        float vals[2];
        values (vals);
        const float k = gr ? 1.0f : jmin (1.0f, (1.0f - std::exp (-dt / 0.05f)) * 3.0f);
        for (int c = 0; c < 2; ++c) nd[c] += (vals[c] - nd[c]) * k;
    }
    void values (float* v) const
    {
        if (live.meterMode == 0) { v[0] = live.grL; v[1] = live.grR; return; }
        const float* a = live.meterMode == 1 ? live.msOut : live.msIn;
        for (int c = 0; c < 2; ++c) v[c] = 10.0f * std::log10 (std::max (a[c], 1.0e-10f)) + (float) ctx.refDb;
    }
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const bool gr = live.meterMode == 0;
        const float cx = w / 2, cy = h + 44, R = 118, span = degreesToRadians (50.0f);
        auto ang = [&] (float val) { const float f = gr ? 1.0f - jlimit (0.0f, 20.0f, val) / 20.0f : (jlimit (-20.0f, 3.0f, val) + 20.0f) / 23.0f; return -span + 2 * span * f; };
        auto P = [&] (float a, float r) { return Point<float> (cx + std::sin (a) * r, cy - std::cos (a) * r); };
        // red zone, then the scale arc
        const float zFrom = gr ? ang (3) : ang (0), zTo = gr ? ang (0) : ang (3);
        {
            Path z; z.addCentredArc (cx, cy, R, R, 0.0f, jmin (zFrom, zTo), jmax (zFrom, zTo), true);
            g.setColour (theme::red); g.strokePath (z, PathStrokeType (3.0f));
            Path a; a.addCentredArc (cx, cy, R, R, 0.0f, -span, span, true);
            g.setColour (t.dim); g.strokePath (a, PathStrokeType (1.2f));
        }
        const int tlo = gr ? 0 : -20, thi = gr ? 20 : 3;
        const std::vector<int> labs = gr ? std::vector<int> { 0, 3, 5, 10, 15, 20 } : std::vector<int> { -20, -10, -5, 0, 3 };
        for (int tk = tlo; tk <= thi; ++tk)
        {
            const bool major = std::find (labs.begin(), labs.end(), tk) != labs.end();
            const float a = ang ((float) tk);
            g.setColour (major ? t.ink : t.dim);
            g.drawLine (Line<float> (P (a, R), P (a, R - (major ? 9.0f : 5.0f))), major ? 1.5f : 1.0f);
        }
        for (int tk : labs)
        {
            const auto p = P (ang ((float) tk), R + 11.0f);
            label (g, String (! gr && tk > 0 ? "+" : "") + String (tk), p.x, p.y + 3, Justification::horizontallyCentred, t.ink);
        }
        label (g, gr ? "dB GR" : "dB VU", 4, h - 6, Justification::left, t.dim, 10.0f);
        float vals[2];
        values (vals);
        for (int c = 0; c < 2; ++c)
        {
            const float a = ang (nd[c]);
            g.setColour (c ? theme::pink : theme::green);
            g.drawLine (Line<float> (P (a, R - 62.0f + (float) c * 6.0f), P (a, R + 2.0f)), 2.0f);
        }
        auto f = [] (float v) { return String (v >= 0 ? "+" : "") + String (v, 1); };
        label (g, String ("L ") + (gr ? String (vals[0], 1) : f (vals[0])), w - 4, h - 20, Justification::right, theme::green, 11.0f);
        label (g, String ("R ") + (gr ? String (vals[1], 1) : f (vals[1])), w - 4, h - 6, Justification::right, theme::pink, 11.0f);
    }
private:
    float nd[2] { 0, 0 };
};

// Old vs New on a loud burst: 1 kHz at -34 dB, then -10 dB from 100 to 300 ms, run through the real core.
class StepView : public CanvasView
{
public:
    StepView (const Ctx& c, const Live& l) : CanvasView (c, l, 300, 100) {}

    // Re-run only when a setting that matters changed.
    void refresh (const apurr::Params& p)
    {
        apurr::Params q = p;
        q.extSc = false; q.blend = 0; q.levelMatch = false;
        const double key[] = { q.thrDbu, (double) q.ratio, q.makeupDb, (double) q.manualMakeup, (double) q.attack, (double) q.release, q.relVarMs,
                               (double) q.knee, (double) q.tilt, (double) q.link, (double) q.shape, (double) q.inSw };
        if (haveKey && std::equal (std::begin (key), std::end (key), keyv)) return;
        std::copy (std::begin (key), std::end (key), keyv);
        haveKey = true;
        for (int pass = 0; pass < 2; ++pass)
        {
            apurr::Compressor e;
            e.prepare (48000.0);
            q.oldType = pass == 1;
            e.setParams (q, true);
            e.snap();
            float L[100], R[100];
            for (int b = 0; b < 240; ++b)
            {
                for (int i = 0; i < 100; ++i)
                {
                    const int k = b * 100 + i;
                    const double tt = k / 48000.0;
                    const double amp = (tt >= 0.1 && tt < 0.3) ? apurr::dbToLin (-10.0) : apurr::dbToLin (-34.0);
                    L[i] = R[i] = (float) (amp * std::sin (2.0 * MathConstants<double>::pi * 1000.0 * tt) * 1.414);
                }
                e.process (L, R, 100);
                out[pass][b] = (float) e.grPeak();
            }
        }
        repaint();
    }
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        float mx = 0;
        for (auto& o : out) for (float v : o) mx = std::max (mx, v);
        const float top = std::max (6.0f, std::ceil (mx / 6.0f) * 6.0f);
        auto X = [=] (float s) { return 28.0f + s / 0.5f * (w - 34.0f); };
        auto Y = [=] (float d) { return 6.0f + std::min (top, d) / top * (h - 24.0f); };
        g.setColour (theme::purple.withAlpha (0.16f));
        g.fillRect (X (0.1f), 4.0f, X (0.3f) - X (0.1f), h - 18.0f);
        for (int k = 0; k <= 3; ++k)
        {
            const float d = top / 3.0f * (float) k;
            g.setColour (t.line); g.drawLine (28, Y (d), w - 4, Y (d), 1.0f);
            label (g, d > 0.01f ? "-" + String (roundToInt (d)) : String ("0"), 25, Y (d) + 3, Justification::right, t.dim);
        }
        for (float s : { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f })
            label (g, String (roundToInt (s * 1000)), X (s), h - 3, Justification::horizontallyCentred, t.dim);
        for (int pass = 0; pass < 2; ++pass)
        {
            Path p;
            for (int i = 0; i < 240; ++i)
            {
                const Point<float> pt (X ((float) i * 100.0f / 48000.0f), Y (out[pass][i]));
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour (pass == 0 ? theme::green : t.yellow);
            g.strokePath (p, PathStrokeType (2.0f));
        }
        label (g, "New", 32, h - 14, Justification::left, theme::green);
        label (g, "Old", 58, h - 14, Justification::left, t.yellow);
        label (g, "ms", w - 18, h - 14, Justification::left, t.dim);
    }
private:
    float out[2][240] {};
    double keyv[12] {};
    bool haveKey = false;
};

// Input or output level strip, 38 x 360, with a dB scale. Click to clear the clip marker.
class LevelMeter : public Component, public TipClient
{
public:
    LevelMeter (const Ctx& c) : ctx (c) {}
    float lin = 0.0f;
    bool latch = false;
    void update (float l)
    {
        const bool was = latch;
        const float prev = lin;
        lin = l;
        if (l2d (l) >= 0.0f) latch = true;
        if (was != latch || std::abs (l2d (prev) - l2d (l)) > 0.05f || prev != l) repaint();
    }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mn = -60, mx = 6;
        auto y = [=] (float db) { return h - 6.0f - (jlimit (mn, mx, db) - mn) / (mx - mn) * (h - 12.0f); };
        g.fillAll (t.disp);
        const float db = l2d (lin), top = y (db);
        auto f = [=] (float d) { return (d - mn) / (mx - mn); };
        ColourGradient grad (theme::blue, 0.0f, y (mn), theme::red, 0.0f, y (mx), false);
        grad.addColour (f (-12.0f), theme::blue);
        grad.addColour (f (-12.0f) + 0.001, theme::purple);
        grad.addColour (f (0.0f), theme::purple);
        grad.addColour (f (0.0f) + 0.001, theme::red);
        g.setGradientFill (grad);
        g.fillRect (2.0f, top, 12.0f, y (mn) - top);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
        g.setColour (latch ? theme::red : Colour (0xff2a2c31));
        g.fillRect (2.0f, 0.0f, 12.0f, 4.0f);
    }
    void mouseDown (const MouseEvent&) override { latch = false; repaint(); }
private:
    const Ctx& ctx;
};

// Gain reduction strip: hangs from the top, 0 to 20 dB, left in mint and right in pink.
class GRMeter : public Component, public TipClient
{
public:
    GRMeter (const Ctx& c, const Live& l) : ctx (c), live (l) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mx = 20;
        auto y = [=] (float d) { return 6.0f + jlimit (0.0f, mx, d) / mx * (h - 12.0f); };
        g.fillAll (t.disp);
        g.setColour (theme::green); g.fillRect (2.0f, 6.0f, 5.0f, y (live.grL) - 6.0f);
        g.setColour (theme::pink);  g.fillRect (8.0f, 6.0f, 5.0f, y (live.grR) - 6.0f);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, 1.0f, 2.0f, 3.0f, 6.0f, 10.0f, 15.0f, 20.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
    }
private:
    const Ctx& ctx;
    const Live& live;
};
} // namespace ui
