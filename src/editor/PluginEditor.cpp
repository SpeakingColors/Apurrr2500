#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"
#include "RenderGate.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) {}
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 8), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};


const char* const kRatL[7] = { "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "Inf:1" };
const char* const kAtkL[7] = { "0.03 ms", "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" };
const char* const kRelS[6] = { "50 ms", "100 ms", "200 ms", "500 ms", "1 s", "2 s" };
const char* const kLinkL[7] = { "IND", "50%", "60%", "70%", "80%", "90%", "100%" };

String fmtMs (double v)
{
    return v < 1000.0 ? String (v, v < 10.0 ? 1 : 0) + " ms" : String (v / 1000.0, 2) + " s";
}

struct KnobCfg { int steps; bool reversed; std::vector<KnobView::Label> labels; };
KnobCfg knobCfg (const String& id)
{
    if (id == "thr")    return { 0, true,  { { 0.0f, "+10" }, { 0.5f, "-5" }, { 1.0f, "-20" } } };
    if (id == "gain")   return { 0, false, { { 0.0f, "0" }, { 0.5f, "12" }, { 1.0f, "+24" } } };
    if (id == "relVar") return { 0, false, { { 0.0f, "50" }, { 1.0f, "3s" } } };
    if (id == "mix")    return { 0, false, { { 0.0f, "0" }, { 0.5f, "50" }, { 1.0f, "100" } } };
    return { 7, false, {} }; // ratio, link, attack, release
}
} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost
{
public:
    Face (ApurrrProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)), logo (makeLogo()),
          curve (ctx, live), filt (ctx, live), hist (ctx, live), needle (ctx, live), step (ctx, live),
          meterIn (ctx), meterOut (ctx), meterGR (ctx, live), ledThr (ctx), bubble (ctx), toastView (ctx), modal (*this), gate (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        ctx.needleMs = jlimit (20, 2000, (int) proc.getSetting (setting::needle, 300));
        ctx.refDb = (int) proc.getSetting (setting::ref, 18);
        if (ctx.refDb != 14 && ctx.refDb != 16) ctx.refDb = 18;
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        cheekL->setBounds (roundToInt (layout::cheekL.x), 0, roundToInt (layout::cheekL.w), layout::H);
        cheekR->setBounds (roundToInt (layout::cheekR.x), 0, roundToInt (layout::cheekR.w), layout::H);
        cheekL->tip = cheekR->tip = "Double-click the wood to reset the window to 100%.";

        viewComp.prepare (48000.0);
        buildHeader();
        buildDisplays();
        buildKnobs();
        buildStrips();
        buildSwitches();
        buildMeters();

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        refreshLive (1.0f / 60.0f);
        syncControls();
        gate.onFrame = [this] { tick(); };
        gate.setPauseInBackground (false);
        gate.start (ctx.power ? 16 : 60);
    }
    ~Face() override { gate.stop(); removeMouseListener (this); }

    void tick()
    {
        refreshLive (1.0f / (float) (ctx.power ? 16 : 60));
        syncControls();
        curve.repaint(); hist.repaint(); needle.advance (1.0f / (float) (ctx.power ? 16 : 60)); needle.repaint();
        filt.repaint();
        step.refresh (live.prm);
        meterIn.update (live.inPk); meterOut.update (live.outPk); meterGR.repaint();
        repaint (Rectangle<int> (1184, 430, 124, 24)); // peak read-outs
    }

    void openForTesting (const String& n)
    {
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide();
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "tip") { tipHost = &meterIn; tipText = "Info bubble check."; showTip(); }
    }

    // ---- DialogHost --------------------------------------------------------------------------
    ApurrrProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncControls(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        gate.start (ctx.power ? 16 : 60);
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    Preset currentValues() override
    {
        Preset p;
        for (int i = 0; i < presets::kCount; ++i)
        {
            double v = real (presets::keys[i]);
            if (i == 0 || i == 12) v = std::round (v * 100.0) / 100.0;
            else if (i == 4) v = std::round (v * 10.0) / 10.0;
            else if (i == 14) v = std::round (v * 10.0) / 10.0;
            p.v[i] = v;
        }
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        for (int i = 0; i < presets::kCount; ++i) setReal (presets::keys[i], (float) p.v[i]);
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        refreshLive (1.0f / 60.0f);
        syncControls();
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i = 0; i < presets::kCount; ++i)
        {
            const double tol = i == 4 ? 0.6 : i == 14 ? 0.11 : 0.011;
            if (std::abs (now.v[i] - base->v[i]) > tol) return true;
        }
        return false;
    }

    // ---- painting ----------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.fillAll (Colours::black);
        g.setColour (t.plate);
        g.fillRect (rc (layout::body).withTop (0).withHeight ((float) layout::H));
        // header
        g.setColour (t.plate2); g.fillRect (rc (layout::hdr));
        g.setColour (t.line); g.fillRect (layout::hdr.x, layout::hdr.h - 1.0f, layout::hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        drawTxt (g, layout::nameA, t);
        drawTxt (g, layout::nameB, t);
        drawTxt (g, layout::s87, t);
        // displays
        g.setColour (t.plate2); g.fillRect (rc (layout::disp));
        g.setColour (t.line); g.fillRect (layout::disp.x, layout::disp.y + layout::disp.h - 1.0f, layout::disp.w, 1.0f);
        static const char* meterNames[3] = { "Meter: gain reduction", "Meter: output level", "Meter: input level" };
        for (int i = 0; i < 5; ++i)
        {
            if (i == 3) { const String s (meterNames[jlimit (0, 2, live.meterMode)]); drawTxt (g, layout::caps[i], t, 1.0f, &s); }
            else drawTxt (g, layout::caps[i], t);
        }
        // section headers
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (theme::ledColour (layout::shCol[i], t));
            g.fillRect (layout::shRect[i].x, layout::shRect[i].y + layout::shRect[i].h - 2.0f, layout::shRect[i].w, 2.0f);
            drawTxt (g, layout::shText[i * 2], t);
            drawTxt (g, layout::shText[i * 2 + 1], t);
        }
        for (auto& s : layout::segLabel) drawTxt (g, s, t);
        // knob labels (dimmed together with their knob)
        for (int i = 0; i < 8; ++i)
            drawTxt (g, layout::knobs[i].label, t, knobOff[i] ? 0.45f : 1.0f);
        // name plate
        {
            auto r = rc (layout::plate);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), r.getX(), r.getY(), Colours::black.withAlpha (0.2f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (t.line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            for (auto& x : layout::plateText) drawTxt (g, x, t);
            Path tail;
            tail.startNewSubPath (4, 28); tail.quadraticTo (20, 4, 36, 20); tail.quadraticTo (52, 36, 68, 14);
            tail.quadraticTo (84, -8, 100, 22); tail.quadraticTo (112, 24, 116, 10);
            g.saveState();
            g.addTransform (AffineTransform::translation (layout::plateSvg.x, layout::plateSvg.y));
            g.setColour (theme::pink);
            g.strokePath (tail, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Colour (0xffFBBDD0)); g.fillEllipse (116 - 3.5f, 10 - 3.5f, 7.0f, 7.0f);
            g.restoreState();
        }
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (layout::meters));
        g.setColour (t.line); g.fillRect (layout::meters.x, layout::meters.y, 1.0f, layout::meters.h);
        for (auto& x : layout::meterCap) drawTxt (g, x, t);
        {
            auto pk = [] (float lin) { return lin > 1.0e-4f ? String (l2d (lin), 1) : String ("-inf"); };
            const String pks[3] = { pk (live.inPk), String (live.grPk, 1), pk (live.outPk) };
            for (int i = 0; i < 3; ++i)
                drawText (g, pks[i], rc (layout::pkRect[i]), theme::font (theme::FontKind::mono, 10.0f), t.dim);
        }
        // footer
        g.setColour (t.plate2); g.fillRect (rc (layout::foot));
        g.setColour (t.line); g.fillRect (layout::foot.x, layout::foot.y, layout::foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " Apurrr2500 v" + JucePlugin_VersionString,
                  { layout::footL.r.x, layout::foot.y, 400.0f, layout::foot.h }, theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 1.0f);
        if (auto* ed = findParentComponentOfClass<ApurrrEditor>())
            drawText (g, String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()) + " px",
                      { layout::foot.x + layout::foot.w - 12.0f - 120.0f, layout::foot.y, 120.0f, layout::foot.h },
                      theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        const bool shift = k.getModifiers().isShiftDown();
        if (c == 'b') toggle (ids::bypass);
        else if (c == 'i') toggle (ids::inSw);
        else if (c == 'k') cycle (ids::knee, 3, 1);
        else if (c == 't') cycle (ids::tilt, 3, 1);
        else if (c == 'y') toggle (ids::type);
        else if (c == 'm') cycle (ids::meter, 3, 1);
        else if (c == 'r') cycle (ids::ratio, 7, shift ? 6 : 1);
        else return false;
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int idx (const char* id) const { return (int) std::lround (real (id)); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggle (const char* id) { setReal (id, real (id) > 0.5f ? 0.0f : 1.0f); syncControls(); }
    void cycle (const char* id, int n, int by) { setReal (id, (float) ((idx (id) + by) % n)); syncControls(); }

    void refreshLive (float dt)
    {
        live.fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const float dec = std::exp (-dt / 0.1f);
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        live.grL = proc.grL.load(); live.grR = proc.grR.load();
        live.grPk = std::max (proc.grPeak.exchange (0.0f), live.grPk * dec);
        live.histL[live.hi] = proc.grPeakL.exchange (0.0f);
        live.histR[live.hi] = proc.grPeakR.exchange (0.0f);
        live.hi = (live.hi + 1) % 360;
        live.matchDb = proc.matchDb.load();
        for (int c = 0; c < 2; ++c) { live.msIn[c] = proc.inMs[c].load(); live.msOut[c] = proc.outMs[c].load(); }
        live.prm = proc.readParams();
        live.bypass = real (ids::bypass) > 0.5f;
        live.meterMode = jlimit (0, 2, idx (ids::meter));
        if (live.prm.tilt != lastTilt || std::abs (live.fs - lastFs) > 1.0)
        {
            lastTilt = live.prm.tilt; lastFs = live.fs;
            viewComp.prepare (live.fs);
            viewComp.setParams (live.prm, true);
            filt.setTilt (viewComp);
        }
    }

    void syncControls()
    {
        const auto& p = live.prm;
        bBypass->setOn (live.bypass);
        // value rows
        auto num = [] (double v) { return String (v >= 0 ? "+" : "") + String (v, 1); };
        values[0]->setParts ({}, num (p.thrDbu), " dB");
        values[1]->setParts ({}, kRatL[p.ratio], {});
        values[2]->setParts (p.manualMakeup ? String() : String ("Auto "), num (p.manualMakeup ? p.makeupDb : apurr::autoMakeupDb (p)), " dB");
        values[3]->setParts ({}, kLinkL[p.link], {});
        values[4]->setParts ({}, kAtkL[p.attack], {});
        values[5]->setParts ({}, p.release < 6 ? kRelS[p.release] : "Var", {});
        values[6]->setParts ({}, fmtMs (p.relVarMs), {});
        values[7]->setParts ({}, String (roundToInt (p.mix * 100.0)), " %");
        // dimmed knobs (as in the mockup)
        const bool off[8] = { false, false, ! p.manualMakeup, false, false, false, p.release < 6, p.blend == 0 };
        bool changed = false;
        for (int i = 0; i < 8; ++i)
        {
            if (off[i] != knobOff[i]) { knobOff[i] = off[i]; changed = true; }
            knobViews[i]->setAlpha (off[i] ? 0.45f : 1.0f);
            values[(size_t) i]->setAlpha (off[i] ? 0.45f : 1.0f);
        }
        if (changed) repaint();
        for (int i = 0; i < 7; ++i)
        {
            chipRatio[i]->setOn (p.ratio == i); chipLink[i]->setOn (p.link == i);
            chipAttack[i]->setOn (p.attack == i); chipRelease[i]->setOn (p.release == i);
        }
        for (int i = 0; i < 3; ++i)
        {
            segKnee[i]->setOn (p.knee == i); segTilt[i]->setOn (p.tilt == i); segShape[i]->setOn (p.shape == i);
            segBlend[i]->setOn (p.blend == i); segMeter[i]->setOn (live.meterMode == i);
        }
        for (int i = 0; i < 2; ++i) { segMake[i]->setOn ((p.manualMakeup ? 1 : 0) == i); segType[i]->setOn ((p.oldType ? 0 : 1) == i); }
        tgExt->setOn (p.extSc); tgExt->setDisabledLook (p.oldType);
        tgIn->setOn (p.inSw);
        tgMatch->setOn (p.levelMatch);
        tgMatch->setShownText (p.levelMatch ? String ("Level Match ") + (live.matchDb >= 0 ? "+" : "") + String (live.matchDb, 1) + " dB" : String());
        // threshold lamp
        ledThr.setAmount (live.grPk > 0.15f && ! live.bypass ? 0.45f + std::min (0.55f, live.grPk / 10.0f) : 0.0f);
        const String shown = presetName() + (dirty() ? " *" : "");
        pName->setText (shown);
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (const layout::Btn& b)
    {
        auto* x = new PButton (ctx, b.t, b.fs, b.fw >= 600 ? theme::FontKind::semi : theme::FontKind::regular);
        owned.add (x);
        addAndMakeVisible (x);
        x->setBounds (roundToInt (b.r.x), roundToInt (b.r.y), roundToInt (b.r.w), roundToInt (b.r.h));
        x->tip = b.tip;
        return x;
    }

    void buildHeader()
    {
        pPrev = addButton (layout::pPrev); pName = addButton (layout::pName); pNext = addButton (layout::pNext); pSave = addButton (layout::pSave);
        bBypass = addButton (layout::bBypass); bGuide = addButton (layout::bGuide); bSettings = addButton (layout::bSettings); bAbout = addButton (layout::bAbout);
        pPrev->text = String::fromUTF8 ("\xe2\x97\x80"); pNext->text = String::fromUTF8 ("\xe2\x96\xb6");
        pName->text = presetName();
        pPrev->onClick = [this] (const ModifierKeys&) { stepPreset (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { stepPreset (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggle (ids::bypass); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide(); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr || p->factory) { toast ("Factory presets cannot be renamed. Use Save to keep your own copy."); return; }
            Preset q = *p; modal.openSave (&q);
        };
    }

    void stepPreset (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    void placeCv (Component& c, const layout::Cv& v)
    {
        addAndMakeVisible (c);
        c.setBounds (roundToInt (v.r.x), roundToInt (v.r.y), roundToInt (v.r.w), roundToInt (v.r.h));
        if (auto* tc = dynamic_cast<TipClient*> (&c)) tc->tip = v.tip;
    }

    void buildDisplays()
    {
        placeCv (curve, layout::cvCurve); placeCv (filt, layout::cvFilt); placeCv (hist, layout::cvHist);
        placeCv (needle, layout::cvNeedle); placeCv (step, layout::cvStep);
    }

    void buildKnobs()
    {
        for (int i = 0; i < 8; ++i)
        {
            const auto& k = layout::knobs[i];
            const auto cfg = knobCfg (k.id);
            auto* kv = new KnobView (*proc.apvts.getParameter (k.id), k, cfg.steps, cfg.reversed, cfg.labels);
            kv->ctxp = &ctx;
            kv->tip = k.tip;
            knobViews.add (kv);
            addAndMakeVisible (kv);
            kv->setBounds (roundToInt (k.knob.x), roundToInt (k.knob.y), roundToInt (k.knob.w), roundToInt (k.knob.h));
            auto* v = new ValueText (ctx);
            values.add (v); addAndMakeVisible (v);
            v->setBounds (roundToInt (k.val.x - 20), roundToInt (k.val.y), roundToInt (k.val.w + 40), roundToInt (k.val.h));
            v->tip = k.tip;
        }
        ledThr.tip = layout::ledThrTip;
        addAndMakeVisible (ledThr);
        ledThr.setBounds (roundToInt (layout::ledThr.x), roundToInt (layout::ledThr.y), 9, 9);
    }

    template <size_t N>
    void makeChips (const layout::Chip (&spec)[N], OwnedArray<ChipButton>& dst, const char* id, const char* hint = nullptr)
    {
        for (size_t i = 0; i < N; ++i)
        {
            auto* c = new ChipButton (ctx, spec[i].t, 11.0f);
            dst.add (c); addAndMakeVisible (c);
            c->setBounds (roundToInt (spec[i].r.x), roundToInt (spec[i].r.y), roundToInt (spec[i].r.w), roundToInt (spec[i].r.h));
            c->tip = spec[i].tip;
            c->onClick = [this, id, i] { setReal (id, (float) i); syncControls(); };
        }
        juce::ignoreUnused (hint);
    }

    void buildStrips()
    {
        makeChips (layout::chipRatio, chipRatio, ids::ratio);
        makeChips (layout::chipLink, chipLink, ids::link);
        makeChips (layout::chipAttack, chipAttack, ids::attack);
        makeChips (layout::chipRelease, chipRelease, ids::release);
    }

    template <size_t N>
    void makeSegs (const layout::Seg (&spec)[N], OwnedArray<LampButton>& dst, const char* id)
    {
        for (size_t i = 0; i < N; ++i)
        {
            auto* b = new LampButton (ctx, spec[i]);
            dst.add (b); addAndMakeVisible (b);
            b->onClick = [this, id, i] { setReal (id, (float) i); syncControls(); };
        }
    }

    LampButton* makeToggle (const layout::Seg& spec, const char* id)
    {
        auto* b = new LampButton (ctx, spec);
        tgOwned.add (b); addAndMakeVisible (b);
        b->onClick = [this, id] { toggle (id); };
        return b;
    }

    void buildSwitches()
    {
        makeSegs (layout::segKnee, segKnee, ids::knee);   makeSegs (layout::segMake, segMake, ids::muMode);
        makeSegs (layout::segTilt, segTilt, ids::tilt);   makeSegs (layout::segType, segType, ids::type);
        // the Type buttons run Old = 0, New = 1, the same as the parameter
        makeSegs (layout::segShape, segShape, ids::shape); makeSegs (layout::segBlend, segBlend, ids::blend);
        makeSegs (layout::segMeter, segMeter, ids::meter);
        tgExt = makeToggle (layout::tglExt, ids::extSc);
        tgIn = makeToggle (layout::tglIn, ids::inSw);
        tgMatch = makeToggle (layout::tglMatch, ids::levelMatch);
    }

    void buildMeters()
    {
        placeCv (meterIn, layout::cvIn); placeCv (meterGR, layout::cvGR); placeCv (meterOut, layout::cvOut);
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (modal.isOpen() || ! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty())
            {
                text = tc->tip; host = c; break;
            }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr || modal.isOpen()) return;
        bubble.showAt (tipText, getLocalArea (tipHost, tipHost->getLocalBounds()), getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    ApurrrProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    Live live;
    presets::Store presetStore;
    std::unique_ptr<Drawable> logo;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    CurveView curve; FiltView filt; HistView hist; NeedleView needle; StepView step;
    OwnedArray<KnobView> knobViews;
    OwnedArray<ValueText> values;
    OwnedArray<ChipButton> chipRatio, chipLink, chipAttack, chipRelease;
    OwnedArray<LampButton> segKnee, segMake, segTilt, segType, segShape, segBlend, segMeter, tgOwned;
    LampButton *tgExt = nullptr, *tgIn = nullptr, *tgMatch = nullptr;
    LevelMeter meterIn, meterOut;
    GRMeter meterGR;
    LedView ledThr;
    bool knobOff[8] {};
    apurr::Compressor viewComp;
    int lastTilt = -1; double lastFs = 0.0;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    catui::RenderGate gate;
    Component* tipHost = nullptr;
    String tipText;
};

// =====================================================================================================
ApurrrEditor::ApurrrEditor (ApurrrProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (544, 246);
    constrainer.setMaximumSize (2040, 924);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

ApurrrEditor::~ApurrrEditor() = default;

void ApurrrEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void ApurrrEditor::resetSize() { setSize (kW, kH); }
void ApurrrEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
void ApurrrEditor::tickForTesting() { face->tick(); }
