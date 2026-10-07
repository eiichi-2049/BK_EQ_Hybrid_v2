<#
.SYNOPSIS
    从 UI 母版素材生成工程用素材（缩放 + 规范化命名）。

.DESCRIPTION
    用户在 E:\个人EQ项目\UI\素材-V2重构高清版本 提供的是高分辨率母版
    （底图 7680x4320，其余按原始像素）。本脚本把它们缩放到 1280x720
    设计分辨率后放入 assets\，供插件运行时加载。

    assets\ 是生成产物，重新生成即可，不必手工维护。

.PARAMETER Source
    母版目录，默认 E:\个人EQ项目\UI\素材-V2重构高清版本。

.PARAMETER Clean
    生成前清空 assets\ 中由本脚本管理的文件（保留 layout.json）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build-assets.ps1
#>
[CmdletBinding()]
param(
    [string]$Source = 'E:\个人EQ项目\UI\素材-V2重构高清版本',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'

if (-not (Test-Path -LiteralPath $Source)) {
    throw "母版目录不存在：$Source"
}

Write-Host '== 生成工程素材 ==' -ForegroundColor Cyan
Write-Host "  母版：$Source"
Write-Host "  输出：$assetDir"
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null

# 优先使用带 Pillow 的运行时：系统 PATH 里的 python 往往没有装 PIL
$py = $null
$candidates = @(
    'C:\Users\Eiichi\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe',
    (Get-Command python.exe -ErrorAction SilentlyContinue).Source
)
foreach ($cand in $candidates) {
    if (-not $cand -or -not (Test-Path $cand)) { continue }
    & $cand -c 'import PIL' 2>$null
    if ($LASTEXITCODE -eq 0) { $py = $cand; break }
}
if (-not $py) { throw '未找到带 Pillow 的 Python，无法处理图像' }
Write-Host "  Python: $py"

# 母版文件 -> 输出文件 的映射。
# 底图缩到 1280x720；元件按其设计尺寸缩放（设计坐标 = 母版像素 / 6）。
$script = @"
import os
from PIL import Image

SRC = r'$Source'
DST = r'$assetDir'
CLEAN = $($Clean.IsPresent)

# (输出名, 源文件名, 目标宽或 None 表示保持原比例, 目标高或 None)
JOBS = [
    ('bg.png',              'MAIN-UI-UNDERLAY.png',  1280, 720),
    ('vu_meter.png',        'MID-UVMETER-BOARD.png', 235,  171),
    ('parallel_knob.png',   'MID-PARALLEL.png',      None, None),
    ('needle.png',          'MID-NIDDLE.png',        None, None),
]

if CLEAN:
    for name, *_ in JOBS:
        p = os.path.join(DST, name)
        if os.path.exists(p):
            os.remove(p)
            print('  已移除旧文件', name)

for out_name, src_name, tw, th in JOBS:
    src = os.path.join(SRC, src_name)
    if not os.path.exists(src):
        print('  [跳过] 源缺失', src_name)
        continue
    im = Image.open(src).convert('RGBA')
    w, h = im.size
    if tw and th:
        im2 = im.resize((tw, th), Image.LANCZOS)
    else:
        im2 = im
    dst = os.path.join(DST, out_name)
    im2.save(dst)
    print(f'  {out_name:20s} <- {src_name:26s} {w}x{h} -> {im2.size[0]}x{im2.size[1]}  ({os.path.getsize(dst)//1024} KB)')

print('素材生成完成。')
"@
$tmp = Join-Path $env:TEMP 'gen_assets.py'
[System.IO.File]::WriteAllText($tmp, $script, (New-Object System.Text.UTF8Encoding($false)))
$env:PYTHONIOENCODING = 'utf-8'
& $py $tmp
if ($LASTEXITCODE -ne 0) { throw "素材生成失败（退出码 $LASTEXITCODE）" }
Remove-Item $tmp -Force

Write-Host ''
Get-ChildItem $assetDir -File | ForEach-Object { "  {0,10:N0}  {1}" -f $_.Length, $_.Name }
