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
  on it. For the fast start (no NextZXOS): the 48K Spectrum's ROM
  (`48.rom`). ZEsarUX's repository contains all three; see "What the user
  brings" below for their terms and the owner's question.
- **What can a test run on a public runner?** Everything here, if the three
  files may be read from the ZEsarUX submodule there (they are in it, as
  they are on this machine). The tests use no game: the program they load is
  made by the test script (waterbox/tests/make-nex.py).
- **How big is it?** The Next has 2 MiB of memory; the SD card image is
  held whole in the core's memory (64 MiB for the card ZEsarUX comes with).
  The layout is sbrk 64, sealed 4, invisible 8, plain 4, mmap 256 MiB. A
  state taken at NextZXOS's menu measured 4.2 MB.

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
- The patches are four small hooks, all under `#ifdef CHIMERA_CORE`:
  - 0001 `start.c`: `zesarux_main` returns once the machine is set up
    instead of entering its loop;
  - 0002 `timer.c`: the 1/50 s tick is "the machine has run its frame", and
    the end of a frame never sleeps;
  - 0003 `audio.c`, `operaciones.c`, `cores/core_spectrum.c`: each sound
    sample to the core as it is made, a note when the program reads the
    keyboard or the Kempston joystick, and a note at the end of each frame.

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

## Decisions (2026-10-31), the owner's questions among them

- **Nothing of the Next's firmware or card is packaged** (awaiting the
  owner). The boot ROM, the 48K ROM and the SD card are firmware and a slot
  file the user brings, although ZEsarUX's repository has all three. The
  sensible default while nobody has read their terms closely; see "What the
  user brings".
- **The tests read those three files from the ZEsarUX submodule** (awaiting
  the owner). They are upstream's own files, at the pinned commit; nothing
  of them is copied into this repository. ZESARUX_TEST_FIRMWARE points the
  tests elsewhere.
- **Two ways to start** (setting "Start"): `nextzxos` (the default, as a
  Next starts: boot ROM, firmware and NextZXOS from the card) and `fast`
  (ZEsarUX's "fast boot": the Next's hardware with the 48K ROM, no card,
  the program loaded straight into memory). Fast needs only the 48K ROM; a
  program that wants NextZXOS's services or files from the card does not
  run that way.
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

## What the user brings

- `tbblue_loader.rom` (8 KiB, SHA-1 8B2E927D0523BC27BFFCC34C39527AB0D2362E0C):
  the TBBlue boot loader, by Victor Trucco and Fabio Belavenuto, which the
  FPGA runs first and which reads TBBLUE.FW from the card. ZEsarUX ships it
  in its repository; no licence for it is in ZEsarUX's repository, and its
  source is part of the Next's FPGA project. Not packaged.
- An SD card image (`.mmc` or `.img`): ZEsarUX ships `tbblue.mmc`, 64 MiB,
  "System/Next 1.3.2, NextZXOS 2.06H, core 3.01.05, firmware 1.29b" (its
  README.MD). The card's LICENSE.MD (SpecNext Ltd's System/Next
  distribution licence) permits copying and distributing exact copies of
  the distribution in its entirety, free of charge, and its use on other
  hardware; parts carry their own licences, the Amstrad ROMs among them.
  Whether ZEsarUX's 64 MiB card is "the distribution in its entirety" is not
  plain. Not packaged.
- `48.rom` (16 KiB, SHA-1 5EA7C2B824672E914525D1D5C419D71B84A426A2), for
  the fast start: Amstrad's ROM, which Amstrad allows to be distributed with
  emulators. The owner allowed the Fuse core to build Amstrad's ROMs in
  (2026-10-12); the same could be done here. Not packaged.

## What was measured (2026-10-31)

On this machine (Linux, GCC 14), against Chimera at 3f8ea7e2
(~/chimera-scratch-bump), miniBox of that checkout.

