<#
.SYNOPSIS
    从 UI 母版素材生成工程用素材 + 一张 1280x720 布局预览图。

.DESCRIPTION
    母版在 E:\个人EQ项目\UI\素材-V2重构高清版本（底图 7680x4320，其余原始像素）。
    本脚本把它们缩放到 1280x720 设计分辨率后放入 assets\，并渲染
    _layout_preview.png —— 按插件真实画法把底图、旋钮、VU 表合成一遍。

    预览图的用途：在打包前直接核对尺寸与位置，不必每次装进 DAW 试。

.PARAMETER Source
    母版目录。默认 E:\个人EQ项目\UI\素材-V2重构高清版本

.PARAMETER Clean
    生成前清空 assets\ 中由本脚本管理的文件。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build-assets.ps1 -Clean
#>
[CmdletBinding()]
param(
    [string]$Source = 'E:\个人EQ项目\UI\素材-V2重构高清版本',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'

if (-not (Test-Path -LiteralPath $Source)) { throw "母版目录不存在：$Source" }

Write-Host '== 生成工程素材 ==' -ForegroundColor Cyan
Write-Host "  母版：$Source"
Write-Host "  输出：$assetDir"
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null

# 选带 Pillow 的 Python（系统 PATH 里的常没装 PIL）
$py = $null
foreach ($cand in @(
    'C:\Users\Eiichi\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe',
    (Get-Command python.exe -ErrorAction SilentlyContinue).Source)) {
    if (-not $cand -or -not (Test-Path $cand)) { continue }
    & $cand -c 'import PIL' 2>$null
    if ($LASTEXITCODE -eq 0) { $py = $cand; break }
}
if (-not $py) { throw '未找到带 Pillow 的 Python' }
Write-Host "  Python: $py"

$script = @"
import os, json
from PIL import Image, ImageDraw

SRC = r'$Source'
DST = r'$assetDir'
ROOT = r'$root'
CLEAN = $($Clean.IsPresent)

# ---------------------------------------------------------------- 素材生成
# (输出名, 源文件名, 目标尺寸或 None 保持原样)
JOBS = [
    ('bg.png',            'MAIN-UI-UNDERLAY.png',       (1280, 720)),
    ('vu_meter.png',      'MID-UVMETER-BOARD.png',      (235, 171)),
    ('parallel_knob.png', 'MID-PARALLEL.png',           None),
    ('needle.png',        'MID-NIDDLE.png',             None),
    ('ssl_red.png',       'SSL KNOB COLORED RED.png',   None),
    ('ssl_green.png',     'SSL KNOB COLORED GREEN.png', None),
    ('ssl_blue.png',      'SSL KNOB COLORED BLUE.png',  None),
    ('ssl_brown.png',     'SSL KNOB COLORED BROWN.png', None),
]

if CLEAN:
    for name, *_ in JOBS:
        p = os.path.join(DST, name)
        if os.path.exists(p): os.remove(p)

for out_name, src_name, size in JOBS:
    src = os.path.join(SRC, src_name)
    if not os.path.exists(src):
        print('  [跳过] 源缺失 ' + src_name)
        continue
    im = Image.open(src).convert('RGBA')
    im2 = im.resize(size, Image.LANCZOS) if size else im
    dst = os.path.join(DST, out_name)
    im2.save(dst)
    print('  %-20s <- %-26s %dx%d -> %dx%d' % (out_name, src_name, im.size[0], im.size[1], im2.size[0], im2.size[1]))

# ---------------------------------------------------------------- 布局参数
# 与 Source/PluginEditor.cpp 的坐标表保持一致（改一处要同步另一处）
SSLPOS = [
    ('HF dB',  (873, 192),  'ssl_red.png'),
    ('HF Hz',  (1149,192),  'ssl_red.png'),
    ('HMF dB', (873, 314),  'ssl_green.png'),
    ('HMF Q',  (1012,314),  'ssl_green.png'),
    ('HMF Hz', (1149,314),  'ssl_green.png'),
    ('LMF dB', (873, 434),  'ssl_blue.png'),
    ('LMF Q',  (1012,434),  'ssl_blue.png'),
    ('LMF Hz', (1149,434),  'ssl_blue.png'),
    ('LF dB',  (873, 567),  'ssl_brown.png'),
    ('LF Hz',  (1150,567),  'ssl_brown.png'),
]
SSL_D = 92          # SSL 旋钮外接矩形边长（设计像素）

PULTEC = [
    ('R1 BOOST',    (128, 190)), ('R1 BD.WITH', (263, 190)), ('R1 频选', (401, 190)),
    ('R2 ATTEN',    (128, 375)), ('R2 ATTEN.SEL', (263, 375)),
    ('R3 ATTEN',    (128, 560)), ('R3 BOOST', (263, 560)),   ('R3 频选', (401, 560)),
]
PULTEC_D = 100

VU = (523, 164, 235, 171)

# ---------------------------------------------------------------- 渲染预览
canvas = Image.open(os.path.join(DST, 'bg.png')).convert('RGBA')
draw = ImageDraw.Draw(canvas, 'RGBA')

def paste_centred(img, cx, cy, w, h):
    im2 = img.resize((max(1,int(w)), max(1,int(h))), Image.LANCZOS)
    canvas.alpha_composite(im2, (int(cx - w/2), int(cy - h/2)))

# VU 表
if os.path.exists(os.path.join(DST,'vu_meter.png')):
    paste_centred(Image.open(os.path.join(DST,'vu_meter.png')).convert('RGBA'),
                  VU[0]+VU[2]/2, VU[1]+VU[3]/2, VU[2], VU[3])

# PARALLEL
if os.path.exists(os.path.join(DST,'parallel_knob.png')):
    paste_centred(Image.open(os.path.join(DST,'parallel_knob.png')).convert('RGBA'), 640, 575, 146, 146)

# Pultec：取 filmstrip 第 45 帧（指针朝上）示意
fsPath = os.path.join(DST, 'fs_pultec.png')
if os.path.exists(fsPath):
    fs = Image.open(fsPath).convert('RGBA')
    w0 = fs.size[0]
    frame = fs.crop((0, 45*w0, w0, 46*w0))
    for name, (cx, cy) in PULTEC:
        paste_centred(frame, cx, cy, PULTEC_D, PULTEC_D)
        draw.ellipse([cx-PULTEC_D/2, cy-PULTEC_D/2, cx+PULTEC_D/2, cy+PULTEC_D/2],
                     outline=(255,0,255,220), width=1)

# SSL
for name, (cx, cy), asset in SSLPOS:
    p = os.path.join(DST, asset)
    if not os.path.exists(p): continue
    paste_centred(Image.open(p).convert('RGBA'), cx, cy, SSL_D, SSL_D)
    draw.ellipse([cx-SSL_D/2, cy-SSL_D/2, cx+SSL_D/2, cy+SSL_D/2],
                 outline=(0,255,255,220), width=1)

out = os.path.join(ROOT, '_layout_preview.png')
canvas.convert('RGB').save(out)
print('  预览图 -> ' + out)

layout = {
    'vu': {'x': VU[0], 'y': VU[1], 'w': VU[2], 'h': VU[3]},
    'parallel': {'cx': 640, 'cy': 575, 'd': 146},
    'pultecDiameter': PULTEC_D,
    'sslDiameter': SSL_D,
    'pultec': [{'name': n, 'cx': c[0], 'cy': c[1]} for n, c in PULTEC],
    'ssl':    [{'name': n, 'cx': c[0], 'cy': c[1]} for n, c, _ in SSLPOS],
}
json.dump(layout, open(os.path.join(DST, 'layout.json'), 'w', encoding='utf-8'),
          ensure_ascii=False, indent=2)
print('  布局数值 -> assets/layout.json')
"@
$tmp = Join-Path $env:TEMP 'gen_assets.py'
[System.IO.File]::WriteAllText($tmp, $script, (New-Object System.Text.UTF8Encoding($false)))
$env:PYTHONIOENCODING = 'utf-8'
& $py $tmp
if ($LASTEXITCODE -ne 0) { throw "素材生成失败（退出码 $LASTEXITCODE）" }
Remove-Item $tmp -Force

Write-Host ''
Get-ChildItem $assetDir -File | ForEach-Object { "  {0,10:N0}  {1}" -f $_.Length, $_.Name }
