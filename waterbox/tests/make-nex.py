#!/usr/bin/env python3
"""Writes the test program the tests load: a ZX Spectrum Next program in a
.nex file (the Next's own format for a program), made here byte by byte so
that the repository needs no assembler and holds no program of anybody's.

usage: make-nex.py <out.nex> [--without sprite,copper,dma,sound,tilemap,ay2,ay3,dacs]

What it does, so that a test can see each part:
 - a Layer 2 picture (the Next's 256x192 screen of 256 colours) is part of
   the file: the bottom 96 lines are stripes of colour, the top 96 lines are
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
 - the Next's own hardware, each part of which a test can leave out
   (--without) to see that it makes a difference:
   - sprite: hardware sprite 0, a 16x16 square of one colour, in the top
     part of the screen;
   - copper: the copper (the Next's raster coprocessor) shifts the Layer 2
     picture sideways from line 140 to line 170 of each frame;
   - dma: the zxnDMA copies 32 bytes of colours into row 10 of the Spectrum
     screen's colours;
   - sound: the first AY sound chip plays a tone on its channel A;
   - tilemap: the tilemap (the Next's layer of 8x8 tiles, 40x32 of them
     over the whole display) shows two rows of solid green tiles, at the
     screen's character rows 9 and 11, between the rows the controls paint
     and the Layer 2 stripes; every other tile is transparent;
   - ay2: the second AY chip (the Next has three, "TurboSound") plays a
     tone on its channel B;
   - ay3: the third AY chip plays a tone on its channel C;
   - dacs: the Next's 8-bit DACs (the Specdrum and Covox ports, 0xDF for
     DACs A and D, 0xB3 for B and C) are written every frame from the frame
     counter: two saw waves of 50 steps a second.
   The program turns on the three AY chips and the DACs (Next register 8)
   whichever parts are left out.
 - a counter of frames at 0x9200 (in the "RAM" domain, bank 2 at 0x8000,
   at 0x9200 - 0x8000 + 0x8000 = 0x9200); its interrupt routine is at
   0x9191, where the processor waits between frames.
"""
import sys

ORG = 0x8000


