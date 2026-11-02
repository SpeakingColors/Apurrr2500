// Renders the real editor offscreen to a PNG so it can be compared with the mockup, and runs self checks that need the plugin classes.
// usage: ApurrSnapshot out.png [scale] [look A|B|C] [dialog none|settings|guide|about|presets|save|export|import|tip] [frames] [meter 0|1|2]
//        ApurrSnapshot --selftest [state-v1.xml]
//        ApurrSnapshot --write-fixture out.xml
#include "../src/plugin/PluginProcessor.h"
#include "../src/editor/PluginEditor.h"

static void setReal (ApurrrProcessor& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}
static float getReal (ApurrrProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

static void setFixtureValues (ApurrrProcessor& a)
{
    setReal (a, "thr", -6.5f); setReal (a, "ratio", 6); setReal (a, "gain", 7.5f); setReal (a, "attack", 2); setReal (a, "release", 6);
    setReal (a, "relVar", 420.0f); setReal (a, "knee", 2); setReal (a, "tilt", 2); setReal (a, "type", 0); setReal (a, "extSc", 0);
    setReal (a, "link", 3); setReal (a, "shape", 2); setReal (a, "muMode", 1); setReal (a, "blend", 2); setReal (a, "mix", 35.0f);
    setReal (a, "inSw", 1); setReal (a, "levelMatch", 1); setReal (a, "meter", 2);
    a.setSetting (setting::look, "B"); a.setSetting (setting::needle, 50); a.setSetting (setting::ref, 16); a.setSetting (setting::preset, "Mix Bus Glue");
}

// Saved state of state version 1 must keep loading (tests/fixtures/state-v1.xml).
static int fixtureTest (const juce::File& f)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    ApurrrProcessor p;
    auto xml = juce::XmlDocument::parse (f);
    check (xml != nullptr, "fixture parses");
    if (xml == nullptr) return 1;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (getReal (p, "thr") + 6.5f) < 0.02f && (int) getReal (p, "ratio") == 6 && std::abs (getReal (p, "gain") - 7.5f) < 0.02f
              && (int) getReal (p, "attack") == 2 && (int) getReal (p, "release") == 6 && std::abs (getReal (p, "relVar") - 420.0f) < 1.0f, "dynamics values from the v1 fixture");
    check ((int) getReal (p, "knee") == 2 && (int) getReal (p, "tilt") == 2 && (int) getReal (p, "type") == 0 && (int) getReal (p, "link") == 3
              && (int) getReal (p, "shape") == 2 && (int) getReal (p, "muMode") == 1 && (int) getReal (p, "blend") == 2
              && std::abs (getReal (p, "mix") - 35.0f) < 0.1f && getReal (p, "levelMatch") > 0.5f && (int) getReal (p, "meter") == 2, "switch and choice values from the v1 fixture");
    check (p.getSetting (setting::look, "C").toString() == "B" && (int) p.getSetting (setting::needle, 300) == 50
              && (int) p.getSetting (setting::ref, 18) == 16 && p.getSetting (setting::preset, "").toString() == "Mix Bus Glue", "settings from the v1 fixture");
    return bad;
}

static void fillProgram (juce::AudioBuffer<float>& buf, long pos, juce::Random& rnd, int ch0 = 0)
{
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const double t = (double) (pos + i) / 48000.0, tk = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.5);
        float v = (float) (0.8 * std::exp (-tk * 14.0) * std::sin (6.2831853 * (52.0 + 90.0 * std::exp (-tk * 40.0)) * tk));
        v += (float) (0.2 * std::exp (-th * 45.0) * (rnd.nextFloat() * 2.0f - 1.0f));
        v += 0.12f * (float) std::sin (6.2831853 * 220.0 * t) * (float) (0.6 + 0.4 * std::sin (6.2831853 * 0.25 * t));
        buf.setSample (ch0, i, v); buf.setSample (ch0 + 1, i, v * 0.97f);
    }
}

