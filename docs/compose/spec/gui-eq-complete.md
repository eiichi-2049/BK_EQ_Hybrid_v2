---
feature: gui-eq-complete
status: delivered
updated: 2026-09-22
branch: workdir
commits: n/a
---

# GUI 接线 + EQ 完整化

## Report

**What was built** — Pultec/SSL 全部面板旋钮已映射到 CurveEq（每路径 4 带，idx=band*5+Gain/Freq/Q/Enabled/Type）。Pultec 离散频率表 20/30/60/100 与 3–16k/5–20k，ATTEN/HF CUT 写负增益，BW→Q=3.5−bw×3.2；SSL 连续 Hz/dB/Q。PARALLEL 等增益交叉与 ShapeFX 保持不变。审查后修复 pHfBoFreq 行程死区（max 6→3）并更新 README 阶段 6 文案。

**Verification** — `rebuild_vst3.ps1` → PASS（`Loading the preset...DONE`，无 script error，安装 `E:\VST3\ReiVerb Work Shop\BK_EQ_Hybrid.vst3`）。审查 general-9：T1–T4 PASS；critical 已修后按上命令重编 PASS。

**Journey log**
1. HISE 组件必须 onInit 顶层创建；图片用 Panel.loadImage 再引用 ID
2. CurveEq Type：0=Peak, 1=LowShelf, 2=HighShelf；每带仅 5 参数
3. 离散频钮 max 必须 = 表长−1，否则 clamp 后死区
4. 失真≠EQ 曲线：Bertom 看曲线，SPAN/Plugin Doctor 看谐波
5. 子代理 bash 权限为 ask 时无法编译，rebuild 须父代理执行

## [S1] Problem
GUI 旋钮已按 PSD 布局摆放，但除 PARALLEL 外均未绑定 DSP；CurveEq 仅 3+3 性格带，无法对应完整面板。

## [S2] Design

### 信号与模块
- 6ch 并行：ch0/1 Pultec · ch2/3 SSL · ch4/5 Bleed
- Pultec Wet / SSL Wet 等增益交叉（PARALLEL 0–1）
- ShapeFX：Pultec Tanh+Bias · SSL Atan · 4× OS · Autogain
- Output Bus −6 dB（I/O 校准）
- CurveEq 每路径 4 带：`Band{i*5+0..4}` = Gain, Freq, Q, Enabled, Type
- Type：`0=Peak, 1=LowShelf, 2=HighShelf`

### Pultec EQ 映射
| UI | DSP |
|----|-----|
| pLfBoFreq | 带0 Freq，表 [20,30,60,100] |
| pLfBoAmt | 带0 Gain LowShelf 0–13.5 |
| pLfCuFreq | 带1 Freq，同表 |
| pLfCuAmt | 带1 Gain LowShelf 0→−17.5 |
| pHfBoFreq | 带2 Freq，表 [3k,5k,10k,16k]，max=3 |
| pHfBoBW | 带2 Q = 3.5−bw×3.2 |
| pHfBoAmt | 带2 Gain Peak 0–18 |
| pHfCuFreq | 带3 Freq，表 [5k,10k,20k] |
| pHfCuAmt | 带3 Gain HighShelf 0→−16 |

### SSL EQ 映射
sHf*→带0 HighShelf；sHmf*→带1 Peak；sLmf*→带2 Peak；sLf*→带3 LowShelf。Hz/dB/Q 连续映射。

### 接口
`Synth.getEffect` + `setAttribute(band*5+param)`；getEffect 失败跳过；离散表 clamp。

## [S3] Out of Scope
完整 WDF、SSL ÷3×3 开关、RMS 自动补偿、电平表、打包发行。

## Tasks
- [x] T1: XML 各 4 带 — acceptance: HISE load DONE，NumFilters=4 (covers: S2)
- [x] T2: onControl 全旋钮映射 — acceptance: 无脚本错误 (covers: S2)
- [x] T3: 离散表 + 负增益 + BW→Q — acceptance: 与 S2 一致 (covers: S2; depends: T2)
- [x] T4: rebuild 安装 — acceptance: DONE + 时间戳更新 (covers: S2; depends: T1, T2)
- [x] T5: README 标「旋钮已绑参」 — acceptance: 文案更新 (covers: S2; depends: T4)
