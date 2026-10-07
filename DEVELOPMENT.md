# 开发说明 / Development Notes

面向开发。使用者请看 [README.md](README.md)。

---

## 1. 这个骨架要验证什么

项目此前由 HISE 实现（v1），现完全重构为 JUCE。重构的核心动机是 v1 的
**旋钮与底图对不齐**，根因已查明：HISE 的 filmstrip 控件

```
绘制尺寸 = 帧宽 × scaleFactor      ← 与控件 rect 无关
```

所以声明 116px 的钮实际只画出 84px（`160 × 0.725`），底图上的占位环必然外露。

本骨架要证明 JUCE 不存在这个问题。**请重点看三点**：

| # | 要验证的 | 怎么看 |
|---|---|---|
| **1** | **尺寸由 rect 决定** | 每个旋钮外的**洋红细框**＝控件矩形。旋钮可见部分应**恰好铺满**该框 |
| **2** | **位置精确** | 洋红框中心的**青十字**应落在底图占位圆圆心上，底图残留的红色虚线环应被**完全盖住** |
| **3** | **缩放不破坏对齐** | 点右下角按钮切换 60/80/100/120/140%，旋钮与底图的相对位置**不变** |

底图共摆 **7 个旋钮**：左 Pultec 3 个（黑钮），右 SSL 4 个（红/绿/蓝/棕）。

> `kKnob2`（20/30/60/100 频选）只有 4 档、底图占位圆 78px，但用的是 91 帧的
> Pultec 钮素材。这里**故意保留**，用于观察「离散档位旋钮」该如何定义角度映射——
> 这是正式版需要单独决策的设计点。

---

## 2. 怎么跑

前提：Visual Studio 2026（含 C++ 桌面开发工作负载）。MSVC v145 与 CMake 4.2.3 随 VS 提供。

```powershell
# 首次：取 JUCE 9.0.3（约 42 MB，校验官方 SHA256）
powershell -ExecutionPolicy Bypass -File tools\fetch-juce.ps1

# 编译并安装到本机测试目录
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Install
```

> 本工程位于 `E:\BK_EQ_Hybrid_v2`，是纯 ASCII 路径，直接用 `build.ps1` 即可。
> 若你把它放到含中文的路径下，构建会失败——那种情况用 `tools\build-ascii.ps1`，
> 它会自动镜像到 ASCII 路径后构建（详见第 3 节）。

`-Install` 会把产物装到 `E:\VST3\ReiVerb Work Shop\`，旧版本自动留档到 `LEGACY\`。

### 在 DAW 里加载

插件名 **BK_EQ_Hybrid_v2**，厂商目录 **ReiVerb Work Shop**。

---

## 3. ⚠️ 路径硬约束：工程路径不能含中文

**这是本项目最重要的一条环境约束。**

### 症状

```
error MSB8066: "...\BinaryData1.cpp.rule" 的自定义生成已退出，代码为 1
error MSB8066: "...\BK_EQ_Hybrid_v2_resources.rc.rule" 的自定义生成已退出，代码为 1
error MSB8066: "...\JuceHeader.h.rule" 的自定义生成已退出，代码为 1
```

直接运行 `juceaide.exe` 只打印一句 `Unhandled exception`，没有堆栈，极难定位。

### 成因（已实测）

1. CMake 把工程路径按**系统 ANSI 代码页**（中文 Windows 为 936）写进
   `Defs.txt` / `Info.txt` / `input_file_list`，例如：
   ```
   LAUNCH_STORYBOARD_FILE E:/涓汉EQ椤圭洰/缂栧爜/BK_EQ_Hybrid_v2/vendor/...
   ```
2. `juceaide` 按 **UTF-8** 读这些文件 → 字节对不上 → 抛异常。

**为什么只有三个模式坏**：`binarydata` / `header` / `rcfile` 都要读写含路径的文件；
`juceaide version` 只打印字符串，所以正常——这点很容易误判成「juceaide 没坏」。

### 对策

工程放在**纯 ASCII 路径**下即可。本工程当前位于 `E:\BK_EQ_Hybrid_v2`，已满足。

若你把它挪到含中文的路径（例如放回 `E:\个人EQ项目\编码\` 下），构建会失败；
那种情况用 `tools\build-ascii.ps1`：它把工程镜像到纯 ASCII 路径后构建，
`assets\` 以目录联接指回真实目录，产物再回收。
脚本会自动判断——路径可用时直接在原地构建，不镜像。

验证方法——检查构建目录下 `Defs.txt` / `Info.txt` 的非 ASCII 字节数应为 0：

```powershell
$f = 'build\BK_EQ_Hybrid_v2_artefacts\JuceLibraryCode\Info.txt'
[System.IO.File]::ReadAllBytes($f) | Where-Object { $_ -gt 127 } | Measure-Object
```

---

## 4. 另外两个环境陷阱

### 4.1 `no_proxy` 与 `NO_PROXY` 大小写重复

Windows 环境变量名**大小写不敏感**，但环境块里可以同时存在两种大小写形式
（代理软件常这么写）。MSBuild 向环境字典插入键时撞车：

```
error MSB6001: "CL.exe"的命令行开关无效。
System.ArgumentException: 已添加项。字典中的关键字:"NO_PROXY"所添加的关键字:"no_proxy"
```

症状是 CMake 只笼统地报 `No CMAKE_C_COMPILER could be found`。手工处理：

```powershell
Remove-Item Env:no_proxy -ErrorAction SilentlyContinue
```

### 4.2 无 BOM 的 UTF-8 被按代码页 936 解析

MSVC 与 PowerShell 5.1 在中文 Windows 上都会按 936 解析无 BOM 的 UTF-8 文件，
中文注释被误读，分别导致：

- `.ps1`：`字符串缺少终止符` 之类语法错误
- `.cpp/.h`：`error C2447: "{": 缺少函数标题`、`C2059 语法错误:")"` 之类离奇错误

**所有 `.ps1` 与 `.cpp/.h` 必须存为 UTF-8 with BOM。**

> 注意：某些编辑操作会**去掉 BOM**。改完用 `fix_bom.py` / `fix_bom_sources.py` 复查。

---

## 5. 关键技术点：旋钮怎么画

`BitmapKnob::paint()` 里四步（完整代码见 `Source/BitmapKnob.cpp`）：

```cpp
// 1) 归一化取值 → 帧号
const int frameIndex = proportion * (numFrames - 1);

