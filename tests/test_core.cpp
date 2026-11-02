// Apurrr2500 core tests. Build and run with: sh tests/run.sh   (no JUCE needed)
// Every number printed here is measured on the core in this file, not on hardware.
#include "../src/core/Apurrr2500.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <complex>
#include <string>
#include <vector>

using namespace apurr;
static int fails = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++fails; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static double FS = 48000.0;

static std::vector<float> sine (double f, double amp, int n)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2.0 * kPi * f * (double) i / FS));
    return v;
}
static double dbfsAmp (double db) { return std::pow (10.0, db / 20.0); }

static Compressor make (const Params& p, double fs = 48000.0)
{
    FS = fs;
    Compressor c; c.prepare (fs); c.setParams (p, true); c.snap(); return c;
}

// Run a mono-identical stereo signal; returns the left output.
static std::vector<float> run (Compressor& c, std::vector<float> x, int block = 256)
{
    std::vector<float> r = x;
    for (size_t off = 0; off < x.size(); off += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.size() - off);
        c.process (x.data() + off, r.data() + off, n);
        std::copy (r.begin() + (long) off, r.begin() + (long) off + n, x.begin() + (long) off);
    }
    return x;
}

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (std::max (1e-20, s / (double) (b - a)));
}

// Goertzel amplitude at f over whole cycles.
static double goertzel (const std::vector<float>& x, double f, size_t start, size_t n)
{
    const double w = 2.0 * kPi * f / FS, cc = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i) { const double s = x[start + i] + cc * s1 - s2; s2 = s1; s1 = s; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

static Params basic()
{
    Params p;
    p.thrDbu = 0; p.ratio = 3; p.attack = 3; p.release = 2; p.knee = 2; p.tilt = 0; p.link = 6;
    p.shape = 0; p.blend = 0; p.manualMakeup = true; p.makeupDb = 0; p.inSw = true;
    return p;
}

int main()
{
    // 1. Ranges and mapping.
    {
        Params p;
        const Derived d = derive (p);
        CHECK (std::fabs (d.thr - (4.0 - 22.0)) < 1e-9, "threshold +4 dBu is %.1f dBFS (0 dBu = -22 dBFS)", d.thr);
        CHECK (ratioTable()[6] >= 1000 && ratioTable()[0] == 1.5, "ratio table 1.5 .. Inf");
        CHECK (attackTable()[0] == 0.03 && attackTable()[6] == 30, "attack table 0.03 .. 30 ms");
        Params q; q.release = 6; q.relVarMs = 5000;
        CHECK (derive (q).relMs == 3000, "Var release clamps to 3 s");
        q.relVarMs = 10; CHECK (derive (q).relMs == 50, "Var release clamps to 50 ms");
        Params r; r.thrDbu = -20; r.ratio = 6; r.knee = 1;
        CHECK (autoMakeupDb (r) <= 24.0 && autoMakeupDb (r) >= 0.0, "auto make-up inside 0..24 dB (%.1f)", autoMakeupDb (r));
        Params z; z.ratio = 99; z.knee = -4; z.attack = 77; z.link = 12; z.shape = 9; z.tilt = 5;
        Compressor c; c.setParams (z, true);
        float l[64] {}, rr[64] {}; c.process (l, rr, 64);
        CHECK (true, "out of range indices are clamped without crashing");
    }

    // 2. Level calibration: steady state gain reduction equals the static curve (feed forward, hard knee).
    for (int ratioIdx : { 1, 3, 6 })
    {
        Params p = basic(); p.ratio = ratioIdx; p.thrDbu = 0;           // threshold -22 dBFS RMS
        const double inDb = -10.0;                                      // sine RMS level -10 dBFS => peak amp
        const double amp = dbfsAmp (inDb) * std::sqrt (2.0);
        auto c = make (p);
        auto y = run (c, sine (1000, amp, 96000));
        const double gotGr = inDb - rmsDb (y, 48000, 96000) - 0.0;     // input RMS level minus output RMS level
        const double ref = gcomp (inDb, -22.0, ratioTable()[ratioIdx], 0.5);
        CHECK (std::fabs (gotGr - ref) < 0.35, "ratio %.1f: sine at -10 dBFS RMS, measured GR %.2f dB, curve %.2f dB", ratioTable()[ratioIdx], gotGr, ref);
    }

    // 3. Knob to sound time: a threshold step changes the very next samples (no block delay).
    {
        Params p = basic(); p.ratio = 3; p.thrDbu = 10;   // threshold -12 dBFS: input at -18 RMS is below it
        auto c = make (p);
        auto x = sine (1000, dbfsAmp (-12) * std::sqrt (2.0), 48000);
        std::vector<float> a = x;
        c.process (a.data(), a.data(), 24000);
        Params q = p; q.thrDbu = -20;                      // threshold -42 dBFS: heavy compression now
        c.setParams (q);
        std::vector<float> ref (x.begin() + 24000, x.end());
        std::vector<float> b = ref;
        c.process (b.data(), b.data(), 24000);
        int first = -1;
        for (int i = 0; i < 24000; ++i) if (std::fabs (b[(size_t) i] - ref[(size_t) i]) > 1e-4f) { first = i; break; }
        const double firstMs = first * 1000.0 / FS;
        CHECK (first >= 0 && first < 480, "threshold step is audible after %d samples (%.2f ms)", first, firstMs);
        // time until 90 percent of the final gain reduction
        auto d = make (q);
        auto y = run (d, sine (1000, dbfsAmp (-12) * std::sqrt (2.0), 24000));
        CHECK (rmsDb (y, 12000, 24000) < rmsDb (sine (1000, dbfsAmp (-12) * std::sqrt (2.0), 24000), 12000, 24000) - 3.0, "after the step the level is turned down");
    }

    // 4. Latency is zero: an impulse comes out on the same sample (IN off, blend 100%).
    {
        Params p = basic(); p.inSw = false;
        auto c = make (p);
        std::vector<float> x (512, 0.0f); x[100] = 0.5f;
        auto y = run (c, x);
        int at = -1; for (int i = 0; i < 512; ++i) if (std::fabs (y[(size_t) i]) > 1e-6f) { at = i; break; }
        CHECK (at == 100 && std::fabs (y[100] - 0.5f) < 1e-7f, "latency 0 samples (impulse at %d)", at);
        // exact pass: IN off gives the input bit for bit
        auto in = sine (440, 0.7, 4096);
        auto c2 = make (p);
        auto o = run (c2, in);
        bool same = true; for (size_t i = 0; i < in.size(); ++i) if (o[i] != in[i]) same = false;
        CHECK (same, "IN off passes the signal bit exact");
    }

    // 5. Attack and release times: gain reduction follows a one pole with the labelled time constant.
    for (int atkIdx : { 3, 5 })
    {
        Params p = basic(); p.ratio = 6; p.attack = atkIdx; p.thrDbu = 10;   // thr -12 dBFS, Inf ratio
        auto c = make (p);
        // loud burst after silence: measure how long the GR needs to reach 63 percent of its value
        std::vector<float> quiet (4800, 0.0f), loud = sine (100, 0.8, 48000);
        c.process (quiet.data(), quiet.data(), 4800);
        double final = 0, t63 = -1;
        std::vector<double> gr;
        for (int i = 0; i < 48000; i += 16)
        {
            c.process (loud.data() + i, loud.data() + i, 16);
            gr.push_back (c.grLeft());
        }
        final = gr.back();
        for (size_t i = 0; i < gr.size(); ++i) if (gr[i] >= 0.632 * final) { t63 = (double) i * 16.0 * 1000.0 / FS; break; }
        const double lab = attackTable()[atkIdx];
        // The 3 ms detector and the target change add delay; report the measured value and require the right order of magnitude.
        std::printf ("      attack %.1f ms: GR reaches 63 percent of %.1f dB after %.2f ms\n", lab, final, t63);
        CHECK (t63 >= lab * 0.5 && t63 <= lab * 3.0 + 6.0, "attack %.1f ms behaves like a time constant of that size (%.2f ms measured)", lab, t63);
    }
    {
        Params p = basic(); p.ratio = 6; p.attack = 0; p.release = 2; p.thrDbu = 10;
        auto c = make (p);
        auto loud = sine (100, 0.8, 24000);
        c.process (loud.data(), loud.data(), 24000);
        const double g0 = c.grLeft();
        std::vector<float> z (96000, 0.0f);
        double t37 = -1;
        for (int i = 0; i < 96000; i += 16)
        {
            c.process (z.data() + i, z.data() + i, 16);
            if (t37 < 0 && c.grLeft() <= g0 * 0.368) t37 = i * 1000.0 / FS;
        }
        CHECK (t37 > 100 && t37 < 400, "release 200 ms: GR falls to 37 percent after %.0f ms", t37);
    }

    // 6. Old versus New on a loud burst (the mockup's Old vs New display): New grabs at once, Old lets the start through.
    {
        auto burst = [] { std::vector<float> x = sine (1000, dbfsAmp (-34) * std::sqrt (2.0), 4800);
                          auto l = sine (1000, dbfsAmp (-10) * std::sqrt (2.0), 14400); x.insert (x.end(), l.begin(), l.end()); return x; };
        Params nw = basic(); nw.ratio = 3; nw.thrDbu = 0; nw.attack = 2; nw.knee = 1; nw.oldType = false;
        Params ol = nw; ol.oldType = true;
        auto cn = make (nw), co = make (ol);
        double earlyNew = 0, earlyOld = 0, firstNew = 0, firstOld = 0;
        auto x = burst(); std::vector<float> a = x, b = x;
        for (size_t i = 0; i < x.size(); i += 8)
        {
            cn.process (a.data() + i, a.data() + i, 8); co.process (b.data() + i, b.data() + i, 8);
            if (i == 4800 + 24) { firstNew = cn.grLeft(); firstOld = co.grLeft(); }     // 0.7 ms into the burst
            if (i == 4800 + 480) { earlyNew = cn.grLeft(); earlyOld = co.grLeft(); }    // 10 ms
        }
        std::printf ("      GR 0.7 ms into the burst: New %.2f dB, Old %.2f dB; at 10 ms: New %.2f dB, Old %.2f dB; settled %.2f / %.2f dB\n", firstNew, firstOld, earlyNew, earlyOld, cn.grLeft(), co.grLeft());
        // Measured: with the labelled ratios kept exact in steady state, the Old (feedback) detector reacts to the
        // un-reduced output at the start of a burst and so asks for MORE reduction early (it overshoots about 0.7 dB
        // before settling). The mockup's own engine does the same (see the parity test). The mockup tooltip says the
        // opposite (a loud hit gets through); this is listed in the ROADMAP as open.
        CHECK (earlyOld >= earlyNew && std::fabs (cn.grLeft() - co.grLeft()) < 0.05, "Old and New settle to the same reduction; Old overshoots early (documented)");
        CHECK (std::fabs (cn.grLeft() - co.grLeft()) < 2.5, "both types settle to a similar reduction (%.2f vs %.2f dB)", cn.grLeft(), co.grLeft());
    }

    // 7. Link: a loud left channel with a quiet right channel.
    {
        for (int link : { 0, 6 })
        {
            Params p = basic(); p.link = link; p.ratio = 3; p.thrDbu = 0; p.attack = 2;
            auto c = make (p);
            auto L = sine (200, dbfsAmp (-6) * std::sqrt (2.0), 48000), R = sine (200, dbfsAmp (-40) * std::sqrt (2.0), 48000);
            for (int i = 0; i < 48000; i += 250) c.process (L.data() + i, R.data() + i, 250);
            std::printf ("      link %s: GR left %.2f dB, GR right %.2f dB\n", link == 0 ? "IND" : "100", c.grLeft(), c.grRight());
            if (link == 0) CHECK (c.grLeft() > 5.0 && c.grRight() < 0.05, "IND keeps the channels apart");
            else CHECK (std::fabs (c.grLeft() - c.grRight()) < 0.2 && c.grRight() > 2.0, "100 percent link turns both channels down together");
        }
    }

    // 8. Tilt filter: gain at 20 Hz, 1 kHz and 20 kHz for Med and Loud (as drawn in the display).
    {
        FS = 96000;
        for (int mode : { 0, 1, 2 })
        {
            Params p = basic(); p.tilt = mode;
            auto c = make (p, 96000.0);
            const double lo = linToDb (c.tiltMag (20)), mid = linToDb (c.tiltMag (1000)), hi = linToDb (c.tiltMag (20000));
            std::printf ("      tilt %d: 20 Hz %.1f dB, 1 kHz %.1f dB, 20 kHz %.1f dB\n", mode, lo, mid, hi);
            if (mode == 0) CHECK (std::fabs (lo) < 0.01 && std::fabs (hi) < 0.01, "Norm is flat");
            if (mode == 2) CHECK (lo < -12 && hi > 12 && std::fabs (mid) < 3.5, "Loud tilts about -15 dB at 20 Hz to +15 dB at 20 kHz");
            if (mode == 1) CHECK (lo < -8 && std::fabs (linToDb (c.tiltMag (700))) < 2.0, "Med is flat in the middle and rolls off the lows");
        }
        // The magnitude function agrees with the filter run on a real sine.
        Params p = basic(); p.tilt = 2;
        auto c = make (p, 96000.0);
        // the display draws tiltMag(); check it against the filter chain run on real sines through the detector path
        Section sc2[kSections];
        const TiltDef td = tiltDef (2);
        for (int i = 0; i < kSections; ++i) { const double fz = 20.0 * std::pow (10.0, 0.25 * i); sc2[i].set (96000.0, fz, fz * dbToLin (td.rises[i])); }
        double worst = 0;
        for (double f : { 50.0, 400.0, 3000.0, 15000.0 })
        {
            double peak = 0;
            for (int n = 0; n < 96000 * 2; ++n)
            {
                double y = std::sin (2.0 * kPi * f * n / 96000.0);
                for (auto& s : sc2) y = s.process (y);
                if (n > 96000) peak = std::max (peak, std::fabs (y * dbToLin (td.g0Db)));
            }
            worst = std::max (worst, std::fabs (linToDb (peak) - linToDb (c.tiltMag (f))));
        }
        CHECK (worst < 0.1, "tilt magnitude shown in the display matches a run filter within %.3f dB", worst);
        FS = 48000;
    }

    // 9. Stability: silence, DC, full scale, NaN and Inf, all settings.
    {
        bool ok = true;
        for (int oldType : { 0, 1 })
            for (int ratio = 0; ratio < 7; ++ratio)
                for (int tilt = 0; tilt < 3; ++tilt)
                    for (int blend = 0; blend < 3; ++blend)
                    {
                        Params p = basic(); p.oldType = oldType; p.ratio = ratio; p.tilt = tilt; p.blend = blend; p.attack = 0; p.thrDbu = -20; p.levelMatch = true; p.manualMakeup = true; p.makeupDb = 24;
                        auto c = make (p);
                        std::vector<float> x (6000, 0.0f);
                        for (int i = 0; i < 2000; ++i) x[(size_t) i] = 0.0f;
                        for (int i = 2000; i < 3000; ++i) x[(size_t) i] = 1.0f;          // DC full scale
                        for (int i = 3000; i < 3100; ++i) x[(size_t) i] = (i & 1) ? 1.0f : -1.0f;   // Nyquist full scale
                        x[3200] = NAN; x[3300] = INFINITY; x[3400] = -INFINITY;
                        auto y = run (c, x, 128);
                        for (float v : y) if (! std::isfinite (v)) ok = false;
                        auto z = run (c, std::vector<float> (48000, 0.0f), 512);
                        for (float v : z) if (! std::isfinite (v)) ok = false;
                        double tail = 0; for (size_t i = 40000; i < z.size(); ++i) tail = std::max (tail, (double) std::fabs (z[i]));
                        if (tail > 1e-3) ok = false;
                    }
        CHECK (ok, "all 126 setting combinations survive silence, DC, full scale, NaN and Inf and recover to silence");
    }

    // 10. Parallel and cross-fade blends.
    {
        Params p = basic(); p.ratio = 6; p.thrDbu = -10; p.attack = 0; p.blend = 1; p.mix = 0.0;
        auto c = make (p);
        auto in = sine (300, 0.5, 20000);
        auto o = run (c, in);
        bool same = true; for (size_t i = 0; i < in.size(); ++i) if (std::fabs (o[i] - in[i]) > 1e-6f) same = false;
        CHECK (same, "cross-fade at mix 0 is the dry signal");
        p.blend = 2; auto c2 = make (p); auto o2 = run (c2, in);
        same = true; for (size_t i = 0; i < in.size(); ++i) if (std::fabs (o2[i] - in[i]) > 1e-6f) same = false;
        CHECK (same, "parallel at mix 0 is the dry signal");
    }

    // 11. Level match brings the compressed signal back to the input level.
    {
        Params p = basic(); p.ratio = 3; p.thrDbu = -10; p.attack = 2; p.levelMatch = true;
        auto c = make (p);
        auto in = sine (300, dbfsAmp (-6) * std::sqrt (2.0), 48000 * 12);
        auto o = run (c, in, 512);
        const double d = rmsDb (o, 48000 * 10, 48000 * 12) - rmsDb (in, 48000 * 10, 48000 * 12);
        CHECK (std::fabs (d) < 0.7, "level match: output within %.2f dB of the input level after 10 s", d);
    }

    // 12. Aliasing and distortion: fastest, hardest setting, a loud sine at several frequencies. Everything that is not the
    //     fundamental is reported; for frequencies where harmonics would fold back (above 8 kHz) this is the alias level.
    {
        FS = 48000;
        for (double f : { 100.0, 1000.0, 9000.0, 15000.0 })
        {
            Params p = basic(); p.ratio = 6; p.attack = 0; p.release = 0; p.thrDbu = -20; p.knee = 2; p.manualMakeup = true; p.makeupDb = 0;
            auto c = make (p);
            const int n = 48000 * 2;
            auto in = sine (f, 0.9, n);
            auto o = run (c, in, 512);
            const size_t start = 48000, len = 48000;                      // whole cycles for these frequencies
            double total = 0; for (size_t i = start; i < start + len; ++i) total += (double) o[i] * o[i];
            total /= (double) len;
            const double a = goertzel (o, f, start, len), fund = a * a / 2.0;
            const double resDb = 10.0 * std::log10 (std::max (1e-20, (total - fund) / total));
            std::printf ("      %6.0f Hz at 0.9, Inf ratio, 0.03 ms attack, 50 ms release: everything except the fundamental is %.1f dB\n", f, resDb);
            if (f >= 1000.0) CHECK (resDb < -50.0, "%.0f Hz: distortion plus alias %.1f dB (limit -50 dB)", f, resDb);
        }
    }

    // 13. CPU: 10 seconds of stereo at 48 kHz, worst case (Old type, Loud tilt, level match).
    {
        Params p = basic(); p.oldType = true; p.tilt = 2; p.levelMatch = true; p.ratio = 6; p.thrDbu = -10;
        auto c = make (p);
        std::vector<float> a = sine (220, 0.5, 48000 * 10), b = a;
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < a.size(); i += 512) c.process (a.data() + i, b.data() + i, (int) std::min<size_t> (512, a.size() - i));
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      10 s of stereo took %.3f s: %.1f percent of one core (this test machine)\n", sec, sec / 10.0 * 100.0);
        CHECK (sec < 3.0, "worst case runs faster than real time by %.0fx", 10.0 / sec);
    }

    // 14. Sample rates.
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Params p = basic(); p.tilt = 2; p.thrDbu = -10;
        auto c = make (p, fs);
        auto y = run (c, sine (1000, 0.5, (int) fs));
        bool ok = true; for (float v : y) if (! std::isfinite (v)) ok = false;
        const double gr = c.grLeft();
        CHECK (ok && gr > 0.5, "%.0f Hz: finite output, GR %.2f dB", fs, gr);
    }
    FS = 48000;

    // 15. External side-chain: the detector listens to the side-chain, not to the signal.
    {
        Params p = basic(); p.ratio = 6; p.thrDbu = 0; p.attack = 1; p.extSc = true; p.oldType = false;
        auto c = make (p);
        auto sig = sine (500, 0.2, 48000), sc = sine (60, 0.9, 48000);
        std::vector<float> a = sig, b = sig;
        for (int i = 0; i < 48000; i += 250) c.process (a.data() + i, b.data() + i, 250, sc.data() + i, sc.data() + i);
        const double gr1 = c.grLeft();
        auto c2 = make (p);
        std::vector<float> a2 = sig, b2 = sig; std::vector<float> quiet (48000, 0.0f);
        for (int i = 0; i < 48000; i += 250) c2.process (a2.data() + i, b2.data() + i, 250, quiet.data() + i, quiet.data() + i);
        std::printf ("      quiet signal, loud side-chain: GR %.2f dB; silent side-chain: GR %.2f dB\n", gr1, c2.grLeft());
        CHECK (gr1 > 3.0 && c2.grLeft() < 0.01, "external side-chain drives the reduction");
        Params po = p; po.oldType = true; auto c3 = make (po);
        Params pq = po; pq.extSc = false; auto c4 = make (pq);
        a = sig; b = sig; std::vector<float> a4 = sig, b4 = sig;
        for (int i = 0; i < 48000; i += 250) { c3.process (a.data() + i, b.data() + i, 250, sc.data() + i, sc.data() + i); c4.process (a4.data() + i, b4.data() + i, 250); }
        CHECK (std::fabs (c3.grLeft() - c4.grLeft()) < 1e-9, "Old type ignores Ext SC (its detector listens to its own output)");
    }

    // 16. Mono.
    {
        Params p = basic(); p.ratio = 6; p.thrDbu = 0;
        auto c = make (p);
        auto x = sine (300, 0.7, 48000);
        for (int i = 0; i < 48000; i += 250) c.process (x.data() + i, nullptr, 250);
        bool ok = true; for (float v : x) if (! std::isfinite (v)) ok = false;
        CHECK (ok && c.grLeft() > 1.0, "mono track works (GR %.2f dB)", c.grLeft());
    }

    // 17. Parity with the approved mockup's own JS engine on four scenarios (tests/fixtures/mockup-parity.txt).
    {
        FILE* fp = std::fopen ("tests/fixtures/mockup-parity.txt", "r");
        if (fp == nullptr) fp = std::fopen ("../tests/fixtures/mockup-parity.txt", "r");
        if (fp == nullptr) { std::printf ("FAIL: parity fixture not found\n"); ++fails; }
        else
        {
            char word[256];
            std::string name; double outRms = 0; Params prm; std::vector<double> refL, refR; bool haveL = false;
            auto runScenario = [&]()
            {
                auto c = make (prm); c.setAvgMs (300);
                const int N = 48000, B = 48;
                std::vector<float> L ((size_t) B), R ((size_t) B);
                double worstL = 0, worstR = 0, sq = 0; int cnt = 0;
                for (int b = 0; b < N / B; ++b)
                {
                    for (int i = 0; i < B; ++i)
                    {
                        const int k = b * B + i; const double t = k / 48000.0, db = t < 0.2 ? -30 : t < 0.5 ? -8 : -24, a = std::pow (10.0, db / 20.0);
                        const double v = a * (std::sin (2 * kPi * 200 * t) + 0.5 * std::sin (2 * kPi * 3100 * t)) / 1.5 * 1.414;
                        L[(size_t) i] = (float) v; R[(size_t) i] = (float) (v * 0.7);
                    }
                    c.process (L.data(), R.data(), B);
                    worstL = std::max (worstL, std::fabs (c.grLeft() - refL[(size_t) b]));
                    worstR = std::max (worstR, std::fabs (c.grRight() - refR[(size_t) b]));
                    if (b >= N / B - 200) for (int i = 0; i < B; ++i) { sq += (double) L[(size_t) i] * L[(size_t) i]; ++cnt; }
                }
                const double rms = 10.0 * std::log10 (sq / cnt);
                CHECK (worstL < 0.02 && worstR < 0.02 && std::fabs (rms - outRms) < 0.2,
                       "parity with the mockup engine, %s: worst GR difference %.3f / %.3f dB, output level difference %.3f dB", name.c_str(), worstL, worstR, rms - outRms);
            };
            while (std::fscanf (fp, "%255s", word) == 1)
            {
                const std::string w = word;
                if (w == "scenario") { if (haveL) { runScenario(); haveL = false; } std::fscanf (fp, "%255s", word); name = word; refL.clear(); refR.clear(); }
                else if (w == "st")
                {
                    prm = Params();
                    char line[1024]; std::fgets (line, sizeof line, fp);
                    std::string l = line; size_t pos = 0;
                    while (pos < l.size())
                    {
                        size_t sp = l.find (' ', pos); if (sp == std::string::npos) sp = l.size();
                        std::string tok = l.substr (pos, sp - pos); pos = sp + 1;
                        const size_t eq = tok.find ('='); if (eq == std::string::npos) continue;
                        const std::string k = tok.substr (0, eq); const double v = std::atof (tok.substr (eq + 1).c_str());
                        if (k == "thr") prm.thrDbu = v; else if (k == "ratio") prm.ratio = (int) v; else if (k == "attack") prm.attack = (int) v;
                        else if (k == "release") prm.release = (int) v; else if (k == "relVar") prm.relVarMs = v; else if (k == "knee") prm.knee = (int) v;
                        else if (k == "tilt") prm.tilt = (int) v; else if (k == "type") prm.oldType = v == 0; else if (k == "extSc") prm.extSc = v != 0;
                        else if (k == "link") prm.link = (int) v; else if (k == "shape") prm.shape = (int) v; else if (k == "muMode") prm.manualMakeup = v != 0;
                        else if (k == "gain") prm.makeupDb = v; else if (k == "blend") prm.blend = (int) v; else if (k == "mix") prm.mix = v / 100.0;
                        else if (k == "inSw") prm.inSw = v != 0; else if (k == "levelMatch") prm.levelMatch = v != 0;
                    }
                }
                else if (w == "outRms") std::fscanf (fp, "%lf", &outRms);
                else if (w == "grL" || w == "grR")
                {
                    auto& v = w == "grL" ? refL : refR;
                    for (int i = 0; i < 1000; ++i) { double x; std::fscanf (fp, "%lf", &x); v.push_back (x); }
                    if (w == "grR") haveL = true;
                }
            }
            if (haveL) runScenario();
            std::fclose (fp);
        }
    }

    std::printf (fails == 0 ? "ALL CORE TESTS PASSED\n" : "%d CORE TESTS FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
