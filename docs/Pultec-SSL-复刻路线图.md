# BK_EQ_Hybrid — Pultec / SSL 复刻路线图

依据调研（Pultec EQP-1A · SSL 4000 G 292 · Plugin Architecture）整理，对齐项目「轻量复刻」决策。

---

## 0. Gain Staging（硬约束 · 最高优先级）

**输入多少 dB，输出尽量保持多少 dB。**

| 规则 | 说明 |
|------|------|
| Unity through | 全程 ≈ 0 dB 通过；插件不是「更响」或「更软」的工具 |
| I/O 校准 | **Output Bus 当前 −6 dB 作 I/O 校准**；目标输入≈输出 |
| 不做 makeup | 阶段 5 的 RMS 补偿 **暂缓**；先保证链路本身不改电平 |
| 饱和层 | 必须 **Autogain=1**（ShapeFX 已开），加染不加响 |
| PARALLEL 交叉 | 0.5 处等增益求和 ≈ 0 dB；0/1 端点单路 ≈ 0 dB |
| Bleed | 很小（−24 dB），不得抬整体响度 |
| 验收 | 旁通对比响度差 **< ~0.3 dB**；有条件做 sine / null test |

响度偏移先修 Autogain / 交叉曲线 / 校准值，**不加输出补偿增益**。

---

## 1. 两种「味」的验收标准

| 维度 | Pultec 管味 | SSL 运放味 |
|------|-------------|------------|
| 主导谐波 | **2 次（偶次）** | **3 次 + 5 次（奇次）** |
| 形态 | 软拐点、圆（管推挽 + 变压器） | 硬拐点、冲（运放削波） |
| 瞬态 | 慢、圆 | 快、冲 |
| 频谱着色 | 中低变厚 ~200–400 Hz | 中高存在感 ~2–5 kHz |
| 目标算法 | `tanh()` soft-knee | `atan()` 或不对称 waveshaper |
| 常态 | EQ 关也带色（管+牛常开） | 几乎透明，推了才有 grit |

**架构铁律**：谐波发生器永不真旁路；Mix=0 只是 wet 置 0；固定 bleed 常在；唯一真旁通 = DAW。

**概念**：**失真 ≠ EQ 曲线**。Bertom 看曲线，SPAN / Plugin Doctor 看谐波。

---

## 2. 目标信号流

```text
Input → Split 3 份（无输入增益）
  ├─ Dry ───────────────────────────────┐
  ├─ Pultec Sat (tanh, 2nd) → Mix A ────┼→ [Sum] → Output Bus (−6 dB 校准) → Out
  └─ SSL Sat (atan, 3rd/5th) → Mix B ───┘
         └─ Fixed Bleed (−24 dB) 常开 ──┘
```

现有 6 通道映射：`To SSL`/`To Bleed` = Split；`Pultec Wet`/`SSL Wet` = Mix A/B（**processorId 绑定**）；`Bleed Sat`+`Bleed Level` = Fixed Bleed。

---

## 3. 分阶段

### 阶段 2 · 谐波层（当前 · ShapeFX）

| 模块 | 曲线 | 参数 |
|------|------|------|
| Pultec Sat | Tanh + Bias 0.22 | Gain +9 dB · OS 4× · Autogain |
| SSL Sat | Atan | Gain +5 dB · OS 4× · Autogain |
| Bleed Sat | Tanh | Level −24 dB |

HighPass 20 Hz（HISE 下限）+ 内置 DC Remover。4× OS。

### 阶段 3 · Pultec EQ（CurveEq · 性格/互补曲线）

LF 搁架 Boost 20/30/60/100 Hz +13.5 / Cut 同频 −17.5；HF 钟形+Bandwidth 3–16 kHz +18；HF Cut 5/10/20 kHz −16。

**低频魔术**：同频 Boost+Cut → **~80 Hz 隆起 + ~200 Hz 舀空**（Boost 略强且中心微偏）。

- Type 枚举：`Peak=0` · `LowShelf=1` · `HighShelf=2`
- 每带 **5 参数**
- **状态**：CurveEq 性格曲线已有，完整旋钮映射进行中

### 阶段 4 · SSL EQ（灵魂三件套 · CurveEq）

LF 搁架 30–450 Hz ±17；LMF 200–2500 Hz ±15，**÷3**；HMF 600–7000 Hz ±15，**×3**；HF 搁架 1.5–16 kHz ±17。

1. **比例 Q**：增益越大 Q 越窄（0.1→3.5）
2. **LMF ÷3 / HMF ×3**
3. **搁架 overshoot**（搁架 + 微 peaking 逼近）

不加 HPF/LPF（决策 #5）。

- Type 枚举：`Peak=0` · `LowShelf=1` · `HighShelf=2`
- 每带 **5 参数**
- **状态**：CurveEq 性格曲线已有，完整旋钮映射进行中

### 阶段 5 · 音量补偿（校准中）

按 §0 Gain Staging：目标输入≈输出。**Output Bus 当前 −6 dB 作 I/O 校准**。RMS makeup 仅当 Autogain + 等增益交叉仍压不住系统性响度偏移时再启用。  
验收：旁通响度差 < ~0.3 dB + Mix=0 **null test** 应静音。

### 阶段 6 · 界面（进行中）

GUI **1280×720**：左 Pultec 8 旋钮 / 中 PARALLEL + 状态灯 / 右 SSL 10 旋钮 + 外圈。  
filmstrip 已嵌入；**旋钮布局完成、绑定进行中**。PARALLEL 无极交叉 + processorId 绑定 Pultec Wet / SSL Wet。

### 阶段 7–8 · 测试 / 打包

扫频 / THD / 听感（注意：失真 ≠ EQ 曲线）→ 预设打包。

---

## 4. 轻量 vs 完整

采用轻量：CurveEq 逼近 EQ + ShapeFX tanh/atan。完整 WDF（EQP-WDF-1A / chowdsp_wdf / Koren 管模型）仅当听感不够再开。

参考：Gyraf/Purple 原理图、Barrera SMC 2024、EQP-WDF-1A、chowdsp_wdf、pywdf、RBJ Cookbook。

---

## 5. 建议顺序

1. Gain Staging 验收（旁通等响、Mix=0 null；核对 Output Bus −6 dB 校准）
2. 试听定味（气质差 ≠ 失真量差）
3. Pultec EQ + 低频魔术（完整旋钮映射）
4. SSL EQ + 比例 Q + ÷3/×3（完整旋钮映射）
5. 4× 过采样核对 / 延迟对齐
6. 界面绑定 / 测试 / 打包

---

## 6. 概念备忘

| 概念 | 说明 |
|------|------|
| 失真 ≠ EQ 曲线 | Bertom 看曲线，SPAN / Plugin Doctor 看谐波 |
| CurveEq | 性格/互补曲线；Type Peak=0 / LowShelf=1 / HighShelf=2；每带 5 参数 |
