#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

/**
    BK_EQ_Hybrid v2 · 骨架阶段

    参数已按最终面板布局定义完整（Pultec 8 钮 + SSL 10 钮 + PARALLEL + 输出增益），
    但 DSP 尚未接入：processBlock 目前只做输出增益，用于确认音频通路。

    参数 ID 集中在这里，避免 v1 那种散落的 setAttribute(band*5+param)
    数字契约——那份契约曾导致频段错位、两个钮写同一参数而无人察觉。
*/
class BK_EQ_HybridAudioProcessor : public juce::AudioProcessor
{
public:
    BK_EQ_HybridAudioProcessor();
    ~BK_EQ_HybridAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BK_EQ_Hybrid_v2"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ------------------------------------------------------------- 参数 ID
    // 左 Pultec（8 钮，三行）
    static constexpr const char* kPultecBoost    = "pultecBoost";
    static constexpr const char* kPultecBw       = "pultecBw";
    static constexpr const char* kPultecHfSel    = "pultecHfSel";
    static constexpr const char* kPultecAtten    = "pultecAtten";
    static constexpr const char* kPultecAttenSel = "pultecAttenSel";
    static constexpr const char* kPultecAtten2   = "pultecAtten2";
    static constexpr const char* kPultecBoost2   = "pultecBoost2";
    static constexpr const char* kPultecLfSel    = "pultecLfSel";

    // 右 SSL（10 钮，四组）
    static constexpr const char* kSslHfDb  = "sslHfDb";
    static constexpr const char* kSslHfHz  = "sslHfHz";
    static constexpr const char* kSslHmfDb = "sslHmfDb";
    static constexpr const char* kSslHmfQ  = "sslHmfQ";
    static constexpr const char* kSslHmfHz = "sslHmfHz";
    static constexpr const char* kSslLmfDb = "sslLmfDb";
    static constexpr const char* kSslLmfQ  = "sslLmfQ";
    static constexpr const char* kSslLmfHz = "sslLmfHz";
    static constexpr const char* kSslLfDb  = "sslLfDb";
    static constexpr const char* kSslLfHz  = "sslLfHz";

    // 全局
    static constexpr const char* kParallel = "parallel";   // 0 = 左/Pultec，1 = 右/SSL
    static constexpr const char* kOutGain  = "outGain";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioParameterFloat* outGainParam = nullptr;
    juce::LinearSmoothedValue<float> smoothedGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BK_EQ_HybridAudioProcessor)
};
