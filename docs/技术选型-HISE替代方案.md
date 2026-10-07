---
feature: framework-selection
status: decided
updated: 2026-10-07
branch: main
commits: pending
---

# 技术选型 · HISE 替代方案

**结论：JUCE 9 + 原生 C++ UI（位图素材 + `juce::Image` 旋转绘制）。**
备选路线：若不愿接受 AGPLv3，则走 **iPlug2**（zlib 类许可，同样支持位图素材与旋转绘制控件）。

前提：**只做 Windows x64 / VST3**，暂不做 macOS / AU。

---

## 1. 为什么要换

v1 用 HISE 4.1.0 完成，功能其实做到了（见 [FROZEN.md](../编码/AnalogBlend/FROZEN.md)）。
放弃它的原因是**框架限制**，不是功能缺失。下面每条都来自对 HISE 源码与产物的实测，作为选型的硬性考察项。

| # | v1 的实际限制 | 证据 |
|---|---|---|
| 1 | **旋钮尺寸不可控**：filmstrip 绘制尺寸 = `帧宽 × scaleFactor`，**与控件 rect 完全无关**。声明 116 px 的钮实际只画 84 px（`160 × 0.725`），底图占位环（约 99 px）必然外露，对齐无法闭环 | HISE 源码 `hi_tools/hi_tools/HI_LookAndFeels.cpp` 的 `FilmstripLookAndFeel::drawRotarySlider` |
| 2 | **图形状态无 save/restore**：无法做局部旋转/裁剪栈，只好把指针烘进 filmstrip 或用 `drawImage` 的 yOffset 切帧 | v1 的 VU 针做成 41 帧贴图，即为此绕行 |
| 3 | **无全局缩放 API**：拖角缩放只能手写坐标乘法逐个重排控件（v1 的 `applyZoom`），且启动调用会把布局拉飞 | `Interface.js` 中 `applyZoom` 硬编码每个控件的新 rect |
| 4 | **DSP 原语受限**：曲线/饱和依赖现成模块；不对称波形、比例 Q、搁架 overshoot 需要更底层的手段 | v1 的 ShapeFX 只有 Tanh/Atan 预设，Pultec 低频魔术未能实现 |
| 5 | **参数映射靠索引硬编码**：`setAttribute(band*5+param)` 这类数字契约散落在 UI 脚本里，无编译期检查；v1 因此出现频段错位、两钮写同一参数的缺陷 | [FROZEN.md](../编码/AnalogBlend/FROZEN.md) 缺陷 2、3 |

> 另有一条非框架问题但重构时必须一并解决：v1 的 Gain Staging 存疑（`Pultec/SSL Wet` 均 −6.02 dB 按不相关信号设计，同源相关信号半半混合理论 ≈ +3 dB，`Output Bus` 固定 −6 dB 疑似在补坑，从未做 null test）。

---

## 2. 需求与约束（决策依据）

### 2.1 必须满足

| 项 | 要求 | 结论 |
|---|---|---|
| 交付格式 | VST3 · Windows x64 | 全部候选均满足 |
| 界面还原度 | 控件尺寸/位置不受框架「自动缩放」干扰，能与底图精确对齐 | **本项目的头号诉求**，见 §4.1 |
| 素材复用 | 现有底图/旋钮/指针/纹理可直接用 | 全部候选均满足（都是位图绘制） |
| DSP 可控性 | 可写自定义 waveshaper、过采样、逐带 biquad | 全部候选均是纯 C++，无限制 |
| 参数契约 | 参数集中定义、有编译期检查 | JUCE `AudioProcessorValueTreeState`；iPlug2 参数 ID 系统 |
| 构建 | 本机（VS 2026 · MSVC v145 · CMake 4.2.3）可构建 | 见 §4.2 |
| 授权 | 允许免费/开源发布 | **候选间差异最大的一项**，见 §3 |

### 2.2 加分项

- 界面能用 Web 技术表达 —— `index.html` 那套原型可复活
- 生态与资料充足（作者一人开发，遇到问题要能搜到答案）
- 编译快、迭代快

### 2.3 明确不要

- 再次被框架隐式行为绑住（如「尺寸由贴图决定」）
- 需要付费授权
- 引入过重工程负担

---

## 3. 授权对比（本项目最关键的差异）

| 框架 | 授权 | 能否闭源 | 费用 | 对本项目的实际影响 |
|---|---|---|---|---|
| **JUCE 9** | **双授权**：AGPLv3 或商业 EULA | AGPLv3 下**必须开源**（含整个插件） | AGPLv3 免费；商业版 Starter 档免版税门槛为年收入 2 万美元 | 本项目已开源、免费、禁商用 → **AGPLv3 路线零成本**。代价是：整个插件源码必须按 AGPLv3 提供，且部分企业用户会规避 AGPL 软件 |
| **iPlug2** | **zlib 类许可** | **可以** | 免费 | 无 copyleft 约束；可自由选择开源或闭源，将来想闭源也不用换框架 |
| **yup** | **ISC** | **可以** | 免费 | JUCE 7 的自由许可分支，无 copyleft |