def assemble(features):
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
    def nextreg(reg, val):
        emit(0xED, 0x91, reg, val)  # nextreg reg,val (the Next's own Z80 instruction)

    def out(port, val):
        emit(0x01); word(port)      # ld bc,port
        emit(0x3E, val)             # ld a,val
        emit(0xED, 0x79)            # out (c),a

    if "sprite" in features:
        # pattern 0: 256 bytes of colour 0x1C (green in the default palette)
        out(0x303B, 0)              # sprite and pattern slot 0
        emit(0x01); word(0x005B)    # ld bc,0x005B (pattern upload port)
        emit(0x3E, 0x1C)            # ld a,0x1C
        emit(0x16, 0x00)            # ld d,0 (256 times)
        emit(0xED, 0x79)            # out (c),a
        emit(0x15)                  # dec d
        emit(0x20, 0xFB)            # jr nz,-5
        # sprite 0: x 96, y 64, no mirror, visible, pattern 0
        out(0x303B, 0)
        for v in (96, 64, 0, 0x80):
            out(0x0057, v)
        nextreg(0x15, 0x01)         # sprites on
    if "copper" in features:
        nextreg(0x62, 0x00)         # copper stopped, writing from 0
        nextreg(0x61, 0x00)
        # WAIT line 140, MOVE layer 2 x offset 64, WAIT line 170, MOVE 0, HALT
        for b in (0x80, 140, 0x16, 0x40, 0x80, 170, 0x16, 0x00, 0xFF, 0xFF):
            nextreg(0x60, b)
        nextreg(0x62, 0xC0)         # run, from the start again each frame
    if "dma" in features:
        # copy 32 bytes from 0x9300 to 0x5940 (row 10 of the colours)
        for v in (0x83,                         # disable
                  0x7D, 0x00, 0x93, 0x20, 0x00, # WR0: A to B, A = 0x9300, 32 bytes
                  0x54, 0x02,                   # WR1: A is memory, incrementing
                  0x50, 0x02,                   # WR2: B is memory, incrementing
                  0xAD, 0x40, 0x59,             # WR4: continuous, B = 0x5940
                  0x82,                         # WR5: stop at the end
                  0xCF,                         # load
                  0x87):                        # enable
            out(0x006B, v)
    # three AY chips, the DACs and the internal speaker on, whatever is
    # left out (Next register 8: bit 1 TurboSound, bit 3 DACs, bit 4 speaker)
    nextreg(0x08, 0x1A)

    def ay(chip, regs):
        # TurboSound: 0xFFFD written with 1 11 111 nn chooses the chip
        # (nn = 3 the first, 2 the second, 1 the third)
        out(0xFFFD, 0xFC | (3 - chip))
        for reg, val in regs:
            out(0xFFFD, reg)
            out(0xBFFD, val)

    if "sound" in features:
        # AY 1: channel A tone period 0x0100, only tone A on, volume 15
        ay(0, ((0, 0x00), (1, 0x01), (7, 0x3E), (8, 0x0F)))
    if "ay2" in features:
        # AY 2: channel B tone period 0x0180, only tone B on, volume 15
        ay(1, ((2, 0x80), (3, 0x01), (7, 0x3D), (9, 0x0F)))
    if "ay3" in features:
        # AY 3: channel C tone period 0x0240, only tone C on, volume 15
        ay(2, ((4, 0x40), (5, 0x02), (7, 0x3B), (10, 0x0F)))
    out(0xFFFD, 0xFF)               # the first AY chosen again
    if "tilemap" in features:
        # the tilemap and its tiles are in bank 5, part of the file: the
        # map (40x32, one byte a tile) at 0x6000, the tiles (32 bytes each,
        # 4 bits a dot) at 0x6600. Palette offset 1: dot 0xC is colour 0x1C,
        # set to green here; dot 0xF is the transparent one (Next register
        # 0x4C).
        nextreg(0x6E, 0x20)         # map at 0x4000 + 0x2000
        nextreg(0x6F, 0x26)         # tiles at 0x4000 + 0x2600
        nextreg(0x6C, 0x10)         # every tile: palette offset 1
        nextreg(0x4C, 0x0F)         # dot 0xF is transparent
        nextreg(0x43, 0x30)         # writing the tilemap's first palette
        nextreg(0x40, 0x1C)         # its colour 0x1C
        nextreg(0x41, 0x1C)         # is green (RRRGGGBB)
        nextreg(0x43, 0x00)
        nextreg(0x6B, 0xA0)         # on, 40x32, no attribute bytes
    emit(0xAF)                      # xor a
    emit(0x32); word(0x9200)        # ld (0x9200),a
    labels["loop"] = here()
    emit(0xFB)                      # ei
    emit(0x76)                      # halt
    emit(0x21); word(0x9200)        # ld hl,0x9200
    emit(0x34)                      # inc (hl)
    if "dacs" in features:
        # every frame: DACs A and D the counter times 16, B and C times 8
        emit(0x7E)                  # ld a,(hl)
        emit(0x07, 0x07, 0x07, 0x07)    # rlca x4
        emit(0x01); word(0x00DF)    # ld bc,0x00DF
        emit(0xED, 0x79)            # out (c),a
        emit(0x7E)                  # ld a,(hl)
        emit(0x07, 0x07, 0x07)      # rlca x3
        emit(0x01); word(0x00B3)    # ld bc,0x00B3
        emit(0xED, 0x79)            # out (c),a
        emit(0x21); word(0x9200)    # ld hl,0x9200
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
    features = {"sprite", "copper", "dma", "sound", "tilemap", "ay2", "ay3", "dacs"}
    if "--without" in sys.argv:
        for f in sys.argv[sys.argv.index("--without") + 1].split(","):
            features.discard(f)
    code = assemble(features)
    # bank 2 is 0x8000-0xBFFF: the code, the interrupt table (0x9000), the
    # interrupt routine (0x9191: ei, reti) and the frame counter (0x9200)
    bank2 = bytearray(0x4000)
    bank2[0:len(code)] = code
    for i in range(0x1000, 0x1101):
        bank2[i] = 0x91
    bank2[0x1191:0x1194] = bytes((0xFB, 0xED, 0x4D))
    # the colours the DMA copies (0x9300): paper colours 1 to 7, again and again
    for i in range(32):
        bank2[0x1300 + i] = ((i % 7) + 1) << 3
    # bank 5 is the Spectrum screen (0x4000): pixels off, colours white
    bank5 = bytearray(0x4000)
    for i in range(0x1800, 0x1B00):
        bank5[i] = 0x38
    # the tilemap (0x6000, 40x32) and its tiles (0x6600): tile 0 all
    # transparent dots (0xF), tile 1 all dots 0xC; tile 1 on the map's rows
    # 13 and 15 (the screen's character rows 9 and 11: the map starts 32
    # dots above and left of the screen), columns 4 to 35 (the screen's
    # width)
    for row in (13, 15):
        for col in range(4, 36):
            bank5[0x2000 + row * 40 + col] = 1
    bank5[0x2600:0x2620] = b"\xff" * 32
    bank5[0x2620:0x2640] = b"\xcc" * 32
    # the Layer 2 picture: 0xE3 is the transparent colour
    layer2 = bytearray(256 * 192)
    for y in range(192):
        for x in range(256):
            layer2[y * 256 + x] = 0xE3 if y < 96 else ((x // 16) * 16 + (y - 96) // 6) & 0xFF
            if layer2[y * 256 + x] == 0xE3 and y >= 96:
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
