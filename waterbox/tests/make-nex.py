#!/usr/bin/env python3
"""Writes the test program the tests load: a ZX Spectrum Next program in a
.nex file (the Next's own format for a program), made here byte by byte so
that the repository needs no assembler and holds no program of anybody's.

usage: make-nex.py <out.nex>

What it does, so that a test can see each part:
 - a Layer 2 picture (the Next's 256x192 screen of 256 colours) is part of
   the file: the bottom 120 lines are stripes of colour, the top 72 lines are
   the transparent colour, so the ordinary Spectrum screen shows through
   there. The picture is then two of the Next's layers mixed.
 - the program waits for each frame's interrupt (interrupt mode 2, its own
   table), and on every second frame reads the Kempston joystick port and
   the eight rows of the keyboard, and paints each answer into one row of
   the Spectrum screen's colours (row 0 the joystick, rows 1 to 8 the
   keyboard): its three low bits as the paper colour of the row's left half,
   the next three as that of its right half. So every key and every
   joystick button changes the picture, each its own way, and only every
   second frame looks at the controls: half the frames are lag frames.
 - a counter of frames at 0x9200 (in the "RAM" domain, bank 2 at 0x8000,
   at 0x9200 - 0x8000 + 0x8000 = 0x9200); its interrupt routine is at
   0x9191, where the processor waits between frames.
"""
import sys

ORG = 0x8000


def assemble():
    code = bytearray()
    labels = {}
    fixups = []  # (offset in code, label) for absolute 16-bit addresses

    def here():
        return ORG + len(code)

    def emit(*b):
        code.extend(b)

    def word(v):
        emit(v & 0xFF, (v >> 8) & 0xFF)

    def ref(label):
        fixups.append((len(code), label))
        emit(0, 0)

    # start: interrupts off, a stack, interrupt mode 2 with a table of 257
    # bytes of 0x91 at 0x9000, so every interrupt goes to 0x9191
    emit(0xF3)                      # di
    emit(0x31); word(0xBFF0)        # ld sp,0xBFF0
    emit(0x21); word(0x9000)        # ld hl,0x9000
    emit(0x36, 0x91)                # ld (hl),0x91
    emit(0x11); word(0x9001)        # ld de,0x9001
    emit(0x01); word(0x0100)        # ld bc,0x0100
    emit(0xED, 0xB0)                # ldir
    emit(0x3E, 0x90)                # ld a,0x90
    emit(0xED, 0x47)                # ld i,a
    emit(0xED, 0x5E)                # im 2
    emit(0xAF)                      # xor a
    emit(0x32); word(0x9200)        # ld (0x9200),a
    labels["loop"] = here()
    emit(0xFB)                      # ei
    emit(0x76)                      # halt
    emit(0x21); word(0x9200)        # ld hl,0x9200
    emit(0x34)                      # inc (hl)
    emit(0x7E)                      # ld a,(hl)
    emit(0xE6, 0x01)                # and 1
    emit(0xC2); ref("loop")         # jp nz,loop   (odd frames: nothing read)
    # each answer, with a pressed button or key as a 1, is shown in one row
    # of the screen's colours: its three low bits as the paper colour of the
    # row's left half, the next three as the paper colour of its right half
    def show(row):
        emit(0xF5)                      # push af
        emit(0xE6, 0x07)                # and 7
        emit(0x07, 0x07, 0x07)          # rlca x3: into the paper's bits
        emit(0x21); word(0x5800 + 32 * row)       # ld hl,row
        emit(0xCD); ref("fill")         # call fill
        emit(0xF1)                      # pop af
        emit(0xE6, 0x38)                # and 0x38: already the paper's bits
        emit(0x21); word(0x5800 + 32 * row + 16)  # ld hl,row + 16
        emit(0xCD); ref("fill")         # call fill
    # the Kempston joystick: right, left, down | up, fire, fire 2
    emit(0x01); word(0x001F)        # ld bc,0x001F
    emit(0xED, 0x78)                # in a,(c)
    show(0)
    # the keyboard, row by row: a pressed key reads as a 0
    for row, high in enumerate((0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F)):
        emit(0x01); word((high << 8) | 0xFE)   # ld bc,high:FE
        emit(0xED, 0x78)            # in a,(c)
        emit(0x2F)                  # cpl
        emit(0xE6, 0x1F)            # and 0x1F
        show(row + 1)
    emit(0xC3); ref("loop")         # jp loop
    # fill: 16 bytes at hl with a
    labels["fill"] = here()
    emit(0x77)                      # ld (hl),a
    emit(0x54)                      # ld d,h
    emit(0x5D)                      # ld e,l
    emit(0x13)                      # inc de
    emit(0x01); word(15)            # ld bc,15
    emit(0xED, 0xB0)                # ldir
    emit(0xC9)                      # ret
    assert here() < 0x9000, "the code runs into the interrupt table"
    for at, label in fixups:
        code[at] = labels[label] & 0xFF
        code[at + 1] = labels[label] >> 8
    return bytes(code)


def main():
    code = assemble()
    # bank 2 is 0x8000-0xBFFF: the code, the interrupt table (0x9000), the
    # interrupt routine (0x9191: ei, reti) and the frame counter (0x9200)
    bank2 = bytearray(0x4000)
    bank2[0:len(code)] = code
    for i in range(0x1000, 0x1101):
        bank2[i] = 0x91
    bank2[0x1191:0x1194] = bytes((0xFB, 0xED, 0x4D))
    # bank 5 is the Spectrum screen (0x4000): pixels off, colours white
    bank5 = bytearray(0x4000)
    for i in range(0x1800, 0x1B00):
        bank5[i] = 0x38
    # the Layer 2 picture: 0xE3 is the transparent colour
    layer2 = bytearray(256 * 192)
    for y in range(192):
        for x in range(256):
            layer2[y * 256 + x] = 0xE3 if y < 72 else ((x // 16) * 16 + (y - 72) // 8) & 0xFF
            if layer2[y * 256 + x] == 0xE3 and y >= 72:
                layer2[y * 256 + x] = 0xE2
    header = bytearray(512)
    header[0:4] = b"Next"
    header[4:8] = b"V1.2"
    header[8] = 0          # 768K of RAM is enough
    header[9] = 2          # two 16K banks
    header[10] = 1 | 128   # a Layer 2 picture, with the default palette
    header[11] = 7         # white border
    header[12:14] = (0xBFF0).to_bytes(2, "little")  # SP
    header[14:16] = ORG.to_bytes(2, "little")       # PC
    header[18 + 5] = 1
    header[18 + 2] = 1
    header[139] = 0        # bank 0 at 0xC000
    with open(sys.argv[1], "wb") as f:
        f.write(header)
        f.write(layer2)
        f.write(bank5)     # the banks in the order the format keeps: 5, 2, ...
        f.write(bank2)


if __name__ == "__main__":
    main()
