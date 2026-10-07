#include "PluginEditor.h"
#include "AssetLoader.h"

namespace
{
    /** 一个旋钮在底图坐标系（1280×720）里的位置与尺寸。 */
    struct KnobPlacement
    {
        const char* paramID;
        const char* stripFile;
        int centreX, centreY;
        int diameter;
    };

    // 素材内容盒（帧内可见部分，用于把图形精确铺满 rect）：
    //   fs_pultec.png                 帧 160，可见 x[9,150]  → 内容 142
    //   fs_red/green/blue/brown.png   帧 160，可见 x[1,158]  → 内容 158
    constexpr int kPultecFrames = 91, kPultecContent = 142;
    constexpr int kSslFrames    = 91, kSslContent    = 158;

    const char* const kPultecStrip = "fs_pultec.png";
    const char* const kRedStrip    = "fs_red.png";
    const char* const kGreenStrip  = "fs_green.png";
    const char* const kBlueStrip   = "fs_blue.png";
    const char* const kBrownStrip  = "fs_brown.png";

    // ------------------------------------------------------------------------
    // 坐标表（底图坐标系 1280×720）—— 来源：MAIN-UI-UNDERLAY 实测
    //
    //  左 Pultec：底图只有文字标签 + 白色定位点。已能检出定位点的三个钮实测质心：
    //      R1-1 BOOST (136.1,164.5)   R1-3 频选 (401.1,172.1)   R3-3 频选 (401.0,542.1)
    //    定位点半径均约 38（即旋钮可见圆半径）。由此得网格：
    //      列中心 x ≈ 128 / 263 / 400（间距约 135）
    //      行中心 y ≈ 170 / 355 / 540（间距 185，与 SSL 行距一致）
    //    其余钮的点太稀疏无法检出，按同一网格推得。
    //  右 SSL：底图上有 10 个灰色实心定位圆（直径 83.2），实测中心为
    //      (873,192) (1149,192) ／ (873,314) (1012,314) (1149,314)
    //      (873,434) (1012,434) (1149,434) ／ (873,567) (1150,567)
    //
    // ⚠ 若整体仍有偏移，直接改这里的数字即可（见 DEVELOPMENT.md「坐标标定」）。
    // ------------------------------------------------------------------------
    const KnobPlacement placements[] =
    {
        // ---------------------------- 左 Pultec（8 钮）
        { BK_EQ_HybridAudioProcessor::kPultecBoost,    kPultecStrip, 128, 170, 100 },  // R1 BOOST
        { BK_EQ_HybridAudioProcessor::kPultecBw,       kPultecStrip, 263, 170, 100 },  // R1 BD.WITH
        { BK_EQ_HybridAudioProcessor::kPultecHfSel,    kPultecStrip, 401, 172,  84 },  // R1 3/5/10/16k
        { BK_EQ_HybridAudioProcessor::kPultecAtten,    kPultecStrip, 128, 355, 100 },  // R2 ATTEN.
        { BK_EQ_HybridAudioProcessor::kPultecAttenSel, kPultecStrip, 263, 355, 100 },  // R2 ATTEN.SEL
        { BK_EQ_HybridAudioProcessor::kPultecAtten2,   kPultecStrip, 128, 540, 100 },  // R3 ATTEN.
        { BK_EQ_HybridAudioProcessor::kPultecBoost2,   kPultecStrip, 263, 540, 100 },  // R3 BOOST
        { BK_EQ_HybridAudioProcessor::kPultecLfSel,    kPultecStrip, 401, 542,  84 },  // R3 20/30/60/100

        // ---------------------------- 右 SSL（10 钮，中心即灰色定位圆位置）
        { BK_EQ_HybridAudioProcessor::kSslHfDb,  kRedStrip,   873, 192,  95 },  // HF dB
        { BK_EQ_HybridAudioProcessor::kSslHfHz,  kRedStrip,  1149, 192,  95 },  // HF Hz
        { BK_EQ_HybridAudioProcessor::kSslHmfDb, kGreenStrip, 873, 314,  95 },  // HMF dB
        { BK_EQ_HybridAudioProcessor::kSslHmfQ,  kGreenStrip,1012, 314,  95 },  // HMF Q
        { BK_EQ_HybridAudioProcessor::kSslHmfHz, kGreenStrip,1149, 314,  95 },  // HMF Hz
        { BK_EQ_HybridAudioProcessor::kSslLmfDb, kBlueStrip,  873, 434,  95 },  // LMF dB
        { BK_EQ_HybridAudioProcessor::kSslLmfQ,  kBlueStrip, 1012, 434,  95 },  // LMF Q
        { BK_EQ_HybridAudioProcessor::kSslLmfHz, kBlueStrip, 1149, 434,  95 },  // LMF Hz
        { BK_EQ_HybridAudioProcessor::kSslLfDb,  kBrownStrip, 873, 567,  95 },  // LF dB
        { BK_EQ_HybridAudioProcessor::kSslLfHz,  kBrownStrip,1150, 567,  95 },  // LF Hz
    };

