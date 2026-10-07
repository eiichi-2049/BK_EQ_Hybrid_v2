#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

/**
    BK_EQ_Hybrid v2 · POC 骨架

    本阶段只做一件事：**验证 JUCE 的位图 UI 能否精确对齐底图**。
    DSP 仅有一个增益参数，用于确认音频通路真的在工作。

    参数：
      knob0  Pultec BOOST   (0 … 24)
      knob1  Pultec ATTEN.  (0 … 24)
      knob2  Pultec 20/30/60/100 频选 (0 … 3)
      knob3  SSL dB         (−24 … 24)
      knob4  SSL HMF dB     (−24 … 24)
      knob5  SSL LMF dB     (−24 … 24)
      knob6  SSL LF dB      (−24 … 24)
      outGain 输出增益 (−24 … 12 dB)，用于验证音频通路
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

    /** 参数 ID 常量，避免散落的字符串字面量（v1 的教训之一）。 */
    static constexpr const char* kKnob0   = "knob0_pultecBoost";
    static constexpr const char* kKnob1   = "knob1_pultecAtten";
    static constexpr const char* kKnob2   = "knob2_pultecLfSel";
    static constexpr const char* kKnob3   = "knob3_sslDb";
    static constexpr const char* kKnob4   = "knob4_sslHmfDb";
    static constexpr const char* kKnob5   = "knob5_sslLmfDb";
    static constexpr const char* kKnob6   = "knob6_sslLfDb";
    static constexpr const char* kOutGain = "outGain";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioParameterFloat* outGainParam = nullptr;
    juce::LinearSmoothedValue<float> smoothedGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BK_EQ_HybridAudioProcessor)
};
