# 🎛️ BK_EQ_Hybrid ( VST3 )

<img width="1920" height="1200" alt="HI V11" src="https://github.com/user-attachments/assets/fd48f158-58b0-471c-a268-a534c33c6cd2" />

一款结合了 **SSL** 与 **Pultec** 声音特点的均衡器。

---

## 状态 / Status

> [!IMPORTANT]
> **本项目正在重构中，尚无可用发行版。**
> 重构完成后会在此页与 [Releases](../../releases) 发布 VST3。

当前代码是重构后的**骨架阶段**：界面框架已经跑通，音频处理部分尚未接入。
开发笔记与构建说明见 [DEVELOPMENT.md](DEVELOPMENT.md)。

---

## 界面DEMO / Interface DEMO

<img width="1280" height="720" alt="equi" src="https://github.com/user-attachments/assets/85bb467b-affa-4c24-9230-d5273c823ad0" />

**持续推进中。**

---

## 灵感 / Inspiration

在经历了一段时间的 Vibe "Learning" 之后，我决定做一款属于自己的插件。那么第一个插件要制作什么呢？想了很久，最后还是回到了最经典、也最常用的工具——**EQ**。

**BK_EQ_Hybrid** 的灵感，来自我最喜欢的**两架传奇硬件**：

| 风格 | 特点 |
|:------:|:------:|
|  **Pultec Passive EQ 1A** | **大线条，大范围**的调整带来的温暖、厚实 |
|  **SSL 4000 E Series Channel Strip** | **直接、干净、带推感**的英式风格总线塑形能力 |

**它们并不"绝对精准"，但却能让声音变得好听。**

---

## 特性 / Features

-  **风格混搭** — 两种性格并联，中间 **PARALLEL** 无极连续混合；也可 **SOLO** 单用一侧，当作纯 Pultec 或纯 SSL EQ
-  **图形UI** — 操作界面直观，所见即所得；支持 60 / 80 / 100 / 120 / 140% 缩放
-  **开箱即用** — VST3 格式，无需繁琐安装
-  **完全免费** — 不绑定 iLok，无订阅

---

## 下载 / Download

重构完成后，前往 [Releases](../../releases) 页面下载 VST3。

---

## 使用 / Use

你需要一个 DAW（数字音频工作站）来作为载体使用。

免费 DAW —— [Reaper](https://www.reaper.fm/index.php)

将 `.vst3` 文件放入 `C:\Program Files\Common Files\VST3`，DAW 会自动扫描并识别。

---

## 从源码构建 / Build

需要 Windows + Visual Studio 2026（含 C++ 桌面开发工作负载）。

```powershell
git clone https://github.com/eiichi-2049/BK_EQ_Hybrid_v2.git
cd BK_EQ_Hybrid_v2

# 取 JUCE（约 42 MB，自动校验官方 SHA256）
powershell -ExecutionPolicy Bypass -File tools\fetch-juce.ps1

# 编译
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

技术栈：**JUCE 9** · C++17 · CMake · MSVC v145。目标平台 Windows x64 / VST3。

> [!NOTE]
> **工程路径不能含中文。** JUCE 的 `juceaide` 无法处理非 ASCII 路径，
> 会以 `MSB8066` 的形式失败且很难定位。详见 [DEVELOPMENT.md](DEVELOPMENT.md)。

---

## 协议 / License

本项目仅供学习交流使用，请勿用于商业用途。

用到的字体文件（有修改）：

[Protest](https://www.dafont.com/protest.font)

Matisse Pro EB

使用到的 Github 项目：

- [JUCE](https://github.com/juce-framework/JUCE) — 插件框架（AGPLv3 / 商业双授权）

---

## 支持我 / Support Me

如果 BKEQ_Hybrid 恰好打动了你，或者你只是单纯喜欢这个奇怪的小 EQ，那么你可以考虑支持我。

---
  <br>
  <i> Just break the silence.</i><br><br>
</p>
