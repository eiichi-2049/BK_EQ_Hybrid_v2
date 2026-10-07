---
feature: vu-meter-audio
status: delivered
updated: 2026-09-27
branch: n/a-not-a-git-repo
commits:
---

# VU Meter Audio Response

## Report

**What was built** — VU 表头用 MAIN METER BOARD _ V8 去针后作表盘（−15…+3 + 红弧），UI/导出/指针.png 烘成 41 帧金/红/残影 filmstrip。Interface.js 在 30ms timer 上读 outputBus.getCurrentLevel()，按 0 VU=−18 dBFS 换算，并做 VU 式阻尼（起约 100ms / 落约 300ms）；帧内两根半透明残影 + 主针，≥0 时切红针。

**Verification** — 
ebuild_vst3.ps1 通过并安装到 E:\VST3\ReiVerb Work Shop\BK_EQ_Hybrid.vst3；离线合成 VU_check_scale.png / VU_check_redzone.png 确认 −20…+3 角度递增、红区红针。

**Journey log** — HISE Graphics 无 save/restore，改预烘焙 needle filmstrip 用 drawImage yOffset 切帧；global 不可用，状态放 paint 内 
eg；V8 去针须收紧金色掩膜以免弄脏红弧。

## [S1] Problem

插件中 VU 表头目前只是静态贴图加一根矩形条，不能正确反映音频电平；需要做成带刻度、金针、红区变色、轻微残影，并按 VU 阻尼响应 `getCurrentLevel()` 的真表头。

## [S2] Design

### 素材（沿用工程原件）

| 用途 | 路径 | 说明 |
|------|------|------|
| 表盘 | `UI/_CENTER/DASHBOARD/MAIN METER BOARD _ V8.png` | 551×398，−15…+3 刻度 + 红弧 0…+3 + OL，需把烘死的金针补掉 |
| 表针 | `UI/导出/指针.png` | 113×144，金色针，轴心在针根圆点 |
| 红针 | 由 `指针.png` 色相/通道生成 | 进入红区（≥0）时使用 |

不引入 mvMeter2 的图片；只借用其 **VU Standard 阻尼** 思路（约 300ms 级平滑）。

### 显示几何

- 控件：`vu` Panel `(524,163) 268×172`（`scaleFactor` 不适用；`Panel` 自绘）
- 底图：`vu_meter.png` ← 由 V8 去针后缩放到面板尺寸
- 针：以 V8 上针根为轴心，映射到面板坐标；转角覆盖 −15…+3
- 刻度：**−15, −10, −5, 0, +1, +2, +3**（V8 原图）；红弧 0…+3；右端 OL 点

### 电平与阻尼（算法）

1. `lv = outputBus.getCurrentLevel()` → 线性 0…1（可能 >1，需夹紧）
2. `db = 20*log10(max(lv, 1e-5))`；0 VU 对齐 **−18 dBFS**（常见校准）：`vu = db + 18`
3. 显示值 `state` 做 VU 式阻尼（`dt ≈ 30ms` timer）：
   - 向上（attack）更快，向下（release）略慢
   - `coef = 1 - exp(-dt / tau)`；`tau_up ≈ 0.10s`，`tau_down ≈ 0.30s`
   - `state += (vu - state) * coef`
4. 将 `state` 夹到 **[-20, +3]** 再线性映射到指针角（−15 刻度对应盘面左端，+3 对应右端）

### 指针绘制（HISE `vu.setPaintRoutine`）

1. 用 `g.drawImage` / 仿射变换画 `指针.png`，或对针做旋转贴图（`save/rotate/drawImage/restore` 若 API 允许；否则预先烘焙少量帧）
2. **残影**：保留 2 个历史 `state`，画 2 根低透明度针（alpha ≈ 0.25 / 0.12）
3. **红针过载**：`state >= 0` 时用红色版针；否则金色
4. 表盘背景由 `vu_meter.png` 在 Panel 上 `setImage` 显示；paint 只画针与残影

### 对齐

- 针根与 V8 针根同心（去针后仍用原针根坐标）
- 角度 0 VU ≈ V8 图中金针当前姿态（略偏右竖直）

## [S3] Out of Scope

- 多主题表盘、PPM/K-Meter 切换、GUI 缩放联动
- 真峰值保持 LED、OL 熄灭逻辑（OL 点只作表盘装饰）
- DSP 侧校准菜单（固定 −18 dBFS = 0 VU）

## Tasks

- [x] T1: 从 V8 生成无针表盘 `vu_meter.png` 并替换 — acceptance: 面板显示完整 −15…+3 刻度且无旧金针 (covers: S2 素材/显示几何)
- [x] T2: 生成红色指针图与残影绘制参数 — acceptance: 同一针有金/红两态 (covers: S2 指针绘制)
- [x] T3: Interface.js 接入电平→dB→VU→阻尼→角度，并画针+残影+红变色 — acceptance: 播放音频时针随电平摆动，0 以上变红，停止后回落 (covers: S2 电平与阻尼, 指针绘制)
- [x] T4: rebuild_vst3 编译安装并在合成预览核对 — acceptance: VST3 安装成功，预览图表盘/针几何正确 (covers: S2, S3 边界)
