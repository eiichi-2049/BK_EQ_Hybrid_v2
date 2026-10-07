"""给 v2 的 C++ 源码加 UTF-8 BOM。

原因：MSVC 在中文 Windows 上默认按代码页 936 解析无 BOM 的 UTF-8 文件，
源码里的中文注释会被误读，进而产生大量离奇语法错误
（例如 error C2447: "{": 缺少函数标题）。带 BOM 即被正确识别为 UTF-8。
"""
import os

ROOTS = [
    r'E:\BK_EQ_Hybrid_v2\Source',
]

count = 0
for root in ROOTS:
    for name in sorted(os.listdir(root)):
        if not name.endswith(('.cpp', '.h')):
            continue
        p = os.path.join(root, name)
        raw = open(p, 'rb').read()
        text = raw.decode('utf-8-sig')
        open(p, 'wb').write(b'\xef\xbb\xbf' + text.encode('utf-8'))
        count += 1
        print(f'  BOM 已确保: {name}')

print(f'完成，共处理 {count} 个源码文件。')
