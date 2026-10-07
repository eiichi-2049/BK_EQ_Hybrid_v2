# BK_EQ_Hybrid v2

**脱离 HISE 的完全重构**，技术路线见 [docs 技术选型](../../docs/技术选型-HISE替代方案.md)：JUCE 9 + 原生 C++ UI，只做 Windows x64 / VST3。

当前阶段：**POC 骨架（对齐验证）**。还没有任何 EQ 处理。

---

## 这个 POC 要验证什么

v1 最大的死结是「旋钮与底图对不齐」，根因已查明：HISE 的 filmstrip 控件
**绘制尺寸 = 帧宽 × scaleFactor，与控件 rect 无关**，所以声明 116px 的钮只画出 84px，
底图上的占位环必然外露。

本 POC 要证明 JUCE 不存在这个问题。**请重点看这三点**：

| # | 要验证的 | 怎么看 |
|---|---|---|
| **1** | **尺寸由 rect 决定** | 每个旋钮周围有**洋红细框**＝控件矩形。旋钮可见部分应**恰好铺满**该框，不多不少 |
| **2** | **位置精确** | 洋红框中心的**青色十字**应落在底图占位圆的圆心上。底图上残留的红色虚线环应被旋钮**完全盖住** |
| **3** | **拖动与参数联动** | 拖动旋钮 → 指针旋转；缩放窗口 → 旋钮与底图相对位置**不变**（这是 v1 里做不到的） |

底图共摆了 **7 个旋钮**：左 Pultec 3 个（黑钮），右 SSL 4 个（红/绿/蓝/棕）。覆盖了两侧面板，可检查多面板尺寸一致性。

> **关于 `kKnob2`（20/30/60/100 频选）**：它只有 4 档、底图占位圆也小（78px），
> 但用的是 91 帧的 Pultec 钮素材。这里**故意保留**，用于观察「档位离散的旋钮」
> 该如何定义角度映射——这是 v2 需要单独决策的设计点。

---

## 怎么跑

前提：Visual Studio 2026（含 C++ 桌面开发工作负载）。其余工具链（MSVC v145、CMake 4.2.3）随 VS 提供。

```powershell
cd "E:\个人EQ项目\编码\BK_EQ_Hybrid_v2"

# 首次：拉取 JUCE 9.0.3（约 42 MB，会校验官方 SHA256）
powershell -ExecutionPolicy Bypass -File tools\fetch-juce.ps1

# 编译（本工程路径含中文，必须走 ASCII 镜像，原因见下方「路径硬约束」）
powershell -ExecutionPolicy Bypass -File tools\build-ascii.ps1 -Install
```

`-Install` 会把产物装到 `E:\VST3\ReiVerb Work Shop\`，并把旧版本自动留档到 `LEGACY\`。

> **为什么不用 `tools\build.ps1`？** 那个脚本只在**纯 ASCII 路径**下可用。
> 本工程位于 `E:\个人EQ项目\...`（含中文），必须走 `build-ascii.ps1`。
> 若你把工程整体移到纯 ASCII 路径（如 `E:\BK_EQ_Hybrid_v2`），就能直接用 `build.ps1`，
> 而且不需要镜像——**这是更推荐的做法**。

### 在 DAW 里加载

装好后插件位于 `E:\VST3\ReiVerb Work Shop\BK_EQ_Hybrid_v2.vst3`（本机既有扫描路径）。
插件名显示为 **BK_EQ_Hybrid_v2**，厂商目录 **ReiVerb Work Shop**。

若不加 `-Install`，产物在 `build\BK_EQ_Hybrid_v2.vst3`（由镜像构建回收而来），
把该目录加入 DAW 插件搜索路径亦可。

---

## 目录结构

```text
BK_EQ_Hybrid_v2/
├── CMakeLists.txt          ← JUCE 插件定义；素材路径校验（拒绝非 ASCII 路径）
├── Source/
│   ├── BitmapKnob.h/.cpp   ← 位图旋钮（本次验证的核心）
│   ├── PluginProcessor.h/.cpp ← 参数定义（7 个旋钮 + 输出增益）
│   └── PluginEditor.h/.cpp ← 底图绘制 + 旋钮摆放坐标
├── assets/                 ← 从 v1 同步的 UI 素材（开发副本，不入库）
├── vendor/                 ← JUCE（由脚本下载，不入库）
├── build-ascii.log         ← 最近一次镜像构建日志（不入库）
└── tools/
    ├── build-ascii.ps1     ← ★ 本工程实际使用：ASCII 镜像构建（见「路径硬约束」）
    ├── build.ps1           ← 纯 ASCII 路径下的直接构建
    ├── setup.ps1           ← 一键：拉 JUCE + 自检 + 编译
    ├── fetch-juce.ps1      ← 下载并校验 JUCE
    ├── sync-assets.ps1     ← 从 v1 工程同步素材
    ├── install-vst3.ps1    ← 装到本机测试目录并留档旧版本
    ├── selfcheck.py        ← 静态自检（无法本地编译时的兜底检查）
    ├── fix_bom.py          ← 确保 .ps1 为 UTF-8 with BOM
    └── fix_bom_sources.py  ← 确保 C++ 源码为 UTF-8 with BOM
