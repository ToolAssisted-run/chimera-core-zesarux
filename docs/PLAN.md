# ZX Spectrum Next core (ZEsarUX) - plan and design log

## What this is

A ZX Spectrum Next core for Chimera, built on ZEsarUX, requested in
chimera issue 407 ("ZesarUX for ZX Spectrum Next"). The ZX Spectrum Next is
a Spectrum made new by enthusiasts: an FPGA computer that runs Spectrum
software and adds its own hardware (a faster Z80 up to 28 MHz, 2 MiB of
memory, Layer 2 bitmaps, tilemaps, hardware sprites, a copper, DMA, three
sound chips) and boots its own system, NextZXOS, from an SD card.

ZEsarUX (https://github.com/chernandezba/zesarux) is a C emulator of many
Sinclair, Amstrad and other machines; the Next is one of them, under the
name of the board it began as, TBBlue.

The repository is local, with no remote; nothing of it is published.

## The pin (2026-10-31)

ZEsarUX 13.0, the release of 2026-06-09: tag `ZEsarUX-13.0`, commit
0bb4277d714128311d78bc0625fb12ae8308899a. A release rather than the head of
the repository, which moves every day.

## The eight questions (chimera docs/new-core.md), answered from the source (2026-10-31)

- **Is it a new core?** Yes. No core of Chimera's has the Next. The Fuse
  core and the ares core have the original Spectrums, not the Next's
  hardware. The system id is `ZXN`, named "ZX Spectrum Next".
- **May it be distributed and modified?** ZEsarUX is under the GNU GPL
  version 3 or later (src/LICENSE at the pinned commit). It includes modules
  by others (src/LICENSES_info): all under free terms (GPL, MIT, BSD,
  Unlicense, CC fonts, UnrealSpeccy's "use without restrictions", the HiLow
  authors' permission to emulate), except PlayTZX, whose licence ZEsarUX
  itself calls unknown. PlayTZX is left out of the core (it turns a .tzx
  tape into sound for ZEsarUX's "real tape" player, which the Next does not
  need); a stub answers that it failed. Every other module is listed in
  waterbox/package-licenses.json and its text travels in the package.
- **One thread on one clock?** Yes. Built with `--disable-pthreads` ZEsarUX
  runs its emulation in one loop, and its timing is in the processor's
  T-states. It asks the host's clock only to pace itself (a "timer" that
  says when 20 ms have passed) and for the real-time clock, file dates and
  random seeds. Patch 0002 makes the 1/50 s tick come the moment the machine
  has run its frame's T-states; the link wraps every clock function to the
  machine's own time (chimera_compat.c). Its network, its remote-control
  protocol and its online play are compiled out.
- **Where does the picture come from?** The machine itself: ZEsarUX draws
  the Next's layers line by line ("real video") into its own buffer and
  hands each pixel to its video driver. The core's driver keeps them. No GPU
  and no OpenGL.
- **Does it recompile?** No. An interpreter.
- **What must the user bring?** For NextZXOS: the Next's boot ROM
  (`tbblue_loader.rom`, 8 KiB) and an SD card image with the Next's system
  on it. For the fast start (no NextZXOS): nothing; the 48K Spectrum's ROM
  it runs on is built into the core (user-decided, 2026-10-31). ZEsarUX's
  repository contains all three; see "What the user brings".
- **What can a test run on a public runner?** Everything here, if the three
  files may be read from the ZEsarUX submodule there (they are in it, as
  they are on this machine). The tests use no game: the program they load is
  made by the test script (waterbox/tests/make-nex.py).
- **How big is it?** The Next has 2 MiB of memory; the SD card image is
  held whole in the core's memory (64 MiB for the card ZEsarUX comes with,
  1 GiB for the System/Next card for emulators). The layout is sbrk 64,
  sealed 4, invisible 8, plain 4, mmap 1280 MiB (256 until the third
  round). A state taken at NextZXOS's menu measured 4.2 MB (4.7 MB with
  the third round's core); one at Warhawk's title from the 1 GiB card,
  5.8 MB.

## Other machines ZEsarUX would bring

ZEsarUX emulates many machines that no Chimera core has today: the ZX80 and
ZX81 (and the Timex TS1000 and TS1500), the Jupiter Ace, the Cambridge Z88,
the SAM Coupe, the Sinclair QL, the Amstrad PCW 8256 and 8512, the
Spectravideo 318 and 328, the MK14, and the Spectrum clones and successors
ZX-Uno, ZX-Evolution (BaseConf and TS-Conf), Chloe 140SE/280SE, Prism,
Inves and the Brazilian TK90X and TK95. (The Spectrums, the Amstrad CPC,
MSX1, ColecoVision, SG-1000 and Master System are in other cores already.)
The same adapter would serve them: the core's display, sound card, clock and
frame loop are not the Next's, only the button list, the machine setting and
the memory domains are. Each needs its own ROMs (most of which ZEsarUX ships)
and its own tests; the Spectrum-family ones run through the same
core_spectrum loop this core already uses, the others through their own
(core_zx8081.c, core_z88.c, core_sam.c, core_ql.c ...), whose end of frame
needs the same hook. Not started: the request is for the Next.

## How it is built

ZEsarUX's own `configure` looks at the machine and writes
`compileoptions.h` (with the date and the folder) and a Makefile. The core
is built twice, for the host and for the sandbox, and both must be the same
program, so neither is used:

- `waterbox/config/compileoptions.h` is written by hand: what configure
  wrote for the null video and audio drivers, no threads, no network, no
  "visual memory" and no processor statistics, with nothing of a machine in
  it (BUILDNUMBER, which a program can read through ZEsarUX's own port, is
  a constant).
