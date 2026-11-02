#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"
#include "Layout.h"

namespace ui
{
using namespace juce;

// Settings every widget needs to paint (look, info bubbles, needle scale).
struct Ctx
{
    char look = 'C';
    bool tips = true;
    bool power = false;
    int needleMs = 300;
    int refDb = 18;                         // needle 0 VU at -refDb dBFS
    const theme::Look& t() const { return theme::look (look); }
};

// Anything that can show an info bubble.
struct TipClient
{
    String tip;
    virtual ~TipClient() = default;
};

inline Rectangle<float> rc (const layout::Rc& r) { return { r.x, r.y, r.w, r.h }; }

inline float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Text centred in a box, never shortened with dots. Letter spacing is in px.
inline void drawText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c,
                      Justification j = Justification::centred, float spacingPx = 0.0f)
{
    if (std::abs (spacingPx) > 1.0e-6f) f = f.withExtraKerningFactor (spacingPx / jmax (1.0f, f.getHeight())); // JUCE kerning is a fraction of the font height
    g.setFont (f);
    g.setColour (c);
    Rectangle<float> box = r.expanded (30.0f, 0.0f); // room for font differences, text is never shortened
    if (j.testFlags (Justification::left))       box = r.withWidth (r.getWidth() + 60.0f);
    else if (j.testFlags (Justification::right)) box = r.withLeft (r.getX() - 60.0f);
    g.drawText (s, box, j, false);
}

inline Colour tokenColour (int col, const theme::Look& t)
{
    switch (col) { case -1: return t.ink; case -2: return t.dim; case -3: return t.title; case -4: return t.accent; default: return Colour ((uint32) col).withAlpha (1.0f); }
}

// A static text of the mockup, with its recorded font, spacing and colour.
inline void drawTxt (Graphics& g, const layout::Txt& x, const theme::Look& t, float alpha = 1.0f, const String* override = nullptr)
{
    const auto kind = x.mono ? theme::FontKind::mono
                    : (x.fw >= 800 && x.italic) ? theme::FontKind::brand
                    : x.fw >= 700 ? theme::FontKind::bold : x.fw >= 600 ? theme::FontKind::semi : theme::FontKind::regular;
    String s = override != nullptr ? *override : String (x.s);
    if (x.up) s = s.toUpperCase();
    drawText (g, s, rc (x.r), theme::font (kind, x.fs), tokenColour (x.col, t).withMultipliedAlpha (alpha), Justification::centred, x.ls);
}

// ---- Button in the mockup's .btn style -----------------------------------------------------------
class PButton : public Component, public TipClient
{
public:
    PButton (const Ctx& c, String t, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
        : text (std::move (t)), ctx (c), size (px), kind (k) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void (const ModifierKeys&)> onClick;
    std::function<void()> onDouble;
    bool on = false;
    float spacing = 0.5f;
    float radius = 4.0f;
    String text;

    void setText (const String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.plate);
        g.fillRoundedRectangle (r, radius);
        g.setColour (on ? theme::selb : (over ? t.dim : t.line));
        g.drawRoundedRectangle (r, radius, 1.0f);
        const Colour tc = on ? theme::lilac : t.ink;
        drawText (g, text, r, theme::font (kind, size), tc, Justification::centred, spacing);
        if (hasKeyboardFocus (true) && isShowing() && focusRing)
        {
            g.setColour (theme::selb);
            g.drawRoundedRectangle (getLocalBounds().toFloat().expanded (1.5f), radius + 1.5f, 2.0f);
        }
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDouble) onDouble(); }
    bool focusRing = false;

private:
    const Ctx& ctx;
    float size;
    theme::FontKind kind;
    bool over = false;
};

// ---- Small stepped-value chip under a knob (the mockup's .chip2) ---------------------------------------
class ChipButton : public Component, public TipClient
{
public:
    ChipButton (const Ctx& c, String t, float px) : ctx (c), text (std::move (t)), size (px) { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    bool on = false;
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.disp); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? theme::selb : (over ? t.dim : t.line)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::mono, size), on ? theme::lilac : t.dim);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
private:
    const Ctx& ctx;
    String text;
    float size;
    bool over = false;
};