```

> **关于 BOM**：`.ps1` 与 `.cpp/.h` 都必须是 **UTF-8 with BOM**。
> PowerShell 5.1 与 MSVC 在中文 Windows 上会按代码页 936 解析无 BOM 的 UTF-8 文件，
> 中文注释会被误读，分别导致脚本语法错误与大量离奇 C++ 编译错误
> （如 `error C2447: "{": 缺少函数标题`）。
> 注意某些编辑器操作会**去掉 BOM**，改完用 `fix_bom.py` / `fix_bom_sources.py` 复查。

---

## 关键技术点：旋钮怎么画

`BitmapKnob::paint()` 里四步（完整代码见 `Source/BitmapKnob.cpp`）：

```cpp
// 1) 归一化取值 → 帧号
const int frameIndex = proportion * (numFrames - 1);

// 2) 尺寸由控件 rect 决定 —— 这是与 HISE 的根本差别
//    把素材的「内容盒」密铺到 rect，而不是让贴图尺寸说了算
const float scale = min(bounds.w, bounds.h) / contentBox;

// 3) 以控件中心为轴心旋转
g.addTransform (AffineTransform::translation (centre.x, centre.y));
g.addTransform (AffineTransform::rotation (degreesToRadians (angle)));
g.addTransform (AffineTransform::scale (scale));
g.addTransform (AffineTransform::translation (-slice.w * 0.5f, -slice.h * 0.5f));

