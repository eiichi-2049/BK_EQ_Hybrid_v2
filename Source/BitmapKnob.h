#pragma once

// 注意：JUCE 的 CMake API 不生成 Projucer 风格的 JuceHeader.h，
// 必须按需直接包含模块头文件。
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

/**
    BitmapKnob —— 位图旋钮，支持两种素材形式。

    这个类存在的意义，是保证 **旋钮的绘制尺寸与位置完全由控件的矩形决定**。
    对照 v1：HISE 的 filmstrip 控件尺寸 = `帧宽 × scaleFactor`，与 rect 无关，
    因此声明 116px 的钮只画出 84px，底图上的定位标记必然外露。

    两种模式：
      1. **filmstrip**（frames > 1）：纵向排布的 N 帧，按取值切帧。
         素材已把指针烘在帧里，切帧即得到指针朝向。
      2. **单张静态图**（frames == 1）：整张图绕中心旋转。
         适用于「指针在 12 点方向的旋钮面 + 指针」这种整体旋转的素材。

    缩放依据「内容盒」：贴图内常有透明留白，只有按可见范围缩放，
    图形才会恰好铺满 rect。实测：
      fs_pultec.png  帧 160，可见 x[9,150]  → 内容 142
      ssl_*.png      802x817，可见 800x815  → 内容 800
*/
class BitmapKnob : public juce::Component
{
public:
    /** 构造。

        @param name              控件名（仅用于调试）
        @param imageFileName     素材文件名（相对 assets 目录）
        @param numberOfFrames    filmstrip 帧数；1 表示单张静态图
        @param contentBoxPixels  内容盒边长（素材内可见部分的尺寸）
        @param minValue          取值范围下限
        @param maxValue          取值范围上限
        @param initialValue      初始值
        @param sweepDegrees      指针总摆幅（仅单张静态图模式使用）
    */
    BitmapKnob (const juce::String& name,
                const juce::String& imageFileName,
                int numberOfFrames,
                int contentBoxPixels,
                double minValue, double maxValue, double initialValue,
                double sweepDegrees = 270.0);

    ~BitmapKnob() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    double getValue() const noexcept { return value; }
    void setValue (double newValue);

    /** 打开后会在控件矩形上画出洋红细框、中心青十字与尺寸标注，
        用于肉眼核对素材是否落在底图标的圈里。 */
    void setDebugOverlay (bool shouldShow) { debugOverlay = shouldShow; repaint(); }
    void setIndexLabel (int i) { indexLabel = i; }

    std::function<void (double)> onValueChange;

private:
    juce::Image image;              // filmstrip 或单张静态图
    int   numFrames;
    int   contentBox;
    double sweepDegrees;
    int   indexLabel = -1;
    bool  debugOverlay = true;

    double value, minVal, maxVal;
    double dragStartValue = 0.0;
    int    dragStartY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitmapKnob)
};
