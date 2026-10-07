#include "PluginEditor.h"
#include "AssetLoader.h"

namespace
{
    /** 一个旋钮在底图坐标系里的位置与尺寸。
        中心点取自 v1 底图（bg.png）上实测的占位圆圆心；
        尺寸取占位圆直径，这样旋钮的可见部分会恰好盖住底图那圈虚线。 */
    struct KnobPlacement
    {
        const char* paramID;
        int centreX, centreY;   // 底图坐标系
        int diameter;
    };

    // 素材内容盒实测值（见 tools\selfcheck.py）：
    //   fs_pultec.png                 帧 160，可见 x[9,150]  → 内容 142
    //   fs_red/green/blue/brown.png   帧 160，可见 x[1,158]  → 内容 158
    // 说明：SSL 四色钮先用同一数值标定，待各色钮实测后再按需细分。
    constexpr int kPultecFrames = 91, kPultecContent = 142;
    constexpr int kSslFrames    = 91, kSslContent    = 158;

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

    /** 每个旋钮用哪张 filmstrip：前三（Pultec）黑钮，后四（SSL）按色分。 */
    struct Strip { const char* fileName; int frames; int content; };
    const Strip strips[] =
    {
        { "fs_pultec.png", kPultecFrames, kPultecContent },
        { "fs_pultec.png", kPultecFrames, kPultecContent },
        { "fs_pultec.png", kPultecFrames, kPultecContent },
        { "fs_red.png",    kSslFrames,    kSslContent    },
        { "fs_green.png",  kSslFrames,    kSslContent    },
        { "fs_blue.png",   kSslFrames,    kSslContent    },
        { "fs_brown.png",  kSslFrames,    kSslContent    },
    };

    static_assert (juce::numElementsInArray (placements) == juce::numElementsInArray (strips),
                   "placements 与 strips 必须一一对应");
}

BK_EQ_HybridAudioProcessorEditor::BK_EQ_HybridAudioProcessorEditor (BK_EQ_HybridAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    background = AssetLoader::loadImage ("bg.png");

    // 中央 VU 表盘：位置取 v1 工程里的面板坐标 (524,163) 268×172。
    // 目前是静态贴图；正式版的表针会跟随真实信号（v1 已做过，见 docs/spec）。
    constexpr int kVuX = 524, kVuY = 163, kVuW = 268, kVuH = 172;
    vuDesignArea = { kVuX, kVuY, kVuW, kVuH };
    if (auto vuImage = AssetLoader::loadImage ("vu_meter.png"); vuImage.isValid())
    {
        vuBox = std::make_unique<ImageBox> (vuImage);
        addAndMakeVisible (*vuBox);
    }

    buildKnobs();
    buildZoomButton();

    // 固定档位缩放：允许改变大小，但不允许自由拉伸
    setResizable (true, false);
    applyZoomLevel();
}

BK_EQ_HybridAudioProcessorEditor::~BK_EQ_HybridAudioProcessorEditor() = default;