`CHIMERA_ROOT=<chimera> waterbox/run-gate.sh`: 15 passed, 0 failed, 0
skipped, in 4 minutes.

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
  shifted sideways for lines 140 to 170), zxnDMA copy and AY tone each
  change the run when left out (make-nex.py --without); the whole is the
  same natively, in the sandbox and with a state every frame.
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
- settings:refused: an unknown start, no card, no boot ROM: each refused
  with a reason.
- frontend:picture, frontend:states, frontend:movie, frontend:firmware, in
  chimera-run with projects: a recorded movie of 1500 frames played back by
  another process shows this repository's runner's picture at frame 1499,
  also with a save and load before every frame; the movie's row has 42 + 6
  columns; NextZXOS with no boot ROM is refused and the ROM named.

`waterbox/tests/run-frontend.sh --chimera-root <chimera>` (Chimera's Qt
window, headless, QT_LIB_DIR set to Qt's lib folder): 3 of 3. NextZXOS 1000
frames in the window leaves the Next's 1.75 MiB of memory byte for byte the
native build's; the fast start with the test program in the project gives
another memory, the native build's with that setting; the package's 46 key
bindings are the window's.

Chimera's contract test (`test-packages` on a folder with only this
package): all passed. `tools/check-core-texts.py`: 20 keys, every one named.

Seen to fail:
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
  purpose, taken out): all four parts failed;
- the sandbox's clock one second ahead again, after game:warhawk was added:
  game:warhawk failed with boot:nextzxos, time:virtual and program:sd-card.

Speed, in the sandbox: about 200 frames a second at NextZXOS's menu
(28 MHz), 193 frames a second for the test program at 28 MHz; 4 times a
real Next.

## What is not done

- One game has been tried, Warhawk, from the card ZEsarUX comes with; no
  commercial game, and none of the card's other games.
- Not tried: the tilemap; the second and third AY chips, the DACs and the
  sound of a real game listened to; programs that load more files from the
  card; a card bigger than 64 MiB; .snx, .tap and the other program formats
  in the program slot.
- The 60 Hz mode: not looked into.
- The Kempston mouse (the Next's PS/2 mouse) has no input here.
- The Next's extended keyboard registers (Next registers 0xB0 and 0xB1)
  are not emulated by ZEsarUX.
- No CI run: the repository has no remote. .github/workflows/chimera.yml is
  written from the Fuse core's and has never run.
- No Windows run (not this round's): the frontend gate's scenario on
  Windows is still to be run before the core is called done.
- No movie recorded by hand in Chimera's window.

## Where this stands (2026-10-31)

Built and tested locally, not published. What works: NextZXOS boots from an
SD card image to its menu and runs a program from its Browser; Warhawk, a
Next game from that card, runs from boot to play; the fast start runs a
.nex directly; picture (Layer 2, sprites, copper), sound, keys, joystick,
resets, DMA, savestates (same process and another process), the project's
clock, the package in chimera-run and in Chimera's Qt window. Native and
sandbox agree on every test.

Next, in order:
1. The owner's answers (the questions above): may the boot ROM, the 48K ROM
   and the card be read from the submodule in tests and CI, and may any of
   them be packaged.
2. More games: the card's others (Nextoid, Lords of Midnight, Night-Knight
   ...) and commercial Next games from the owner; a movie recorded by hand
   in Chimera's window and played back.
3. The tilemap in the test program.
4. CI (the workflow is written), and the Windows run of the frontend gate.

Traps met:
- A frame cannot be ended by watching the T-state counter: the firmware's
  resets set it to 0 in the middle of a frame.
- ZEsarUX's errors before its display is up are printed, not passed to the
  driver: catch them at debug_printf.
- `getcwd` is wrapped for ZEsarUX, and run-native is linked with the same
  wraps: the native runner uses realpath(".") for its own paths.
- The 48K ROM does not read the clock, so a clock change shows only on the
  NextZXOS tests.
