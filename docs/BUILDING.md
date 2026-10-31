# Building and testing

This builds `zesarux.chimeraCore` from source and runs its tests. It has
been done on Ubuntu 24.04 with GCC 14, on 2026-10-31, and from a fresh
clone with an empty HOME (the same core.wbx, byte for byte). A native build
with GCC 13, which is what a CI runner has, runs NextZXOS to its menu with
the same lines as the GCC 14 build.

## What is needed

- GCC, `meson`, `ninja`, `python3`, `git`.
- A Chimera checkout with its submodules, built far enough to have miniBox,
  the sandbox:

  ```sh
  git clone --recurse-submodules https://github.com/ToolAssisted-run/chimera
  mb=chimera/extern/chimera-common-minibox
  meson setup $mb/build/meson-linux $mb && meson compile -C $mb/build/meson-linux
  meson setup $mb/build/meson-cpp $mb -Dguest_cpp=true && meson compile -C $mb/build/meson-cpp
  ```

  The first is the sandbox's host library; the second is the toolchain a
  core is compiled with.
- For the tests that run the package in Chimera's headless runner, Chimera's
  engine (`build/dll/chimera-run` after `meson install`); for the window
  test, Chimera's Qt window (`build/meson-linux/source/ui/chimera`) and Qt's
  libraries (Chimera's `tools/ci-chimera.sh` installs Qt 6.12.0).

Nothing is downloaded while building, and ZEsarUX's `configure` and its
Makefile are not used ("How it is built", in docs/PLAN.md, says why).

## The sources

```sh
git clone --recurse-submodules <this repository>
```

`extern/zesarux` is ZEsarUX 13.0. The patches in `patches/` are applied to
it by `waterbox/apply-patches.sh`, which the build runs by itself; a tree
that already has them is left alone, and one that has only some of them
stops the build and says which files.

## The builds

```sh
mb=<chimera>/extern/chimera-common-minibox

# the native reference and the sandbox runner
meson setup build/meson-native -Dminibox_dir=$mb
ninja -C build/meson-native

# the sandboxed core and the package, into build/pkg
waterbox/build-package.sh -o build/pkg -m $mb
```

- `build/meson-native/run-native` is the core compiled for the host:
  `waterbox/cinterface.c` and ZEsarUX, with nothing of the sandbox.
- `build/meson-guest/core.wbx` is the same sources compiled for the sandbox.
  `build-package.sh` configures that build the first time
  (`waterbox/setup-guest.sh -m <miniBox>`), checks the result
  (`check-wbx.sh`: nothing in it may use thread-local storage), and writes
  the package.
- `build-package.sh -r <chimera>` installs the package into the Chimera
  checkout's `build/Cores` instead (what a publishing job does).

A package built here is stamped with the commit and `+local`; CI stamps it
with the commit alone (`CORE_VERSION`).

## The tests

```sh
CHIMERA_ROOT=<chimera> waterbox/run-gate.sh
QT_LIB_DIR=<qt>/6.12.0/gcc_64/lib bash waterbox/tests/run-frontend.sh --chimera-root <chimera>
<chimera>/build/meson-linux/source/ui/test-packages build/pkg
```

- `run-gate.sh` (the test script) compares the native build with the
  sandbox, test by test, and runs the package in chimera-run when
  `CHIMERA_ROOT` names a Chimera checkout; without it, the six frontend
  tests are skipped and say so. `ZESARUX_SN_CARD=<cspect-next-1gb.img>`
  names the System/Next 24.11 card for emulators (from
  specnext.com/latestdistro; not in this repository) for game:sn-card,
  which says SKIP without it, as game:sn-snapshot (Dark Star from that
  card) does. 23 tests.
- `run-frontend.sh` loads the package in Chimera's window, headless.
- `test-packages` is Chimera's own check of a package.

The Next's boot ROM (`tbblue_loader.rom`), the 48K ROM (`48.rom`) and the
SD card image (`tbblue.mmc`) are read from the ZEsarUX submodule's `src/`;
`ZESARUX_TEST_FIRMWARE=<folder>` takes them from another folder. The 48K
ROM is also built into the core: `waterbox/gen-roms.py` reads it from the
submodule while building and checks its SHA-1. The test program is made by
`waterbox/tests/make-nex.py`.

## Troubleshooting

- `the series does not apply` from apply-patches.sh: the submodule was moved
  without the patches being rebased.
- `extern/zesarux is partly patched`: a file of the tree was changed by hand
  or reverted. `git -C extern/zesarux reset --hard` and build again (turn any
  edit into a patch first, with `waterbox/make-patches.sh`).
- The window does not start, "libQt6Core.so.6: cannot open shared object
  file": set `QT_LIB_DIR` to Qt's `lib` folder.
- A sandboxed run dies with "the core asked for system call N": a call
  ZEsarUX makes that the sandbox lacks; answer it in
  `waterbox/guest-syscalls.c`, or wrap it in both builds in
  `waterbox/chimera_compat.c` when the answer is the machine's business.
