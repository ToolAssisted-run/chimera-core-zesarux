#!/usr/bin/env python3
"""Writes a .z80 snapshot of a 128K Spectrum for the test script.

    make-z80.py <out.z80>

A version 2 .z80 file (the 23-byte extra header) of a Spectrum 128K, its
eight RAM banks stored uncompressed, with RAM bank 3 paged in at 0xC000
(port 0x7ffd = 0x03) and the processor at 0xC000. Bank 3 holds the program
the snapshot is about: it fills the screen (bitmap and colours, 0x4000 to
0x5AFF) with 0x0F and stops at 0xC00E in a jump to itself. Bank 7 holds a
jump to itself at 0xC000 and nothing else. ZEsarUX loads the banks one
after the other through port 0x7ffd, the last one bank 7; a Next that is
not given the snapshot's own 0x7ffd back afterwards runs bank 7's code and
ends at 0xC000 with an empty screen. The test script asks for 0xC00E.

The snapshot is made here; nothing of any game is in it.
"""
import struct
import sys

PROGRAM = bytes([
    0xF3,              # c000 di
    0x21, 0x00, 0x40,  # c001 ld hl,0x4000
    0x11, 0x01, 0x40,  # c004 ld de,0x4001
    0x01, 0xFF, 0x1A,  # c007 ld bc,0x1aff (6912 - 1)
    0x36, 0x0F,        # c00a ld (hl),0x0f
    0xED, 0xB0,        # c00c ldir
    0x18, 0xFE,        # c00e jr c00e
])
DECOY = bytes([0x18, 0xFE])  # bank 7, 0xc000: jr c000


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    main_header = bytearray(30)
    main_header[0] = 0x00                                  # A
    main_header[1] = 0x00                                  # F
    struct.pack_into('<H', main_header, 6, 0)              # PC 0: a later version
    struct.pack_into('<H', main_header, 8, 0x8000)         # SP
    main_header[10] = 0x3F                                 # I
    main_header[12] = 0x02                                 # border 1
    main_header[27] = 0                                    # IFF1
    main_header[28] = 0                                    # IFF2
    main_header[29] = 1                                    # interrupt mode 1
    extra = bytearray(25)
    struct.pack_into('<H', extra, 0, 23)                   # a version 2 header
    struct.pack_into('<H', extra, 2, 0xC000)               # PC
    extra[4] = 3                                           # a 128K (version 2)
    extra[5] = 0x03                                        # port 0x7ffd: bank 3
    out = bytearray(main_header + extra)
    for bank in range(8):
        data = bytearray(16384)
        if bank == 3:
            data[0:len(PROGRAM)] = PROGRAM
        if bank == 7:
            data[0:len(DECOY)] = DECOY
        out += struct.pack('<HB', 0xFFFF, bank + 3)        # 16 KiB, not compressed
        out += data
    with open(sys.argv[1], 'wb') as f:
        f.write(out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
