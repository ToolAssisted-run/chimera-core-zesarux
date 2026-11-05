# chimera-core-zesarux

A ZX Spectrum Next core for [Chimera](https://github.com/ToolAssisted-run/chimera),
the frontend for tool-assisted speedruns. It is built on
[ZEsarUX](https://github.com/chernandezba/zesarux), and produces one file,
`zesarux.chimeraCore`, that goes in Chimera's `cores` folder. It was asked
for in chimera issue 407.

## Status

Published: a dated nightly is made once a day when `main` moved and its
tests pass. Chimera's Core Manager lists it. Chimera issue 407.

NextZXOS starts from an SD card image to its
menu and runs a program from its Browser; a program can also be loaded
straight into the machine without NextZXOS. Picture, sound, keys, the
joystick, both resets and savestates work, and the sandboxed core gives the
same results as a plain native build. Free games from the SD card ZEsarUX
comes with (Warhawk, Nextoid, The Lords of Midnight, the Night-Knight demo)
and from the System/Next 24.11 card for emulators (Revival Survival,
Warhawk, NextBASIC Invaders) have been played, and of the owner's
commercial games Monster Max (a Next game) and three Spectrum tapes (Manic
Miner, Jet Set Willy 2, Golden Baton, loaded by ZEsarUX's own tape
loading); the tests also use a small program they make themselves. A
.tzx does not load through NextZXOS's own TZX loader, which needs the
Next's Raspberry Pi, not emulated.

## What you bring

- To start the Next as a Next does, with NextZXOS: the Next's boot ROM
  (`tbblue_loader.rom`, on Chimera's Firmware page) and an SD card image
  with the Next's system on it, in the project. ZEsarUX comes with both
  (`tbblue_loader.rom`, `tbblue.mmc`).
- To start it with the System/Next distribution's card for emulators
  (specnext.com/latestdistro, 1 GiB, NextZXOS 2.09): only the card, with
  Start set to `card`. The Next's own firmware cannot read that card, so
  the core does the firmware's work. A card may be up to 1 GiB; the whole
  card is held in the machine's memory (about 1.1 GB while it runs), but
  savestates carry only what the machine wrote to it.
- To start it fast, without NextZXOS: nothing. The 48K Spectrum's ROM it
  runs on is built into the core.

A program (`.nex` and the other formats the Next opens) goes in the project
too: with NextZXOS it is put on the card, where the Browser finds it.

## Buttons and settings

The forty keys of the Spectrum's keyboard, each on the same key of yours,
with CAPS SHIFT on Shift and SYMBOL SHIFT on Ctrl (the Next's cursor keys,
EDIT and the rest are CAPS SHIFT with a number, as on the Next). Reset and
Hard Reset. A joystick of four directions and two fire buttons, on the
arrow keys, Tab and the backslash; the Next's programs choose what kind of
joystick it is.

- **Start**: `nextzxos`, `card` or `fast`. The New Project window
  suggests `card` for a card the firmware cannot read, `nextzxos` for
  another card, and `fast` for a project with only a program.
- **Hold NextZXOS at 7 MHz**: ZEsarUX's way of sparing a slow computer; off
  by default, as on a Next.

Both are part of the machine: a movie made with one value does not play
back with another. The machine's clock starts at the project's clock start.

RAM Watch and RAM Search see the machine two ways: "RAM" is the Next's
1.75 MiB of memory in its own bank numbering, "System Bus" the 64 KiB the
processor sees.

## Building and testing

[docs/BUILDING.md](docs/BUILDING.md) has every step. [docs/PLAN.md](docs/PLAN.md)
is the design log: what was decided, why, and what was measured.

## Licence

This repository's own code is MIT; the patches to ZEsarUX are under
ZEsarUX's terms. ZEsarUX is under the GNU General Public License version 3
or later, and so is the program in the built package, which carries the
licence texts of everything in it. The package contains the 48K Spectrum's
ROM, which Amstrad allows to be distributed (but not sold on its own); it
contains no other ROM, no SD card and no game. See [LICENSE](LICENSE).
