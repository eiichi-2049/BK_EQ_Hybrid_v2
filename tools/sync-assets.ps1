<#
.SYNOPSIS
    从已冻结的 v1 工程同步 UI 素材到 v2 的 assets\ 目录。

.DESCRIPTION
    v1（编码\AnalogBlend\Images）的贴图是已验证可用的素材，v2 直接复用，
    不重复加工。源目录保持不变，只做只读复制。

    说明：assets\ 里的图是「开发用副本」。正式发行时若要让插件体积更小，
    可只保留实际用到的图，或改为运行时从外部加载。

.PARAMETER Source
    v1 素材目录，默认 ..\..\AnalogBlend\Images（相对本脚本）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\sync-assets.ps1
#>
[CmdletBinding()]
param(
    [string]$Source
)

$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'

if (-not $Source) {
    $Source = Join-Path $root '..\AnalogBlend\Images'
}
$Source = (Resolve-Path -LiteralPath $Source -ErrorAction Stop).Path

Write-Host '== 同步 UI 素材 ==' -ForegroundColor Cyan
Write-Host "  源：$Source"
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null

# 当前 POC 需要的素材（后续按需扩充）
$wanted = @(
    'bg.png',           # 底图 1280x720
    'fs_pultec.png',    # 左 Pultec 黑钮 filmstrip（91 帧纵向）
    'fs_red.png',       # SSL 红钮
    'fs_green.png',     # SSL 绿钮
    'fs_blue.png',      # SSL 蓝钮
    'fs_brown.png'      # SSL 棕钮
)

$copied = 0
foreach ($name in $wanted) {
    $src = Join-Path $Source $name
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Host "  跳过（源缺失）：$name" -ForegroundColor Yellow
        continue
    }
    Copy-Item -LiteralPath $src -Destination (Join-Path $assetDir $name) -Force
    $kb = [math]::Round((Get-Item -LiteralPath $src).Length / 1KB)
    Write-Host "  $name  ($kb KB)"
    $copied++
}

Write-Host "  共 $copied 个文件 → $assetDir" -ForegroundColor Green
exit 0
