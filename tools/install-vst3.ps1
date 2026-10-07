<#
.SYNOPSIS
    把编译出的 VST3 安装到本机测试目录，并把被替换的旧版本留档到 LEGACY。

.DESCRIPTION
    本机约定（与 v1 一致）：

      E:\VST3\ReiVerb Work Shop\
        ├── BK_EQ_Hybrid_v2.vst3              当前安装位
        └── LEGACY\
            ├── BK_EQ_Hybrid_v2.vst3          上一个版本
            └── BK_EQ_Hybrid_v2_<标签>.vst3   更早的历史版本

    若 DAW 正占用目标会失败，请先在 DAW 卸载插件或退出 DAW。

.PARAMETER Source
    待安装的 .vst3（文件或目录）。默认自动在 build\ 下查找。

.PARAMETER Label
    留档标签，默认时间戳。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\install-vst3.ps1
#>
[CmdletBinding()]
param(
    [string]$Source,
    [string]$Label
)

$ErrorActionPreference = 'Stop'
$root       = Split-Path -Parent $PSScriptRoot
$installDir = 'E:\VST3\ReiVerb Work Shop'
$target     = Join-Path $installDir 'BK_EQ_Hybrid_v2.vst3'

# 历史版本必须放在 **VST3 扫描路径之外**。
# 曾经把 LEGACY 放在 $installDir 下，结果 Ableton 把每一份副本都当插件扫描：
#   check plugin at path: "...\BK_EQ_Hybrid_v2.vst3"
#   check plugin at path: "...\LEGACY\BK_EQ_Hybrid_v2.vst3"          ← 同 ID 冲突
#   check plugin at path: "...\LEGACY\BK_EQ_Hybrid_v2_poc-*.vst3"    ← 同 ID 冲突
# 三份副本共享同一个插件 ID，宿主会加载其中任意一份（往往是旧的）。
$legacyDir  = 'E:\VST3_LEGACY\ReiVerb Work Shop'
$legacyCur  = Join-Path $legacyDir  'BK_EQ_Hybrid_v2.vst3'

if (-not $Label) { $Label = Get-Date -Format 'yyyyMMdd-HHmmss' }

if (-not $Source) {
    # 只认本项目产物，避免误抓到 build 目录里可能存在的其它插件
    $found = Get-ChildItem (Join-Path $root 'build') -Recurse -Filter 'BK_EQ_Hybrid_v2.vst3' -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $found) { throw '未找到 BK_EQ_Hybrid_v2.vst3，请先编译（tools\build.ps1）或用 -Source 指定。' }
    $Source = $found.FullName
}

function Get-PathSize($p) {
    $i = Get-Item -LiteralPath $p -Force
    if ($i.PSIsContainer) {
        return (Get-ChildItem -LiteralPath $p -Recurse -File -Force | Measure-Object Length -Sum).Sum
    }
    return $i.Length
}

Write-Host '== 1/4 校验源 ==' -ForegroundColor Cyan
if (-not (Test-Path -LiteralPath $Source)) { throw "源不存在：$Source" }
Write-Host ("  {0}  ({1:N1} MB)" -f $Source, ((Get-PathSize $Source)/1MB))

Write-Host '== 2/4 旧版本留档 ==' -ForegroundColor Cyan
New-Item -ItemType Directory -Path $legacyDir -Force | Out-Null
if (Test-Path -LiteralPath $target) {
    $archived = Join-Path $legacyDir ("BK_EQ_Hybrid_v2_{0}.vst3" -f $Label)
    if (Test-Path -LiteralPath $archived) { throw "留档文件已存在，请换 -Label：$archived" }
    Copy-Item -LiteralPath $target -Destination $archived -Recurse -Force
    Write-Host ("  已留档：{0}" -f (Split-Path -Leaf $archived)) -ForegroundColor Green
} else {
    Write-Host '  安装位当前无文件，跳过留档' -ForegroundColor Yellow
}

Write-Host '== 3/4 安装 ==' -ForegroundColor Cyan
if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
try {
    Copy-Item -LiteralPath $Source -Destination $target -Recurse -Force
} catch {
    throw "复制失败，目标可能被 DAW 占用。请在 DAW 卸载插件或退出后重试。`n原始错误：$($_.Exception.Message)"
}
Write-Host ("  已安装：{0}" -f $target) -ForegroundColor Green

Write-Host '== 4/4 存为「上一个版本」 ==' -ForegroundColor Cyan
if (Test-Path -LiteralPath $legacyCur) { Remove-Item -LiteralPath $legacyCur -Recurse -Force }
Copy-Item -LiteralPath $target -Destination $legacyCur -Recurse -Force
Write-Host ("  已更新：{0}" -f $legacyCur) -ForegroundColor Green

Write-Host ''
Write-Host '完成。' -ForegroundColor Green
