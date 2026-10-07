"""精确测量 MAIN-UI-UNDERLAY 中各元素坐标（设计坐标系 1280x720）。

关键点：用不含圆角的行/列剖面确定边界，避免圆角导致白边判定偏差；
每个元素都打印底图像素坐标与设计坐标两组值以便交叉核对。
"""
import json
import os

import numpy as np
from PIL import Image

ASSETS = r'E:\个人EQ项目\UI\素材-V2重构高清版本'
UNDERLAY = os.path.join(ASSETS, 'MAIN-UI-UNDERLAY.png')
UVMETER = os.path.join(ASSETS, 'MID-UVMETER-BOARD.png')
NEEDLE = os.path.join(ASSETS, 'MID-NIDDLE.png')
PARALLEL = os.path.join(ASSETS, 'MID-PARALLEL.png')
OUT_DIR = r'E:\BK_EQ_Hybrid_v2\assets'

DW, DH = 1280, 720


def bb_of(mask, min_area=500):
    """返回连通域的包围盒列表 [(x0,y0,x1,y1,area)]（迭代式 4 邻域）。"""
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    out = []
    ys, xs = np.nonzero(mask)
    for sy, sx in zip(ys, xs):
        if seen[sy, sx]:
            continue
        stack = [(sy, sx)]
        seen[sy, sx] = True
        x0 = x1 = sx
        y0 = y1 = sy
        area = 0
        while stack:
            cy, cx = stack.pop()
            area += 1
            x0 = min(x0, cx); x1 = max(x1, cx)
            y0 = min(y0, cy); y1 = max(y1, cy)
            for ny, nx in ((cy-1, cx), (cy+1, cx), (cy, cx-1), (cy, cx+1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        if area >= min_area:
            out.append((x0, y0, x1, y1, area))
    return out


def profile_bounds(mask, thresh_frac=0.5):
    """用行/列剖面在 50% 处截断，得到不含圆角的矩形。"""
    rows = mask.sum(axis=1).astype(float)
    cols = mask.sum(axis=0).astype(float)
    ry = np.nonzero(rows >= rows.max() * thresh_frac)[0]
    rx = np.nonzero(cols >= cols.max() * thresh_frac)[0]
    if len(ry) == 0 or len(rx) == 0:
        return None
    return rx.min(), ry.min(), rx.max(), ry.max()


def main():
    under = np.asarray(Image.open(UNDERLAY).convert('RGB')).astype(int)
    H, W = under.shape[:2]
    sx, sy = DW / W, DH / H
    print(f'底图 {W}x{H}   折算比 {sx:.6f}')

    res = {}

    # ============================================================ 1. VU 开窗
    print('\n' + '=' * 68)
    print('1) 中央 VU 开窗')
    left_end, right_start = int(W * 0.29), int(W * 0.63)
    mid = under[:, left_end:right_start]
    black = (mid.max(axis=2) < 70)
    b = profile_bounds(black)
    if b:
        x0, y0, x1, y1 = b
        x0 += left_end; x1 += left_end
        print(f'   底图像素 x[{x0},{x1}] y[{y0},{y1}]  = {x1-x0+1} x {y1-y0+1}')
        print(f'   设计坐标 x[{x0*sx:.1f},{x1*sx:.1f}] y[{y0*sy:.1f},{y1*sy:.1f}]'
              f'  = {(x1-x0+1)*sx:.1f} x {(y1-y0+1)*sy:.1f}')
        res['vuWindow'] = {
            'x': round(x0 * sx, 1), 'y': round(y0 * sy, 1),
            'w': round((x1 - x0 + 1) * sx, 1), 'h': round((y1 - y0 + 1) * sy, 1),
        }

    # ======================================================= 2. SSL 定位圆
    print('\n' + '=' * 68)
    print('2) 右侧 SSL 定位圆（灰色圆环，旋钮正常应完全遮住）')
    right = under[:, right_start:]
    rr, gg, bb_ = right[:, :, 0], right[:, :, 1], right[:, :, 2]
    # 灰环：中灰、R≈G≈B
    grey = (abs(rr - gg) < 14) & (abs(gg - bb_) < 14) & (rr > 120) & (rr < 210)
    # 填洞：先取灰度掩膜的外包围盒（按行/列剖面）
    gboxes = profile_bounds_cluster(grey) if False else None
    # 逐行扫描聚类：把 grey 掩膜按行分组
    labels = cluster_rows(grey, min_area=4000)
    print(f'   找到 {len(labels)} 个灰环')
    res['sslKnobs'] = []
    for (x0, y0, x1, y1, area) in labels:
        x0 += right_start; x1 += right_start
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        d = max(x1 - x0 + 1, y1 - y0 + 1)
        print(f'     底图中心({cx:7.1f},{cy:7.1f}) 外径{d:5d}  →  设计中心'
              f'({cx*sx:7.1f},{cy*sy:7.1f}) 直径 {d*sx:6.1f}')
        res['sslKnobs'].append({
            'cx': round(cx * sx, 1), 'cy': round(cy * sy, 1), 'd': round(d * sx, 1),
        })

    # ==================================================== 3. Pultec 定位点
    print('\n' + '=' * 68)
    print('3) 左侧 Pultec 定位点')
    leftpx = under[:, :left_end]
    # 白点：三通道都高，且要求紧凑（用腐蚀去掉细笔画文字）
    lw = (leftpx.min(axis=2) > 200)
    lw_eroded = erode(lw, 4)            # 去掉细笔画
    dots = bb_of(lw_eroded, min_area=60)
    # 只保留近似圆形的
    dots = [d for d in dots
            if 0.6 < (d[2]-d[0]+1) / max(1, (d[3]-d[1]+1)) < 1.7]
    dots.sort(key=lambda b: (round(b[1] / 100), b[0]))
    print(f'   找到 {len(dots)} 个候选点')
    res['pultecDots'] = []
    for (x0, y0, x1, y1, area) in dots:
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        d = max(x1 - x0 + 1, y1 - y0 + 1)
        print(f'     设计({cx*sx:7.1f},{cy*sy:7.1f}) 直径 {d*sx:5.1f} (底图 {d}px, area {area})')
        res['pultecDots'].append({
            'cx': round(cx * sx, 1), 'cy': round(cy * sy, 1), 'd': round(d * sx, 1),
        })

    # ==================================================== 4. PARALLEL 旋钮
    print('\n' + '=' * 68)
    print('4) 中央 PARALLEL 旋钮（底图上已烘入图形）')
    mr, mg, mb = mid[:, :, 0], mid[:, :, 1], mid[:, :, 2]
    dark = (mr < 130) & (abs(mr - mg) < 30) & (abs(mg - mb) < 30)
    dark[int(H * 0.35):int(H * 0.55), :] = False       # 排除上方 VU 开窗
    pb = bb_of(dark, min_area=int(W * H * 0.0005))
    pb.sort(key=lambda b: -b[4])
    if pb:
        x0, y0, x1, y1, area = pb[0]
        x0 += left_end; x1 += left_end
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        d = max(x1 - x0 + 1, y1 - y0 + 1)
        print(f'   底图 x[{x0},{x1}] y[{y0},{y1}] = {x1-x0+1}x{y1-y0+1}')
        print(f'   设计中心({cx*sx:.1f},{cy*sy:.1f}) 直径 {d*sx:.1f}')
        res['parallel'] = {'cx': round(cx * sx, 1), 'cy': round(cy * sy, 1),
                           'd': round(d * sx, 1)}

    # ================================================== 5. VU 表盘枢轴/弧
    print('\n' + '=' * 68)
    print('5) VU 表盘素材')
    uv = np.asarray(Image.open(UVMETER).convert('RGB')).astype(int)
    uh, uw = uv.shape[:2]
    print(f'   MID-UVMETER-BOARD {uw}x{uh}   设计尺寸 {uw/6:.1f}x{uh/6:.1f}')
    ur, ug, ub = uv[:, :, 0], uv[:, :, 1], uv[:, :, 2]
    # 枢轴：表盘中央的黑色小圆点
    dot = (ur < 90) & (ug < 90) & (ub < 90)
    dot[:int(uh * 0.42), :] = False
    dot[int(uh * 0.72):, :] = False
    dot[:, :int(uw * 0.32)] = False
    dot[:, int(uw * 0.72):] = False
    db = bb_of(dot, min_area=80)
    db.sort(key=lambda b: -b[4])
    res['vu'] = {'sourceW': uw, 'sourceH': uh}
    if db:
        x0, y0, x1, y1, area = db[0]
        pcx, pcy = (x0 + x1) / 2, (y0 + y1) / 2
        print(f'   枢轴底图({pcx:.1f},{pcy:.1f}) 直径 {max(x1-x0,y1-y0)+1}')
        print(f'   枢轴比例 fx={pcx/uw:.4f} fy={pcy/uh:.4f}')
        print(f'   枢轴在设计坐标(若素材按 {uw*sx:.0f}x{uh*sy:.0f} 摆放):'
              f' 相对 ({(pcx/uw)*uw*sx:.1f},{(pcy/uh)*uh*sy:.1f})')
        res['vu']['pivotFx'] = round(pcx / uw, 4)
        res['vu']['pivotFy'] = round(pcy / uh, 4)
    # 红区（0 以上刻度的颜色）
    red = (ur > 130) & (ur - ug > 45) & (ur - ub > 45)
    ys, xs = np.nonzero(red)
    if len(xs):
        print(f'   红区 x[{xs.min()},{xs.max()}] y[{ys.min()},{ys.max()}]')
        res['vu']['redZone'] = {'x0': int(xs.min()), 'x1': int(xs.max()),
                                'y0': int(ys.min()), 'y1': int(ys.max())}

    # ============================================================ 6. 指针
    print('\n' + '=' * 68)
    print('6) 指针素材')
    nd = np.asarray(Image.open(NEEDLE).convert('RGBA'))
    nh, nw = nd.shape[:2]
    na = nd[:, :, 3]
    bbox = Image.fromarray(na).getbbox()
    print(f'   MID-NIDDLE {nw}x{nh}  内容盒 {bbox}')
    if bbox:
        ys, xs = np.nonzero(na > 60)
        print(f'   非透明范围 x[{xs.min()},{xs.max()}] y[{ys.min()},{ys.max()}]')
        res['needle'] = {'w': nw, 'h': nh, 'content': list(bbox)}

    # ============================================================ 7. PARALLEL 素材
    par = np.asarray(Image.open(PARALLEL).convert('RGBA'))
    ph, pw = par.shape[:2]
    print(f'\n7) MID-PARALLEL {pw}x{ph}  设计尺寸 {pw/6:.1f}x{ph/6:.1f}')
    res['parallelSource'] = {'w': pw, 'h': ph}

    os.makedirs(OUT_DIR, exist_ok=True)
    out = os.path.join(OUT_DIR, 'layout.json')
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print('\n' + '=' * 68)
    print(f'已写出 {out}')


def erode(mask, n):
    """简单腐蚀 n 次（4 邻域），用于去掉细笔画。"""
    m = mask.copy()
    for _ in range(n):
        e = m.copy()
        e[1:, :] &= m[:-1, :]
        e[:-1, :] &= m[1:, :]
        e[:, 1:] &= m[:, :-1]
        e[:, :-1] &= m[:, 1:]
        e[0, :] = False; e[-1, :] = False; e[:, 0] = False; e[:, -1] = False
        m = e
    return m


def cluster_rows(mask, min_area=4000):
    """按行投影粗聚类，返回每个簇的包围盒（用于灰环这类带洞图形）。"""
    rows = np.nonzero(mask.sum(axis=1) > 0)[0]
    if len(rows) == 0:
        return []
    groups = []
    start = rows[0]
    prev = rows[0]
    for y in rows[1:]:
        if y - prev > 12:
            groups.append((start, prev))
            start = y
        prev = y
    groups.append((start, prev))

    out = []
    for (y0, y1) in groups:
        band = mask[y0:y1 + 1, :]
        cols = np.nonzero(band.sum(axis=0) > 0)[0]
        if len(cols) == 0:
            continue
        # 带内再按列分组，得到并排的多个圆
        cstart = cols[0]; cprev = cols[0]
        cgroups = []
        for x in cols[1:]:
            if x - cprev > 12:
                cgroups.append((cstart, cprev))
                cstart = x
            cprev = x
        cgroups.append((cstart, cprev))

        for (x0, x1) in cgroups:
            sub = band[:, x0:x1 + 1]
            area = int(sub.sum())
            if area >= min_area:
                ry = np.nonzero(sub.sum(axis=1) > 0)[0]
                out.append((x0, y0 + ry.min(), x1, y0 + ry.max(), area))
    return out


if __name__ == '__main__':
    main()
