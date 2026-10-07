"""POC 源码静态自检：在我无法编译的前提下，把能自动验证的部分全部验证掉。"""
import os
import re
import sys

ROOT = r'E:\个人EQ项目\编码\BK_EQ_Hybrid_v2'
problems = []


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return f.read()


def strip_noise(text):
    """去掉注释与字符串字面量，避免统计括号时被干扰。"""
    t = re.sub(r'//[^\n]*', '', text)
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    t = re.sub(r'"(?:[^"\\]|\\.)*"', '""', t)
    t = re.sub(r"'(?:[^'\\]|\\.)*'", "''", t)
    return t


print('=== 1) CMakeLists 引用的文件是否都存在 ===')
cm = read('CMakeLists.txt')
for m in sorted(set(re.findall(r'assets/([\w.]+)', cm))):
    p = os.path.join(ROOT, 'assets', m)
    ok = os.path.exists(p)
    print(f'  assets/{m:20s} exists={ok}')
    if not ok:
        problems.append(f'CMake 引用了不存在的素材 assets/{m}')

for m in sorted(set(re.findall(r'Source/([\w.]+)', cm))):
    p = os.path.join(ROOT, 'Source', m)
    ok = os.path.exists(p)
    print(f'  Source/{m:20s} exists={ok}')
    if not ok:
        problems.append(f'CMake 引用了不存在的源文件 Source/{m}')

print()
print('=== 2) 二进制资源符号：CMake 声明 vs 代码使用 ===')
src_block = cm.split('SOURCES')[1].split(')')[0]
assets = re.findall(r'assets/([\w.]+)', src_block)
syms = {a: re.sub(r'[^A-Za-z0-9]', '_', a) for a in assets}
for a, s in syms.items():
    print(f'  {a:20s} -> BkEqAssets::{s} / BkEqAssets::{s}Size')

used = set()
for f in ('Source/PluginEditor.cpp', 'Source/BitmapKnob.cpp', 'Source/PluginProcessor.cpp'):
    used |= set(re.findall(r'BkEqAssets::(\w+)', read(f)))
print('  代码中使用：', sorted(used))
for s in syms.values():
    if s not in used and (s + 'Size') not in used:
        problems.append(f'素材 {s} 已声明但代码未使用')
        print(f'  [!] {s} 未使用')
print('  （juce_add_binary_data 生成的符号名 = 文件名中的非字母数字替换为下划线）')
print()
print('=== 3) 括号配平 ===')
for f in ('Source/BitmapKnob.h', 'Source/BitmapKnob.cpp',
          'Source/PluginProcessor.h', 'Source/PluginProcessor.cpp',
          'Source/PluginEditor.h', 'Source/PluginEditor.cpp'):
    t = strip_noise(read(f))
    b = {c: t.count(c) for c in '{}()'}
    ok = b['{'] == b['}'] and b['('] == b[')']
    print(f'  {f:30s} {{}}={b["{"]}/{b["}"]}  ()={b["("]}/{b[")"]}  {"OK" if ok else "**不平衡**"}')
    if not ok:
        problems.append(f'{f} 括号不平衡')

print()
print('=== 4) 头文件声明 vs 实现定义（内联实现视为已定义）===')
for cpp, hdr in (('Source/BitmapKnob.cpp', 'Source/BitmapKnob.h'),
                 ('Source/PluginProcessor.cpp', 'Source/PluginProcessor.h'),
                 ('Source/PluginEditor.cpp', 'Source/PluginEditor.h')):
    h = read(hdr)
    c = read(cpp)
    # 形如 `返回类型 名字(...) [const] [noexcept] [override] { ... }` 的声明
    decls = set(re.findall(
        r'\b(\w+)\s*\([^;{]*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*([;{=])', h))
    ignore = {'AudioProcessor', 'Component', 'JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR',
              'getValue', 'setValue', 'setDebugOverlay', 'setIndexLabel',
              'if', 'for', 'while', 'return', 'using', 'explicit', 'static', 'constexpr', 'operator'}
    unresolved = []
    for name, terminator in sorted(decls):
        if name in ignore or name.startswith('k'):
            continue
        if terminator == '{':
            continue                      # 头文件里内联实现，无需在 cpp 中定义
        if (name + ' (') in c or (name + '(') in c or ('::' + name + ' ') in c:
            continue
        unresolved.append(name)
    print(f'  {hdr:28s} 声明 {len(decls):2d} 项   未实现: {unresolved if unresolved else "无"}')
    for m in unresolved:
        problems.append(f'{hdr} 声明的 {m}() 在 {cpp} 中未找到定义')

