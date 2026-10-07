<#
.SYNOPSIS
    一步到位的环境准备脚本：拉取 JUCE → 同步素材 → 编译（可选安装）。

.DESCRIPTION
    这条命令把 POC 从零跑通。首次运行会下载约 42 MB 的 JUCE，
    之后重跑会跳过下载。

.PARAMETER Install
    编译后安装到 E:\VST3\ReiVerb Work Shop\（并把旧版本留档到 LEGACY\）。

.PARAMETER Config
    Release（默认）或 Debug。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup.ps1 -Install
#>
[CmdletBinding()]
param(
    [switch]$Install,
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$tools = $PSScriptRoot

Write-Host '############ 步骤 1/3：拉取 JUCE ############' -ForegroundColor Magenta
& (Join-Path $tools 'fetch-juce.ps1')

Write-Host ''
Write-Host '############ 步骤 2/3：静态自检 ############' -ForegroundColor Magenta
$py = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
if (-not $py) {
    $cand = 'C:\Users\Eiichi\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe'
    if (Test-Path $cand) { $py = $cand }
}
if ($py) {
    $env:PYTHONIOENCODING = 'utf-8'
    & $py (Join-Path $tools 'selfcheck.py')
    if ($LASTEXITCODE -ne 0) { throw '静态自检未通过，请先修正上面的问题' }
} else {
    Write-Host '  未找到 python，跳过静态自检（不影响编译）' -ForegroundColor Yellow
}

Write-Host ''
Write-Host '############ 步骤 3/3：编译 ############' -ForegroundColor Magenta
$buildArgs = @('-Config', $Config)
if ($Install) { $buildArgs += '-Install' }
& (Join-Path $tools 'build.ps1') @buildArgs

Write-Host ''
Write-Host '全部完成。' -ForegroundColor Green
