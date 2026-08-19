import re, io, os, subprocess

ROOT = r'C:\Users\Deck\OneDrive\Documents\Programs\HeavenClient-Android'
HDR = os.path.join(ROOT, 'libs', 'glfw', 'include', 'GLFW', 'glfw3.h')

# 1. which GLFW_ constants does the client source actually reference?
used = set()
skip_dirs = {'libs', 'includes', '.git', 'fonts', 'Data'}
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in skip_dirs]
    for fn in filenames:
        if not fn.endswith(('.cpp', '.h')):
            continue
        p = os.path.join(dirpath, fn)
        try:
            txt = io.open(p, encoding='utf-8', errors='replace').read()
        except Exception:
            continue
        used.update(re.findall(r'\bGLFW_[A-Z_0-9]+', txt))

# 2. pull their real values out of glfw3.h
defs = {}
for line in io.open(HDR, encoding='utf-8', errors='replace'):
    m = re.match(r'\s*#define\s+(GLFW_[A-Z_0-9]+)\s+(.+?)\s*$', line)
    if m:
        name, val = m.group(1), m.group(2).strip()
        val = re.sub(r'\s*/\*.*?\*/\s*$', '', val).strip()
        defs[name] = val

missing = sorted(n for n in used if n not in defs)
resolved = sorted(n for n in used if n in defs)

out = io.StringIO()
out.write('''// GLFWKeys.h - GLFW key/constant values, extracted from GLFW 3.3 headers.
//
// HeavenClient uses GLFW_* values as its keymap vocabulary: the config file
// stores them, and Keyboard.cpp / UI.cpp compare against them. That is the
// only reason ~1100 references to GLFW exist in this codebase.
//
// On Android we drop the GLFW library entirely (it has no Android backend)
// and use SDL2 for windowing and input. These constants are kept verbatim so
// the existing keymap, and every config file already written against it,
// continue to work untouched.
//
// GENERATED - do not hand-edit. See tools/gen_keys.py.

#pragma once

#if defined(PLATFORM_ANDROID)

''')
for n in resolved:
    out.write('#define {:<34} {}\n'.format(n, defs[n]))
out.write('\n#endif // PLATFORM_ANDROID\n')

dest = os.path.join(ROOT, 'Util', 'GLFWKeys.h')
io.open(dest, 'w', encoding='utf-8', newline='\n').write(out.getvalue())

print('used by client :', len(used))
print('resolved       :', len(resolved))
print('MISSING        :', missing if missing else 'none')
print('written        :', dest)