    /** 中央 VU 开窗：底图烘入的黑矩形实测 x[523,757] y[164,334]。 */
    constexpr int kVuX = 523, kVuY = 164, kVuW = 235, kVuH = 171;

    /** 中央 PARALLEL 翼形旋钮：底图上的深色圆，直径约 146。 */
    constexpr int kParX = 640, kParY = 575, kParD = 146;
}


BK_EQ_HybridAudioProcessorEditor::BK_EQ_HybridAudioProcessorEditor (BK_EQ_HybridAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    background = AssetLoader::loadImage ("bg.png");

    // 中央 VU 表盘（静态贴图；表针跟随信号留待 DSP 接入后再做）
    vuDesignArea = { kVuX, kVuY, kVuW, kVuH };
    if (auto img = AssetLoader::loadImage ("vu_meter.png"); img.isValid())
    {
        vuBox = std::make_unique<ImageBox> (img);
        addAndMakeVisible (*vuBox);
    }

    // 中央 PARALLEL 翼形旋钮：底图上虽有图形，仍单独绘制一份以便日后做旋转交互
    parallelDesignArea = { kParX - kParD / 2, kParY - kParD / 2, kParD, kParD };
    if (auto img = AssetLoader::loadImage ("parallel_knob.png"); img.isValid())
    {
        parallelBox = std::make_unique<ImageBox> (img);
        addAndMakeVisible (*parallelBox);
    }

    buildKnobs();
    buildZoomButton();

    setResizable (true, false);   // 固定档位缩放，不允许自由拉伸
    applyZoomLevel();
}

BK_EQ_HybridAudioProcessorEditor::~BK_EQ_HybridAudioProcessorEditor() = default;

void BK_EQ_HybridAudioProcessorEditor::buildKnobs()
{
    for (int i = 0; i < (int) juce::numElementsInArray (placements); ++i)
    {
        const auto& pl = placements[i];

        const bool isPultec = pl.stripFile == kPultecStrip;
        const int frames  = isPultec ? kPultecFrames  : kSslFrames;
        const int content = isPultec ? kPultecContent : kSslContent;

        // 由参数自身决定取值范围，避免 UI 与参数各写一套常量
        auto* param = processor.apvts.getParameter (pl.paramID);
        if (param == nullptr)
        {
            DBG ("编辑器：找不到参数 " + juce::String (pl.paramID));
            continue;
        }

        const auto& range = param->getNormalisableRange();
        const auto def = param->convertFrom0to1 (param->getDefaultValue());

        auto* knob = new BitmapKnob (pl.paramID, pl.stripFile, frames, content,
                                     (double) range.start, (double) range.end, (double) def);
        knob->setIndexLabel (i);
        addAndMakeVisible (knob);
        knobs.add (knob);

        // 旋钮 → 参数（拖动时上报宿主）
        knob->onValueChange = [param] (double v)
        {
            param->setValueNotifyingHost (param->convertTo0to1 ((float) v));
        };

        // 参数 → 旋钮（宿主自动化或载入预设时刷新显示）
        attachments.push_back (std::make_unique<juce::ParameterAttachment> (
            *param,
            [knob] (float newValue)
            {
                if (! juce::approximatelyEqual (knob->getValue(), (double) newValue))
                    knob->setValue (newValue);
            }));
    }
}

void BK_EQ_HybridAudioProcessorEditor::buildZoomButton()
{
    zoomButton.setButtonText ("100%");
    zoomButton.setTooltip ("选择缩放档位");
    zoomButton.setLookAndFeel (&zoomLookAndFeel);
    zoomButton.onClick = [this] { showZoomMenu(); };
    addAndMakeVisible (zoomButton);
}

