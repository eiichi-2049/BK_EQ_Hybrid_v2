<#
.SYNOPSIS
    生成 BuildInfo.h —— 把构建时间与布局版本号编进插件，显示在界面角落。

.DESCRIPTION
    目的：当「重新扫描后界面没变」时，能立刻判断加载的到底是哪一份二进制。
    没这个标识时，只能靠肉眼比对界面细节，容易误判成「没生效」。

    界面左下角会显示  <构建时间> · <布局版本>。
    每次构建自动刷新，所以只要时间变了，就说明新二进制已被加载。

.PARAMETER Layout
    布局版本标识，便于区分结构变化。默认 L4。
#>
[CmdletBinding()]
param(
    [string]$Layout = 'L4'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $root 'Source\BuildInfo.h'

$stamp = Get-Date -Format 'MM-dd HH:mm:ss'
$content = @"
#pragma once

// 本文件由 tools\write-build-info.ps1 在每次构建前重新生成，请勿手工编辑。
// 界面左下角会显示这个标识，用于判断当前加载的是哪一份二进制。

#define BK_BUILD_STAMP  "$stamp"
#define BK_LAYOUT_TAG   "$Layout"
"@

[System.IO.File]::WriteAllText($out, $content, (New-Object System.Text.UTF8Encoding($true)))
Write-Host "  BuildInfo.h -> $stamp / $Layout" -ForegroundColor DarkGray
