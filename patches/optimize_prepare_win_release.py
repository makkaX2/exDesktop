"""Optimize Telegram/build/prepare/prepare.py for fast Release-only Windows x64 CI builds.

1. Skips unused breakpad dump_syms.vcxproj (requires VS ATL component, unused when crash reports are disabled).
2. Skips Debug builds in breakpad, tg_angle, qt_6, tg_owt, ada, and tde2e (saves ~60-75 minutes on CI).
Leaves stages 1..26 untouched so existing Libraries cache stays valid.
"""
import pathlib
import re
import sys


def optimize(text: str) -> str:
    # 1. breakpad: skip Debug ninja and unused dump_syms.vcxproj (needs ATL)
    old_bp = (
        '    ninja -C out/Debug%FolderPostfix% common crash_generation_client exception_handler\n'
        'release:\n'
        '    ninja -C out/Release%FolderPostfix% common crash_generation_client exception_handler\n'
        '    cd tools\\\\windows\\\\dump_syms\n'
        '    gyp dump_syms.gyp --format=msvs\n'
        '    msbuild -m dump_syms.vcxproj /property:Configuration=Release /property:Platform="x64" %ToolsetProp%\n'
    )
    new_bp = (
        'release:\n'
        '    ninja -C out/Release%FolderPostfix% common crash_generation_client exception_handler\n'
    )
    assert old_bp in text, 'breakpad block not found'
    text = text.replace(old_bp, new_bp, 1)

    # 2. tg_angle & tg_owt: Release only (2 occurrences)
    old_cmake_dbg = (
        '    cmake --build out --config Debug\n'
        'release:\n'
        '    cmake --build out --config Release\n'
    )
    new_cmake_rel = (
        'release:\n'
        '    cmake --build out --config Release\n'
    )
    cnt_cmake = text.count(old_cmake_dbg)
    assert cnt_cmake >= 2, f'Expected >=2 tg_angle/tg_owt blocks, found {cnt_cmake}'
    text = text.replace(old_cmake_dbg, new_cmake_rel)

    # 3. qt_6 (win): -release only, skip Debug build/install
    old_qt_cfg = (
        '    SET CONFIGURATIONS=-debug\n'
        '    SET ASSERTS=\n'
        'release:\n'
        '    SET CONFIGURATIONS=-debug-and-release\n'
    )
    new_qt_cfg = (
        '    SET CONFIGURATIONS=-release\n'
        '    SET ASSERTS=\n'
        'release:\n'
        '    SET CONFIGURATIONS=-release\n'
    )
    assert old_qt_cfg in text, 'qt_6 config block not found'
    text = text.replace(old_qt_cfg, new_qt_cfg, 1)

    old_qt_build = (
        '    cmake --build . --config Debug\n'
        '    cmake --install . --config Debug\n'
        '    cmake --build .\n'
        '    cmake --install .\n'
    )
    new_qt_build = (
        '    cmake --build . --config Release\n'
        '    cmake --install . --config Release\n'
    )
    assert old_qt_build in text, 'qt_6 build block not found'
    text = text.replace(old_qt_build, new_qt_build, 1)

    # 4. ada: Release only
    old_ada = (
        '    cmake --build out --config Debug\n'
        '    cmake --build out --config Release\n'
    )
    new_ada = '    cmake --build out --config Release\n'
    assert old_ada in text, 'ada block not found'
    text = text.replace(old_ada, new_ada, 1)

    # 5. tde2e: skip Debug block, keep mkdir out -> cd out -> mkdir Release
    m = re.search(
        r'(    mkdir out\n    cd out\n)    mkdir Debug\n.*?release:\n    cd \.\.\n(    mkdir Release\n)',
        text,
        flags=re.S,
    )
    assert m, 'tde2e Debug block not found'
    text = text[:m.start()] + m.group(1) + 'release:\n' + m.group(2) + text[m.end():]

    compile(text, 'prepare.py', 'exec')
    return text


def main() -> None:
    dry_run = '--check' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--check']
    target = pathlib.Path(args[0])
    orig = target.read_text(encoding='utf-8')
    updated = optimize(orig)
    if dry_run:
        print(f'DRY-RUN OK: {target} ({len(orig)} -> {len(updated)} chars)')
    else:
        target.write_text(updated, encoding='utf-8', newline='\n')
        print(f'OPTIMIZED OK: {target} ({len(orig)} -> {len(updated)} chars)')


if __name__ == '__main__':
    main()
