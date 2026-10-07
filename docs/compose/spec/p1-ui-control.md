---
feature: p1-ui-control
status: delivered
updated: 2026-09-27
branch: n/a-not-a-git-repo
commits:
---

# P1 UI Control — Push/Pull + Corner Zoom

## Report

**What was built** — (1) PARALLEL 改为 ScriptPanel：单击=Push（蓝环/Pultec 100%），上拖=Pull（紫环/SSL 100%），Normal 红环可纵向拧混合；双击循环三模式，Push/Pull 下旋转被模式锁忽略。(2) 右下角 zoomGrip 拖角对角缩放 0.75–1.35，双击回 100%；启动不调 applyZoom。

**Verification** — rebuild_vst3.ps1 导出无脚本错误并安装 E:\VST3\ReiVerb Work Shop\BK_EQ_Hybrid.vst3。

**Journey log** — setMouseCallback 必须用 inline function（匿名 function 不被当成回调）；drawImage 固定 4 参；变量名勿用 mode（组件属性冲突）；reg/local 作用域与 HISE 解析器对 callback 判定不一致。

## [S1] Problem

1. PARALLEL 只能旋钮交叉混合，缺少 Push（Pultec 100%）/ Pull（SSL 100%）手势与模式色环，可发现性差。
2. 右下角缩放已有 `applyZoom`，但仍是旋钮式，需真·拖角缩放，且启动不得自触发。

## [S2] Design

### 2.1 PARALLEL 推拉（ScriptPanel）

**状态**

| 模式 | 色环 | 路由 |
|------|------|------|
| Normal | **红** | `v` 可调：Pultec `1−v` + SSL `v`（与现 `applyParallelMix` 相同） |
| Push | **蓝** | Pultec 100%，SSL 旁通（`v=0`） |
| Pull | **紫** | SSL 100%，Pultec 旁通（`v=1`） |

**手势**（`parallelPanel`，`setMouseCallback`）

| 手势 | 条件 | 行为 |
|------|------|------|
| 单击 | `mouseUp` 且位移 < 6px 且非 doubleClick | → **Push** |
| 上拖 | `isDragOnly` 且 `dragY ≤ -18` | → **Pull** |
| 旋转/拖动 | Normal 且纵向位移 | 调 `v`（0=Pultec，1=SSL） |
| 双击 | `doubleClick` | 循环 Normal→Push→Pull→Normal |
| 模式锁 | Push/Pull 下 | **忽略旋转**，防误拧；双击才换模式 |

**绘制**（`setPaintRoutine`）

1. 底：`fs_parallel.png` 当前 `v` 对应帧（或 Panel 叠在原旋钮上）
2. 色环：圆环 `setColour` + 线宽，按模式 红 `0xFFC43B3B` / 蓝 `0xFF3B6FC4` / 紫 `0xFF8B4BC4`
3. 模式锁时环外侧加虚线/点环提示

**数据**

- 隐藏 `parallelValue` Knob（0–1，`saveInPreset`）保存混合比
- 模式 `reg PAR_MODE`（0/1/2），Push/Pull 时写 `v=0/1` 并调用 `applyParallelMix`
- 与现有 `pultecGain`/`sslGain` 绑定不变（0=左/Pultec）

### 2.2 拖角缩放

- 控件：`zoomGrip` Panel `(1215,665) 55×55`，`setMouseCallback`
- **拖动**：用 `dragX+dragY` 的对角增量 → `z = clamp(1.0 + (dx+dy)/400, 0.75, 1.35)`
- **双击**：回到 1.0
- 只在拖动/双击里调 `applyZoom()`；**onInit / 启动路径禁止调用**
- `uiScale` 隐藏 Knob 仅存 `z`（`saveInPreset`），不再当旋钮用
- `applyZoom` 现有坐标乘法保持；`zoomHint` 显示百分比

### 2.3 公共

- 状态灯 `showStatus` 在 Push=left、Pull=right、Normal=mix（沿用区间逻辑）
- 全部改动在 `Interface.js`；不改 DSP XML

## [S3] Out of Scope

- 外部预设写入模式字段（仅 `parallelValue`/`uiScale` 进 preset）
- 触摸/多点；动画补间
- 缩放到 1.35 以上或窗口真分辨率切换

## Tasks

- [x] T1: ScriptPanel 推拉 + 色环 + 模式锁 — acceptance: 单击=Push 蓝环，上拖=Pull 紫环，双击循环，Push/Pull 下拧不改混合 (covers: S2.1)
- [x] T2: 拖角缩放 — acceptance: 拖角连续缩放 0.75–1.35，双击复位，启动布局不跑偏 (covers: S2.2)
- [x] T3: rebuild 安装并合成预览 — acceptance: 编译成功，预览/截图可核对手势目标 (covers: S2.1, S2.2)
