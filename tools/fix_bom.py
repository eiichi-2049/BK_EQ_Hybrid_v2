"""Repair: 把无 BOM 的 UTF-8 .ps1 补上 BOM（PowerShell 5.1 需要它才能正确解析中文）。"""
import sys

paths = [
    r'E:\BK_EQ_Hybrid_v2\tools\build.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\build-ascii.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\fetch-juce.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\install-vst3.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\setup.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\sync-assets.ps1',
    r'E:\BK_EQ_Hybrid_v2\tools\selfcheck.py',
    r'E:\个人EQ项目\tools\check-repo-hygiene.ps1',
    r'E:\个人EQ项目\tools\install-vst3.ps1',
]

for p in paths:
    try:
        raw = open(p, 'rb').read()
    except FileNotFoundError:
        print(f'  skip (missing): {p}')
        continue

    had_bom = raw.startswith(b'\xef\xbb\xbf')
    # 解码时容忍已有 BOM
    text = raw.decode('utf-8-sig')
    try:
        text.encode('utf-8')
    except UnicodeEncodeError as e:
        print(f'  !! {p} 解码后仍含非法字符: {e}')
        continue

    open(p, 'wb').write(b'\xef\xbb\xbf' + text.encode('utf-8'))
    print(f'  {"已有" if had_bom else "补上"} BOM: {p.split(chr(92))[-1]}')

print('完成。')
