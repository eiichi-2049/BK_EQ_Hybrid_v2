#include "BitmapKnob.h"
#include "AssetLoader.h"

namespace
{
    /** 纵向拖动的灵敏度：走完整个量程需要的像素数。 */
    constexpr double kPixelsForFullRange = 180.0;

    /** 单张静态图模式的默认摆幅：覆盖旋钮面上刻度所占的角度。 */
    constexpr double kDefaultSweep = 270.0;
}

BitmapKnob::BitmapKnob (const juce::String& name,
                        const juce::String& imageFileName,
                        int numberOfFrames,
                        int contentBoxInPixels,
                        double minValue, double maxValue, double initialValue,
                        double sweep)
    : numFrames (juce::jmax (1, numberOfFrames)),
      contentBox (juce::jmax (1, contentBoxInPixels)),
      sweepDegrees (sweep),
      value (initialValue),
      minVal (minValue),
      maxVal (maxValue)
{
    setName (name);

    // 统一走 AssetLoader：它会按编译期宏、可执行文件同级、逐级向上三种方式找素材
    image = AssetLoader::loadImage (imageFileName);

    if (! image.isValid())
        DBG ("BitmapKnob：素材载入失败 " + imageFileName);

    // 位图旋钮不需要键盘焦点，但需要接收鼠标拖动
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setBufferedToImage (true);
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

    if (image.isValid())
    {
        // 归一化取值。范围为零时退化为 0，避免除零。
        const double span = (maxVal - minVal);
        const double proportion = (span > 0.0)
                                    ? juce::jlimit (0.0, 1.0, (value - minVal) / span)
                                    : 0.0;

        const bool isFilmstrip = (numFrames > 1);

        // --- 取待绘制的位图与旋转角 -----------------------------------------
        juce::Image  slice;
        juce::Point<float> pivot;      // 旋转轴心（位图坐标系）
        float angleDegrees = 0.0f;

        if (isFilmstrip)
        {
            const int frameH = image.getHeight() / numFrames;
            const int idx = juce::jlimit (0, numFrames - 1,
                                          (int) std::floor (proportion * (double) (numFrames - 1)));
            slice = image.getClippedImage ({ 0, idx * frameH, image.getWidth(), frameH });
            pivot = { slice.getWidth() * 0.5f, slice.getHeight() * 0.5f };
            // filmstrip 素材已把指针烘在各帧里，无需额外旋转
            angleDegrees = 0.0f;
        }
        else
        {
            slice = image;
            pivot = { slice.getWidth() * 0.5f, slice.getHeight() * 0.5f };
            // 单张静态图：整图绕中心旋转。
            // 素材的指针指向 12 点方向，故以 0° 为基准向两侧摆开。
            angleDegrees = (float) ((proportion - 0.5) * sweepDegrees);
        }

        // --- 尺寸由控件 rect 决定 --------------------------------------------
        // 把「内容盒」铺满 rect，而不是让贴图尺寸或某个缩放系数说了算。
        const float scale = juce::jmin (bounds.getWidth(), bounds.getHeight()) / (float) contentBox;

        juce::Graphics::ScopedSaveState saved (g);

        // 一次写清「把位图哪一点放到控件中心、并绕该点旋转」
        const auto transform =
            juce::AffineTransform::translation (-pivot.x, -pivot.y)
                .scaled (scale)
                .rotated (juce::degreesToRadians (angleDegrees))
                .translated (bounds.getCentreX(), bounds.getCentreY());

        g.drawImageTransformed (slice, transform);
    }

    // --- 对齐核对用的辅助标记 ------------------------------------------------
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
    // 纵向拖动：向上增加
    const double deltaY = (double) (dragStartY - e.getPosition().y);
    const double range  = maxVal - minVal;

    setValue (dragStartValue + (deltaY / kPixelsForFullRange) * range);
}

void BitmapKnob::mouseUp (const juce::MouseEvent&)
{
    // 后续可在此加入双击复位、右键菜单等
}
