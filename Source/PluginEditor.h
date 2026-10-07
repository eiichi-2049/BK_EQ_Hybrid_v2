#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "BitmapKnob.h"

/**
    编辑器：底图 + 7 个位图旋钮。

    屏幕上的所有坐标都在 **1280×720 的底图坐标系**里定义，
    绘制时统一映射到窗口尺寸。因此缩放窗口时，旋钮与底图的相对位置
    恒定不变 —— 这正是 v1 里靠手写坐标乘法重排每个控件所难以维持的。

    缩放采用固定档位（60/80/100/120/140%），不做自由缩放：
    自由缩放会让贴图落在非整数像素边界上，产生模糊与对齐偏差。
*/
class BK_EQ_HybridAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BK_EQ_HybridAudioProcessorEditor (BK_EQ_HybridAudioProcessor&);
    ~BK_EQ_HybridAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** 缩放到下一个 / 上一个固定档位。 */
    void zoomIn();
    void zoomOut();

private:
    // BitmapKnob 是 Component 而非 Slider，故用 ParameterAttachment 桥接参数
    using Attachment = juce::ParameterAttachment;

    /** 固定缩放档位。窗口尺寸 = 设计尺寸 × 档位。 */
    static constexpr std::array<double, 5> kZoomLevels { 0.6, 0.8, 1.0, 1.2, 1.4 };
    static constexpr int kDefaultZoomIndex = 2;   // 100%

    void buildKnobs();
    void buildZoomButton();
    void layOutKnobs();
    void applyZoomLevel();

    BK_EQ_HybridAudioProcessor& processor;

    juce::Image background;                                  // 底图（1280×720）
    juce::Rectangle<int> backgroundArea;                     // 底图当前占据的屏幕矩形

    juce::OwnedArray<BitmapKnob> knobs;
    std::vector<std::unique_ptr<Attachment>> attachments;

    juce::TextButton zoomButton;                             // 右下角：点击循环档位
    int zoomIndex = kDefaultZoomIndex;

    /** TextButton 没有公开的 setFont，字号由 LookAndFeel 决定，故自带一个。 */
    struct ZoomButtonLookAndFeel : public juce::LookAndFeel_V4
    {
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions (
                juce::jlimit (10.0f, 18.0f, (float) buttonHeight * 0.5f)));
        }
    };
    ZoomButtonLookAndFeel zoomLookAndFeel;

    static constexpr int kDesignWidth  = 1280;
    static constexpr int kDesignHeight = 720;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BK_EQ_HybridAudioProcessorEditor)
};
