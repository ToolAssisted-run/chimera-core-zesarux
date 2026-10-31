#!/usr/bin/env python3
"""Writes the buttons of a test movie, in one of two forms.

  make-movie.py <frames> <first>:<count>:<button>...
        the movie as Chimera's engine reads and writes it: a row a frame;
        between bars, 42 columns for the keyboard, Reset and Hard Reset (the
        console's),
        then 6 for the joystick (player 1's); a button's own letter where it
        is held and a dot where it is not
  make-movie.py <frames> <first>:<count>:<button>... --as-presses
        the same buttons as this repository's runners take them:
        --press F:N:MASK ...

<button> is the button's place in the package's list (0 is the key 1, 29
ENTER, 30 CAPS SHIFT, 39 SPACE, 40 Reset, 41 Hard Reset, 42 to 47 the
joystick's Up, Down, Left, Right, Fire and Fire 2), or several joined by "+".

The engine counts frames from 0 and the runners from 1, so row r of the movie
is the runners' frame r + 1.
"""
import sys

# the letters, in the order the package declares the buttons
LETTERS = "1234567890QWERTYUIOPASDFGHJKL<^ZXCVBNM$_%!" + "udlrfg"
GROUPS = (42, 6)


def main():
    frames = int(sys.argv[1])
    presses = []
    for a in sys.argv[2:]:
        if a.startswith("--"):
            continue
        first, count, buttons = a.split(":")
        mask = 0
        for b in buttons.split("+"):
            mask |= 1 << int(b)
        presses.append((int(first), int(count), mask))
    if "--as-presses" in sys.argv:
        print(" ".join("--press %d:%d:%x" % (first + 1, count, mask) for first, count, mask in presses if first < frames))
        return
    for row in range(frames):
        held = 0
        for first, count, mask in presses:
            if first <= row < first + count:
                held |= mask
        cells = "".join(LETTERS[i] if held >> i & 1 else "." for i in range(len(LETTERS)))
        out, at = "|", 0
        for width in GROUPS:
            out += cells[at:at + width] + "|"
            at += width
        print(out)


if __name__ == "__main__":
    main()
