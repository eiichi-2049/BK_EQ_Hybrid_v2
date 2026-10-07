#pragma once

// 注意：JUCE 的 CMake API 不生成 Projucer 风格的 JuceHeader.h，
// 必须按需直接包含模块头文件。
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

/**
    BitmapKnob —— 用位图（filmstrip）绘制的旋钮。

    这个类存在的唯一目的，是验证 JUCE 能否做到 HISE 做不到的事：
    **旋钮的绘制尺寸与位置完全由控件的矩形（rect）决定。**

    对照 v1 的问题：HISE 的 filmstrip 旋钮尺寸 = `帧宽 × scaleFactor`，
    与控件 rect 无关，因此声明 116px 的按钮只画出 84px，底图上的占位环
    必然外露，对齐无法闭环（见 v1 FROZEN.md 缺陷 1）。

    做法：
      1. 载入纵向 filmstrip（N 帧，每帧 frameSize × frameSize）
      2. 按内容盒（contentBox）把「可见部分」密铺到控件矩形 —— 尺寸由 rect 决定
      3. 用 AffineTransform 绕中心旋转帧，得到指针朝向

    内容盒的含义：贴图帧内往往有透明留白。实测 v1 素材：
      fs_pultec.png  帧 160，可见范围 x[9,150] → 内容 142
      fs_red.png     帧 160，可见范围 x[1,158] → 内容 158
    只有按内容盒缩放，可见图形才会恰好铺满 rect。
*/
class BitmapKnob : public juce::Component
{
public:
    BitmapKnob (const juce::String& name,
                const juce::String& filmstripFileName,
                int numberOfFrames,
                int contentBoxInPixels,
                double minValue, double maxValue, double initialValue);

    ~BitmapKnob() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    double getValue() const noexcept { return value; }
    void setValue (double newValue);

    /** 打开后会在控件矩形上画出：
          - 洋红细框：控件的精确 rect
          - 青色十字：rect 中心
          - 白字：序号 / 像素尺寸 / 当前值
        用于肉眼核对「素材是否落在底图标的圈里」。 */
    void setDebugOverlay (bool shouldShow) { debugOverlay = shouldShow; repaint(); }
    void setIndexLabel (int i) { indexLabel = i; }

    std::function<void (double)> onValueChange;

private:
    juce::Image filmstrip;
    int   numFrames;
    int   contentBox;
    int   indexLabel = -1;
    bool  debugOverlay = true;

    double value, minVal, maxVal;
    double dragStartValue = 0.0;
    int    dragStartY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitmapKnob)
};
