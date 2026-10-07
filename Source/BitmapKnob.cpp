#include "BitmapKnob.h"

namespace
{
    /** 旋钮的可用角度范围（与 HISE filmstrip 的约定一致：−135° → +135°）。 */
    constexpr float kMinAngle = -135.0f;
    constexpr float kMaxAngle =  135.0f;

    /** 纵向拖动的灵敏度：走完整个量程需要的像素数。 */
    constexpr double kPixelsForFullRange = 180.0;
}

BitmapKnob::BitmapKnob (const juce::String& name,
                        const juce::String& filmstripFileName,
                        int numberOfFrames,
                        int contentBoxInPixels,
                        double minValue, double maxValue, double initialValue)
    : numFrames (numberOfFrames),
      contentBox (contentBoxInPixels),
      value (initialValue),
      minVal (minValue),
      maxVal (maxValue)
{
    setName (name);

    // 素材目录由 CMake 通过 BK_ASSETS_DIR 注入（见 CMakeLists.txt）
    const auto file = juce::File (juce::String (BK_ASSETS_DIR)).getChildFile (filmstripFileName);
    filmstrip = juce::ImageFileFormat::loadFrom (file);

    if (! filmstrip.isValid())
        DBG ("BitmapKnob: 位图载入失败 -> " + file.getFullPathName());

    // 位图旋钮不需要键盘焦点，但需要接收鼠标拖动
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setBufferedToImage (true);   // 旋转绘制开销小，但缓存后可避免重绘闪烁
}

void BitmapKnob::setValue (double newValue)
{
    const double clamped = juce::jlimit (minVal, maxVal, newValue);

    if (! juce::approximatelyEqual (clamped, value))
    {
        value = clamped;
        repaint();

        if (onValueChange != nullptr)
            onValueChange (value);
    }
}

void BitmapKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    if (filmstrip.isValid() && numFrames > 0 && contentBox > 0)
    {
        // --- 归一化取值 → 帧号 -------------------------------------------------
        const double proportion = juce::jlimit (0.0, 1.0, (value - minVal) / (maxVal - minVal));
        const int frameIndex = juce::jlimit (0, numFrames - 1,
                                             (int) std::floor (proportion * (double) (numFrames - 1)));

        // --- 帧切片（纵向排布） -----------------------------------------------
        const int frameWidth  = filmstrip.getWidth();
        const int frameHeight = filmstrip.getHeight() / numFrames;
        const juce::Rectangle<int> slice (0, frameIndex * frameHeight, frameWidth, frameHeight);

        // --- 关键：尺寸由控件 rect 决定 ---------------------------------------
        // 把「内容盒」密铺到 rect，而不是让贴图尺寸或某个缩放系数说了算。
        const float scale = juce::jmin (bounds.getWidth(), bounds.getHeight()) / (float) contentBox;

        // --- 旋转角度 ----------------------------------------------------------
        const float angleDegrees = kMinAngle + (float) proportion * (kMaxAngle - kMinAngle);

        juce::Graphics::ScopedSaveState saved (g);

        // 以控件中心为轴心旋转，然后把帧的中心对齐到该轴心
        g.addTransform (juce::AffineTransform::translation (bounds.getCentreX(), bounds.getCentreY()));
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (angleDegrees)));
        g.addTransform (juce::AffineTransform::scale (scale));
        g.addTransform (juce::AffineTransform::translation (-slice.getWidth()  * 0.5f,
                                                            -slice.getHeight() * 0.5f));

        g.drawImage (filmstrip, slice.toFloat(),
                     juce::RectanglePlacement::stretchToFit, false);
    }

    // --- 对齐核对用的辅助标记（POC 阶段开启） --------------------------------
    if (debugOverlay)
    {
        g.setColour (juce::Colours::magenta.withAlpha (0.9f));
        g.drawRect (bounds, 1.0f);

        const auto centre = bounds.getCentre();
        g.setColour (juce::Colours::cyan);
        g.drawLine (centre.x - 7.0f, centre.y, centre.x + 7.0f, centre.y, 1.0f);
        g.drawLine (centre.x, centre.y - 7.0f, centre.x, centre.y + 7.0f, 1.0f);

        if (indexLabel >= 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::String (indexLabel) + "  " + juce::String ((int) bounds.getWidth()) + "px",
                        bounds.getX(), bounds.getBottom() - 12.0f, bounds.getWidth(), 12.0f,
                        juce::Justification::centred, false);
        }
    }
}

void BitmapKnob::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = value;
    dragStartY = e.getPosition().y;
}

void BitmapKnob::mouseDrag (const juce::MouseEvent& e)
{
    // 纵向拖动：向上增加。JUCE 会给出相对 mouseDown 的位移。
    const double deltaY = (double) (dragStartY - e.getPosition().y);
    const double range  = maxVal - minVal;

    setValue (dragStartValue + (deltaY / kPixelsForFullRange) * range);
}

void BitmapKnob::mouseUp (const juce::MouseEvent&)
{
    // POC 阶段无附加行为；后续可在此加入双击复位、右键菜单等
}
