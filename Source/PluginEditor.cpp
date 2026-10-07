#include "PluginEditor.h"

namespace
{
    /** 素材目录由 CMake 通过 BK_ASSETS_DIR 注入（见 CMakeLists.txt）。
        POC 阶段从磁盘加载；正式版可改回二进制嵌入（juce_add_binary_data）。 */
    juce::File getAssetFile (const juce::String& fileName)
    {
        return juce::File (juce::String (BK_ASSETS_DIR)).getChildFile (fileName);
    }

    /** 从素材目录解码一张 PNG。失败时返回无效 Image 并留下调试信息。 */
    juce::Image loadAssetImage (const juce::String& fileName)
    {
        const auto file = getAssetFile (fileName);

        if (! file.existsAsFile())
        {
            DBG ("素材缺失：" + file.getFullPathName());
            return {};
        }

        auto image = juce::ImageFileFormat::loadFrom (file);

        if (! image.isValid())
            DBG ("素材解码失败：" + file.getFullPathName());

        return image;
    }
    /** 一个旋钮在底图坐标系里的位置与尺寸。
        中心点取自 v1 底图（bg.png）上实测的占位圆圆心；
        尺寸取占位圆直径，这样旋钮的可见部分会恰好盖住底图那圈虚线。 */
    struct KnobPlacement
    {
        const char* paramID;
        int centreX, centreY;   // 底图坐标系
        int diameter;
    };

    // 素材内容盒实测值（见 tools\selfcheck.py 与 POC 文档）：
    //   fs_pultec.png      帧 160，可见 x[9,150]  → 内容 142
    //   fs_red/green/blue/brown.png  帧 160，可见 x[1,158] → 内容 158
    // 说明：前三个直接量取。SSL 四色钮先用同一素材（fs_red）标定，
    //       待各色钮实测后再换成各自的精确值。
    constexpr int kPultecFrame = 160, kPultecContent = 142, kPultecFrames = 91;
    constexpr int kSslFrame    = 160, kSslContent    = 158, kSslFrames    = 91;

    // 底图（bg.png）上占位圆的实测圆心与直径
    const KnobPlacement placements[] =
    {
        { BK_EQ_HybridAudioProcessor::kKnob0, 123, 194,  99 },  // Pultec BOOST
        { BK_EQ_HybridAudioProcessor::kKnob1, 267, 377,  99 },  // Pultec ATTEN.
        { BK_EQ_HybridAudioProcessor::kKnob2, 399, 570,  78 },  // Pultec 20/30/60/100
        { BK_EQ_HybridAudioProcessor::kKnob3, 872, 191, 100 },  // SSL 红·dB
        { BK_EQ_HybridAudioProcessor::kKnob4, 873, 313, 100 },  // SSL 绿·HMF dB
        { BK_EQ_HybridAudioProcessor::kKnob5, 872, 433, 100 },  // SSL 蓝·LMF dB
        { BK_EQ_HybridAudioProcessor::kKnob6, 874, 566, 100 },  // SSL 棕·LF dB
    };
}

BK_EQ_HybridAudioProcessorEditor::BK_EQ_HybridAudioProcessorEditor (BK_EQ_HybridAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // 底图从素材目录加载（POC 阶段不做二进制嵌入）
    background = loadAssetImage ("bg.png");

    buildKnobs();

    setResizable (true, true);
    setResizeLimits (640, 360, 2560, 1440);
    setSize (kDesignWidth, kDesignHeight);
}

BK_EQ_HybridAudioProcessorEditor::~BK_EQ_HybridAudioProcessorEditor() = default;