static int selfTest (const juce::File& fixture)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    ApurrrProcessor a, b;
    // defaults
    check (std::abs (getReal (a, "thr") - 4.0f) < 0.01f && (int) getReal (a, "ratio") == 1 && (int) getReal (a, "attack") == 5 && (int) getReal (a, "release") == 2, "defaults: Thr +4, 2:1, 10 ms, 200 ms");
    check ((int) getReal (a, "knee") == 1 && (int) getReal (a, "type") == 1 && (int) getReal (a, "link") == 6 && std::abs (getReal (a, "mix") - 100.0f) < 0.01f && getReal (a, "inSw") > 0.5f, "defaults: Med knee, New, Link 100, Mix 100, IN on");
    // every choice item survives a set and read (last item included)
    struct C { const char* id; int n; };
    const C choices[] = { { "ratio", 7 }, { "attack", 7 }, { "release", 7 }, { "link", 7 }, { "knee", 3 }, { "tilt", 3 }, { "type", 2 },
                          { "shape", 3 }, { "muMode", 2 }, { "blend", 3 }, { "meter", 3 } };
    for (auto& c : choices)
    {
        bool all = true;
        for (int i = 0; i < c.n; ++i) { setReal (a, c.id, (float) i); all = all && (int) getReal (a, c.id) == i; }
        check (all, (juce::String ("all ") + juce::String (c.n) + " items of " + c.id).toRawUTF8());
    }
    // ranges at both ends
    setReal (a, "thr", -20.0f); check (std::abs (getReal (a, "thr") + 20.0f) < 0.001f, "Threshold -20 dBu");
    setReal (a, "thr", 10.0f);  check (std::abs (getReal (a, "thr") - 10.0f) < 0.001f, "Threshold +10 dBu");
    setReal (a, "gain", 24.0f); check (std::abs (getReal (a, "gain") - 24.0f) < 0.001f, "Make-up +24 dB");
    a.apvts.getParameter ("relVar")->setValueNotifyingHost (0.0f); check (std::abs (getReal (a, "relVar") - 50.0f) < 0.01f, "Var 50 ms at the left end");
    a.apvts.getParameter ("relVar")->setValueNotifyingHost (1.0f); check (std::abs (getReal (a, "relVar") - 3000.0f) < 0.1f, "Var 3 s at the right end");
    a.apvts.getParameter ("relVar")->setValueNotifyingHost (0.5f); check (std::abs (getReal (a, "relVar") - 387.3f) < 1.0f, "Var is logarithmic (middle = 387 ms)");
    // state round trip including plugin settings
    setFixtureValues (a);
    juce::MemoryBlock mb; a.getStateInformation (mb);
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (getReal (b, "thr") + 6.5f) < 0.02f && (int) getReal (b, "ratio") == 6 && (int) getReal (b, "release") == 6 && (int) getReal (b, "blend") == 2
              && std::abs (getReal (b, "mix") - 35.0f) < 0.1f && getReal (b, "levelMatch") > 0.5f, "parameters survive a state round trip");
    check (b.getSetting (setting::look, "C").toString() == "B" && (int) b.getSetting (setting::needle, 300) == 50 && (int) b.getSetting (setting::ref, 18) == 16
              && b.getSetting (setting::preset, "").toString() == "Mix Bus Glue", "settings survive a state round trip");
    {   // garbage state must not crash and must leave defaults alone
        ApurrrProcessor c;
        const char junk[] = "not a state";
        c.setStateInformation (junk, (int) sizeof (junk));
        check ((int) getReal (c, "ratio") == 1, "garbage state is ignored");
    }
    // processing: bypass is bit exact once the ramp is done, the output is finite, IN off passes the signal bit exactly
    {
        ApurrrProcessor p;
        p.prepareToPlay (48000.0, 512);
        setReal (p, "bypass", 1.0f);
        juce::AudioBuffer<float> in (2, 512), x (2, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (3);
        bool exact = true, finite = true;
        for (int blk = 0; blk < 8; ++blk)
        {
            fillProgram (in, blk * 512L, rnd);
            x.makeCopyOf (in);
            p.processBlock (x, midi);
            if (blk >= 2)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i) { exact = exact && x.getSample (ch, i) == in.getSample (ch, i); finite = finite && std::isfinite (x.getSample (ch, i)); }
        }
        check (exact && finite, "Bypass passes the signal bit exactly");
        {
            ApurrrProcessor q;                 // IN off from the start: exact pass (the 8 ms IN ramp never reaches exactly 0 when switched off mid-signal)
            setReal (q, "inSw", 0.0f);
            q.prepareToPlay (48000.0, 512);
            exact = true;
            for (int blk = 0; blk < 6; ++blk)
            {
                fillProgram (in, blk * 512L, rnd);
                x.makeCopyOf (in);
                q.processBlock (x, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i) exact = exact && x.getSample (ch, i) == in.getSample (ch, i);
            }
            check (exact, "IN off from the start passes the signal bit exactly");
        }
        setReal (p, "bypass", 0.0f);
        setReal (p, "inSw", 1.0f); setReal (p, "thr", -20.0f); setReal (p, "ratio", 6);
        float maxGr = 0.0f; bool fin = true;
        for (int blk = 0; blk < 40; ++blk)
        {
            fillProgram (in, blk * 512L, rnd);
            x.makeCopyOf (in);
            p.processBlock (x, midi);
            maxGr = std::max (maxGr, p.grPeak.load());
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) fin = fin && std::isfinite (x.getSample (ch, i));
        }
        check (maxGr > 3.0f && fin, "compression shows on the GR meter data and the output stays finite");
        check (p.getLatencySamples() == 0, "reported latency is 0 samples");
    }
    // external side-chain: a quiet signal is ducked by a loud side-chain; with the bus off it is not
    {
        ApurrrProcessor p;
        juce::AudioProcessor::BusesLayout lay;
        lay.inputBuses.add (juce::AudioChannelSet::stereo()); lay.inputBuses.add (juce::AudioChannelSet::stereo());
        lay.outputBuses.add (juce::AudioChannelSet::stereo());
        const bool ok = p.setBusesLayout (lay);
        check (ok, "stereo side-chain bus can be enabled");
        if (ok)
        {
            p.prepareToPlay (48000.0, 512);
            setReal (p, "type", 1); setReal (p, "extSc", 1); setReal (p, "thr", -10.0f); setReal (p, "ratio", 5); setReal (p, "attack", 0);
            juce::AudioBuffer<float> buf (4, 512);
            juce::MidiBuffer midi;
            float gr = 0;
            for (int blk = 0; blk < 30; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = (float) std::sin (6.2831853 * 440.0 * (blk * 512 + i) / 48000.0);
                    buf.setSample (0, i, 0.02f * s); buf.setSample (1, i, 0.02f * s);        // quiet main
                    buf.setSample (2, i, 0.8f * s); buf.setSample (3, i, 0.8f * s);          // loud side-chain
                }
                p.processBlock (buf, midi);
                gr = p.grPeak.load();
            }
            check (gr > 6.0f, "Ext SC: a loud side-chain turns a quiet main signal down");
        }
    }
    // an editor can be made, resized and deleted
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (680, 308);
        ed->setSize (2040, 924);
        check (ed->getWidth() == 2040, "editor creates and resizes");
    }
    if (fixture != juce::File()) bad += fixtureTest (fixture);
    std::printf (bad == 0 ? "ALL SELF TESTS PASSED\n" : "SELF TESTS FAILED\n");
    return bad;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--write-fixture")
    {
        ApurrrProcessor p; setFixtureValues (p);
        auto state = p.apvts.copyState(); state.setProperty ("stateVersion", 1, nullptr);
        if (auto xml = state.createXml()) juce::File (argv[2]).replaceWithText (xml->toString());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--selftest")
        return selfTest (argc > 2 ? juce::File (argv[2]) : juce::File()) == 0 ? 0 : 1;
    ApurrrProcessor proc;
    proc.prepareToPlay (48000.0, 800);
    if (argc > 3) proc.setSetting (setting::look, juce::String (argv[3]));
    if (argc > 6) setReal (proc, "meter", (float) juce::String (argv[6]).getIntValue());
    // the factory preset "Drum Bus Glue", so the window shows a real preset name and moving displays
    setReal (proc, "thr", 3.0f); setReal (proc, "ratio", 3); setReal (proc, "attack", 6); setReal (proc, "release", 1);
    setReal (proc, "knee", 1); setReal (proc, "tilt", 1);
    proc.setSetting (setting::preset, "Drum Bus Glue");
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const double sc = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    ed->setSize (juce::roundToInt (1360 * sc), juce::roundToInt (616 * sc));
    const juce::String dialog = argc > 4 ? juce::String (argv[4]) : juce::String ("none");
    const int frames = argc > 5 ? juce::String (argv[5]).getIntValue() : 90;
    juce::AudioBuffer<float> buf (2, 800);
    juce::MidiBuffer midi;
    juce::Random rnd (7);
    long pos = 0;
    auto* pe = dynamic_cast<ApurrrEditor*> (ed.get());
    for (int f = 0; f < frames; ++f)
    {
        fillProgram (buf, pos, rnd); pos += 800;
        proc.processBlock (buf, midi);
        if (pe != nullptr) pe->tickForTesting();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (4);
    }
    if (dialog != "none" && pe != nullptr) pe->openDialogForTesting (dialog);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    juce::Image img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