- `meson.build` lists the sources: the 175 files the Makefile compiled for
  those options, less the null drivers (the core's own replace them), less
  `main_unix.c` (the core is started by a call), less PlayTZX.
- The patches are seven, all under `#ifdef CHIMERA_CORE`:
  - 0001 `start.c`: `zesarux_main` returns once the machine is set up
    instead of entering its loop;
  - 0002 `timer.c`: the 1/50 s tick is "the machine has run its frame", and
    the end of a frame never sleeps;
  - 0003 `audio.c`, `operaciones.c`, `cores/core_spectrum.c`: each sound
    sample to the core as it is made, a note when the program reads the
    keyboard or the Kempston joystick, and a note at the end of each frame;
  - 0004 `third_party/ff.c`, `third_party/diskio.c` (FatFs, the FAT
    reader ZEsarUX bundles): a volume laid out as FAT32 is read as FAT32
    whatever its number of clusters, and the core can lend FatFs the card
    the machine holds instead of FatFs reading a second copy of the file
    ("The System/Next 24.11 card");
  - 0005 `machines/tbblue.c`, `machines/tbblue.h`, `storage/diviface.c`,
    `ula.c`: three things of the Next's newer cores that NextZXOS 2.09
    needs (same section), and the Multiface's NMI by Next register 2 bit 3
    ("Fourth round"), and the register hooks of the Next's CTC and
    hardware IM2 mode ("The Next's CTC and hardware IM2 mode");
  - 0006 `snap/snap.c`: a .z80 snapshot of a 128K loaded into the Next
    gets its own port 0x7ffd back, as ZEsarUX gives it to a 128K
    ("Fourth round");
  - 0007 `cpus/z80_codpred.c`: RETI tells the Next's interrupt
    controller (same section).
  Patch 0003 also carries the hooks for the CTC's ports, the ULA's
  interrupt, the step after each instruction and the vector.

## How the core is put together

- **Switched on by a call.** `waterbox/cinterface.c` writes the frontend's
  settings as ZEsarUX's own command line (`--noconfigfile --machine TBBlue
  --vo null --ao null --realvideo ...`) and calls `zesarux_main`.
- **A frame at a time.** `FrameAdvance` calls ZEsarUX's step function
  (`cpu_core_loop`, one instruction and its housekeeping) until ZEsarUX says
  its frame has ended (hook in `core_spectrum_fin_frame_pantalla`). The first
  version ended a frame when the T-state counter went back; the firmware's
  own resets set it to 0 in the middle of a frame, which made short frames
  (the sound count showed it: 68, 153, 159 samples instead of 312).
- **Time.** No host clock reaches the machine. The link wraps `time`,
  `gettimeofday`, `localtime`, `mktime`, `sleep`, `usleep` (and `rand`,
  `srand`, `signal`, `uname`, `getcwd`): the time is the project's clock
  start (Chimera's own setting, read from the sandbox's clock, which stands
  still at that second) plus the frames run over 50. NextZXOS's menu shows
  that date (2030-01-02 03:04 for a project started at 2030-01-02 03:04:05).
  The noise generators and the first contents of memory, which ZEsarUX seeds
  from the clock, are the same every run.
- **The picture** (`waterbox/scrchimera.c`) is ZEsarUX's null video driver
  replaced: the same function names, the pixels kept. 704x608: every dot of
  the Next's finest mode is one dot, so the Spectrum's 256x192 screen with
  its border is drawn at twice its size.
- **The sound** (`waterbox/audiochimera.c`): ZEsarUX makes one stereo
  8-bit sample per scan line, 312 a frame, 15600 a second; the core takes
  each as it is made.
- **Files** are mounted by bare name in the sandbox. ZEsarUX makes the name
  of a file on its command line absolute with the current folder; the core
  answers "." and the sandbox build opens "./name" as "name".
- **The SD card** is read whole into the machine's memory by ZEsarUX
  (`--mmc-file`, `--mmc-no-persistent-writes`): what the machine writes to
  it stays in the machine and its states, and the file is never written.
- **A program in the project**, with NextZXOS, is copied into the card's top
  folder before the machine starts, by the FAT code ZEsarUX uses for its own
  `--copy-file-to-mmc`, with the copy kept in memory instead of written back
  to the file; the Browser lists it and NextZXOS's own loader runs it. With
  the fast start, ZEsarUX loads it straight into the machine.
- **Input**: the forty keys of the Spectrum's keyboard (the Next's extra
  keys are combinations of these on a real Next too), Reset (the Next's soft
  reset), Hard Reset, and a joystick of four directions and two fire
  buttons, set as levels before each frame. What the joystick is (Kempston,
  Sinclair, cursor, Mega Drive pad) is the Next's own setting, which its
  programs choose (Next register 5); ZEsarUX reads it from there.
- **Lag frames**: a frame in which the program read the keyboard port or
  the Kempston port is a frame with input.
- **Memory for tools**: the domain "RAM", the Next's 1.75 MiB of memory in
  its own bank numbering (bank 0 first), and the bus "System Bus", 64 KiB as
  the processor sees them. ZEsarUX's "visual memory" is compiled out: it
  wrote down every memory read, so a tool reading the machine would have
  changed it.
- **Errors**: every message of ZEsarUX's passes `debug_printf`, which the
  link wraps; an error is kept and becomes the reason `Init` refuses (a ROM
  that is not there is reported before ZEsarUX's display is up).

## The owner's decisions (user-decided, 2026-10-31)

The owner accepted every recommendation of the first round:

- **The tests may read the Next's boot ROM (`tbblue_loader.rom`), the 48K
  ROM (`48.rom`) and the SD card image (`tbblue.mmc`) from the pinned
  ZEsarUX submodule**, on this machine and on a public CI runner. They are
  upstream's own files at the pinned commit; nothing of them is copied into
  this repository. ZESARUX_TEST_FIRMWARE points the tests elsewhere.
- **The 48K ROM is packaged**, on the same terms as the Fuse core's Amstrad
  ROMs (built into the core; Amstrad allows its distribution). Done: see
  "The 48K ROM is built in" below. The fast start needs nothing from the
  user any more.
- **The Next's boot ROM stays firmware the user supplies**, and **the SD
  card image is not packaged**.
- **Start defaults to `nextzxos`**, and the core suggests `fast` for a
  project that has a program and no card (as built).
- **The system id is `ZXN`**, with the Fuse core's key names and movie
  letters (as built).
- **The Next only, for now.** ZEsarUX's other machines may come later, if
  wanted ("Other machines ZEsarUX would bring").
- **Publishing waits for the owner's review.**

## The owner's decisions on the second round (user-decided, 2026-10-31)

The owner accepted the second round's recommendations:

- **"Browser" stays untranslated, as a name**, in every language: it is
  the name NextZXOS's own menu gives its file browser. Applied: the second
  round had already written it so in the 26 language tables, and
  `waterbox/tests/check-names.py` (texts:names) holds it. The texts added
  in the third round keep it too.

## The owner's decisions on the third round (user-decided, 2026-10-31)

- **The SD card stays whole in the machine's memory, for now.** Reading it
  sector by sector from the file ("How the card is carried") is not done.
- **The ZEsarUX gaps behind patches 0004 and 0005 are not reported to
  ZEsarUX's author, for now.** They stay this core's patches: FatFs's
  FAT32 rule, the divMMC's paging by register 0x0A bit 4, register 0xB9,
  and register 0x8E with bit 3 clear. The gaps found in the fourth round
  (the Multiface's NMI, the 128K snapshot's 0x7ffd) are kept the same way.

## The owner's decisions on the fourth round (user-decided, 2026-10-31)

- **TX-1696 stays documented as not working, for now.** Its interrupts
  and music run since the fourth round, its picture stands still from
  frame about 1740 ("Fourth round"); nothing more is done on it now.
- **The CTC control word's bit 7 and the channel's bit in Next register
  0xC5 are one interrupt-enable bit.** The fourth round built it so as a
  guess the Next's documentation does not settle; it is now the owner's
  decision, and stays as built ("The Next's CTC and hardware IM2 mode").

## The 48K ROM is built in (2026-10-31)

- `waterbox/gen-roms.py` writes ZEsarUX's own `src/48.rom` (at the pinned
  commit) into a C file when the core is built, and stops if its SHA-1 is
  not 5EA7C2B8... (the same 16 KiB as Fuse's 48.rom). This repository
  holds no ROM.
- `waterbox/chimera_roms.c` hands it to ZEsarUX. ZEsarUX loads a ROM in
  three steps (cpu.c, `rom_load`): `find_sharedfile` looks for it in a
  list of folders, `get_file_size` asks its size, `fopen` reads it. The
  link wraps all three (and `fopen64` natively, which the host's headers
  turn `fopen` into); for the name `48.rom` the first answers with a path
  no mounted file can have (`chimera-built-in/48.rom`), and the other two
  answer that path from the core's memory (`fmemopen`). No patch to
  ZEsarUX.
- The built-in ROM wins over a file of the same name, as in the Fuse
  core: what the machine runs must not depend on what is mounted beside it.
- The firmware entry for `48.rom` is gone from `waterbox.config`, and with
  it its two texts in the 26 tables.
- `waterbox/ROMS.txt` says whose the ROM is and on what terms (Amstrad's,
  as Fuse's `roms/README.copyright` states them; ZEsarUX's repository has
  no such text); `package-licenses.json` lists it and the package carries
  it.

## Names are never translated or bent (owner's rule, 2026-10-31)

Proper nouns stay as they are in every language: this core's name
(ZEsarUX), the machine's (Next, Spectrum), its system's (NextZXOS), its
file browser's (the Browser, as NextZXOS's menu calls it) and Chimera's. A
language whose grammar puts endings on names says the sentence with a
common noun beside the name ("pocitac Next" in Czech, "komputer Next" in
Polish, "Next-tietokoneen" and "NextZXOS-jarjestelmalla" in Finnish,
"Next gep" and "NextZXOS rendszerrel" in Hungarian, "calculatorul Next" in
Romanian, "Next bilgisayarinin" in Turkish, "Next-datorns" in Swedish,
"Next-maskinen" in Norwegian and Danish). (Written here without their
accents, as this file is ASCII.)

- Found by the new check, before the fix: 130 names bent or missing in 13
  of the 25 translated tables, in 66 texts: Turkish (22, "Next'in",
  "NextZXOS'un"), Finnish (25, "Nextin", "NextZXOS:lla", "ZEsarUX:n"),
  Hungarian and Romanian (15 each, "Nexthez", "NextZXOS-szel",
  "Next-ul"), Polish and Czech (12 each, "Nexta", "Nextu", "Spectra"),
  Swedish, Norwegian and Danish (7 each, the genitive "Nexts",
  "Spectrums"), and Spanish, Catalan, French and Portuguese (2 each,
  where the Browser had been translated as "Navegador" or "Navigateur").
  All 66 texts were rewritten by hand.
- `waterbox/tests/check-names.py` is the check, and the test script runs
  it first (texts:names). It asks that every name an English text holds is
  in the same text in every language as it is written, and that no letter
  is glued to a name: no case ending, no genitive "s", no ending after an
  apostrophe or a colon ("Next'in", "ZEsarUX:n"), and no ending after a
  hyphen (a word of five letters or fewer after the hyphen, "Next-ul",
  "NextZXOS-szel"; a longer one is a compound noun, "Next-tietokone").
  Letters of the Latin, Cyrillic and Greek alphabets count; Chinese,
  Japanese and Korean characters do not (a particle in another script
  leaves the name whole). English is not checked ("the Next's" is its own
  grammar, as Chimera's own check also allows). File names (tbblue.mmc,
  .nex) are not names in a sentence and are skipped.
- Seen to fail: with every text fixed but one (Turkish suggest.fast,
  "Next'i"), check-names.py printed that one text and exited 1, and the
  test script reported texts:names FAIL (18 ok, 1 failed); with it fixed,
  0. Chimera's own `tools/check-proper-nouns.py`, pointed at `waterbox/`,
  finds nothing either (it knows only Chimera's names).

## Decisions (2026-10-31)

- **Three ways to start** (setting "Start"): `nextzxos` (the default, as a
  Next starts: boot ROM, firmware and NextZXOS from the card), `card`
  (NextZXOS from the card with the firmware's work done by the core, for
  cards the firmware cannot read; third round, "The System/Next 24.11
  card") and `fast`
  (ZEsarUX's "fast boot": the Next's hardware with the 48K ROM, no card,
  the program loaded straight into memory). Fast needs only the 48K ROM; a
  program that wants NextZXOS's services or files from the card does not
  run that way.
- **The core suggests the start** (`SuggestSettings`): `card` when the
  project's SD card is one the firmware cannot read, `nextzxos` for any
  other card, `fast` when it has only a program, so that a
  person who brings only a .nex is not refused for want of a card. The New
  Project window shows the suggestion with a note saying why.
- **"Hold NextZXOS at 7 MHz"** (off by default). ZEsarUX by default holds
  the processor at 7 MHz while NextZXOS's own code runs, to spare the host
  (its `--denyturbotbbluerom`). A Next does not; the core does not either
  unless asked. Measured: NextZXOS idle at its menu runs at about 200
  frames a second in the sandbox either way it is set (10.7 s for 3000
  frames held, 15.2 s not held).
- **A .nex file is loaded without its folder** (`--nex-no-automount-esxdos`).
  ZEsarUX's fast loading of a .nex offers the program the folder the file is
  in through its esxDOS emulation, a folder of the host; the sandbox has no
  folders. A program loaded the fast way that reads more files therefore
  does not find them; with NextZXOS the files can be put on the card.
- **The system id is `ZXN`** and the input device "ZX Spectrum Next
  Keyboard and Joystick"; the key names and their letters in a movie are
  the Fuse core's ("Keyboard Q", `Q`), so that movies of the two read alike.
- **50 frames a second.** The core runs the Next at 50 frames a second, as
  ZEsarUX's own timer does. Whether ZEsarUX's Next has a 60 Hz mode at all
  (Next register 5) was not looked into. The first start of a card shows
  the firmware's video mode test, which cycles the modes; a frame stayed
  312 sound samples throughout it.

## The System/Next 24.11 card (2026-10-31, third round)

The owner provided System/Next 24.11 for emulators, the official
distribution from specnext.com/latestdistro (free to download), on this
machine outside the repository: `cspect-next-1gb.img` (a 1 GiB SD card
image: NextZXOS 2.09, firmware 1.44.db, the Next's games and demos) and
`enNextZX.rom` and `enNxtmmc.rom`. The two ROMs are the same bytes as the
card's own `/machines/next/enNextZX.rom` and `enNxtmmc.rom` (SHA-1
compared); they are there for CSpect, which is started with them.

### What ZEsarUX needs from it

Neither of the two ways ZEsarUX has. With ZEsarUX's own boot ROM
(`tbblue_loader.rom`) the card's firmware (`TBBLUE.FW`, 1.44.db) loads and
runs, reads the card's partition table and boot sector, and stops at
"Error opening 'menu.ini/.def'!" without reading another sector (traced:
sectors 0 and 63, then nothing). The card's file system is FAT32 laid out
with 32766 clusters of 32 KiB, and the firmware's FAT reader (FatFs
R0.12b, its source is on the card in `/src/firmware`) calls a volume of
fewer than 65525 clusters FAT16, and a FAT16 volume with no fixed root
folder no file system. A real Next would stop the same way: the card is
made for emulators, which start NextZXOS from the two ROMs without the
firmware, as CSpect does. NextZXOS's own file system code reads it.

So the core has a third way to start, **"card"**: no boot ROM and no
firmware; the core does the firmware's work itself, from the card in the
machine's memory, in the order the firmware does it (`boot.c` and
`config.c` of firmware 1.44, on the card): it reads
`/machines/next/config.ini` if there is one, `menu.ini` or else
`menu.def`, takes the menu line `config.ini` names (the first when it names
none: "ZX Spectrum Next (standard)", mode 2, `enNextZX.rom`,
`enNxtmmc.rom`, `enNextMF.rom`), loads `keymap.bin` into the keymap
registers, the divMMC ROM to 0x10000, the Multiface ROM to 0x14000 and the
64 KiB of NextZXOS's ROMs to 0 of the Next's memory, sets the peripheral
registers (5, 6, 8, 9, 0x0A, 0x82 to 0x85) from the firmware's defaults,
the machine type (register 3), and soft-resets into the ROM. ZEsarUX is
started in its fast boot mode for this (no boot ROM to load) and is told
afterwards it is a Next like any other. Hard Reset does the same again.
The user brings only the card: no firmware. A card whose boot sector is
laid out as FAT32 with too few clusters is one the firmware cannot read,
and for it the core suggests "card" (`SuggestSettings`); for any other
card it suggests "nextzxos" as before.

The card's ROMs are read with FatFs, the FAT reader ZEsarUX bundles for
its `--copy-file-to-mmc` (the core already uses it to put a program on the
card). FatFs R0.14a has the same cluster rule as the firmware's, so patch
0004 makes it read a volume laid out as FAT32 (no 16-bit FAT size, no
fixed root folder) as FAT32, as NextZXOS and Linux do. The same patch
lets the core lend FatFs the card the machine holds: before, FatFs read a
second whole copy of the file and the core copied it back over the
machine's card, which for 1 GiB would have needed 2 GiB at once.

NextZXOS 2.09 then needed three things ZEsarUX 13.0 does not have
(patch 0005, each found by tracing the processor where it went wrong):

- **The divMMC's automatic paging by register 0x0A bit 4.** NextZXOS 2.09
  clears register 6 bit 4 at start (on the newer cores that bit is the
  DRIVE button's NMI) and counts on 0x0A bit 4; ZEsarUX paged the divMMC
  by register 6 only, so 2.09 rebooted every eight frames. Either bit now
  turns the paging on, and a change to 0x0A applies it.
- **Register 0xB9, "entry points valid only when ROM3 is paged".**
  NextZXOS 2.09 sets 0xB9 to 0 and calls its own RST 8 with ROM0 paged in;
  ZEsarUX paged the divMMC in at 0x0008 regardless, and ran esxDOS code
  where none was loaded. Now an entry point of 0xB8 whose 0xB9 bit is 0,
  and those of 0xBB that the Next's documentation marks ROM3 (0x3DXX,
  0x04C6, 0x0562, 0x04D7, 0x056A), page the divMMC in only with ROM3 (the
  48K BASIC ROM) paged in. 0xB9 is 0x01 and 0xBA 0x00 after a reset, as
  on a Next.
- **Register 0x8E with bit 3 clear leaves MMU6 and MMU7 alone.** ZEsarUX
  applied it as a write to port 0x7FFD, which also maps the 128K RAM bank
  at 0xC000. NextZXOS 2.09's esxDOS calls write 0x8E with bit 3 clear in
  the middle of a load into a bank the loader mapped there, so every bank
  of a .nex but 5 and 2 landed in bank 0 (measured: Warhawk's banks found
  at 0x40000 one after another), and the games jumped into empty memory.

Tried and not kept: a soft reset by register 2 doing the whole of
ZEsarUX's reset (as the Reset button does) changed nothing once 0x8E was
right, and was taken out; answering as an SDHC card (block addressing)
made NextZXOS 2.09 say "Drive C: not found", so the card stays a
byte-addressed one, as ZEsarUX has it.

### How the card is carried (decision, 2026-10-31)

The card stays whole in the machine's memory, as ZEsarUX keeps it, read
while the core starts, before the sandbox seals the machine. The sealed
image is the baseline every state is measured against, and a state holds
only the pages that changed since: what NextZXOS wrote to the card, not
the card. Measured with the 24.11 card: a state at Warhawk's title (frame
2000) is 5.8 MB; a state at the old card's menu was 4.2 MB, and is 4.7 MB
with this round's core. A state is not bloated, and nothing needs Chimera
to learn anything new.

What it costs, measured on this machine: the memory the package declares
grows to 1280 MiB of mmap (`memoryLayoutMiB`, was 256), room for a card
of up to 1 GiB beside ZEsarUX's own tables; a bigger card is refused with
a reason (`error.sd_too_big`, tested with a 2 GiB file of zeros). A
running machine with the 1 GiB card uses 1.08 GB of memory (PSS; the
process's RSS says 2.1 GB because the sandbox maps the machine's memory
twice, once for the guest and once for the host). Starting takes 1.5 s
(reading the card). The speed is that of the old card: about 190 frames a
second at NextZXOS's menu and in Warhawk. A state saved and loaded before
every frame costs about 11 ms a frame (500 frames in 5.8 s).

The other way, not done: read the card's sectors from the file as the
machine asks for them, and keep in the machine only the sectors it writes.
Chimera mounts a project's files by path without reading them
(`wbx_mount_file_path`), and the open file and its position are in
states, so this needs nothing new from Chimera either; it would save the
1 GB of memory a running machine holds. It needs ZEsarUX's card emulation
(`mmc.c`) and the FAT copy reworked, and was left for the owner to weigh
against the memory.

### What on the card is free to use

The card's `LICENSE.MD` (the System/Next distribution licence, SpecNext
Ltd): the distribution may be used, copied and distributed free of charge
in its entirety, and used on hardware other than the Next (emulators); it
may not be sold. Its parts carry their own terms, which come first: the
Amstrad ROMs may be distributed only as Amstrad allows (emulators are
allowed); software with source under `/src` is under the MIT licence
unless a licence in `/docs/licenses` says otherwise (the firmware's
source is GPL 3); software without a licence is "further distribution
denied" on its own; documents and media without one are CC BY-NC-SA. The
games (Warhawk, Revival Survival, Halls of the Things, Santa's Pressie and
the rest) are "distributed under license" by their authors with no grant
to pass them on alone. So: the card may be used for tests on this machine
and by anyone who downloads it; nothing of it is copied into this
repository or the package, a test reads it from where the owner put it
(`ZESARUX_SN_CARD`), and only the whole distribution, unchanged, may be
passed on.

### Games from the card (2026-10-31)

All started from switching on with "card", through NextZXOS 2.09's
Browser, native and sandbox the same lines every 500 frames, the same with
a state saved and loaded before every frame, and not the run left alone:

- **Revival Survival** (`GAMES/Next/Revival Survival`, a .nex), in the
  test script as game:sn-card: its title by frame 2500, SPACE, 1
  (Arcade) and SPACE through the story start a round; moved with the
  joystick and its keys and fired, the score is 2150 at frame 5200.
- **Warhawk** (`GAMES/Next/Warhawk`, the 2024 build on this card), by
  hand: title by frame 2000, SPACE starts it, P and SPACE to frame 4000,
  the ship moved right and the score is 52.
- **NextBASIC Invaders** (`GAMES/Next/NextBASIC Invaders`, a NextBASIC
  program), by hand: its menu by frame 2500, K (keys) and 5 (difficulty)
  start it, P and SPACE to frame 3500, the score is 50.

In the third round Halls of the Things, Angry Bloaters, Santa's Pressie,
TX-1696 and DARKSTAR.Z80 were started and not played; the fourth round
says why each failed ("Fourth round").

## Fourth round: the programs that did not work (2026-10-31)

Each started from switching on with "card" through NextZXOS 2.09's
Browser, from the 24.11 card. For each: what it was, and whether it is the
core, a gap of ZEsarUX's, or the program. The Next's documentation
compared with is the card's own: `docs/EXTRA-HW/io-port-system/registers.txt`
and `PORTS.TXT` (read from the card; not copied here).

- **DARKSTAR.Z80: a gap of ZEsarUX's, the Multiface's NMI.** The file is a
  .z80 snapshot of a 128K ("modified for the Next" by Design Design,
  2018). The Browser hands a .z80 to NextZXOS's snapshot loader
  (`snapload.bas`, as the card's `NEXTZXOS/BROWSER.CFG` says), which loads
  the snapshot's memory and ends by writing 0x08 to Next register 2: "bit
  3 = Generate multiface nmi". The Multiface's ROM (`enNextMF.rom`, which
  "card" and the firmware load to 0x014000) then puts the snapshot's
  registers back. ZEsarUX documents the bit in its source and does not act
  on it, so the loader carried on into code that was never meant to run,
  returned through a stack that was not there and ran into empty memory
  (traced: the processor ran the text of the file's path at 0xFF00, and
  then a loop at 0x0000); the screen stayed black. Patch 0005 now makes
  that bit page in the Next's Multiface (ZEsarUX's own Multiface module
  already knows where the Next keeps its ROM and RAM) with the ports
  register 0x0A bits 7:6 choose, under the documentation's rules (only
  when the bit was 0, not in config mode, not while the divMMC or the
  Multiface is paged in). The Multiface is paged in for the fetch at
  0x0066 itself: its ROM has a jump there (JP 0x006D), where the system's
  ROM has a RETN that would end the NMI at once. A divMMC NMI does not
  page the Multiface in. Played: its Hall of Fame by frame 2500, SPACE
  starts it, the ship turned with P and O; in the test script as
  game:sn-snapshot.
- **The fast start with a .z80 (found while looking at DARKSTAR): the
  core.** ZEsarUX turns into the machine a .z80 or .sna was made for (a
  128K here); the core's memory exports kept pointing at a Next's 2 MiB,
  and the native runner crashed reading them. The core now starts ZEsarUX
  with `--snap-no-change-machine`, so the snapshot is loaded into the
  Next. A 128K snapshot was then left with the last bank ZEsarUX loaded
  paged in (it loads each bank through port 0x7ffd) and ran into the
  48K ROM: patch 0006 writes the snapshot's own 0x7ffd afterwards, as
  ZEsarUX does for a 128K. DARKSTAR.Z80 in the fast start shows its Hall
  of Fame at frame 600 and plays, natively, in the sandbox and with a
  state every frame the same. Any other program that turns ZEsarUX into
  another machine (a .zsf of another machine does) is now refused with a
  reason (`error.other_machine`, a new text in the 26 tables) instead of
  running a machine the core does not describe: seen with the option
  taken out on purpose, "ZEsarUX did not start: the program is for
  another machine than the Next". The test script's snapshot:128k loads a
  128K snapshot it makes itself (`tests/make-z80.py`).
- **Angry Bloaters and Santa's Pressie: they work now.** Both failed in the
  third round before patch 0005; with it, Angry Bloaters starts with 1
  and S and was played to stage 1 (score 10, a life lost), and Santa's
  Pressie ("Santa's Chimney Challenge!") starts with ENTER and was played
  (two presents delivered, three wasted). The third round's Browser steps
  for Santa's Pressie had opened LICENSE.TXT instead of the .nex.
- **Halls of the Things: the program; there is no map to leave.** What
  looked like a map is the game: a view of the tower with "Terror
  Incognita..?" over it, and the man (white, on the right) moves with
  Z, X, N and M (the game's Key Layout page: Z left, X right, N up, M
  down, F fireball, L lightning, A arrow, S sword, SPACE centres the
  man). Played: the man moved left and up, the view scrolled, a fireball
  fired.
- **TX-1696: a gap of ZEsarUX's, the Next's CTC and its interrupt
  controller.** Its intro ("A man in his technoshed") plays; at frame
  about 1740 it sets Next register 0xC0 to hardware IM2 mode, enables the
  CTC's channel 0 interrupt (0xC5), programs channel 0 as a timer at
  28 MHz / 16 / 112 = 15625 times a second (port 0x183B, control word
  0x85, constant 0x70) and the line interrupt, and plays its music from an
  interrupt routine at 0x8000 that writes samples to a DAC. ZEsarUX 13.0
  has no CTC and no hardware IM2 mode: no interrupt came, and the
  picture stood still from frame 1740 on (the program still ran its main
  loop). The CTC and the hardware IM2 mode are now the core's
  ("The Next's CTC and hardware IM2 mode", below): TX-1696's interrupt
  routine then runs 313 times a frame and its music streams from the card
  (the main loop reads 512 bytes of TITLE15.RAW a frame through esxDOS and
  the routine writes them to the DAC at port 0xDF). Its picture still
  stands still from frame 1740 to frame 9000 (tried), and it reads no key
  in that time: the program updates its 128 hardware sprites every frame
  with the same values. Why was not found. Not said to work.

Tests of this step, against Chimera main cb46d2b7: run-gate.sh with the
24.11 card, 23 passed, 0 failed, 0 skipped (two new tests: game:sn-snapshot
and snapshot:128k); run-frontend.sh 3 of 3; Chimera's test-packages on a
folder with only this package, all passed; check-core-texts.py 25 keys,
every one named; check-names.py 0; check-proper-nouns.py 0. Seen to fail:
with the Multiface's NMI and patch 0006 both taken out on purpose,
game:sn-snapshot failed ("the game played and the game left alone end
alike": the screen stays black either way) and snapshot:128k failed ("the
processor is at c000"). Angry Bloaters, Santa's Pressie, Halls of the
Things and Dark Star, each played by hand: native and sandbox the same
lines every 500 frames, the same with a state every frame, and not the run
left alone.

## The Next's CTC and hardware IM2 mode (2026-10-31, fourth round)

ZEsarUX 13.0 has neither the Next's CTC (its counter/timer) nor its
interrupt controller's "hardware IM2 mode", in which the controller gives
the processor a vector of its own for each device. Both are now the
core's own code, `waterbox/next_interrupts.c`, called from small hooks
under `CHIMERA_CORE`: the CTC's ports and the controller's registers
(patches 0003 and 0005), the ULA's interrupt at the end of a frame, the
step after each instruction and the vector (0003), and RETI (0007,
`cpus/z80_codpred.c`). What it follows, from the card's
`registers.txt` and `PORTS.TXT`:

- Four CTC channels on ports 0x183B to 0x1B3B ("temporarily reduced to
  four channels from eight"), each a Zilog CTC channel: timer (prescaler
  16 or 256) or counter, a time constant (0 is 256), each channel's zero
  count feeding the next channel's trigger, the Next's hard and soft
  reset rules. A read gives the down counter.
- The CTC's clock is the Next's 28 MHz: eight ticks for each 3.5 MHz
  T-state, whatever speed the processor runs at. (TX-1696's 28 MHz / 16 /
  112 is 15625 a second, its music's sample rate.)
- Register 0xC0 bit 0 chooses the pulse (ZEsarUX's own handling, kept) or
  hardware IM2 mode: vector = 0xC0 bits 7:5, the device times 2 (0 the
  line interrupt, 3 to 6 the CTC's channels, 11 the ULA). In that mode the
  line and ULA interrupts are made by this file too. A device waits until
  it is taken; once taken it is "in service" until RETI, and it and every
  device of lower priority wait until then, as Zilog peripherals on a
  daisy chain do.
- 0xC4 to 0xC6 enable the devices' interrupts (0xC4's line and ULA bits
  are register 0x22's, both ways); 0xC8 to 0xCA say which devices made one
  (written with 1s to clear; a pending one stays set).
- The CTC control word's bit 7 (its interrupt) and the channel's bit in
  0xC5 are treated as one bit. The documentation does not settle it; the
  owner decided it so (user-decided, 2026-10-31).

Not done: the UARTs' interrupts, a DMA transfer being interrupted (0xCC
to 0xCE), the CTC's trigger edge bit and its vector word, and channels 4
to 7.

The test script's next:ctc runs the test program with a CTC part
(`make-nex.py --with ctc`): channel 0 at 28 MHz / 256 / 256, its own
interrupt routine reached through vector 0x06; it counted 1706 interrupts
in 200 frames (the 48K timing's 69888 T-states a frame give 1706.25).

## Fifth round: Monster Max and three Spectrum tapes (2026-10-31)

The owner provided, on this machine outside the repository, Monster Max
for the Next (`monster-max-next-v1.0.nex`, 721408 bytes, a "NextV1.2"
file; Rare's 1994 Game Boy game, licensed to Titus) and three ZX Spectrum
tapes: Manic Miner (Software Projects), Jet Set Willy 2 and Golden Baton
(Mysterious Adventures 1, Channel 8 Software). They are commercial games:
the tests copy them into their own folders, and nothing of them is in
this repository or the package.

### Monster Max

- **The fast start** (no card, the .nex loaded by ZEsarUX): the Titus page
  by frame 300, SPACE gives the licence page, SPACE again the title
  ("UP(Q) DOWN(A) LEFT(O) RIGHT(P) SELECT(S) START(X)"), X the language
  page, X (English) the first room, the PLAYPEN, by frame 2100. Played: P,
  Q and O walk, SPACE or the joystick's Fire jumps, the joystick walks
  right. The picture is the Game Boy's 160x144 screen in the middle of
  the Next's.
- **From the card**: the System/Next 24.11 card started with "card", the
  .nex put on it by the core; the Browser lists it as the 11th line of
  its top folder (after MACHINES), ENTER runs it with NextZXOS's own
  loader, and the Titus page is up by frame 800. The same buttons 700
  frames later end on the fast start's picture, hash for hash. Not tried
  from ZEsarUX's own 64 MiB card.
- Native and sandbox the same lines (every 250 frames in the fast start,
  every 400 from the card), a state saved and loaded before every frame
  changes nothing, a state saved at frame 2000 (4.4 MB) and loaded by
  another process ends where the unbroken run does, and the game played
  is not the game left alone.
- **In chimera-run**: a project that starts fast with the game and a movie
  of those buttons; the movie recorded by chimera-run is the movie played,
  and played back by another process, and again with the engine's save
  and load before every frame, its picture at frame 2500 is this
  repository's runner's.
- **In Chimera's Qt window** (headless): the same project, its movie
  played by the window to frame 2500: the Next's 1.75 MiB of memory is the
  native build's with the same buttons, and not the native build's with
  the game left alone in its first room. The window counted 931 frames
  without input in 2500.
- In the test script as game:monster-max (ZESARUX_MONSTER_MAX names the
  .nex; it says SKIP without it), and in the window test as
  window:monster-max (the same variable).

### The three tapes

- **ZEsarUX's own tape loading, in the fast start** (the Next with the 48K
  Spectrum's ROM: the Next in its 48K personality). A .tzx in the program
  slot is loaded by ZEsarUX's tape loader, which reads it at once instead
  of at the tape's speed: Manic Miner's title by frame 500, Golden
  Baton's first question by frame 500, Jet Set Willy 2's first page at
  frame 250. Each played a little:
  - Manic Miner: ENTER at the title, the Central Cavern; Willy walked
    right with P and jumped with SPACE.
  - Jet Set Willy 2: "PRESS ENTER TO CONTINUE", then its code card ("Enter
    code at location M3 on page 4"). There is no code card here, so the
    code was found by trying all 256 from a state (in the sandbox, about
    5 minutes): blue, yellow, blue, green. The game takes the colours
    from keys 1, 2, 4 and 6 (blue, red, green, yellow); 3 and 5 do
    nothing, and a key is taken only when held for a while (15 frames;
    ENTER at the title wanted 60). The location asked depends on the
    moment, so another run asks for another (in NextZXOS's 48K mode it
    was N7 on page 7: yellow, blue, red, yellow). After it, the title and
    The Bathroom; Willy walked right to the tap ("Items : 001"). Two of
    his lives were gone by then; why was not looked into.
  - Golden Baton: "Resume play on a Game ?", N, the forest picture and
    "WHAT NOW ?"; LOOK gave "I see nothing special", N gave "O.k." and the
    next place's picture.
  In the test script as game:tapes (ZESARUX_OWNER_TZX names the folder;
  it says SKIP without it): each native and sandbox the same lines every
  250 frames, the same with a state every frame, not the game left alone.
- **NextZXOS's own tape loading**, on the 24.11 card with "card": the
  Browser hands a .tzx to NextZXOS's TZX loader, which asks for a mode
  (128K, USR0, 48K, 48K CODE, Pentagon, Next/+3) and says "TZX loading
  requires a Pi Zero installed": chosen 48K, it stops at "Error
  communicating with Pi...". The Next's Raspberry Pi Zero (its
  accelerator board, which plays a .tzx for it) is not emulated by
  ZEsarUX. So a .tzx does not load through NextZXOS here; ZEsarUX's own
  tape loading (the fast start) is the way.
- All three tapes hold only standard blocks, so each was also turned into
  a .tap (the same bytes, a few lines of Python in a scratch folder; not
  committed) and put on the card: NextZXOS's TAP loader, which needs no
  Pi, loads them. Manic Miner in its 48K mode and its 128K mode, played
  as above; Golden Baton in both, LOOK and N typed; Jet Set Willy 2 in
  48K mode, its code found again and The Bathroom's tap taken. Native and
  sandbox the same lines for Manic Miner in 128K mode, Golden Baton in 48K
  mode and Jet Set Willy 2 in 48K mode (the others were seen in the
  sandbox only). Not in the test script: it would need the .tap copies,
  which the owner did not give.
- A .tzx in the program slot with NextZXOS is put on the card like any
  program; the core does not also put it in ZEsarUX's tape player, so
  NextZXOS's 48K or 128K BASIC cannot LOAD "" it from the tape. Not done.

### Tests of this round

Against Chimera main cb46d2b7 (~/chimera-scratch-bump), with the core of
the fourth round (unchanged this round): run-gate.sh with the 24.11 card,
Monster Max and the tapes, 26 passed, 0 failed, 0 skipped (new:
game:monster-max, game:tapes); run-frontend.sh (Qt window, headless) 4
of 4 (new: window:monster-max); Chimera's test-packages on a folder with
only this package, all passed; check-core-texts.py 25 keys, every one
named; check-names.py 0; check-proper-nouns.py 0. No text was added to
the language tables. Without their variables the three new tests say
SKIP and why. Seen to fail: game:monster-max with the game left alone made
the game played (on purpose, taken out): "the game played and the game
left alone end alike"; game:tapes with the sandbox run one frame short (on
purpose, taken out): "native and sandbox differ" for each tape;
window:monster-max with the native run one frame short (on purpose, taken
out): "same as native with the buttons no".

## What the user brings

- `tbblue_loader.rom` (8 KiB, SHA-1 8B2E927D0523BC27BFFCC34C39527AB0D2362E0C):
  the TBBlue boot loader, by Victor Trucco and Fabio Belavenuto, which the
  FPGA runs first and which reads TBBLUE.FW from the card. ZEsarUX ships it
  in its repository; no licence for it is in ZEsarUX's repository, and its
  source is part of the Next's FPGA project. Not packaged. Needed only to
  start with "nextzxos".
- An SD card image (`.mmc` or `.img`, at most 1 GiB): ZEsarUX ships `tbblue.mmc`, 64 MiB,
  "System/Next 1.3.2, NextZXOS 2.06H, core 3.01.05, firmware 1.29b" (its
  README.MD). The card's LICENSE.MD (SpecNext Ltd's System/Next
  distribution licence) permits copying and distributing exact copies of
  the distribution in its entirety, free of charge, and its use on other
  hardware; parts carry their own licences, the Amstrad ROMs among them.
  Whether ZEsarUX's 64 MiB card is "the distribution in its entirety" is not
  plain. Not packaged. Or the System/Next distribution's card for
  emulators (24.11: `cspect-next-1gb.img`, 1 GiB, NextZXOS 2.09), started
  with "card" and nothing else ("The System/Next 24.11 card").
- Not brought: `48.rom` (16 KiB, SHA-1
  5EA7C2B824672E914525D1D5C419D71B84A426A2), for the fast start:
  Amstrad's ROM, which Amstrad allows to be distributed. Built into the
  core (user-decided, 2026-10-31), as the Fuse core builds in Amstrad's
  ROMs.

## What was measured (2026-10-31)

Third round (against Chimera main cb46d2b7 in ~/chimera-scratch-bump):

- `ZESARUX_SN_CARD=<the 24.11 card> CHIMERA_ROOT=<chimera>
  waterbox/run-gate.sh`: 21 passed, 0 failed, 0 skipped. The 20 tests of
  the second round all pass with patches 0004 and 0005 and the bigger
  memory layout (the old card, the fast start, the window), and the new
  game:sn-card (Revival Survival from the 24.11 card, "Games from the
  card"; with CHIMERA_ROOT it also asks chimera-run what the core
  suggests for a project with that card: "card"). settings:refused now
  also refuses a card of 2 GiB for its size. Without ZESARUX_SN_CARD,
  game:sn-card says SKIP and why.
- `run-frontend.sh` (Qt window, headless): 3 of 3. Chimera's
  `test-packages` on a folder with only this package: all passed.
  `tools/check-core-texts.py`: 24 keys, every one named, every name a key.
  `check-names.py`: 0. Chimera's `check-proper-nouns.py`: 0.
- Hard Reset with "card" (frame 500, at the welcome page) brings the
  welcome page back by frame 900, natively and in the sandbox with a state
  every frame the same lines.
- Seen to fail: game:sn-card with the run left alone made the run played
  (on purpose, taken out): "the game played and the game left alone end
  alike"; settings:refused expecting another size in the refusal (on
  purpose, taken out): "a card of 2 GiB is not refused for its size".

Earlier rounds, on this machine (Linux, GCC 14). First against Chimera at 3f8ea7e2, then
(second round, the same day) against Chimera main cb46d2b7, both in
~/chimera-scratch-bump, with the miniBox of that checkout. The numbers
below are the second round's unless they say otherwise.

`CHIMERA_ROOT=<chimera> waterbox/run-gate.sh`: 20 passed, 0 failed, 0
skipped, in 308 seconds (first round: 16 of 16, in 4 minutes).

- texts:names: every name in the 26 language tables as it is written
  ("Names are never translated or bent", above).
- rom:built-in, frontend:built-in-rom: see "The 48K ROM is built in".

- boot:fast: 600 frames with buttons by a pattern, native and sandbox the
  same lines every 60 frames; 50 frames a second, 312 samples a frame,
  704x608.
- boot:nextzxos: NextZXOS from the card through the video mode test and the
  welcome pages to its menu (1500 frames), and 1500 frames with buttons by a
  pattern: native and sandbox the same every 100 frames.
- time:virtual: the host waiting 0.3 s every 100 frames changes nothing; the
  native build on the host's clock (the control) gives another machine; the
  clock started at 2030 gives another machine, native and sandbox the same.
- state:every-frame: a state saved and loaded before every frame changes
  nothing (NextZXOS 1500 frames; the test program 150 frames with keys); one
  stale state (the control) shows.
- state:other-process: saved at frame 400, loaded by another process, the
  run ends where the unbroken one does. A state is 4.2 MB.
- input:buttons: a key, the joystick's Fire, Fire 2 and Up each make their
  own picture, the same natively; the controls were read in 49 of 100 frames
  (the program reads every second frame).
- input:reset: Hard Reset gives the 48K ROM's first screen; Reset (soft)
  starts the ROM again and keeps the Next's settings (the program's Layer 2
  stays on), which is how ZEsarUX does it.
- next:hardware: the test program's hardware sprite, copper list (Layer 2
  shifted sideways for lines 140 to 170), zxnDMA copy, tilemap (two rows of
  green tiles at the screen's character rows 9 and 11, its own palette
  colour set by the program), a tone on each of the three AY chips (chip 1
  channel A, chip 2 channel B, chip 3 channel C, chosen through port 0xFFFD
  as TurboSound does) and two saw waves on the DACs (ports 0xDF and 0xB3,
  written every frame) each change the run when left out (make-nex.py
  --without): the picture for the first four, the sound and not the
  picture for the others. The whole is the same natively, in the sandbox
  and with a state every frame. The program turns on TurboSound and the
  DACs itself (Next register 8 = 0x1A); ZEsarUX's fast start already does,
  NextZXOS may not.
- game:nextoid (second round): Nextoid, a bat-and-ball game on the same
  card (GAMES/Next/Nextoid/NEXTOID.NEX), through the Browser as Warhawk is;
  its menu by frame 2500, 1 (keyboard) and S ("Press S to start game!")
  give level 1; SPACE launches the ball, which breaks a brick (score 20 at
  frame 3400). Native and sandbox the same lines every 500 frames, the same
  with a state every frame, and not the run left alone after the start.
- Two more games from the card, tried by hand and not in the test script
  (second round), each through the Browser and to frame 3000, native and
  sandbox the same lines every 500 frames: The Lords of Midnight
  (GAMES/Next/LordsOfMidnight/LOM.NEX), its title page and, after SPACE
  and ENTER, the first view ("Luxor the Moonprince ... looking Southeast
  to the Downs of Shadows"); and the Night-Knight demo
  (Night-Knight-Demo.nex), its "Get ready" page and stage 1 played with
  the joystick (the floor turned yellow under the knight, score 50).
- game:warhawk: Warhawk (Rusty Pixels, 2019), a Next game on the card
  ZEsarUX comes with (the Next's own system distribution): from switching
  on through NextZXOS's Browser (GAMES, Next, Warhawk, Warhawk.nex), its
  title at about frame 4000, started with SPACE and played (the ship moved
  right and fired, the score 40) to frame 5000. Native and sandbox the same
  lines every 500 frames, the same with a state every frame, and not the
  run left alone after the start.
- program:sd-card: the program copied to the card is listed by the Browser
  and run by NextZXOS (the processor in its code at frame 1800), native and
  sandbox the same; the card file is unchanged.
- rom:built-in (2026-10-31): the fast start with no ROM handed over, with
  upstream's 48.rom handed over and with 16 KiB of noise named 48.rom
  handed over: the same 300 frames with buttons, natively and in the
  sandbox. frontend:built-in-rom: a project set to start fast, with no
  firmware, runs in chimera-run and shows the runner's picture at frame 99.
  The first native build of this crashed: the host's headers turn `fopen`
  into `fopen64`, which was not wrapped yet.
- settings:refused: an unknown start, no card, no boot ROM: each refused
  with a reason.
- frontend:picture, frontend:states, frontend:movie, frontend:firmware, in
  chimera-run with projects: a recorded movie of 1500 frames played back by
  another process shows this repository's runner's picture at frame 1499,
  also with a save and load before every frame; the movie's row has 42 + 6
  columns; NextZXOS with no boot ROM is refused and the ROM named; the core
  suggests the fast start for a project with a program and no card, and
  NextZXOS for one with the card (chimera-run --suggest).

`waterbox/tests/run-frontend.sh --chimera-root <chimera>` (Chimera's Qt
window, headless, QT_LIB_DIR set to Qt's lib folder): 3 of 3. NextZXOS 1000
frames in the window leaves the Next's 1.75 MiB of memory byte for byte the
native build's; the fast start with the test program in the project gives
another memory, the native build's with that setting; the package's 46 key
bindings are the window's.

Chimera's contract test (`test-packages` on a folder with only this
package): all passed. `tools/check-core-texts.py`: 20 keys, every one named.

Seen to fail:
- rom:built-in and frontend:built-in-rom (2026-10-31), with the core made
  not to answer for `48.rom` (on purpose, taken out): both failed ("Unable
  to open rom file 48.rom"), with every other fast-start test;
- the native-against-sandbox tests, with the sandbox's clock one second
  ahead of the native build's (a change made on purpose and taken out):
  boot:nextzxos, time:virtual and program:sd-card failed. A change of the
  sandbox's rand() seed did not fail any test: nothing of the machine uses
  rand(), so that was no test of them;
- input:buttons failed for a real reason before it passed: the first test
  program put the joystick's bits into the colours' flash and bright bits,
  so Fire and Up looked like nothing pressed;
- input:reset failed for a real reason: Reset (soft) leaves Layer 2 on, so
  the test now asks Hard Reset for the first screen;
- settings:refused and frontend:firmware failed for a real reason: the boot
  ROM's absence was printed before ZEsarUX's display was up and so was not
  kept as the reason; the debug_printf wrap fixed it;
- the window test, with the native build run one frame short (on purpose,
  taken out): window:memory and window:setting failed;
- next:hardware, with make-nex.py's --without made to do nothing (on
  purpose, taken out): all four parts failed; in the second round, with
  --without made to ignore the four new parts: the test failed on each of
  the four ("without the tilemap the picture is the same", and the sound
  for ay2, ay3 and dacs);
- game:nextoid, with the run left alone made the same as the run played
  (on purpose, taken out): it failed ("the game played and the game left
  alone end alike");
- the sandbox's clock one second ahead again, after game:warhawk was added:
  game:warhawk failed with boot:nextzxos, time:virtual and program:sd-card;
- frontend:suggest, with the core suggesting nothing for a project with only
  a program (on purpose, taken out): it failed.

A fresh clone (submodule from the local checkout), built with HOME set to
an empty folder: the same core.wbx byte for byte. A native build with GCC
13: NextZXOS to its menu with the GCC 14 build's lines.

Speed, in the sandbox: about 200 frames a second at NextZXOS's menu
(28 MHz), 193 frames a second for the test program at 28 MHz; 4 times a
real Next.

## What is not done

- Games tried: from the card ZEsarUX comes with, Warhawk and Nextoid in
  the test script, The Lords of Midnight and the Night-Knight demo by hand;
  from the 24.11 card, Revival Survival in the test script, Warhawk (2024)
  and NextBASIC Invaders by hand, and in the fourth round Dark Star in
  the test script, Angry Bloaters, Santa's Pressie and Halls of the
  Things by hand. Commercial games (fifth round): Monster Max in the test
  script and the window test, Manic Miner, Jet Set Willy 2 and Golden
  Baton in the test script. TX-1696 runs with a still picture ("Fourth
  round"; it stays so, the owner's decision); the 24.11 card's other
  games and demos not tried.
- A .tzx through NextZXOS's own TZX loader: it needs the Next's
  Raspberry Pi, which ZEsarUX does not emulate ("Fifth round"). A tape in
  the project with NextZXOS is not put in ZEsarUX's tape player either.
- Not tried: the sound of a real game listened to (the tests compare
  sound by its hash); .snx and the other program formats in the program
  slot (.tzx and .tap were tried in the fifth round) (with "card" the test program is put on the 24.11 card and
  listed by its Browser, natively and in the sandbox the same, the card
  file unchanged; it was not run from there).
- The card is held whole in the machine's memory (about 1.1 GB with the
  24.11 card); reading it sector by sector from the file instead is the
  owner's to weigh ("How the card is carried").
- A card bigger than 1 GiB (the CSpect 2 GiB card, a real Next's card) is
  refused.
- The 60 Hz mode: not looked into.
- The Kempston mouse (the Next's PS/2 mouse) has no input here.
- The Next's extended keyboard registers (Next registers 0xB0 and 0xB1)
  are not emulated by ZEsarUX.
- No CI run: the repository has no remote. .github/workflows/chimera.yml is
  written from the Fuse core's and has never run; it has no 24.11 card,
  so game:sn-card would say SKIP there.
- No Windows run: the frontend gate's scenario on Windows is still to be
  run before the core is called done; whether a machine of 1.4 GiB
  declared memory costs Windows commit charge up front was not measured.
- No movie recorded by hand in Chimera's window: Monster Max's movie was
  written by a script and played by the window and by chimera-run.

## Where this stands (2026-10-31, fifth round)

Built and tested locally, against Chimera main cb46d2b7; not published,
no remote. The core itself did not change this round: the round added
tests and what was found. The owner's answers to the fourth round are
written down ("The owner's decisions on the fourth round"): TX-1696 stays
documented as not working, and the CTC's bit 7 and register 0xC5's bit
are one interrupt-enable bit, as built.

New this round ("Fifth round"): Monster Max, a commercial Next game,
from switching on to play, in the fast start and from the 24.11 card
through the Browser (the same picture at the end), native = sandbox,
states (every frame, and in another process), a movie through chimera-run
played back by another process, and in the Qt window; in the test script
as game:monster-max and in the window test as window:monster-max, each
SKIP without ZESARUX_MONSTER_MAX. The three Spectrum tapes load and play
in the fast start (the Next with the 48K ROM) by ZEsarUX's own tape
loading, native = sandbox, a state every frame changes nothing
(game:tapes, SKIP without ZESARUX_OWNER_TZX). Through NextZXOS a .tzx does
not load: its TZX loader needs the Next's Raspberry Pi, not emulated; the
same tapes made into .tap files load through NextZXOS's TAP loader in its
48K and 128K modes and play (by hand, not in the test script).

Tests of this round: run-gate.sh 26 passed, 0 failed, 0 skipped;
run-frontend.sh 4 of 4; test-packages all passed; check-core-texts.py 25
keys; check-names.py 0; check-proper-nouns.py 0 ("Tests of this round",
under "Fifth round").

Next, in order:
1. The owner's review, then publishing (the owner's).
2. Whether a tape in a NextZXOS project should also go into ZEsarUX's
   tape player, so that LOAD "" in NextZXOS's 48K or 128K BASIC reads it
   (the Next with a real tape); and whether the tests may keep .tap copies
   of the owner's tapes for game:tapes through NextZXOS. Both the owner's.
3. More commercial games from the owner; a movie recorded by hand in
   Chimera's window.
4. CI (the workflow is written, never run: no remote), and the Windows run
   of the frontend tests.

Traps met:
- A frame cannot be ended by watching the T-state counter: the firmware's
  resets set it to 0 in the middle of a frame.
- ZEsarUX's errors before its display is up are printed, not passed to the
  driver: catch them at debug_printf.
- `getcwd` is wrapped for ZEsarUX, and run-native is linked with the same
  wraps: the native runner uses realpath(".") for its own paths.
- The 48K ROM does not read the clock, so a clock change shows only on the
  NextZXOS tests.
- The host's C library turns `fopen` into `fopen64` (large files), so a
  link wrap of `fopen` alone misses ZEsarUX's calls in the native build;
  musl, in the sandbox, has only `fopen`.
- Nextoid's menu says "Press S", in a font where S looks like 5.
- Colour 0x1C of the tilemap's palette showed black in ZEsarUX until the
  test program set it; the test program now sets it to green.
- A file of ZEsarUX's bundled FatFs (`ff.c`, `diskio.c`) has Windows line
  endings: a script that rewrites it must keep them, or the patch becomes
  the whole file.
- The Browser's list is "mixed" (folders and files sorted together, case
  ignored); count lines from its picture, not from a directory listing.
- A NextZXOS that reboots every few frames, or a game that jumps into
  empty memory, was each time a register of the Next's newer cores that
  ZEsarUX does not have; a trace of the last few thousand instructions
  before the processor reached address 0 (or a run of NOPs) found each.
  In the fourth round the same trace found the Multiface's NMI (Next
  register 2 bit 3). A black screen with the processor at 0x0038 every
  frame was a loop at 0x0000 after such a jump, not a program waiting.
- The test program fills its interrupt table at start: a vector written
  into the table in the .nex file is overwritten; the program must write
  it after the fill.
- The Layer 2 picture covers the bottom half of the test program's
  screen: a test that paints colours there shows nothing.
- Jet Set Willy 2 takes a key only when it is held for a while (its
  colours 15 frames, ENTER at the title 60); a press of a few frames is
  lost without a sign.
- Jet Set Willy 2's code card location depends on the moment: a test
  that moves its presses must find the code again (from a state, trying
  all 256 in the sandbox takes about 5 minutes).
