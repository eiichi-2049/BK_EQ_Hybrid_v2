<#
.SYNOPSIS
    Generate project assets from the UI master artwork, then patch the
    background and render a 1280x720 layout preview.

.DESCRIPTION
    NOTE: This file is intentionally ASCII-only.

    PowerShell 5.1 decodes a BOM-less .ps1 as ANSI (cp936 here), which turns
    any non-ASCII text into mojibake and raises a parse error. A parse error
    happens BEFORE any statement runs, so a script cannot repair its own
    encoding. All Chinese-bearing logic therefore lives in gen-assets.py and
    patch-bg-vu.py (Python 3 source is UTF-8 by default), and this wrapper
    stays pure ASCII.

    Steps:
      1. tools/gen-assets.py   - scale master artwork into assets\
      2. tools/patch-bg-vu.py  - remove the baked-in VU placeholder rectangle
      3. self-check            - fail loudly if the patch did not take effect

.PARAMETER Source
    UI master artwork directory.

.PARAMETER Clean
    Remove managed assets before generating.
#>
[CmdletBinding()]
param(
    [string]$Source = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'

# Default master directory. The path contains non-ASCII characters, so it is
# built from code points to keep this file ASCII-only.
if (-not $Source) {
    $Source = Join-Path ('E:\' + [char]0x4E2A + [char]0x4EBA + 'EQ' + [char]0x9879 + [char]0x76EE) `
                       ('UI\' + [char]0x7D20 + [char]0x6750 + '-V2' + [char]0x91CD + [char]0x6784 + [char]0x9AD8 + [char]0x6E05 + [char]0x7248 + [char]0x672C)
}

if (-not (Test-Path -LiteralPath $Source)) {
    throw "Master artwork directory not found: $Source"
}

Write-Host '== Generate project assets ==' -ForegroundColor Cyan

# Pick a Python that has Pillow (the one on PATH often does not).
$py = $null
$candidates = @(
    'C:\Users\Eiichi\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe'
)
$onPath = Get-Command python.exe -ErrorAction SilentlyContinue
if ($onPath) { $candidates += $onPath.Source }

foreach ($cand in $candidates) {
    if (-not $cand -or -not (Test-Path -LiteralPath $cand)) { continue }
    & $cand -c 'import PIL' 2>$null
    if ($LASTEXITCODE -eq 0) { $py = $cand; break }
}
if (-not $py) { throw 'No Python with Pillow found.' }
Write-Host "  Python: $py" -ForegroundColor DarkGray

# ---------------------------------------------------------------- 1. assets
$genArgs = @(
    (Join-Path $PSScriptRoot 'gen-assets.py'),
    '--source', $Source,
    '--assets', $assetDir,
    '--root',   $root
)
if ($Clean) { $genArgs += '--clean' }

& $py @genArgs
if ($LASTEXITCODE -ne 0) { throw "Asset generation failed (exit $LASTEXITCODE)." }

# ---------------------------------------------------------------- 2. patch
# Regenerating bg.png brings back the baked-in VU placeholder rectangle.
# This step is idempotent: with a clean master it changes nothing.
Write-Host '== Patch VU placeholder area ==' -ForegroundColor Cyan
& $py (Join-Path $PSScriptRoot 'patch-bg-vu.py') --assets $assetDir
if ($LASTEXITCODE -ne 0) { throw "Background patch failed (exit $LASTEXITCODE)." }

# ---------------------------------------------------------------- 3. verify
# Guard against "the step ran but had no effect": previously a lost BOM made
# this script abort before the patch, and the black rectangle shipped all the
# way into the plugin, only to be spotted by eye inside the DAW.
$verify = @'
import sys
import numpy as np
from PIL import Image
bg = sys.argv[1]
a = np.asarray(Image.open(bg).convert('RGB')).astype(float)
print(int((a[150:345, 470:805].mean(axis=2) < 90).sum()))
'@
$vtmp = Join-Path $env:TEMP 'bk_verify_bg.py'
[System.IO.File]::WriteAllText($vtmp, $verify, (New-Object System.Text.UTF8Encoding($false)))
$remain = (& $py $vtmp (Join-Path $assetDir 'bg.png') | Select-Object -Last 1).Trim()
Remove-Item $vtmp -Force

if ($remain -ne '0') {
    throw "Background patch had no effect: $remain dark pixels remain (expected 0)."
}
Write-Host '  Verified: placeholder area cleared (0 dark pixels).' -ForegroundColor Green