void BK_EQ_HybridAudioProcessorEditor::showZoomMenu()
{
    // 直接列出 5 个档位，比「点击即循环」直观
    juce::PopupMenu menu;
    menu.setLookAndFeel (&zoomLookAndFeel);

    for (int i = 0; i < (int) kZoomLevels.size(); ++i)
    {
        const auto percent = juce::roundToInt (kZoomLevels[(size_t) i] * 100.0);
        menu.addItem (i + 1, juce::String (percent) + "%", true, i == zoomIndex);
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&zoomButton)
                            .withMinimumWidth (zoomButton.getWidth()),
                        [this] (int result)
                        {
                            if (result > 0 && result <= (int) kZoomLevels.size())
                            {
                                zoomIndex = result - 1;
                                applyZoomLevel();
                            }
                        });
}

void BK_EQ_HybridAudioProcessorEditor::zoomIn()
{
    zoomIndex = (zoomIndex + 1) % (int) kZoomLevels.size();
    applyZoomLevel();
}

void BK_EQ_HybridAudioProcessorEditor::zoomOut()
{
    zoomIndex = (zoomIndex + (int) kZoomLevels.size() - 1) % (int) kZoomLevels.size();
    applyZoomLevel();
}

void BK_EQ_HybridAudioProcessorEditor::applyZoomLevel()
{
    const auto scale = kZoomLevels[(size_t) zoomIndex];

    setSize (juce::roundToInt (kDesignWidth  * scale),
             juce::roundToInt (kDesignHeight * scale));

    zoomButton.setButtonText (juce::String (juce::roundToInt (scale * 100.0)) + "%");
    resized();
    repaint();
}

void BK_EQ_HybridAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (background.isValid())
    {
        g.drawImage (background, backgroundArea.toFloat(),
                     juce::RectanglePlacement::stretchToFit, false);
    }
    else
    {
        g.setColour (juce::Colours::orangered);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("素材未找到：assets/bg.png  （检查 tools\\build-assets.ps1）",
                    getLocalBounds(), juce::Justification::centredTop, true);
    }
}

void BK_EQ_HybridAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();

    if (background.isValid() && background.getWidth() > 0 && background.getHeight() > 0)
        backgroundArea = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                            .appliedTo (juce::Rectangle<int> (background.getWidth(),
                                                              background.getHeight()),
                                        area);
    else
        backgroundArea = area;

    // 底图坐标系 → 屏幕坐标系的统一映射
    const auto sx = (float) backgroundArea.getWidth()  / (float) kDesignWidth;
    const auto sy = (float) backgroundArea.getHeight() / (float) kDesignHeight;

    auto toScreen = [this, sx, sy] (juce::Rectangle<int> design)
    {
        return juce::Rectangle<int> (
            backgroundArea.getX() + juce::roundToInt (design.getX() * sx),
            backgroundArea.getY() + juce::roundToInt (design.getY() * sy),
            juce::jmax (1, juce::roundToInt (design.getWidth()  * sx)),
            juce::jmax (1, juce::roundToInt (design.getHeight() * sy)));
    };
    auto centreScreen = [this, sx, sy] (int cx, int cy, int d)
    {
        const int w = juce::jmax (1, juce::roundToInt (d * sx));
        const int h = juce::jmax (1, juce::roundToInt (d * sy));
        return juce::Rectangle<int> (w, h).withCentre (
            { backgroundArea.getX() + juce::roundToInt (cx * sx),
              backgroundArea.getY() + juce::roundToInt (cy * sy) });
    };

    if (vuBox != nullptr)
        vuBox->setBounds (toScreen (vuDesignArea));

    if (parallelBox != nullptr)
        parallelBox->setBounds (toScreen (parallelDesignArea));

    for (int i = 0; i < knobs.size() && i < (int) juce::numElementsInArray (placements); ++i)
    {
        const auto& pl = placements[i];
        knobs[i]->setBounds (centreScreen (pl.centreX, pl.centreY, pl.diameter));
    }

    const auto scale = kZoomLevels[(size_t) zoomIndex];
    const int bw = juce::roundToInt (78 * scale);
    const int bh = juce::roundToInt (22 * scale);
    const int margin = juce::roundToInt (14 * scale);

    zoomButton.setBounds (backgroundArea.getRight()  - margin - bw,
                          backgroundArea.getBottom() - margin - bh,
                          bw, bh);
}
