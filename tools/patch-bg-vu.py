"""在 bg.png 上抹掉中央 VU 开窗的黑色残留，改用面板灰填充。

背景
----
底图 MAIN-UI-UNDERLAY 的中央有一块黑色矩形（VU 表的占位残留），
实测范围约 x[486,792] y[163,334] = 307x172，而用户设计的 VU 表
只有 236 宽。黑区比表宽 71px，因此无论怎么摆放，左右都会露出黑块。

处理
----
把该区域内的暗色像素用面板灰填充（带轻微渐变与纹理，避免出现
一块死板的纯色矩形）。VU 表仍居中绘制在其上，于是露出的是面板灰
而不是黑色。

若日后用户重导母版并去掉这块黑，本修补自动失效——因为检测不到
暗色像素时不会做任何改动。
"""
import os
import numpy as np
from PIL import Image

ASSETS = r'E:\BK_EQ_Hybrid_v2\assets'
BG = os.path.join(ASSETS, 'bg.png')

# 先备份一次原图（只备份第一次）
BACKUP = os.path.join(ASSETS, '_bg_original.png')

# 黑区搜索范围（比实测略放宽，确保覆盖边缘的柔和过渡）
SEARCH = dict(x0=470, x1=805, y0=150, y1=345)
# 判定为「残留黑」的亮度阈值
DARK_MAX = 90

# 面板参考色采样位置（中央面板内、VU 区之外的干净区域）
PANEL_SAMPLES = [(500, 400), (770, 400), (500, 600), (770, 600)]


def main():
    if not os.path.exists(BG):
        raise SystemExit(f'找不到 {BG}')

    if not os.path.exists(BACKUP):
        Image.open(BG).save(BACKUP)
        print(f'  已备份原图 -> {os.path.basename(BACKUP)}')

    im = Image.open(BG).convert('RGB')
    a = np.asarray(im).astype(np.float32)
    h, w = a.shape[:2]

    # --- 面板参考色 ---------------------------------------------------------
    cols = []
    for (x, y) in PANEL_SAMPLES:
        if 0 <= x < w and 0 <= y < h:
            cols.append(a[y, x])
    panel = np.mean(cols, axis=0)
    print('  面板参考色 RGB = (%.0f, %.0f, %.0f)' % tuple(panel))

    # --- 暗色掩膜 -----------------------------------------------------------
    s = SEARCH
    sub = a[s['y0']:s['y1'], s['x0']:s['x1']]
    lum = sub.mean(axis=2)
    dark = lum < DARK_MAX
    n = int(dark.sum())
    print(f'  搜索区内暗色像素 {n} 个')

    if n == 0:
        print('  未发现残留黑块，无需修补（母版可能已修好）。')
        return

    ys, xs = np.nonzero(dark)
    bx0, bx1 = int(xs.min()) + s['x0'], int(xs.max()) + s['x0']
    by0, by1 = int(ys.min()) + s['y0'], int(ys.max()) + s['y0']
    print(f'  黑块范围 x[{bx0},{bx1}] y[{by0},{by1}] = {bx1-bx0+1}x{by1-by0+1}')

    # --- 用面板灰 + 轻渐变 + 轻纹理填充 --------------------------------------
    rng = np.random.default_rng(20261013)       # 固定种子，保证可复现
    bh, bw = by1 - by0 + 1, bx1 - bx0 + 1

    # 垂直方向轻微渐变（上略亮，模拟面板受光）
    grad = np.linspace(1.035, 0.965, bh).reshape(bh, 1, 1)

    # 细颗粒纹理，幅度 ±1.5，避免死板
    noise = rng.normal(0.0, 1.5, size=(bh, bw, 1))

    patch = panel.reshape(1, 1, 3) * grad + noise

    # 只在「暗色像素」处替换，保留区域内原本就亮的像素（例如刻度、文字）
    region = a[by0:by1 + 1, bx0:bx1 + 1]
    region_lum = region.mean(axis=2)
    replace = region_lum < DARK_MAX
    region[replace] = patch[replace]
    a[by0:by1 + 1, bx0:bx1 + 1] = region

    # --- 边缘柔化：对黑块外围 2px 做加权混合，避免留下硬边 -------------------
    for d in (1, 2):
        alpha = 0.55 if d == 1 else 0.25
        for yy in (by0 - d, by1 + d):
            if 0 <= yy < h:
                row = a[yy, bx0:bx1 + 1]
                row_lum = row.mean(axis=1, keepdims=True)
                m = (row_lum < DARK_MAX + 30)
                row[m[:, 0]] = panel * (1 - alpha) + row[m[:, 0]] * alpha
        for xx in (bx0 - d, bx1 + d):
            if 0 <= xx < w:
                col = a[by0:by1 + 1, xx]
                col_lum = col.mean(axis=1, keepdims=True)
                m = (col_lum < DARK_MAX + 30)
                col[m[:, 0]] = panel * (1 - alpha) + col[m[:, 0]] * alpha

    out = np.clip(a, 0, 255).astype(np.uint8)
    Image.fromarray(out).save(BG)

    # --- 复核 ---------------------------------------------------------------
    check = np.asarray(Image.open(BG).convert('RGB')).astype(np.float32)
    sub2 = check[s['y0']:s['y1'], s['x0']:s['x1']]
    left = int((sub2.mean(axis=2) < DARK_MAX).sum())
    print(f'  修补后同一区域暗色像素 {left} 个（原 {n} 个）')
    print(f'  已写回 {os.path.basename(BG)}')


if __name__ == '__main__':
    main()
