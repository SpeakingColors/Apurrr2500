#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/Apurrr2500.h"
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects and automation use them).
namespace ids
{
    inline constexpr const char* thr = "thr";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* gain = "gain";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* relVar = "relVar";
    inline constexpr const char* knee = "knee";
    inline constexpr const char* tilt = "tilt";
    inline constexpr const char* type = "type";
    inline constexpr const char* extSc = "extSc";
    inline constexpr const char* link = "link";
    inline constexpr const char* shape = "shape";
    inline constexpr const char* muMode = "muMode";
    inline constexpr const char* blend = "blend";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* inSw = "inSw";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* levelMatch = "levelMatch";
    inline constexpr const char* meter = "meter";
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
    inline constexpr const char* needle = "needle";   // needle speed in ms (300 slow, 50 fast)
    inline constexpr const char* ref = "ref";         // needle 0 VU at -ref dBFS (18, 16, 14)
}

class ApurrrProcessor : public juce::AudioProcessor
{
public:
    ApurrrProcessor();
    ~ApurrrProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Apurrr2500"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter data for the editor (written on the audio thread, read by the editor timer).
    std::atomic<float> grL { 0.0f }, grR { 0.0f };          // gain reduction now, positive dB
    std::atomic<float> grPeakL { 0.0f }, grPeakR { 0.0f };  // largest reduction since the editor last read it (exchange to 0)
    std::atomic<float> grPeak { 0.0f };
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };   // linear peaks since the editor last read them (exchange to 0)
    std::atomic<float> inMs[2] { { 1.0e-9f }, { 1.0e-9f } };   // needle mean squares
    std::atomic<float> outMs[2] { { 1.0e-9f }, { 1.0e-9f } };
    std::atomic<float> matchDb { 0.0f };                    // Level Match gain
    std::atomic<double> sampleRateRt { 48000.0 };

    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

    apurr::Params readParams() const;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    apurr::Compressor core;
    juce::AudioBuffer<float> dryCopy;
    float bypassMix = 0.0f; // 0 processed, 1 dry (ramped over about 10 ms)
    int maxChunk = 512;
    bool lastLevelMatch = false;
    std::atomic<int> needleRt { 300 };
    int needleApplied = -1;
    std::atomic<float>* pThr; std::atomic<float>* pRatio; std::atomic<float>* pGain; std::atomic<float>* pAttack;
    std::atomic<float>* pRelease; std::atomic<float>* pRelVar; std::atomic<float>* pKnee; std::atomic<float>* pTilt;
    std::atomic<float>* pType; std::atomic<float>* pExtSc; std::atomic<float>* pLink; std::atomic<float>* pShape;
    std::atomic<float>* pMuMode; std::atomic<float>* pBlend; std::atomic<float>* pMix; std::atomic<float>* pInSw;
    std::atomic<float>* pBypass; std::atomic<float>* pLevelMatch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApurrrProcessor)
};
