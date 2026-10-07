<#
.SYNOPSIS
    下载并解包 JUCE（本机 vendor，不入库）。

.DESCRIPTION
    JUCE 体积较大且属于第三方框架，因此放在 vendor\ 并由 .gitignore 排除。

    默认使用官方 Windows 发布包（含 SHA256 校验），比 git clone 快且不受代理影响。
    下载后解包为 vendor\JUCE\，其中应包含 modules\、CMakeLists.txt 等。

.PARAMETER Version
    JUCE 版本号，默认 9.0.3。

.PARAMETER Force
    已存在时强制重新下载。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\fetch-juce.ps1
#>
[CmdletBinding()]
param(
    [string]$Version = '9.0.3',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$root      = Split-Path -Parent $PSScriptRoot
$vendorDir = Join-Path $root 'vendor'
$juceDir   = Join-Path $vendorDir 'JUCE'
$zipPath   = Join-Path $vendorDir "juce-$Version.zip"

# 已知版本的官方校验值（来自 GitHub Releases API 的 digest 字段）
$knownHash = @{
    '9.0.3' = '68831dc41adb6e16e67166ce7d51dbf7f487f4b5dcf81dd2f75fa83a7a996043'
}

Write-Host "== 准备目录 ==" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $vendorDir -Force | Out-Null
Write-Host "  $vendorDir"

if ((Test-Path (Join-Path $juceDir 'CMakeLists.txt')) -and -not $Force) {
    Write-Host "JUCE 已存在于 $juceDir（加 -Force 可重新下载）" -ForegroundColor Green
    $verLine = (Select-String -Path (Join-Path $juceDir 'CMakeLists.txt') -Pattern 'project\(JUCE VERSION' |
                Select-Object -First 1).Line
    if ($verLine) { Write-Host ("版本：" + $verLine.Trim()) }
    return
}

if ($Force -and (Test-Path $juceDir)) {
    Write-Host "== 移除旧版本 ==" -ForegroundColor Yellow
    Remove-Item $juceDir -Recurse -Force
}

Write-Host "== 下载 JUCE $Version ==" -ForegroundColor Cyan
$url = "https://github.com/juce-framework/JUCE/releases/download/$Version/juce-$Version-windows.zip"
Write-Host "  $url"

# 走系统代理设置（本机有 HTTP_PROXY 时 curl 会自动识别）
& curl.exe -L --fail --retry 2 --connect-timeout 30 -o $zipPath $url
if ($LASTEXITCODE -ne 0) {
    throw "下载失败（退出码 $LASTEXITCODE）。若用了代理，请确认代理已开启；也可手动下载后放到 $zipPath"
}
$sizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "  已下载 $sizeMB MB" -ForegroundColor Green

Write-Host "== 校验 SHA256 ==" -ForegroundColor Cyan
$actual = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLower()
if ($knownHash.ContainsKey($Version)) {
    $expected = $knownHash[$Version]
    if ($actual -ne $expected) {
        throw "校验失败！`n  期望 $expected`n  实际 $actual`n包可能损坏或被篡改，已保留在 $zipPath 供检查。"
    }
    Write-Host "  OK（与官方发布值一致）" -ForegroundColor Green
} else {
    Write-Host "  该版本无内置校验值，实际 SHA256：$actual" -ForegroundColor Yellow
}

Write-Host "== 解包 ==" -ForegroundColor Cyan
$tmp = Join-Path $vendorDir '_unzip'
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
Expand-Archive -LiteralPath $zipPath -DestinationPath $tmp -Force

# 压缩包内顶层目录名随版本变化，取第一个包含 CMakeLists.txt 的目录
$inner = Get-ChildItem $tmp -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'CMakeLists.txt') } | Select-Object -First 1
if (-not $inner) { throw "解包后未找到 JUCE 根目录（应含 CMakeLists.txt），请检查 $tmp" }

Move-Item $inner.FullName $juceDir -Force
Remove-Item $tmp -Recurse -Force
Remove-Item $zipPath -Force

Write-Host ""
Write-Host "JUCE 就绪：$juceDir" -ForegroundColor Green
Write-Host "下一步：powershell -ExecutionPolicy Bypass -File tools\build.ps1"