void BK_EQ_HybridAudioProcessorEditor::buildKnobs()
{
    for (int i = 0; i < (int) juce::numElementsInArray (placements); ++i)
    {
        const auto& pl = placements[i];
        const auto& st = strips[i];

        double minV = 0.0, maxV = 24.0;
        if (i == 2) { minV = 0.0;   maxV = 3.0;  }   // 频选 4 档
        if (i >= 3) { minV = -24.0; maxV = 24.0; }   // SSL dB

        auto* knob = new BitmapKnob (pl.paramID, st.fileName,
                                     st.frames, st.content,
                                     minV, maxV, 0.0);
        knob->setIndexLabel (i);
        addAndMakeVisible (knob);
        knobs.add (knob);

        // BitmapKnob 是 juce::Component 而非 juce::Slider，因此不能用
        // SliderAttachment，改用 ParameterAttachment 做双向桥接：
        //   旋钮 → 参数（拖动时上报宿主）
        //   参数 → 旋钮（宿主自动化或载入预设时刷新显示）
        auto* param = processor.apvts.getParameter (pl.paramID);
        if (param == nullptr)
        {
            DBG ("编辑器：找不到参数 " + juce::String (pl.paramID));
            continue;
        }

        knob->onValueChange = [param] (double v)
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

void BK_EQ_HybridAudioProcessorEditor::buildZoomButton()
{
    // 右下角：点击弹出档位菜单，随档位一起缩放
    zoomButton.setButtonText ("100%");
    zoomButton.setTooltip ("选择缩放档位");
    zoomButton.setLookAndFeel (&zoomLookAndFeel);
    zoomButton.onClick = [this] { showZoomMenu(); };
    addAndMakeVisible (zoomButton);
}

void BK_EQ_HybridAudioProcessorEditor::showZoomMenu()
{
    // 弹出二级菜单让用户选择档位。
    // 不用「点击即循环」是因为档位只有 5 个，直接列出比反复点击直观得多。
    juce::PopupMenu menu;
    menu.setLookAndFeel (&zoomLookAndFeel);

    for (int i = 0; i < (int) kZoomLevels.size(); ++i)
    {
        const auto percent = juce::roundToInt (kZoomLevels[(size_t) i] * 100.0);
        menu.addItem (i + 1,
                      juce::String (percent) + "%",
                      true,                       // enabled
                      i == zoomIndex);            // 当前档位打勾
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
        // 素材缺失时给出明确提示。否则只会看到一块黑屏，无从判断原因。
        g.setColour (juce::Colours::orangered);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("素材未找到：assets/bg.png  （检查 tools\\sync-assets.ps1 与 CMake 的 BK_ASSETS_DIR）",
                    getLocalBounds(), juce::Justification::centredTop, true);
    }
}

void BK_EQ_HybridAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds();

    // 底图按比例铺满窗口（保持 16:9）
    if (background.isValid() && background.getWidth() > 0 && background.getHeight() > 0)
        backgroundArea = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                            .appliedTo (juce::Rectangle<int> (background.getWidth(),
                                                              background.getHeight()),
                                        area);
    else
        backgroundArea = area;

    layOutKnobs();

    // VU 表盘随底图坐标系摆放
    if (vuBox != nullptr)
    {
        const auto vx = (float) backgroundArea.getWidth()  / (float) kDesignWidth;
        const auto vy = (float) backgroundArea.getHeight() / (float) kDesignHeight;

        vuBox->setBounds (backgroundArea.getX() + juce::roundToInt (vuDesignArea.getX() * vx),
                          backgroundArea.getY() + juce::roundToInt (vuDesignArea.getY() * vy),
                          juce::jmax (1, juce::roundToInt (vuDesignArea.getWidth()  * vx)),
                          juce::jmax (1, juce::roundToInt (vuDesignArea.getHeight() * vy)));
    }

    const auto scale = kZoomLevels[(size_t) zoomIndex];
    const int bw = juce::roundToInt (78 * scale);
    const int bh = juce::roundToInt (22 * scale);
    const int margin = juce::roundToInt (14 * scale);

    zoomButton.setBounds (backgroundArea.getRight()  - margin - bw,
                          backgroundArea.getBottom() - margin - bh,
                          bw, bh);
    // 字号由 zoomLookAndFeel 依据按钮高度决定，这里无需再设
}

void BK_EQ_HybridAudioProcessorEditor::layOutKnobs()
{
    // 底图坐标系 → 屏幕坐标系。所有摆放计算都在底图坐标系里做，
    // 因此缩放不会破坏旋钮与底图的相对位置。
    const auto sx = (float) backgroundArea.getWidth()  / (float) kDesignWidth;
    const auto sy = (float) backgroundArea.getHeight() / (float) kDesignHeight;

    for (int i = 0; i < knobs.size() && i < (int) juce::numElementsInArray (placements); ++i)
    {
        const auto& pl = placements[i];

        const int screenX = backgroundArea.getX() + juce::roundToInt (pl.centreX * sx);
        const int screenY = backgroundArea.getY() + juce::roundToInt (pl.centreY * sy);
        const int w = juce::jmax (1, juce::roundToInt (pl.diameter * sx));
        const int h = juce::jmax (1, juce::roundToInt (pl.diameter * sy));

        knobs[i]->setBounds (juce::Rectangle<int> (w, h).withCentre ({ screenX, screenY }));
    }
}