// 4) 绘出该帧
g.drawImage (filmstrip, slice, RectanglePlacement::stretchToFit, false);
```

**「内容盒」是什么**：贴图帧内往往有透明留白。实测 v1 素材：

| 素材 | 帧尺寸 | 可见范围 | 内容盒 | 若不做处理 |
|---|---|---|---|---|
| `fs_pultec.png` | 160 | x[9,150] | **142** | 可见图形只占 rect 的 89%，露出底圈 |
| `fs_red/green/blue/brown.png` | 160 | x[1,158] | **158** | 基本铺满，可忽略 |

只有按内容盒缩放，可见图形才恰好落在 rect 内。这个数值是量出来的，不是猜的
（`tools/selfcheck.py` 会记录并校验）。

---

## 已知限制（POC 阶段有意为之）

| 项 | 说明 |
|---|---|
| 无 EQ 处理 | `processBlock` 只做输出增益，用于确认音频通路与参数自动化 |
| SSL 四色钮共用内容盒 158 | 先用同一素材标定；各色钮若留白不同，再逐张实测 |
| `kKnob2` 的 4 档映射未定 | 91 帧素材表示 4 档，角度映射方式待定 |
| 调试标记默认开启 | 洋红框 / 青十字 / 白字是为对齐核对加的，正式版会去掉 |
| 只支持立体声 | `isBusesLayoutSupported` 目前只接受立体声进出 |

---

## 常见问题

### ⚠️ 路径硬约束：工程路径不能含中文（JUCE 9 的 juceaide 缺陷）

**这是本工程最重要的一条环境约束。** 症状是编译时报：

```
error MSB8066: "...\BinaryData1.cpp.rule" 的自定义生成已退出，代码为 1
error MSB8066: "...\BK_EQ_Hybrid_v2_resources.rc.rule" 的自定义生成已退出，代码为 1
error MSB8066: "...\JuceHeader.h.rule" 的自定义生成已退出，代码为 1
```

而直接运行 `juceaide.exe` 只会打印一句 `Unhandled exception`，没有堆栈。

**成因**（已实测定位）：

1. CMake 把工程路径按**系统 ANSI 代码页**（中文 Windows 为 936）写进
   `Defs.txt` / `Info.txt` / `input_file_list`。例如：
   ```
   LAUNCH_STORYBOARD_FILE E:/涓汉EQ椤圭洰/缂栧爜/BK_EQ_Hybrid_v2/vendor/...
   ```
2. `juceaide` 按 **UTF-8** 读这些文件，字节对不上 → 解析失败 → 抛异常。

**为什么只有三个模式坏**：`binarydata` / `header` / `rcfile` 都要读写含路径的文件；
而 `juceaide version` 只打印字符串，所以它正常——这点很容易误判成「juceaide 没坏」。

**两条出路**：

| 方案 | 做法 | 评价 |
|---|---|---|
| **把工程放到纯 ASCII 路径** | 例如 `E:\BK_EQ_Hybrid_v2`，然后直接用 `tools\build.ps1` | **推荐**，一劳永逸 |
| **用 ASCII 镜像构建** | 保持工程原位，运行 `tools\build-ascii.ps1` | 已在用。脚本把工程镜像到 `E:\BK_EQ_Hybrid_v2_ascii` 后构建，素材以目录联接指回真实目录，产物再回收 |

验证方法：检查构建目录下的 `Defs.txt` / `Info.txt` 非 ASCII 字节数是否为 0：

```powershell
$f = 'build\BK_EQ_Hybrid_v2_artefacts\JuceLibraryCode\Info.txt'
[System.IO.File]::ReadAllBytes($f) | Where-Object { $_ -gt 127 } | Measure-Object
```

### CMake 报 `No CMAKE_C_COMPILER could be found`

**这是本机环境问题，不是项目问题。** 详见下一节「环境陷阱」。

### 环境陷阱：`no_proxy` 与 `NO_PROXY` 大小写重复

Windows 环境变量名**大小写不敏感**，但环境块里可以同时存在 `no_proxy` 和 `NO_PROXY`。
代理软件常同时写入多种大小写形式（`http_proxy` / `HTTP_PROXY` 等）。

后果：MSBuild 构建编译器探测工程时，向环境字典插入 `NO_PROXY` 撞上已有的 `no_proxy`：

```
error MSB6001: "CL.exe"的命令行开关无效。
System.ArgumentException: 已添加项。字典中的关键字:"NO_PROXY"所添加的关键字:"no_proxy"
```

于是编译器探测失败，CMake 只会笼统地报 `No CMAKE_C_COMPILER could be found`，**极难定位**。
同样的症状还会让 `Get-ChildItem Env:` 直接抛异常。

**`tools/build.ps1` 已内置清理**（只在同名大写形式也存在时才移除小写项，避免误删唯一可用的变量）。
若你在别处也遇到此问题，手动清理：

```powershell
Remove-Item Env:no_proxy -ErrorAction SilentlyContinue
```

彻底解决请检查系统环境变量（用户级/机器级）并只保留一种大小写形式。

### 编译报文件被占用

目标 `.vst3` 被 DAW 加载时无法覆盖。先在 DAW 卸载插件或退出 DAW。

---

## 下一步

POC 通过后（旋钮与底图对齐无误）才开始正式立项：

1. 定 v2 目录结构与参数契约（集中定义、编译期检查）
2. 迁移 DSP 设计：Pultec 8 钮 / SSL 10 钮 / PARALLEL 无极交叉 / 谐波层
3. 修 v1 遗留的 DSP 缺陷（见 [FROZEN.md](../AnalogBlend/FROZEN.md) 缺陷 2–5）
4. 用 Bertom 做曲线验收、用 null test 定 Gain Staging