来源：[JUCE 授权页与 FAQ](https://juce.com/get-juce/)（JUCE 9 EULA、Starter 档 2 万美元门槛）、[iPlug2 LICENSE](https://github.com/iPlug2/iPlug2/blob/master/LICENSE.txt)、[yup LICENSE](https://github.com/kunitoki/yup/blob/master/LICENSE)。

> **这不是法律意见。** 商业授权条款以官方 EULA 原文为准。此处只记录影响技术决策的事实。

**这一项决定走哪条路：**

- 接受 **AGPLv3**（继续开源、免费）→ 走 **JUCE 9**
- 不接受任何 copyleft（保留将来闭源的可能）→ 走 **iPlug2**

---

## 4. 候选对比

| | **JUCE 9** | **JUCE 9 + WebView** | **iPlug2** | **yup** |
|---|---|---|---|---|
| 语言 / 构建 | C++ · CMake | C++ + HTML/CSS/JS | C++ · CMake | C++ · CMake |
| VST3 | 官方支持 | 官方支持 | 支持 | 需核实 |
| UI 方式 | `juce::Component` 自绘 | WebView2 承载网页 | `IGraphics`（NanoVG/Skia 后端） | RHI（GPU 渲染） |
| **位图精确摆放** | ✅ 像素级 | ✅ CSS 精确 | ✅ 像素级 | ✅ |
| **图形状态栈** | ✅ `Graphics::saveState/restoreState` | ✅（浏览器） | ✅ | ✅ |
| **全局缩放** | ✅ `AffineTransform` / `setScaleFactor` | ✅ CSS transform | ✅ IGraphics 缩放 | ✅ |
| 位图旋钮旋转 | ✅ 自写 `AffineTransform` | ✅ CSS | ✅ 内置 `IBKnobRotaterControl`（先转位图再绘制） | ✅ |
| 社区 / 资料 | **最大** | 大 | 中（有专属论坛/Discord、iPlug2GPT） | 小 |
| 编译速度 | 较慢（全量首次 10 分钟级） | 同 JUCE | **较快** | 快 |
| 学习曲线 | 中（生态大、教程多） | 中 + 前端 | 中（API 更简单） | 中 |
| 授权 | AGPLv3 / 商业 | 同 JUCE | **zlib 类** | **ISC** |
| 主要风险 | AGPL 传染性；企业用户规避 | 同左 + WebView2 运行库依赖 | 生态较小 | **过于小众，长期维护与资料风险高** |

其他弱候选（不推荐，仅记录）：DPF（无 Windows VST3 优先支持、社区偏 Linux）、Dplug（D 语言，生态小）、Rust 系 nice-plug（作者已停止维护）、Mostly Harmless（过新）。

---

## 5. 结论与理由

### 5.1 首选：JUCE 9 + 原生 C++ UI

1. **直接消灭 v1 的死结。** JUCE 里控件尺寸就是 rect，位图旋转用 `AffineTransform` 明确指定，不存在「尺寸由贴图推导」这种隐式规则。§4.1 的三条对齐痛点全部消失。
2. **生态与资料量最大**，单人开发遇到问题最容易被解决；也是 AI 辅助编码最熟悉的 C++ 音频框架，这一点对本项目（大量依赖 AI 协作）权重很高。
3. **本机工具链已就绪**：VS 2026 / MSVC 14.51（v145）/ CMake 4.2.3，JUCE 9 要求 CMake ≥ 3.22，满足。v1 踩过的工具集补丁问题不再需要。
4. **授权零成本**：项目本就开源、免费、禁商用，走 AGPLv3 不需要付费。
5. 加分项可后置：`AudioPluginAudioProcessorEditor` 里加一个 WebView 承载 HTML 界面是**同一框架内的增量改造**，可先做原生 UI，日后需要更强表现力再切 WebView，不需要换框架。

### 5.2 何时改选 iPlug2

出现以下任一情况就换 iPlug2（UI 能力等价，授权无约束）：

- 你希望**保留将来闭源或商业化的可能**，不想让 AGPLv3 覆盖整个插件
- 你希望编译更快、工程更轻
- 你不介意放弃 JUCE 的大生态（iPlug2 有专属文档、论坛、Discord 和 iPlug2GPT）

### 5.3 明确否决

- **yup**：授权最宽松、渲染最新，但用户基数与资料太少，单人项目不该承担这个维护风险。
- **JUCE + WebView 作为起点**：多一层 WebView2 运行库依赖与前后端通信复杂度，建议作为后续可选项而非起点。

---

## 6. 下一步（含需人工执行的动作）

| 步骤 | 内容 | 谁做 |
|---|---|---|
| S1 | 建**空** GitHub 仓库（不要初始化 README），用于验证构建链 | **你** |
| S2 | 首次推送现有工作仓库（见 [工作流文档](工作流-Git与版本管理.md) §2） | **你** |
| S3 | 做 POC 骨架：JUCE 9 最小插件（**1 个位图旋钮 + 1 个精确坐标标签**），编译并通过 `pluginval`，在 Live 里加载 | 我（需你确认） |
| S4 | 用真实素材验收对齐：把 `Images/knob_black.png` + `bg.png` 放进 POC，确认像素级对齐 | **你**在 Live 里确认 |
| S5 | 通过后正式立项 v2：定目录、定参数契约、迁移 DSP 设计 | 我 |

**S3 之所以必须先做**：v1 的教训是一次改全盘再猜。骨架能编译、能加载、能对齐之后，再谈 DSP 与完整界面。

---

## 7. 变化记录

| 日期 | 变化 |
|---|---|
| 2026-10-07 | 初稿：列出决策框架与待核实清单 |
| 2026-10-07 | 完成对比：确认 JUCE 9 为 AGPLv3/商业双授权、iPlug2 为 zlib 类许可；核实本机 CMake 4.2.3 与 JUCE 9（要求 ≥3.22）兼容；结论定为 JUCE 9 + 原生 UI，备选 iPlug2 |
