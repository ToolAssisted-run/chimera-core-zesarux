# AGENTS.md - ZX Spectrum Next core (ZEsarUX) for Chimera

This repository builds ZEsarUX (https://github.com/chernandezba/zesarux), a
C emulator of many Sinclair and other machines, as a ZX Spectrum Next core
for Chimera (https://github.com/ToolAssisted-run/chimera), a frontend for
tool-assisted speedruns. It produces one file, `zesarux.chimeraCore`:
ZEsarUX compiled to run inside Chimera's sandbox (miniBox), a thin layer
that runs the machine one frame at a time, and the declarations Chimera
reads (buttons, settings, file slots, firmware, texts in 26 languages).
ZEsarUX is a git submodule at a fixed release; every change to it is a
numbered patch. The picture is the emulated machine's own.

Read [docs/BUILDING.md](docs/BUILDING.md) before building and
[docs/PLAN.md](docs/PLAN.md) before changing how the core works: it is the
design log, with what was decided, why, and what was measured.

## Layout

- `extern/zesarux` - ZEsarUX 13.0. The sources are in its `src/`.
- `patches/` - the numbered patches applied to `extern/zesarux`.
- `meson.build` - the build of both flavours, with the list of sources.
  `waterbox/config/compileoptions.h` - written by hand in place of what
  ZEsarUX's configure writes.
- `waterbox/cinterface.c` - the core: the functions Chimera's engine calls.
  The same file is compiled for the sandbox and for the host.
- `waterbox/scrchimera.c`, `audiochimera.c` - the core's display and sound
  card, compiled in place of ZEsarUX's null drivers.
- `waterbox/chimera_roms.c`, `gen-roms.py`, `ROMS.txt` - the 48K ROM built
  into the core (read from the submodule at build time), how ZEsarUX is
  given it, and its terms.
  `waterbox/chimera_compat.c` - the machine's clock and the other system
  calls the link wraps. `chimera_glue.h` is what they share.
- `waterbox/guest-syscalls.c`, `waterbox/guest-shim/` - the sandbox build's
  answers to calls the sandbox lacks.
- `waterbox/run-native.c`, `run-wbx.c`, `gate-harness.h` - the native
  runner and the sandbox runner, which take the same options and print the
  same lines.
- `waterbox/*.sh` - `setup-guest.sh`, `build-package.sh`, `run-gate.sh`
  (the test script), `apply-patches.sh`, `make-patches.sh`.
- `waterbox/tests/` - the window test (`run-frontend.sh` and its Lua and
  Python helpers), `make-nex.py` (writes the test program), `make-movie.py`.
- `waterbox/waterbox.config`, `file_slots.json`, `default_keybinds.json`,
  `package-licenses.json`, `languages/` - what the package declares.
- `build/` - every build output, ignored by git.

## Build and test

```sh
git submodule update --init
mb=<chimera>/extern/chimera-common-minibox
meson setup build/meson-native -Dminibox_dir=$mb
ninja -C build/meson-native
waterbox/build-package.sh -o build/pkg -m $mb
CHIMERA_ROOT=<chimera> waterbox/run-gate.sh
QT_LIB_DIR=<qt>/6.12.0/gcc_64/lib bash waterbox/tests/run-frontend.sh --chimera-root <chimera>
<chimera>/build/meson-linux/source/ui/test-packages build/pkg
```

The test script must end with `0 failed`. Run it before every commit that
changes the core, and say in the commit what it printed. On a shared
machine, run builds and tests under a memory cap
(`systemd-run --user --scope -p MemoryMax=8G ...`).

The tests read the Next's boot ROM, the 48K ROM and the SD card image from
the ZEsarUX submodule (`ZESARUX_TEST_FIRMWARE=<folder>` takes them from
elsewhere), on this machine and on CI (user-decided, 2026-10-31). None of
them is in this repository; the 48K ROM alone is in the package, built into
the core.

## Rules

- **Never edit `patches/` by hand.** Edit the tree under `extern/zesarux`,
  add the file to the list in `waterbox/make-patches.sh` if it is new, and
  run that script. `waterbox/apply-patches.sh` must then say "already
  applied".
- **Patch as little as possible.** ZEsarUX is told what to do by its own
  command line (`cinterface.c` writes one); its drivers are replaced by
  compiling the core's own under their names; system calls are wrapped at
  the link (`meson.build`). Every patch is a hook under `CHIMERA_CORE`.
- **The native run is the reference.** `run-native` and `run-wbx` must print
  the same lines for the same input.
- **The same input gives the same run, every time.** Nothing may read the
  host's clock, the user's files or the network.
- **No ROM, firmware, SD card or game is committed.** The 48K ROM is
  packaged (built in, user-decided 2026-10-31); packaging anything else is
  the owner's to decide.
- **A new test is broken on purpose once and seen to fail.** docs/PLAN.md
  says how each was.
- **Names are never translated or bent** in the 26 language tables
  (owner, 2026-10-31): ZEsarUX, Next, NextZXOS, Spectrum, Browser, Chimera
  and any other program, chip, machine, game or company name. A language
  that puts endings on names puts a common noun beside the name instead.
  `waterbox/tests/check-names.py` (run by the test script) holds it; a new
  name goes into its list.
- **Write in plain language**, ASCII in every `.md`.
- **Commits.** Subject: `type(zesarux): what changed`. Body: what changed,
  why, and how it was tested, with the numbers. Stage explicit paths. No
  amending, no force push.

## What is the owner's to decide

Publishing a repository, a place in Chimera's list of official cores,
anything a licence does not plainly allow (packaging the Next's boot ROM
or an SD card above all), and reporting anything to ZEsarUX's author.
