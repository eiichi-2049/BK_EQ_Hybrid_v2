<#
.SYNOPSIS
    编译 BK_EQ_Hybrid v2（CMake 配置 + MSBuild 构建 VST3）。

.DESCRIPTION
    流程：检查 JUCE → CMake 生成 VS 工程 → MSBuild 编译 → 报告产物路径。

    若加 -Install，编译后调用 install-vst3.ps1 复制到本机 VST3 测试目录
    （E:\VST3\ReiVerb Work Shop\），并把旧版本留档到 LEGACY\。

.PARAMETER Config
    Release（默认）或 Debug。

.PARAMETER Install
    编译后安装到本机测试目录。

.PARAMETER Clean
    先删除 build 目录再来（CMake 配置异常时用）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Install
#>
[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Release',
    [switch]$Install,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build'
$juceDir  = Join-Path $root 'vendor\JUCE'

# ---- 工具定位 ----------------------------------------------------------------
function Find-CMake {
    $cmd = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # VS 自带
    $candidates = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -ErrorAction SilentlyContinue
    if ($candidates) { return $candidates[0].FullName }
    throw 'CMake 未找到。请安装 CMake，或安装 Visual Studio（其自带 CMake）。'
}
function Find-MSBuild {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
        if ($p -and (Test-Path $p)) { return $p }
    }
    $candidates = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe' -ErrorAction SilentlyContinue
    if ($candidates) { return $candidates[0].FullName }
    throw 'MSBuild 未找到。请确认已安装 Visual Studio 的 C++ 桌面开发工作负载。'
}

Write-Host '== 0/4 环境检查 ==' -ForegroundColor Cyan

# --- 规范化环境变量 -----------------------------------------------------------
# Windows 环境变量名大小写不敏感，但环境块里可以同时存在 no_proxy 与 NO_PROXY。
# 这种重复会让 MSBuild 在插入环境字典时抛：
#   MSB6001 ... System.ArgumentException: 已添加项。字典中的关键字:"NO_PROXY"...
# 症状是 CMake 报 "No CMAKE_C_COMPILER could be found"，非常难查。
# 代理软件常同时写入多种大小写形式，这里主动清掉重复的小写项。
$removed = @()
foreach ($name in @('http_proxy', 'https_proxy', 'no_proxy', 'all_proxy')) {
    if ((Test-Path "Env:$name") -and (Test-Path "Env:$($name.ToUpper())")) {
        Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue
        $removed += $name
    }
}
if ($removed.Count) {
    Write-Host "  已清理重复大小写的环境变量：$($removed -join ', ')" -ForegroundColor Yellow
}

if (-not (Test-Path (Join-Path $juceDir 'CMakeLists.txt'))) {
    throw "未找到 JUCE：$juceDir`n请先运行：powershell -ExecutionPolicy Bypass -File tools\fetch-juce.ps1"
}

# --- ASCII 素材路径（规避 juceaide 的中文路径缺陷） ---------------------------
# JUCE 9 的 juceaide 在路径含非 ASCII 字符时会抛 "Unhandled exception"：
# CMake 把路径按系统 ANSI 代码页写进 Defs.txt / Info.txt / input_file_list，
# 而 juceaide 按 UTF-8 读，字节对不上就解析失败。
# 症状是 MSB8066：binarydata / header / rcfile 三个自定义命令全部失败
# （version 模式只打印字符串，所以它正常，容易误判为 juceaide 没坏）。
#
# 对策：用一个纯 ASCII 的目录联接（junction）指向本项目，
# 素材路径经由该联接传给编译期定义，于是中文不会再进入 CMake 生成的文件。
$asciiLink = 'E:\BKv2assets'
if (-not (Test-Path $asciiLink)) {
    cmd /c mklink /J "$asciiLink" $root 2>&1 | Out-Null
}
if (Test-Path $asciiLink) {
    Write-Host "  ASCII 素材联接：$asciiLink -> $root"
} else {
    Write-Host "  警告：无法创建目录联接 $asciiLink，若素材路径含中文将导致编译失败" -ForegroundColor Yellow
}
$cmake   = Find-CMake
$msbuild = Find-MSBuild
Write-Host "  CMake   : $cmake"
Write-Host "  MSBuild : $msbuild"

Write-Host '== 1/4 素材准备 ==' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'sync-assets.ps1')
if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) { throw '素材同步失败' }

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host '== 清理 build 目录 ==' -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

Write-Host '== 2/4 CMake 配置 ==' -ForegroundColor Cyan
& $cmake -S $root -B $buildDir -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host '  生成器不可用，退回默认生成器重试' -ForegroundColor Yellow
    & $cmake -S $root -B $buildDir -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败（退出码 $LASTEXITCODE）" }
}

Write-Host "== 3/4 编译 ($Config|x64) ==" -ForegroundColor Cyan

# CMake 4 生成的是新版 XML 解决方案 .slnx（MSBuild 18 支持），
# 旧版 CMake 生成 .sln。两者都兼容，优先取 .slnx。
$solution = Get-ChildItem $buildDir -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in '.slnx', '.sln' } |
            Sort-Object { if ($_.Extension -eq '.slnx') { 0 } else { 1 } } |
            Select-Object -First 1
if (-not $solution) { throw "未找到解决方案文件（.slnx / .sln），请检查 $buildDir" }
Write-Host "  解决方案：$($solution.Name)"

& $msbuild $solution.FullName `
    /p:Configuration=$Config /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "编译失败（退出码 $LASTEXITCODE）" }

Write-Host '== 4/4 产物 ==' -ForegroundColor Cyan
$vst3 = Get-ChildItem $buildDir -Recurse -Filter 'BK_EQ_Hybrid_v2.vst3' -ErrorAction SilentlyContinue |
        Select-Object -First 1
if (-not $vst3) { throw "未找到 .vst3 产物，请检查 $buildDir" }
Write-Host ("  {0}" -f $vst3.FullName) -ForegroundColor Green

if ($Install) {
    Write-Host '== 安装到本机测试目录 ==' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'install-vst3.ps1') -Source $vst3.FullName -Label 'v2-poc'
}

Write-Host ''
Write-Host '编译完成。' -ForegroundColor Green
Write-Host "在 DAW 里把插件路径指向：$(Split-Path -Parent $vst3.FullName)"
Write-Host '或加 -Install 复制到 E:\VST3\ReiVerb Work Shop\'
exit 0
