<#
.SYNOPSIS
    在纯 ASCII 路径下镜像本项目并编译，然后把产物回收。

.DESCRIPTION
    规避 JUCE 9 juceaide 的中文路径缺陷。

    juceaide 无法处理含非 ASCII 字符的路径：CMake 把工程路径按系统 ANSI
    代码页写进 Defs.txt / Info.txt / input_file_list，而 juceaide 按 UTF-8
    读取，字节对不上即抛 "Unhandled exception"。症状为 MSB8066，且
    binarydata / header / rcfile 三个自定义命令全部失败。

    因此工程放在含中文的路径下无法编译。本脚本把工程镜像到纯 ASCII 路径
    后在那里构建，assets 以目录联接指回真实目录，素材只维护一份。

    更彻底的做法：把整个工程放到纯 ASCII 路径，即可直接用 build.ps1。

.PARAMETER MirrorPath
    镜像位置，默认 E:\BK_EQ_Hybrid_v2_ascii（必须纯 ASCII）。

.PARAMETER Config
    Release（默认）或 Debug。

.PARAMETER Clean
    先清空镜像里的 build 目录。

.PARAMETER Install
    编译后装到本机 VST3 测试目录。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build-ascii.ps1 -Install
#>
[CmdletBinding()]
param(
    [string]$MirrorPath = 'E:\BK_EQ_Hybrid_v2_ascii',
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Release',
    [switch]$Clean,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$root       = Split-Path -Parent $PSScriptRoot
$assetsReal = Join-Path $root 'assets'

if (($MirrorPath -replace '[^!-~]', '') -ne $MirrorPath) {
    throw "镜像路径必须只含 ASCII 字符：$MirrorPath"
}

# 清理重复大小写的代理变量（否则 MSBuild 抛 MSB6001）
foreach ($name in @('http_proxy', 'https_proxy', 'no_proxy', 'all_proxy')) {
    if ((Test-Path "Env:$name") -and (Test-Path "Env:$($name.ToUpper())")) {
        Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue
    }
}

Write-Host '== 1/5 建立 ASCII 镜像 ==' -ForegroundColor Cyan
Write-Host "  源　：$root"
Write-Host "  镜像：$MirrorPath"
New-Item -ItemType Directory -Path $MirrorPath -Force | Out-Null

Get-ChildItem $root -Force |
    Where-Object { $_.Name -notin @('build', 'assets', '.git') -and $_.Extension -ne '.log' } |
    ForEach-Object { Copy-Item $_.FullName -Destination $MirrorPath -Recurse -Force }

# assets 用目录联接，避免两份素材不同步
$assetsMirror = Join-Path $MirrorPath 'assets'
if (Test-Path $assetsMirror) {
    $attr = (Get-Item $assetsMirror -Force).Attributes
    if (-not ($attr -band [System.IO.FileAttributes]::ReparsePoint)) {
        Write-Host '  镜像里的 assets 是副本，改为目录联接' -ForegroundColor Yellow
        Remove-Item $assetsMirror -Recurse -Force
    }
}
if (-not (Test-Path $assetsMirror)) {
    cmd /c mklink /J "$assetsMirror" "$assetsReal" | Out-Null
}
if (-not (Test-Path (Join-Path $assetsMirror 'bg.png'))) {
    throw "素材联接不可用：$assetsMirror"
}
Write-Host "  素材联接：$assetsMirror" -ForegroundColor Green

function Find-CMake {
    $c = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $c = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($c) { return $c.FullName }
    throw 'CMake 未找到'
}
function Find-MSBuild {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
        if ($p -and (Test-Path $p)) { return $p }
    }
    $c = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($c) { return $c.FullName }
    throw 'MSBuild 未找到'
}

Write-Host '== 2/5 同步素材 ==' -ForegroundColor Cyan
& (Join-Path $root 'tools\sync-assets.ps1')

$buildDir = Join-Path $MirrorPath 'build'
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host '== 清理镜像 build 目录 ==' -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

Write-Host '== 3/5 CMake 配置 ==' -ForegroundColor Cyan
$cmake = Find-CMake
Write-Host '== 3/5 CMake 配置 ==' -ForegroundColor Cyan
$cmake = Find-CMake

# 显式传入镜像里的 ASCII 素材路径。
# 不依赖 CMake 缓存——缓存优先级高于 CMakeLists 里的默认值，
# 曾因此把素材路径留在一个已删除的旧目录，插件运行时全黑且难以察觉。
$asciiAssets = ($assetsMirror -replace '\\', '/')
Write-Host "  素材路径定义：BK_ASSETS_DIR=$asciiAssets"

& $cmake -S $MirrorPath -B $buildDir -G 'Visual Studio 18 2026' -A x64 -DBK_ASSETS_DIR="$asciiAssets"
if ($LASTEXITCODE -ne 0) {
    Write-Host '  指定生成器不可用，退回默认生成器' -ForegroundColor Yellow
    & $cmake -S $MirrorPath -B $buildDir -A x64 -DBK_ASSETS_DIR="$asciiAssets"
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败（退出码 $LASTEXITCODE）" }
}

# 校验实际写入编译定义的值，避免又出现陈旧缓存
$vcx = Join-Path $buildDir 'BK_EQ_Hybrid_v2.vcxproj'
if (Test-Path $vcx) {
    $m = [regex]::Match((Get-Content $vcx -Raw), 'BK_ASSETS_DIR=\\?"([^"\\]+)\\?"')
    if ($m.Success) {
        $actual = $m.Groups[1].Value
        Write-Host "  编译定义实际值：$actual"
        if ($actual -ne $asciiAssets) {
            throw "编译定义与预期不一致（缓存陈旧？）。`n  预期：$asciiAssets`n  实际：$actual`n请删除镜像 build 目录后重试：Remove-Item '$buildDir' -Recurse -Force"
        }
    }
}

Write-Host "== 4/5 编译 ($Config|x64) ==" -ForegroundColor Cyan
$msbuild = Find-MSBuild
$solution = Get-ChildItem $buildDir -File | Where-Object { $_.Extension -in '.slnx', '.sln' } |
            Sort-Object { if ($_.Extension -eq '.slnx') { 0 } else { 1 } } | Select-Object -First 1
if (-not $solution) { throw "未找到解决方案文件，请检查 $buildDir" }
Write-Host "  $($solution.Name)"

& $msbuild $solution.FullName /p:Configuration=$Config /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "编译失败（退出码 $LASTEXITCODE）" }

Write-Host '== 5/5 回收产物 ==' -ForegroundColor Cyan
$built = Get-ChildItem $buildDir -Recurse -Filter 'BK_EQ_Hybrid_v2.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $built) { throw "未找到 .vst3 产物，请检查 $buildDir" }

$destDir = Join-Path $root 'build'
New-Item -ItemType Directory -Path $destDir -Force | Out-Null
$dest = Join-Path $destDir 'BK_EQ_Hybrid_v2.vst3'
if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
Copy-Item $built.FullName $dest -Recurse -Force
Write-Host "  $dest" -ForegroundColor Green

if ($Install) {
    # 标签带时间戳：固定标签在重复构建时会与已有留档重名而报错
    & (Join-Path $root 'tools\install-vst3.ps1') -Source $dest -Label ("poc-" + (Get-Date -Format 'MMdd-HHmm'))
}

Write-Host ''
Write-Host '完成。' -ForegroundColor Green
Write-Host "镜像目录：$MirrorPath"