print()
print('=== 5) 关键 API 使用是否与 JUCE 10 前的写法一致 ===')
allcode = '\n'.join(read(f) for f in ('Source/BitmapKnob.cpp', 'Source/PluginEditor.cpp',
                                      'Source/PluginProcessor.cpp'))
checks = [
    ('addTransform',            'addTransform' in allcode),
    ('AffineTransform::rotation', 'AffineTransform::rotation' in allcode),
    ('ScopedSaveState',         'ScopedSaveState' in allcode),
    ('ImageFileFormat::loadFrom','ImageFileFormat::loadFrom' in allcode),
    ('drawImage(..., stretchToFit)', 'stretchToFit' in allcode or 'RectanglePlacement' in allcode),
    ('ParameterID{...}',        'juce::ParameterID' in allcode),
    ('SliderAttachment',        'SliderAttachment' in allcode),
    ('LinearSmoothedValue',     'LinearSmoothedValue' in allcode),
    ('FontOptions',             'FontOptions' in allcode),
]
for name, present in checks:
    print(f'  {name:32s} {"OK" if present else "**未使用**"}')

print()
print('=== 6) 参数 ID 常量：定义处 vs 引用处 ===')
ph = read('Source/PluginProcessor.h')
pc = read('Source/PluginProcessor.cpp')
pe = read('Source/PluginEditor.cpp')
ids = re.findall(r'static constexpr const char\*\s+(\w+)\s*=\s*"([^"]+)"', ph)
for const, val in ids:
    # .cpp 里以 `kKnob0` 或 `BK_EQ_HybridAudioProcessor::kKnob0` 形式引用
    in_cpp = (const in pc)
    in_editor = (const in pe)
    print(f'  {const:12s} = "{val:20s}" Processor引用={in_cpp}  Editor引用={in_editor}')
    if not in_cpp:
        problems.append(f'{const} 在 PluginProcessor.cpp 中未被引用（参数可能未注册）')
    if const.startswith('kKnob') and not in_editor:
        problems.append(f'{const} 在 PluginEditor.cpp 中未被引用（旋钮可能未摆放）')

print()
print('=== 7) 编辑器里 placements 与参数 ID 的对应 ===')
pl = re.findall(r'\{ BK_EQ_HybridAudioProcessor::(\w+),\s*(-?\d+),\s*(-?\d+),\s*(\d+) \}', pe)
print(f'  placements 条目数：{len(pl)}')
for i, (pid, x, y, d) in enumerate(pl):
    print(f'    [{i}] {pid:8s} 中心({x:>4},{y:>4}) 直径 {d}')
knob_ids = [c for c, v in ids if c.startswith('kKnob')]
placed = [p[0] for p in pl]
for c in knob_ids:
    if c not in placed:
        problems.append(f'{c} 有参数但没有摆放位置')
print(f'  参数中的旋钮：{knob_ids}')
print(f'  已摆放：{placed}')

print()
print('=== 8) 底图坐标是否在 1280x720 范围内 ===')
for pid, x, y, d in pl:
    x, y, d = int(x), int(y), int(d)
    ok = 0 <= x - d // 2 and x + d // 2 <= 1280 and 0 <= y - d // 2 and y + d // 2 <= 720
    if not ok:
        problems.append(f'{pid} 的矩形超出底图范围')
    print(f'  {pid:8s} x[{x-d//2},{x+d//2}] y[{y-d//2},{y+d//2}]  在范围内={ok}')

print()
print('=' * 60)
if problems:
    print(f'发现 {len(problems)} 个问题：')
    for p in problems:
        print('  - ' + p)
    sys.exit(1)
print('静态自检全部通过。')
