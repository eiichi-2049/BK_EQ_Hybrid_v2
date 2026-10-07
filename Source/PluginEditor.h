#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BitmapKnob.h"

/**
    编辑器：底图 + 4 个位图旋钮。

    屏幕上的所有坐标都在 **1280×720 的底图坐标系**里定义，
    绘制时统一映射到当前窗口尺寸。因此改变窗口大小（或日后接缩放手势）
    时，旋钮与底图的相对位置恒定不变 —— 这正是 v1 里靠手写坐标乘法
    重排每个控件所难以维持的部分。
*/
class BK_EQ_HybridAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BK_EQ_HybridAudioProcessorEditor (BK_EQ_HybridAudioProcessor&);
    ~BK_EQ_HybridAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** 在底图上再叠一层放大的对齐辅助线，便于核对像素级位置。 */
    void setZoomedInspection (bool shouldZoom);

private:
    // BitmapKnob 是 Component 而非 Slider，故用 ParameterAttachment 桥接参数
    using Attachment = juce::ParameterAttachment;

    void buildKnobs();
    void layOutKnobs();

    BK_EQ_HybridAudioProcessor& processor;

    juce::Image background;                                  // 底图（1280×720）
    juce::Rectangle<int> backgroundArea;                     // 底图当前占据的屏幕矩形

    juce::OwnedArray<BitmapKnob> knobs;
    std::vector<std::unique_ptr<Attachment>> attachments;

    bool zoomedInspection = false;

    static constexpr int kDesignWidth  = 1280;
    static constexpr int kDesignHeight = 720;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BK_EQ_HybridAudioProcessorEditor)
};