// ---- Grey hardware style button with a lamp (the mockup's .sbtn); vertical (lamp above text) or toggle (lamp left) ---------
class LampButton : public Component, public TipClient
{
public:
    LampButton (const Ctx& c, const layout::Seg& s) : ctx (c), seg (s)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setBounds (roundToInt (s.r.x), roundToInt (s.r.y), roundToInt (s.r.w), roundToInt (s.r.h));
        tip = s.tip;
    }
    std::function<void()> onClick;
    bool on = false, disabledLook = false;
    String shownText;                                   // overrides the label (Level Match read-out)
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    void setDisabledLook (bool v) { if (v != disabledLook) { disabledLook = v; repaint(); } }
    void setShownText (const String& s) { if (s != shownText) { shownText = s; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float alpha = disabledLook ? 0.45f : 1.0f;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        r.removeFromBottom (2.0f);
        if (down) r.translate (0.0f, 1.0f);
        if (! down) { g.setColour (Colours::black.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 5.0f); }
        g.setGradientFill (ColourGradient (Colour (0xff3a3c43).withMultipliedAlpha (alpha), 0.0f, r.getY(), Colour (0xff2a2c31).withMultipliedAlpha (alpha), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (down ? 0.10f : 0.15f * alpha));
        g.drawLine (r.getX() + 3.0f, r.getY() + 1.5f, r.getRight() - 3.0f, r.getY() + 1.5f, 1.0f);
        g.setColour (Colours::black.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        // lamp
        const auto d = Rectangle<float> (seg.dot.x - seg.r.x, seg.dot.y - seg.r.y, seg.dot.w, seg.dot.h);
        const Colour lc = theme::ledColour (seg.lc < 0 ? 0 : seg.lc, t);
        const auto c = d.getCentre();
        if (on)
        {
            ColourGradient glow (lc.withAlpha (0.55f * alpha), c.x, c.y, lc.withAlpha (0.0f), c.x + 8.0f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - 8.0f, c.y - 8.0f, 16.0f, 16.0f);
            g.setColour (lc.withMultipliedAlpha (alpha)); g.fillEllipse (d);
            g.setColour (Colours::white.withAlpha (0.45f * alpha)); g.drawEllipse (d.reduced (1.5f), 1.0f);
        }
        else
        {
            g.setColour (Colour (0xff1a1b1e).withMultipliedAlpha (alpha)); g.fillEllipse (d);
            g.setColour (Colours::black.withAlpha (0.7f * alpha)); g.drawEllipse (d.reduced (0.5f), 1.0f);
        }
        const auto tr = Rectangle<float> (seg.tr.x - seg.r.x, seg.tr.y - seg.r.y, seg.tr.w, seg.tr.h);
        const String s = shownText.isNotEmpty() ? shownText : String (seg.t);
        auto f = theme::font (theme::FontKind::regular, seg.fs);
        float shrink = 1.0f;
        const float avail = (float) getWidth() - (seg.dot.x - seg.r.x) - seg.dot.w - 14.0f;
        if (shownText.isNotEmpty() && textWidth (f, s) > avail) shrink = avail / textWidth (f, s);
        const bool horiz = seg.r.h < 34.0f;           // toggles keep their text starting at the same place
        drawText (g, s, tr.withWidth (std::max (tr.getWidth(), 1.0f)), f.withHorizontalScale (shrink), (on ? t.ink : t.dim).withMultipliedAlpha (alpha),
                  horiz ? Justification::centredLeft : Justification::centred, 0.3f);
    }
    void mouseDown (const MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        down = false; repaint();
        if (e.mouseWasClicked() && onClick) onClick();
    }
private:
    const Ctx& ctx;
    const layout::Seg& seg;
    bool down = false;
};

// ---- Faceplate knob, drawn like the mockup's SVG (150 x 150 units into 96 px) -----------------------------
class KnobView : public Component, public TipClient
{
public:
    struct Label { float pos; const char* text; };

    KnobView (RangedAudioParameter& prm, const layout::Knob& k, int stepCount, bool isReversed, std::vector<Label> labs)
        : p (prm), spec (k), steps (stepCount), reversed (isReversed), labels (std::move (labs)),
          att (prm, [this] (float) { repaint(); })
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        tip = k.tip;
        att.sendInitialUpdate();
    }

    // position 0..1 as drawn (the Threshold knob runs backwards: right is lower)
    double pos() const { const double n = p.getValue(); return reversed ? 1.0 - n : n; }
    void setPos (double n)
    {
        n = jlimit (0.0, 1.0, n);
        if (steps > 0) n = std::round (n * (steps - 1)) / (double) (steps - 1);
        att.setValueAsPartOfGesture (p.convertFrom0to1 ((float) (reversed ? 1.0 - n : n)));
    }
    bool isStepped() const { return steps > 0; }
    String paramName() const { return spec.name; }

    void paint (Graphics& g) override
    {
        const auto& lk = Look();
        g.addTransform (AffineTransform::scale (getWidth() / 150.0f));
        const int cnt = steps > 0 ? steps : 21;
        for (int i = 0; i < cnt; ++i)
        {
            const float f = (float) i / (float) (cnt - 1), a = degreesToRadians (-135.0f + 270.0f * f);
            const bool major = steps > 0 ? true : i % 5 == 0;
            const float r1 = major ? 63.0f : 67.0f, r2 = 72.0f;
            g.setColour (major ? Colour (0xfff4f5f7) : Colour (0xff8c8f99));
            g.drawLine (75 + std::sin (a) * r1, 75 - std::cos (a) * r1, 75 + std::sin (a) * r2, 75 - std::cos (a) * r2, major ? 2.2f : 1.2f);
        }
        g.setColour (Colour (0xffc9cbd3));
        g.setFont (theme::font (theme::FontKind::semi, 15.0f));
        for (auto& l : labels)
        {
            const float a = degreesToRadians (-135.0f + 270.0f * l.pos);
            g.drawText (l.text, Rectangle<float> (75 + std::sin (a) * 54 - 25, 75 - std::cos (a) * 54 + 5 - 11, 50, 14), Justification::centred, false);
        }
        g.setColour (Colour (0xff0b0b0d));
        g.fillEllipse (75 - 41, 75 - 41, 82, 82);
        g.setColour (theme::ledColour (spec.col, lk));
        g.drawEllipse (75 - 41, 75 - 41, 82, 82, 3.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (degreesToRadians (-135.0f + 270.0f * (float) pos()), 75.0f, 75.0f));
            ColourGradient grad (Colour (0xff6b6d78), 66.6f, 62.4f, Colour (0xff1b1c20), 66.6f + 52.5f, 62.4f, true);
            grad.addColour (0.5, Colour (0xff3b3d45));
            g.setGradientFill (grad);
            g.fillEllipse (75 - 35, 75 - 35, 70, 70);
            g.setColour (Colours::black);
            g.drawEllipse (75 - 35, 75 - 35, 70, 70, 1.0f);
            Path ring; ring.addEllipse (75 - 31, 75 - 31, 62, 62);
            Path dashes; const float d[2] = { 1.6f, 2.0f };
            PathStrokeType (3.0f, PathStrokeType::mitered, PathStrokeType::butt).createDashedStroke (dashes, ring, d, 2);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillPath (dashes);
            g.setColour (Colour (0xfff4f5f7));
            g.drawLine (75, 75, 75, 46, 4.5f);
            g.fillEllipse (75 - 2.25f, 46 - 2.25f, 4.5f, 4.5f);
            g.fillEllipse (75 - 2.25f, 75 - 2.25f, 4.5f, 4.5f);
        }
        if (hasKeyboardFocus (false))
        {
            g.setColour (theme::selb);
            g.drawEllipse (1.0f, 1.0f, 148.0f, 148.0f, 3.0f);
        }
    }

    void mouseDown (const MouseEvent&) override { start = pos(); att.beginGesture(); grabKeyboardFocus(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const double per = steps > 0 ? 18.0 * (steps - 1) : 220.0;
        const double d = -(double) e.getDistanceFromDragStartY() / per * (e.mods.isShiftDown() && steps == 0 ? 0.1 : 1.0);
        setPos (start + d);
    }
    void mouseUp (const MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { att.beginGesture(); att.setValueAsPartOfGesture (p.convertFrom0to1 (p.getDefaultValue())); att.endGesture(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const float dy = w.isReversed ? -w.deltaY : w.deltaY;
        if (std::abs (dy) < 1.0e-6f) return;
        const int dir = dy > 0 ? 1 : -1;
        att.beginGesture();
        if (steps > 0) setPos (pos() + dir / (double) (steps - 1));
        else setPos (pos() + dir * (e.mods.isShiftDown() ? 0.002 : 0.02));
        att.endGesture();
    }
    bool keyPressed (const KeyPress& k) override
    {
        const double s = steps > 0 ? 1.0 / (steps - 1) : (k.getModifiers().isShiftDown() ? 0.002 : 0.01);
        double n = pos();
        if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) n += s;
        else if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) n -= s;
        else if (k.isKeyCode (KeyPress::homeKey)) n = 0.0;
        else if (k.isKeyCode (KeyPress::endKey)) n = 1.0;
        else return false;
        att.beginGesture(); setPos (n); att.endGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    const Ctx* ctxp = nullptr;

private:
    const theme::Look& Look() const { return ctxp != nullptr ? ctxp->t() : theme::look ('C'); }
    RangedAudioParameter& p;
    const layout::Knob& spec;
    int steps;
    bool reversed;
    std::vector<Label> labels;
    ParameterAttachment att;
    double start = 0.0;
};

// Value text under a knob, for example "Auto +4.0 dB": the number in ink, the rest dim.
class ValueText : public Component, public TipClient
{
public:
    ValueText (const Ctx& c) : ctx (c) {}
    String pre, num, post;
    void setParts (const String& a, const String& b, const String& c2) { if (a != pre || b != num || c2 != post) { pre = a; num = b; post = c2; repaint(); } }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (theme::FontKind::mono, 12.0f);
        const float wa = pre.isEmpty() ? 0.0f : textWidth (f, pre), wb = textWidth (f, num), wc = post.isEmpty() ? 0.0f : textWidth (f, post);
        float x = getWidth() * 0.5f - (wa + wb + wc) * 0.5f;
        const float y = getHeight() * 0.5f + f.getAscent() * 0.5f - 1.0f;
        g.setFont (f);
        g.setColour (t.dim); g.drawSingleLineText (pre, roundToInt (x), roundToInt (y));
        x += wa;
        g.setColour (t.ink); g.drawSingleLineText (num, roundToInt (x), roundToInt (y));
        x += wb;
        g.setColour (t.dim); g.drawSingleLineText (post, roundToInt (x), roundToInt (y));
    }
private:
    const Ctx& ctx;
};

class LedView : public Component, public TipClient
{
public:
    LedView (const Ctx& c) : ctx (c) {}
    float amount = 0.0f;            // 0 off, 0.45 to 1 on
    void setAmount (float a) { if (std::abs (a - amount) > 0.01f) { amount = a; repaint(); } }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (9.0f, 9.0f);
        auto c = r.getCentre();
        if (amount > 0.0f)
        {
            ColourGradient glow (theme::red.withAlpha (0.55f * amount), c.x, c.y, theme::red.withAlpha (0.0f), c.x + 8.0f, c.y, true);
            g.setGradientFill (glow); g.fillEllipse (c.x - 8, c.y - 8, 16, 16);
            g.setColour (theme::red.withAlpha (amount)); g.fillEllipse (r);
        }
        else { g.setColour (Colour (0xff26282c)); g.fillEllipse (r); }
        juce::ignoreUnused (ctx);
    }
private:
    const Ctx& ctx;
};
} // namespace ui
