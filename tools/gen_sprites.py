#!/usr/bin/env python3
"""Extract the pixel sprites (PAL + characters) from patty-project/xmas-app/web/js/pixelart.js
and write src/art/sprite_data.inc for the C++ build. Edit characters on the web side, then rerun.

Usage:  python3 tools/gen_sprites.py [path/to/pixelart.js]
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "..", "patty-project", "xmas-app", "web", "js", "pixelart.js")
OUT = os.path.join(ROOT, "src", "art", "sprite_data.inc")
# JS const name -> name used in the game
CHARS = {"PATTY": "patty", "TER": "ter", "MOMONGA": "momonga", "SUNCONURE": "cham", "SENEGAL": "juice", "PUPPY": "puppy", "SLID": "slid", "SNOWMAN": "snowman"}

js = open(SRC, encoding="utf-8").read()

pal_block = js[js.index("const PAL = {"):]
pal_block = pal_block[:pal_block.index("\n}\n")]
pal = re.findall(r"\b([A-Za-z]):\s*0x([0-9a-fA-F]{6})", pal_block)

out = ["// Generated from pixelart.js by tools/gen_sprites.py - do not edit by hand", ""]
out.append("static const PaletteEntry kPalette[] = {")
out += [f"    {{'{k}', 0x{v.lower()}}}," for k, v in pal]
out.append("};")
out.append("")

for js_name, name in CHARS.items():
    m = re.search(rf"const {js_name} = \{{\n(.*?)\n\}}\n", js, re.S)
    if not m:
        print(f"not found: {js_name}", file=sys.stderr)
        continue
    frames = re.findall(r"(\w+): \[\n(.*?)\n  \]", m.group(1), re.S)
    for fname, body in frames:
        rows = re.findall(r"'([^']*)'", body)
        out.append(f"static const char* k_{name}_{fname}[] = {{")
        out += [f'    "{r}",' for r in rows]
        out.append("    nullptr};")
    out.append(f"static const FrameDef k_{name}[] = {{")
    out += [f'    {{"{f}", k_{name}_{f}}},' for f, _ in frames]
    out.append("    {nullptr, nullptr}};")
    out.append("")

out.append("static const CharDef kCharDefs[] = {")
out += [f'    {{"{n}", k_{n}}},' for n in CHARS.values()]
out.append("    {nullptr, nullptr}};")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(out) + "\n")
print(f"wrote {OUT}: {len(pal)} palette colours, {len(CHARS)} characters")
