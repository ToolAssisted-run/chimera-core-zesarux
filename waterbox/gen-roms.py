#!/usr/bin/env python3
"""Writes the ROM that is built into the core, as a C file.

  gen-roms.py <ZEsarUX's src directory> <C file to write>

The 48K Spectrum's ROM, which the Next's fast start runs on, is built into
the core (user-decided, 2026-10-31: docs/PLAN.md), so that a person who
starts a program fast has nothing to find. It is read from the pinned
ZEsarUX submodule when the core is built; this repository holds no ROM.
It is Amstrad's, who allow it to be distributed (waterbox/ROMS.txt, which
the package carries). The SHA-1 is checked, so that a change of the
submodule cannot put another ROM in unnoticed.

Not here: the Next's boot ROM (tbblue_loader.rom), which stays firmware the
user supplies, and the SD card.
"""
import hashlib
import sys

ROMS = {"48.rom": "5ea7c2b824672e914525d1d5c419d71b84a426a2"}


def main():
    folder, out_path = sys.argv[1], sys.argv[2]
    out = ["/* Written by waterbox/gen-roms.py from ZEsarUX's src/. Not edited by hand. */",
           '#include "chimera_roms.h"', ""]
    for name, sha1 in ROMS.items():
        data = open(folder + "/" + name, "rb").read()
        if hashlib.sha1(data).hexdigest() != sha1:
            sys.exit("gen-roms.py: %s is not the ROM expected (SHA-1 %s)" % (name, sha1))
        out.append("static const unsigned char rom_%s[%d] = {" % (name.replace(".", "_"), len(data)))
        for i in range(0, len(data), 16):
            out.append("\t" + ",".join("%d" % b for b in data[i:i + 16]) + ",")
        out.append("};")
    out.append("")
    out.append("const struct chimera_rom chimera_roms[] = {")
    for name in ROMS:
        ident = "rom_" + name.replace(".", "_")
        out.append('\t{ "%s", %s, sizeof %s },' % (name, ident, ident))
    out.append("\t{ 0, 0, 0 },")
    out.append("};")
    open(out_path, "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