void BK_EQ_HybridAudioProcessorEditor::buildKnobs()
{
    // 每个旋钮用哪张 filmstrip：前三（Pultec）用黑钮，后四（SSL）按色分。
    struct Strip { const char* fileName; int frames; int content; };
    const Strip pultec { "fs_pultec.png", kPultecFrames, kPultecContent };
    const Strip red    { "fs_red.png",    kSslFrames,    kSslContent };
    const Strip green  { "fs_green.png",  kSslFrames,    kSslContent };
    const Strip blue   { "fs_blue.png",   kSslFrames,    kSslContent };
    const Strip brown  { "fs_brown.png",  kSslFrames,    kSslContent };

    // 顺序须与 placements 一一对应
    const Strip strips[] = { pultec, pultec, pultec, red, green, blue, brown };

    for (int i = 0; i < (int) juce::numElementsInArray (placements); ++i)
    {
        const auto& pl = placements[i];
        const auto& st = strips[i];

        double minV = 0.0, maxV = 24.0;
        if (i == 2) { minV = 0.0;  maxV = 3.0; }     // 频选 4 档
        if (i >= 3) { minV = -24.0; maxV = 24.0; }   // SSL dB

        auto* knob = new BitmapKnob (pl.paramID,
                                     st.fileName,
                                     st.frames, st.content,
                                     minV, maxV, 0.0);
        knob->setIndexLabel (i);
        addAndMakeVisible (knob);
        knobs.add (knob);

        // BitmapKnob 是 juce::Component，不是 juce::Slider，
        // 因此不能用 SliderAttachment，改用 ParameterAttachment 双向桥接：
        //   旋钮 → 参数（用户拖动时上报，用整段手势通知宿主）
        //   参数 → 旋钮（宿主自动化或加载预设时刷新显示）
        auto* param = processor.apvts.getParameter (pl.paramID);
        if (param == nullptr)
        {
            DBG ("编辑器：找不到参数 " + juce::String (pl.paramID));
            continue;
        }

        knob->onValueChange = [this, param] (double v)
        {
            param->setValueNotifyingHost (param->convertTo0to1 ((float) v));
        };

        attachments.push_back (std::make_unique<juce::ParameterAttachment> (
            *param,
            [knob] (float newValue)
            {
                if (! juce::approximatelyEqual (knob->getValue(), (double) newValue))
                    knob->setValue (newValue);
            }));
    }
}

void BK_EQ_HybridAudioProcessorEditor::setZoomedInspection (bool shouldZoom)
{
    zoomedInspection = shouldZoom;
    resized();
    repaint();
}

void BK_EQ_HybridAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (background.isValid())
        g.drawImage (background, backgroundArea.toFloat(),
                     juce::RectanglePlacement::stretchToFit, false);

    if (zoomedInspection)
    {
        g.setColour (juce::Colours::yellow);
        g.drawRect (backgroundArea, 2.0f);
    }
}

void BK_EQ_HybridAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();

    // 底图按比例铺满窗口（保持 16:9），居中留边
    if (background.isValid() && background.getWidth() > 0 && background.getHeight() > 0)
        backgroundArea = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                            .appliedTo (juce::Rectangle<int> (background.getWidth(),
                                                              background.getHeight()),
                                        area);
    else
        backgroundArea = area;

    // 放大检视：把底图放大 3 倍，只显示左下区域。
    // 目的只有一个 —— 让像素级偏差能被肉眼看见，而不是靠猜。
    if (zoomedInspection)
    {
        const auto centre = juce::Point<int> (backgroundArea.getCentreX(),
                                              backgroundArea.getCentreY());
        backgroundArea = juce::Rectangle<int> (backgroundArea.getWidth()  * 3,
                                               backgroundArea.getHeight() * 3)
                            .withCentre (centre);
    }

    layOutKnobs();
}

void BK_EQ_HybridAudioProcessorEditor::layOutKnobs()
{
    // 底图坐标系 → 屏幕坐标系的映射。所有布局计算都在底图坐标系里做，
    // 因此窗口缩放不会破坏旋钮与底图的相对位置。
    const auto sx = (float) backgroundArea.getWidth()  / (float) kDesignWidth;
    const auto sy = (float) backgroundArea.getHeight() / (float) kDesignHeight;

    for (int i = 0; i < knobs.size(); ++i)
    {
        const auto& pl = placements[i];

        const int screenX = backgroundArea.getX() + juce::roundToInt (pl.centreX * sx);
        const int screenY = backgroundArea.getY() + juce::roundToInt (pl.centreY * sy);
        const int w = juce::jmax (1, juce::roundToInt (pl.diameter * sx));
        const int h = juce::jmax (1, juce::roundToInt (pl.diameter * sy));

        knobs[i]->setBounds (juce::Rectangle<int> (w, h).withCentre ({ screenX, screenY }));
    }
}
