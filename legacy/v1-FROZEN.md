# ⚠️ v1 已冻结（HISE 实现）

**冻结日期**：2026-10-07
**对应 Git 标签**：`v1.0.0-hise`
**最终构建**：2026-09-29 12:06 — `BK_EQ_Hybrid.vst3`（32.9 MB）

---

## 为什么冻结

本工程用 **HISE 4.1.0** 实现，已完成以下功能（详见仓库根 `HANDOFF_EQ_Plugin.md`）：

- 6 通道并行路由：Pultec / SSL / Bleed（`RouteFX` 加法发送 + `Wet` 交叉）
- PARALLEL 无极交叉混合，`processorId/parameterId` 直连增益
- CurveEq 4+4 带 EQ 与全部面板旋钮映射
- ShapeFX 谐波层（Pultec Tanh+Bias / SSL Atan，4× 过采样 + Autogain）
- PARALLEL 推拉手势（单击 Push / 上拖 Pull / 双击循环 / 模式锁）
- VU 表头跟真实信号（41 帧金/红/残影针 + VU 阻尼）
- 一键编译安装链路 `rebuild_vst3.ps1`

**决定放弃 HISE 并完全重构**，主要原因是框架限制而非功能缺失：

1. **filmstrip 旋钮尺寸不可控** — 实测源码 `HI_LookAndFeels.cpp` 中绘制尺寸 = `帧宽 × scaleFactor`，**与控件 rect 完全无关**。左 Pultec 钮声明 116 px、实际只画 84 px，导致底图占位环（约 99 px）始终外露，旋钮对齐无法闭环。
2. **UI 表现力受限** — 无 `save/restore` 图形状态、无全局缩放 API、拖角缩放需手写坐标乘法重排。
3. **DSP 控制粒度不足** — 曲线/饱和依赖现成模块，不对称波形、比例 Q 等需要更底层的手段。

---

## 本目录的用途

**只读参考与素材来源**，不再迭代：

| 可复用内容 | 路径 |
|---|---|
| 信号流与参数契约设计 | `XmlPresetBackups/AnalogBlend.xml`、`docs/Pultec-SSL-复刻路线图.md` |
| 参数绑定与交互逻辑 | `Scripts/ScriptProcessors/AnalogBlend/Interface.js` |
| 已加工素材 | `Images/`（filmstrip、底图 bg.png、VU 表与针、色环、状态灯） |
| 编译链路参考 | `rebuild_vst3.ps1` |

**请勿在此目录继续开发。** v2 将另起目录（详见 `docs/技术选型-HISE替代方案.md`）。

---

## 已知缺陷（重构时需解决）

| # | 缺陷 | 说明 |
|---|---|---|
| 1 | 左 Pultec 旋钮偏小 | 见上文原因 1；底图 `bg.png` 内还残留红色虚线占位环未清干净 |
| 2 | Pultec 频段映射错位 | `pAtten` / `pBoost2` 写入同一参数互相抵消；频选钮与目标带不一致，低频魔术未实现 |
| 3 | SSL 高频为 HighShelf | XML 带0 `Type=2`，规格要求铃形 Peak |
| 4 | Gain Staging 存疑 | `Pultec/SSL Wet` 均 −6.02 dB 按不相关信号设计，同源相关信号半半混合理论上 ≈ +3 dB；`Output Bus` 固定 −6 dB 疑似在补这个坑，未经 null test 验证 |
| 5 | 缺比例 Q / ÷3×3 / 搁架 overshoot | 规格允许后置，始终未做 |
| 6 | 主界面 UI 预览非真实渲染 | `UI预览.png` 为合成图，不含 VU 表盘与 PARALLEL 面板，**不可用作对齐基准** |
| 7 | 浏览器验收台已失效 | 根目录 `index.html` / `gui.js` / `gui.css` 引用了大量不存在的 DOM（`modeRing`、`modeBadge` 等），无验收价值；仅 CSS 视觉可参考 |

---

## 本地测试产物

```text
E:\VST3\ReiVerb Work Shop\BK_EQ_Hybrid.vst3          ← 安装位（下次 rebuild 会重建）
E:\VST3\ReiVerb Work Shop\LEGACY\BK_EQ_Hybrid.vst3   ← 本机历史版本留存
```

`LEGACY\` 只作本机版本留存，**不参与发行**。发行产物走远端 `VST3 Plugin/<版本>/` 与 GitHub Releases。
