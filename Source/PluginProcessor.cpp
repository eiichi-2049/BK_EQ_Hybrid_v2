#include "PluginProcessor.h"
#include "PluginEditor.h"

BK_EQ_HybridAudioProcessor::BK_EQ_HybridAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    outGainParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (kOutGain));
}

BK_EQ_HybridAudioProcessor::~BK_EQ_HybridAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
BK_EQ_HybridAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 参数集中定义：ID、显示名、范围、默认值都在一处，
    // 避免 v1 那种「UI 里写 setAttribute(band*5+param) 数字契约」的错位风险。
    auto db  = [] { return juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f); };
    auto amt = [] { return juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f); };
    auto q   = [] { return juce::NormalisableRange<float> (0.1f, 5.0f, 0.01f); };

    auto add = [&layout] (const char* id, const char* name,
                          juce::NormalisableRange<float> range, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def));
    };

    // ---------------------------------------------------------------- Pultec
    add (kPultecBoost,    "Pultec LF Boost",     amt(),                    0.0f);
    add (kPultecBw,       "Pultec LF Bandwidth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.005f), 0.5f);
    add (kPultecHfSel,    "Pultec HF Freq",      juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 0.0f);
    add (kPultecAtten,    "Pultec HF Atten",     amt(),                    0.0f);
    add (kPultecAttenSel, "Pultec HF Atten Freq",juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 0.0f);
    add (kPultecAtten2,   "Pultec LF Atten",     amt(),                    0.0f);
    add (kPultecBoost2,   "Pultec LF Boost 2",   amt(),                    0.0f);
    add (kPultecLfSel,    "Pultec LF Freq Sel",  juce::NormalisableRange<float> (0.0f, 3.0f, 1.0f), 0.0f);

    // ------------------------------------------------------------------- SSL
    add (kSslHfDb,  "SSL HF dB",   db(),                       0.0f);
    add (kSslHfHz,  "SSL HF Hz",   juce::NormalisableRange<float> (1500.0f, 16000.0f, 5.0f),  8000.0f);
    add (kSslHmfDb, "SSL HMF dB",  db(),                       0.0f);
    add (kSslHmfQ,  "SSL HMF Q",   q(),                        1.0f);
    add (kSslHmfHz, "SSL HMF Hz",  juce::NormalisableRange<float> (600.0f, 8000.0f, 5.0f),   2000.0f);
    add (kSslLmfDb, "SSL LMF dB",  db(),                       0.0f);
    add (kSslLmfQ,  "SSL LMF Q",   q(),                        1.0f);
    add (kSslLmfHz, "SSL LMF Hz",  juce::NormalisableRange<float> (200.0f, 3000.0f, 5.0f),    800.0f);
    add (kSslLfDb,  "SSL LF dB",   db(),                       0.0f);
    add (kSslLfHz,  "SSL LF Hz",   juce::NormalisableRange<float> (30.0f, 450.0f, 2.0f),      100.0f);

    // ------------------------------------------------------------------ 全局
    // 0 = 左侧 Pultec，1 = 右侧 SSL（用户明确要求的方向）
    add (kParallel, "Parallel",    juce::NormalisableRange<float> (0.0f, 1.0f, 0.005f), 0.5f);
    add (kOutGain,  "Output Gain", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f);

    return layout;
}

void BK_EQ_HybridAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    smoothedGain.reset (sampleRate, 0.02);
    smoothedGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outGainParam->get()));
}

void BK_EQ_HybridAudioProcessor::releaseResources() {}

bool BK_EQ_HybridAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 只接受立体声进出（POC 阶段；v2 正式版再考虑单声道/多通道）
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::disabled());
}

void BK_EQ_HybridAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // POC：只做输出增益，用来确认音频通路与参数自动化确实生效。
    // v2 正式版在此接入 Pultec / SSL 双链与 PARALLEL 交叉。
    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (outGainParam->get()));

    // 对整块做线性斜坡增益：从当前值平滑推进到目标值，避免参数跳变爆音。
    // 用 applyGainRamp 而非逐样本循环，既正确也更快。
    const int numSamples = buffer.getNumSamples();
    const auto target  = (float) smoothedGain.getTargetValue();
    const auto current = (float) smoothedGain.getCurrentValue();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.applyGainRamp (ch, 0, numSamples, current, target);

    smoothedGain.skip (numSamples);
}

juce::AudioProcessorEditor* BK_EQ_HybridAudioProcessor::createEditor()
{
    return new BK_EQ_HybridAudioProcessorEditor (*this);
}

void BK_EQ_HybridAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BK_EQ_HybridAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BK_EQ_HybridAudioProcessor();
}
