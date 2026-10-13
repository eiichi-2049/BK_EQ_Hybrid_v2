"""从 UI 母版素材生成工程用素材，并渲染一张 1280x720 的布局预览图。

为什么是独立的 .py 而不是写在 .ps1 里
------------------------------------
Python 3 的源码默认按 UTF-8 读取，中文可以随便写；
而 PowerShell 5.1 读取无 BOM 的 .ps1 时会按 ANSI(cp936) 解码，
中文随即乱码并引发语法错误，而且**在解析阶段就失败、代码一行都不会执行**
（所以脚本无法自保）。因此把所有含中文的逻辑放在这里，.ps1 只做纯 ASCII 转发。

按显示尺寸预降采样
------------------
SSL 旋钮母版是 802x817，而插件里只显示 92px。若把 8.7 倍降采样留给插件的
渲染后端，细部（中心白圆那圈描边、滚花纹理）会碎成明显的暗点。因此在这里
先用「预乘 alpha + 颜色扩散」高质量降到 2 倍显示尺寸。

用法
----
    python gen-assets.py --source <母版目录> --assets <素材输出目录> \
                         --root <工程根> [--clean]
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

PANEL_GREY = (203, 203, 203)


def slug(name):
    """把母版文件名映射为工程内使用的文件名。

    命名规则取自内容而非原文件名（原文件名含空格、大小写不统一），
    并保证结果是 ASCII，避免跨平台与命令行转义问题。
    """
    s = name.lower()
    if 'underlay' in s and 'main' in s:
        return 'bg.png'
    if 'uvmeter' in s or 'uv-meter' in s:
        return 'vu_meter.png'
    if 'parallel' in s:
        return 'parallel_knob.png'
    if 'niddle' in s or 'needle' in s:
        return 'needle.png'
    if 'knob' in s and 'colored' in s:
        for colour in ('red', 'green', 'blue', 'brown'):
            # 用 " COLORED <COLOUR>" 精确匹配：只判断 "colored" 会误判，
            # 因为 "colo**red**" 本身含有红这个颜色词。
            if ('colored ' + colour) in s or ('coloured ' + colour) in s:
                return 'ssl_%s.png' % colour
    return None


# 需要生成的文件及其目标尺寸（None = 保持母版原始像素）
TARGET_SIZES = {
    'bg.png': (1280, 720),
    'vu_meter.png': (235, 171),
    'parallel_knob.png': None,
    'needle.png': None,
    # 2 倍显示尺寸：兼顾 140% 缩放，同时避免 8.7 倍降采样的伪影
    'ssl_red.png': (184, 184),
    'ssl_green.png': (184, 184),
    'ssl_blue.png': (184, 184),
    'ssl_brown.png': (184, 184),
}

# ---------------------------------------------------------------- 布局参数
# 与 Source/PluginEditor.cpp 的坐标表保持一致（改一处要同步另一处）
SSL_POSITIONS = [
    ('HF dB',  (873, 192),  'ssl_red.png'),
    ('HF Hz',  (1149, 192), 'ssl_red.png'),
    ('HMF dB', (873, 314),  'ssl_green.png'),
    ('HMF Q',  (1012, 314), 'ssl_green.png'),
    ('HMF Hz', (1149, 314), 'ssl_green.png'),
    ('LMF dB', (873, 434),  'ssl_blue.png'),
    ('LMF Q',  (1012, 434), 'ssl_blue.png'),
    ('LMF Hz', (1149, 434), 'ssl_blue.png'),
    ('LF dB',  (873, 567),  'ssl_brown.png'),
    ('LF Hz',  (1150, 567), 'ssl_brown.png'),
]
SSL_DIAMETER = 92

PULTEC_POSITIONS = [
    ('R1 BOOST',     (128, 190)), ('R1 BD.WITH', (263, 190)), ('R1 SEL1', (401, 190)),
    ('R2 ATTEN',     (128, 375)), ('R2 ATTEN.SEL', (263, 375)),
    ('R3 ATTEN',     (128, 560)), ('R3 BOOST', (263, 560)),   ('R3 SEL2', (401, 560)),
]
PULTEC_DIAMETER = 100

VU_RECT = (523, 164, 235, 171)
PARALLEL_CENTRE = (640, 575)
PARALLEL_DIAMETER = 146


def resize_premultiplied(im, size, bleed=6):
    """高质量降采样：预乘 alpha + 颜色向透明区扩散。

    非预乘缩放时，完全透明处的 RGB 通常是 0（黑），与相邻不透明像素平均后
    会把黑色掺进边缘，形成黑晕与锯齿。先预乘、缩放后再除回即可消除。
    """
    src = im.convert('RGBA')

    if bleed:
        arr = np.asarray(src).astype(np.float32)
        rgb, alpha = arr[:, :, :3].copy(), arr[:, :, 3].copy()
        h, w = alpha.shape
        for _ in range(bleed):
            known = alpha > 8
            acc = np.zeros_like(rgb)
            cnt = np.zeros((h, w), np.float32)
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                sk = np.roll(known, (dy, dx), (0, 1))
                acc += np.roll(rgb, (dy, dx), (0, 1)) * sk[:, :, None]
                cnt += sk
            fill = (~known) & (cnt > 0)
            if not fill.any():
                break
            rgb[fill] = acc[fill] / cnt[fill][:, None]
            alpha[fill] = 1.0
        src = Image.fromarray(
            np.concatenate([np.clip(rgb, 0, 255), arr[:, :, 3:4]], axis=2).astype(np.uint8),
            'RGBA')

    a = np.asarray(src).astype(np.float32)
    al = a[:, :, 3:4] / 255.0
    pm = np.concatenate([a[:, :, :3] * al, a[:, :, 3:4]], axis=2).astype(np.uint8)

    r = np.asarray(Image.fromarray(pm, 'RGBA').resize(size, Image.LANCZOS)).astype(np.float32)
    ral = r[:, :, 3:4] / 255.0
    rgb_out = np.where(ral > 0, r[:, :, :3] / np.maximum(ral, 1e-6), 0.0)

    return Image.fromarray(
        np.concatenate([np.clip(rgb_out, 0, 255), r[:, :, 3:4]], axis=2).astype(np.uint8),
        'RGBA')


def build_assets(source, assets, clean):
    print('== 生成工程素材 ==')
    print('  母版：%s' % source)
    print('  输出：%s' % assets)
    os.makedirs(assets, exist_ok=True)

    # 扫描母版，按内容映射出目标文件名
    found = {}
    for name in sorted(os.listdir(source)):
        if not name.lower().endswith('.png'):
            continue
        target = slug(name)
        if target:
            found[target] = name

    if clean:
        for target in TARGET_SIZES:
            p = os.path.join(assets, target)
            if os.path.exists(p):
                os.remove(p)

    for target, size in TARGET_SIZES.items():
        src_name = found.get(target)
        if src_name is None:
            print('  [跳过] 母版里没有 %s 的来源' % target)
            continue
        src_path = os.path.join(source, src_name)
        im = Image.open(src_path).convert('RGBA')
        out = resize_premultiplied(im, size) if size else im
        out.save(os.path.join(assets, target))
        print('  %-20s <- %-28s %dx%d -> %dx%d'
              % (target, src_name, im.size[0], im.size[1], out.size[0], out.size[1]))


def render_preview(assets, root):
    """按插件真实画法合成一遍，用于打包前核对尺寸与位置。"""
    bg_path = os.path.join(assets, 'bg.png')
    if not os.path.exists(bg_path):
        print('  [跳过预览] 缺少 bg.png')
        return

    canvas = Image.open(bg_path).convert('RGBA')

    def paste_centred(img, cx, cy, w, h):
        im2 = img.resize((max(1, int(w)), max(1, int(h))), Image.LANCZOS)
        canvas.alpha_composite(im2, (int(cx - w / 2), int(cy - h / 2)))

    def load(name):
        p = os.path.join(assets, name)
        return Image.open(p).convert('RGBA') if os.path.exists(p) else None

    vu = load('vu_meter.png')
    if vu:
        x, y, w, h = VU_RECT
        paste_centred(vu, x + w / 2, y + h / 2, w, h)

    par = load('parallel_knob.png')
    if par:
        paste_centred(par, PARALLEL_CENTRE[0], PARALLEL_CENTRE[1],
                      PARALLEL_DIAMETER, PARALLEL_DIAMETER)

    # Pultec：取 filmstrip 第 45 帧（指针朝上）示意
    fs = load('fs_pultec.png')
    if fs:
        w0 = fs.size[0]
        frame = fs.crop((0, 45 * w0, w0, 46 * w0))
        for _, (cx, cy) in PULTEC_POSITIONS:
            paste_centred(frame, cx, cy, PULTEC_DIAMETER, PULTEC_DIAMETER)

    for _, (cx, cy), asset in SSL_POSITIONS:
        img = load(asset)
        if img:
            paste_centred(img, cx, cy, SSL_DIAMETER, SSL_DIAMETER)

    out = os.path.join(root, '_layout_preview.png')
    canvas.convert('RGB').save(out)
    print('  预览图 -> %s' % out)

    layout = {
        'vu': {'x': VU_RECT[0], 'y': VU_RECT[1], 'w': VU_RECT[2], 'h': VU_RECT[3]},
        'parallel': {'cx': PARALLEL_CENTRE[0], 'cy': PARALLEL_CENTRE[1], 'd': PARALLEL_DIAMETER},
        'pultecDiameter': PULTEC_DIAMETER,
        'sslDiameter': SSL_DIAMETER,
        'pultec': [{'name': n, 'cx': c[0], 'cy': c[1]} for n, c in PULTEC_POSITIONS],
        'ssl': [{'name': n, 'cx': c[0], 'cy': c[1]} for n, c, _ in SSL_POSITIONS],
    }
    with open(os.path.join(assets, 'layout.json'), 'w', encoding='utf-8') as fh:
        json.dump(layout, fh, ensure_ascii=False, indent=2)
    print('  布局数值 -> assets/layout.json')


def main():
    ap = argparse.ArgumentParser(description='Generate project assets and layout preview')
    ap.add_argument('--source', required=True, help='UI master asset directory')
    ap.add_argument('--assets', required=True, help='asset output directory')
    ap.add_argument('--root', required=True, help='project root (preview destination)')
    ap.add_argument('--clean', action='store_true', help='remove managed assets first')
    args = ap.parse_args()

    if not os.path.isdir(args.source):
        print('master directory not found: %s' % args.source, file=sys.stderr)
        return 1

    build_assets(args.source, args.assets, args.clean)
    render_preview(args.assets, args.root)

    print('')
    total = 0
    for name in sorted(os.listdir(args.assets)):
        if name.endswith('.png') or name.endswith('.json'):
            size = os.path.getsize(os.path.join(args.assets, name))
            total += size
            print('  %10d  %s' % (size, name))
    print('  ---------- total %.1f MB' % (total / 1048576.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