// 2) 尺寸由控件 rect 决定 —— 与 HISE 的根本差别
//    把素材的「内容盒」密铺到 rect，而不是让贴图尺寸说了算
const float scale = min(bounds.getWidth(), bounds.getHeight()) / contentBox;

// 3) 以控件中心为轴心旋转
g.addTransform (AffineTransform::translation (centre.x, centre.y));
g.addTransform (AffineTransform::rotation (degreesToRadians (angle)));
g.addTransform (AffineTransform::scale (scale));
g.addTransform (AffineTransform::translation (-slice.getWidth()  * 0.5f,
                                              -slice.getHeight() * 0.5f));

// 4) 绘出该帧
g.drawImage (filmstrip, slice, RectanglePlacement::stretchToFit, false);
```

**「内容盒」是什么**：贴图帧内往往有透明留白。实测：

| 素材 | 帧尺寸 | 可见范围 | 内容盒 | 若不处理 |
|---|---|---|---|---|
| `fs_pultec.png` | 160 | x[9,150] | **142** | 可见图形只占 rect 的 89%，露出底圈 |
| `fs_red/green/blue/brown.png` | 160 | x[1,158] | **158** | 基本铺满，可忽略 |

这个数值是量出来的，不是猜的。

---

## 6. 参数与素材

**参数**集中定义在 `PluginProcessor::createParameterLayout()`，用 `constexpr const char*`
常量做 ID，避免 v1 那种散落的 `setAttribute(band*5+param)` 数字契约。

旋钮是 `juce::Component` 而非 `juce::Slider`，因此不能用 `SliderAttachment`，
改用 `ParameterAttachment` 双向桥接（旋钮→参数、参数→旋钮）。

**素材**当前从磁盘加载（见 `AssetLoader`），不做二进制嵌入——因为 `juceaide binarydata`
同样受第 3 节的路径缺陷影响。加载路径按优先级：

1. 编译期宏 `BK_ASSETS_DIR`
2. 可执行文件同级 `assets\`
3. 逐级向上查找 `assets\`（最多 6 层）

素材由 `tools\sync-assets.ps1` 从 v1 工程（`编码\AnalogBlend\Images`）同步。
**正式版建议改回二进制嵌入**（发行单文件、不依赖外部路径），
`CMakeLists.txt` 末尾保留了切换说明。

---

## 7. 目录结构

```text
BK_EQ_Hybrid_v2/
├── CMakeLists.txt            ← 插件定义；素材路径 ASCII 校验
├── Source/
│   ├── BitmapKnob.h/.cpp     ← 位图旋钮（本次验证的核心）
│   ├── AssetLoader.h/.cpp    ← 素材路径解析与加载
│   ├── PluginProcessor.h/.cpp← 参数定义
│   └── PluginEditor.h/.cpp   ← 底图绘制、旋钮摆放、档位缩放
├── assets/                   ← UI 素材（7 张，由 sync-assets.ps1 从 v1 同步）
├── vendor/                   ← JUCE（由脚本下载，不入库）
└── tools/
    ├── build.ps1             ← ★ 本工程用这个（路径为纯 ASCII）
    ├── build-ascii.ps1       ← 工程路径含中文时才需要（自动镜像后构建）
    ├── fetch-juce.ps1        ← 下载并校验 JUCE
    ├── sync-assets.ps1       ← 从 v1 工程同步素材
    ├── install-vst3.ps1      ← 装到本机测试目录并留档旧版本
    ├── selfcheck.py          ← 静态自检（无法本地编译时的兜底检查）
    └── fix_bom*.py           ← 修复缺失的 UTF-8 BOM
```

---

## 8. 已知限制（骨架阶段有意为之）

| 项 | 说明 |
|---|---|
| 无 EQ 处理 | `processBlock` 只做输出增益，用于确认音频通路与参数自动化 |
| SSL 四色钮共用内容盒 158 | 先用同一数值标定；各色钮若留白不同再逐张实测 |
| `kKnob2` 的 4 档映射未定 | 91 帧素材表示 4 档，角度映射方式待定 |
| 调试标记默认开启 | 洋红框 / 青十字 / 白字是为对齐核对加的，正式版会去掉 |
| 只支持立体声 | `isBusesLayoutSupported` 目前只接受立体声进出 |

---

## 9. 下一步

骨架验证通过后正式立项：

1. 参数契约定稿（集中定义、编译期检查）
2. DSP：Pultec 8 钮 / SSL 10 钮 / PARALLEL 无极交叉 / 谐波层
3. 修 v1 遗留的 DSP 缺陷（见 [legacy/v1-FROZEN.md](legacy/v1-FROZEN.md)）
4. Bertom 做曲线验收、null test 定 Gain Staging
5. 去调试标记、接入预设与打包
