#!/bin/sh
# The core's tests. Each line says what was compared. The script ends with a
# count of tests that passed, failed and were skipped, and with an error if
# any failed. A skipped test is not a pass.
#
#   waterbox/run-gate.sh
#
# CHIMERA_ROOT=<a Chimera checkout> adds the tests that run the package in
# Chimera's own headless runner (chimera-run); the package is the one
# waterbox/build-package.sh -o build/pkg made (ZESARUX_PACKAGE names another).
#
# The Next's firmware and SD card: the tests take the files ZEsarUX itself
# comes with, from the pinned submodule (extern/zesarux/src):
# tbblue_loader.rom (the Next's boot ROM) and tbblue.mmc (an SD card with
# NextZXOS on it), and 48.rom (the Spectrum's ROM) to compare with the copy
# built into the core, which the fast start runs on (user-decided,
# 2026-10-31: docs/PLAN.md). ZESARUX_TEST_FIRMWARE=<folder> takes them from
# another folder instead. None of them is copied into this repository; the
# 48K ROM alone is in the package.
#
# The program the tests load is made by this script (tests/make-nex.py): no
# game is involved, and no test needs one.
#
# Needs the native build (meson setup build/meson-native ... && ninja) and the
# sandboxed core (waterbox/setup-guest.sh && ninja -C build/meson-guest).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
native="$root/build/meson-native/run-native"
runwbx="$root/build/meson-native/run-wbx"
core="$root/build/meson-guest/core.wbx"
fw="${ZESARUX_TEST_FIRMWARE:-$root/extern/zesarux/src}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ok=0; failed=0; skipped=0
report() { # <test> <PASS|FAIL|SKIP> <what>
	printf '%-22s %-6s %s\n' "$1" "$2" "$3"
	case "$2" in PASS) ok=$((ok + 1)) ;; FAIL) failed=$((failed + 1)) ;; *) skipped=$((skipped + 1)) ;; esac
}
# what a run ended with, without the word that says how it got there
last() { grep -E '^end ' "$1" 2> /dev/null | tail -1 | cut -d' ' -f2-; }
field() { # <file> <name>: the value of name= in the file's last digest line
	grep -E '^(at|end) ' "$1" 2> /dev/null | tail -1 | tr ' ' '\n' | sed -n "s/^$2=//p"
}
lines() { grep -E '^(framesPerSecond|at|end|saved) ' "$1" 2> /dev/null; }
run_n=0
nat() { # <name> <options...>: a native run, its lines in $work/<name>.out
	run_name="$1"; shift
	run_n=$((run_n + 1))
	"$native" "$work/dir-$run_n" "$@" > "$work/$run_name.out" 2> "$work/$run_name.err"
}
box() { # <name> <options...>: the same in the sandbox
	run_name="$1"; shift
	"$runwbx" "$core" "$@" > "$work/$run_name.out" 2> "$work/$run_name.err"
}
same() { # <name> <name>: both runs printed lines, and the same lines
	[ -n "$(lines "$work/$1.out")" ] && [ "$(lines "$work/$1.out")" = "$(lines "$work/$2.out")" ]
}
vh() { field "$work/$1.out" videoHash; }

if [ ! -x "$native" ] || [ ! -x "$runwbx" ] || [ ! -f "$core" ]; then
	echo "not built: this needs build/meson-native/run-native and run-wbx, and build/meson-guest/core.wbx (docs/BUILDING.md)" >&2
	exit 2
fi
if [ ! -f "$fw/tbblue_loader.rom" ] || [ ! -f "$fw/tbblue.mmc" ] || [ ! -f "$fw/48.rom" ]; then
	echo "the Next's firmware is not in $fw (tbblue_loader.rom, tbblue.mmc, 48.rom): git submodule update --init, or ZESARUX_TEST_FIRMWARE=<folder>" >&2
	exit 2
fi
nextzxos="--firmware $fw/tbblue_loader.rom --sd $fw/tbblue.mmc"
# Chimera's headless runner and this repository's package, when CHIMERA_ROOT
# names a Chimera checkout (the frontend tests below, and game:sn-card)
chimera_root="${CHIMERA_ROOT:-}"
chimera_engine="$chimera_root/build/dll/chimera-run"
package="${ZESARUX_PACKAGE:-$root/build/pkg/zesarux.chimeraCore}"
# the fast start needs nothing handed over: its ROM is built in
fast="--set boot=fast"
prog="$work/test.nex"
python3 "$here/tests/make-nex.py" "$prog" || exit 2

# Buttons, as the runners' --press takes them (hex, bit 0 the first button
# the package declares).
hex() { printf '%x' "$1"; }
key_q=$(hex $((1 << 10))); key_enter=$(hex $((1 << 29))); key_space=$(hex $((1 << 39)))
key_down=$(hex $(((1 << 30) | (1 << 5)))) # CAPS SHIFT + 6, the Next's cursor down
btn_reset=$(hex $((1 << 40))); btn_hard=$(hex $((1 << 41)))
joy_up=$(hex $((1 << 42))); joy_fire=$(hex $((1 << 46))); joy_fire2=$(hex $((1 << 47)))
# From switching on to NextZXOS's menu: the first start of a card shows the
# video mode test (ENTER chooses the mode), then the welcome pages (SPACE
# leaves them); the menu is up by frame 1100.
to_menu="--press 200:5:$key_enter --press 700:5:$key_space"

# Names are never translated or bent (owner, 2026-10-31): ZEsarUX, Next,
# NextZXOS, Spectrum, Browser and Chimera are written as they are in every
# one of the 26 language tables, with no ending glued to them
# (tests/check-names.py says each one that is not).
if python3 "$here/tests/check-names.py" > "$work/names.out" 2> "$work/names.err"; then
	report "texts:names" PASS "every name in the 26 language tables as it is written, with nothing glued to it"
else
	report "texts:names" FAIL "$(grep -c . "$work/names.out") names bent or missing, the first: $(head -1 "$work/names.out" | cut -c1-120) $(tail -1 "$work/names.err")"
fi

# The Next started fast (its hardware with the 48K ROM, no card), switched on
# with nothing in it and buttons pressed by a fixed pattern: the sandbox
# prints the native build's line every 60 frames - where the processor is,
# the machine's 2 MiB of memory, its sound, its picture and every picture on
# the way. And it is a Next at 50 frames a second, 312 sound samples a frame,
# picture 704x608.
nat f-n $fast --frames 600 --every 60 --exercise
box f-s $fast --frames 600 --every 60 --exercise
why=""
same f-n f-s || why="$why native and sandbox differ [$(last "$work/f-s.out" | cut -c1-80)] $(tail -1 "$work/f-s.err" | cut -c1-80);"
[ "$(grep '^framesPerSecond=' "$work/f-s.out")" = "framesPerSecond=50/1" ] || why="$why it says [$(grep '^framesPerSecond=' "$work/f-s.out")];"
[ "$(field "$work/f-s.out" video)" = 704x608 ] || why="$why picture $(field "$work/f-s.out" video);"
[ "$(field "$work/f-s.out" audioPairs)" = $((600 * 312)) ] || why="$why $(field "$work/f-s.out" audioPairs) sound samples in 600 frames;"
[ "$(field "$work/f-s.out" inputFrames)" -gt 0 ] 2> /dev/null || why="$why the ROM never looked at its keyboard;"
if [ -z "$why" ]; then
	report "boot:fast" PASS "600 frames with buttons pressed: the sandbox gives the native build's lines; 50 frames a second, 312 samples a frame, 704x608"
else
	report "boot:fast" FAIL "$why"
fi

# The 48K ROM is built into the core. The fast start with no ROM handed
# over, with upstream's 48.rom handed over, and with a wrong file of that
# name handed over (16 KiB of noise) runs the same 300 frames with buttons,
# natively and in the sandbox: the built-in ROM is upstream's, and it wins
# over any file of its name. (Seen to fail, docs/PLAN.md: with the core
# not answering for the name, the run with no ROM did not start and the
# wrong ROM's run differed.)
wrong="$work/wrong"
mkdir -p "$wrong"
python3 -c 'import random,sys; r=random.Random(48); sys.stdout.buffer.write(bytes(r.randrange(256) for _ in range(16384)))' > "$wrong/48.rom"
box rom-none $fast --frames 300 --every 50 --exercise
box rom-up $fast --firmware "$fw/48.rom" --frames 300 --every 50 --exercise
box rom-wrong $fast --firmware "$wrong/48.rom" --frames 300 --every 50 --exercise
nat rom-wrong-n $fast --firmware "$wrong/48.rom" --frames 300 --every 50 --exercise
why=""
same rom-none rom-up || why="$why with upstream's 48.rom handed over the run differs [$(last "$work/rom-up.out" | cut -c1-60)] $(tail -1 "$work/rom-none.err" | cut -c1-80);"
same rom-none rom-wrong || why="$why a wrong 48.rom handed over changed the run (the built-in ROM did not win);"
same rom-wrong rom-wrong-n || why="$why native and sandbox differ;"
if [ -z "$why" ]; then
	report "rom:built-in" PASS "the fast start with no ROM handed over, with upstream's 48.rom and with a wrong file of that name gives the same 300 frames, natively and in the sandbox"
else
	report "rom:built-in" FAIL "$why"
fi

# NextZXOS, started from the card as a Next starts: the boot ROM, the
# firmware and the system read from the SD card, through the video mode test
# and the welcome pages to the menu; and the same 1500 frames with buttons
# pressed by the fixed pattern. Native and sandbox print the same lines. The
# menu is not the screen of a machine nobody pressed anything on.
nat n-n $nextzxos --frames 1500 --every 100 $to_menu
box n-s $nextzxos --frames 1500 --every 100 $to_menu
nat x-n $nextzxos --frames 1500 --every 100 --exercise
box x-s $nextzxos --frames 1500 --every 100 --exercise
box i-s $nextzxos --frames 1500
why=""
same n-n n-s || why="$why to the menu: native and sandbox differ [$(last "$work/n-s.out" | cut -c1-60)] $(tail -1 "$work/n-s.err" | cut -c1-80);"
same x-n x-s || why="$why buttons by a pattern: native and sandbox differ;"
[ -n "$(vh i-s)" ] && [ "$(vh n-s)" != "$(vh i-s)" ] || why="$why the menu is the screen of a machine nobody touched;"
if [ -z "$why" ]; then
	report "boot:nextzxos" PASS "NextZXOS from the SD card to its menu, and 1500 frames with buttons by a pattern: the sandbox gives the native build's lines at every 100th frame"
else
	report "boot:nextzxos" FAIL "$why"
fi

# Time is the machine's. The menu shows the date and time from the Next's
# clock, which is the project's start plus the frames run: the host waiting
# 0.3 s at every 100th frame changes nothing; the same with the native build
# reading the host's clock (CHIMERA_TEST_HOST_CLOCK, the test's control: what
# ZEsarUX does on its own) does change the machine. And another start of the
# clock gives another machine, the same natively and in the sandbox.
HARNESS_STALL_MS=300 box n-stall $nextzxos --frames 1500 --every 100 $to_menu
CHIMERA_TEST_HOST_CLOCK=1 nat n-host $nextzxos --frames 1500 --every 100 $to_menu
nat c-n $nextzxos --frames 1500 --every 100 $to_menu --clock-start 1893553445
box c-s $nextzxos --frames 1500 --every 100 $to_menu --clock-start 1893553445
why=""
same n-s n-stall || why="$why the host's wait changed the machine;"
[ -n "$(field "$work/n-host.out" memHash)" ] && [ "$(field "$work/n-host.out" memHash)" != "$(field "$work/n-n.out" memHash)" ] || why="$why on the host's clock the machine is the same, so this test cannot tell;"
same c-n c-s || why="$why with the clock at 2030: native and sandbox differ;"
[ "$(field "$work/c-s.out" memHash)" != "$(field "$work/n-s.out" memHash)" ] || why="$why another start of the clock gives the same machine;"
if [ -z "$why" ]; then
	report "time:virtual" PASS "the host waiting 0.3 s every 100 frames changes nothing; the host's clock would (the control); a clock started at 2030 gives another machine, the same natively and in the sandbox"
else
	report "time:virtual" FAIL "$why"
fi

# A state is the whole machine: saved and loaded before every frame, the run
# is the run that was never stopped - NextZXOS to its menu (the SD card in
# the machine's memory, 64 MiB), and the test program with keys pressed. The
# test's control: a state one frame old loaded once half way
# (HARNESS_BREAK_STALE_STATE) must show at the end.
box r-n $nextzxos --frames 1500 --every 100 $to_menu --rerecord
HARNESS_BREAK_STALE_STATE=1 box r-stale $nextzxos --frames 1500 --every 100 $to_menu --rerecord
presses="--press 30:20:$key_q --press 60:20:$joy_fire --press 90:10:$joy_up"
box p-s $fast --program "$prog" --frames 150 --every 10 $presses
box p-r $fast --program "$prog" --frames 150 --every 10 $presses --rerecord
why=""
same n-s r-n || why="$why NextZXOS: [$(last "$work/r-n.out" | cut -c1-60)] $(tail -1 "$work/r-n.err" | cut -c1-80);"
same p-s p-r || why="$why the test program: [$(last "$work/p-r.out" | cut -c1-60)];"
[ -n "$(lines "$work/r-stale.out")" ] && ! same n-s r-stale || why="$why a stale state changed nothing (or the control did not run), so this test cannot tell;"
if [ -z "$why" ]; then
	report "state:every-frame" PASS "a state saved and loaded before each frame changes nothing (NextZXOS to its menu, 1500 frames; the test program with keys, 150); one stale state does show"
else
	report "state:every-frame" FAIL "$why"
fi

# Saved by one process half way and loaded by another, a run ends where the
# unbroken run ends: NextZXOS, with the state taken after ENTER and before
# SPACE.
box so-half $nextzxos --frames 1500 --every 100 $to_menu --save-state "$work/next.state" --at 400
box so-rest $nextzxos --frames 1500 --every 100 $to_menu --state "$work/next.state"
if [ -s "$work/next.state" ] && [ -n "$(last "$work/n-s.out")" ] && [ "$(last "$work/so-rest.out")" = "$(last "$work/n-s.out")" ]; then
	report "state:other-process" PASS "saved at frame 400 and loaded by another process, the run ends at the menu where the unbroken run ends ($(wc -c < "$work/next.state") bytes)"
else
	report "state:other-process" FAIL "unbroken [$(last "$work/n-s.out" | cut -c1-80)] carried on [$(last "$work/so-rest.out" | cut -c1-80)] $(tail -1 "$work/so-rest.err" | cut -c1-80)"
fi

# The buttons reach the machine. The test program paints what it reads from
# the keyboard and the Kempston joystick into the screen's colours, and reads
# them only on every second frame. So: a key, the joystick's Fire, its Fire 2
# and Up each make another picture than nothing pressed, and other pictures
# than each other; the same natively; and the controls were looked at in half
# of the frames (the other half are lag frames).
for b in none:0 q:$key_q fire:$joy_fire fire2:$joy_fire2 up:$joy_up; do
	b_name="${b%%:*}"; b_mask="${b#*:}"
	box "in-$b_name" $fast --program "$prog" --frames 100 --press "50:60:$b_mask"
done
nat in-q-n $fast --program "$prog" --frames 100 --press "50:60:$key_q"
nat in-fire-n $fast --program "$prog" --frames 100 --press "50:60:$joy_fire"
asked="$(field "$work/in-none.out" inputFrames)"
why=""
[ -n "$(vh in-none)" ] || why="$why the run printed nothing [$(tail -1 "$work/in-none.err" | cut -c1-80)];"
seen=""
for b_name in none q fire fire2 up; do
	for other in $seen; do [ "$(vh "in-$b_name")" != "$(vh "in-$other")" ] || why="$why $b_name and $other make the same picture;"; done
	seen="$seen $b_name"
done
same in-q in-q-n && same in-fire in-fire-n || why="$why native and sandbox differ;"
[ "${asked:-0}" -ge 45 ] && [ "${asked:-0}" -le 55 ] || why="$why the controls were looked at in ${asked:-no} of 100 frames, not about half;"
if [ -z "$why" ]; then
	report "input:buttons" PASS "a key, the joystick's Fire, Fire 2 and Up each reach the program and differ from each other and from nothing pressed, the same natively; the controls were looked at in $asked of 100 frames"
else
	report "input:buttons" FAIL "$why"
fi

# Reset and Hard Reset. The test program, hard reset at frame 100: the
# machine starts again from nothing and shows the 48K ROM's first screen, the
# screen a fast start with no program shows at the same frame. Reset (the
# Next's soft reset) at frame 100 starts the ROM again too (the processor is
# in the ROM at the end) but keeps what the Next was set to: here the
# program's Layer 2 picture stays on, as ZEsarUX does it. Neither is the
# picture of the program left alone.
box rs-hard $fast --program "$prog" --frames 400 --press "100:3:$btn_hard"
box rs-soft $fast --program "$prog" --frames 400 --press "100:3:$btn_reset"
box rs-prog $fast --program "$prog" --frames 400
box rs-boot $fast --frames 400
soft_pc="$(field "$work/rs-soft.out" pc)"
why=""
[ -n "$(vh rs-boot)" ] && [ "$(vh rs-hard)" = "$(vh rs-boot)" ] || why="$why Hard Reset did not bring back the ROM's first screen;"
[ -n "$soft_pc" ] && [ $((0x$soft_pc)) -lt 16384 ] || why="$why after Reset the processor is at ${soft_pc:-nothing}, not in the ROM;"
[ "$(vh rs-soft)" != "$(vh rs-prog)" ] && [ "$(vh rs-hard)" != "$(vh rs-prog)" ] || why="$why a reset left the program's picture as it was;"
if [ -z "$why" ]; then
	report "input:reset" PASS "Hard Reset brings back the 48K ROM's first screen; Reset starts the ROM again (the processor at $soft_pc)"
else
	report "input:reset" FAIL "$why"
fi

# The Next's own hardware. The test program sets up a hardware sprite, a
# copper list that shifts Layer 2 sideways part way down the screen, a DMA
# copy into the screen's colours, two rows of tiles on the tilemap, a tone
# on each of the three AY chips (each on another channel) and two saw waves
# on the 8-bit DACs. Each, left out of the program (make-nex.py --without),
# changes the run: the picture for the sprite, the copper, the DMA and the
# tilemap; the sound, and not the picture, for the tones and the DACs. The
# whole program runs the same natively, in the sandbox and with a state
# saved and loaded before every frame.
hw_bad=""
box hw-all $fast --program "$prog" --frames 100 --every 50
nat hw-all-n $fast --program "$prog" --frames 100 --every 50
box hw-all-r $fast --program "$prog" --frames 100 --every 50 --rerecord
for f in sprite copper dma tilemap sound ay2 ay3 dacs; do
	python3 "$here/tests/make-nex.py" "$work/no-$f.nex" --without "$f" || exit 2
	box "hw-no-$f" $fast --program "$work/no-$f.nex" --frames 100 --every 50
	case "$f" in sound|ay2|ay3|dacs)
		[ -n "$(field "$work/hw-no-$f.out" audioHash)" ] && [ "$(field "$work/hw-no-$f.out" audioHash)" != "$(field "$work/hw-all.out" audioHash)" ] || hw_bad="$hw_bad without the $f the sound is the same;"
		[ "$(vh "hw-no-$f")" = "$(vh hw-all)" ] || hw_bad="$hw_bad without the $f the picture changed;" ;;
	*)
		[ -n "$(vh "hw-no-$f")" ] && [ "$(vh "hw-no-$f")" != "$(vh hw-all)" ] || hw_bad="$hw_bad without the $f the picture is the same;" ;;
	esac
done
same hw-all hw-all-n || hw_bad="$hw_bad native and sandbox differ;"
same hw-all hw-all-r || hw_bad="$hw_bad a state every frame changed the run;"
if [ -z "$hw_bad" ]; then
	report "next:hardware" PASS "a hardware sprite, the copper, the DMA, the tilemap, a tone on each of the three AY chips and the DACs each change the run when the program leaves them out; the whole is the same natively, in the sandbox and with a state every frame"
else
	report "next:hardware" FAIL "$hw_bad"
fi

# The Next's CTC and its interrupt controller's hardware IM2 mode (patch
# 0007 and waterbox/next_interrupts.c; ZEsarUX 13.0 had neither). The test
# program with its CTC part (make-nex.py --with ctc): channel 0 a timer of
# 28 MHz / 256 / 256 whose interrupt goes, by the vector the controller
# makes for it (0x06), to a routine of its own that counts. Over 200
# frames the count is 200 frames' worth of 28 MHz clock ticks over 65536:
# 8 ticks for each T-state at 3.5 MHz, 69888 or 70908 T-states a frame,
# 1706 to 1731 (the CTC's clock, as the Next's documentation gives it; a
# wrong vector leaves it at 0). The sandbox prints the native build's
# lines, a state every frame changes nothing, and the picture is not the
# program's without the CTC.
ctcprog="$work/ctc.nex"
python3 "$here/tests/make-nex.py" "$ctcprog" --with ctc || exit 2
nat ctc-n $fast --program "$ctcprog" --frames 200 --every 50
box ctc-s $fast --program "$ctcprog" --frames 200 --every 50 --dump-ram "$work/ctc.ram"
box ctc-r $fast --program "$ctcprog" --frames 200 --every 50 --rerecord
box ctc-none $fast --program "$prog" --frames 200 --every 50
count=$(python3 -c 'import sys; d=open(sys.argv[1],"rb").read(); o=2*16384+0x1202; print(d[o]|d[o+1]<<8)' "$work/ctc.ram" 2> /dev/null)
why=""
same ctc-n ctc-s || why="$why native and sandbox differ [$(last "$work/ctc-s.out" | cut -c1-60)] $(tail -1 "$work/ctc-s.err" | cut -c1-80);"
same ctc-s ctc-r || why="$why a state every frame changed the run;"
[ "$(vh ctc-s)" != "$(vh ctc-none)" ] || why="$why the picture is the program's without the CTC;"
[ "${count:-0}" -ge 1700 ] && [ "${count:-0}" -le 1740 ] || why="$why the CTC's routine counted ${count:-nothing} interrupts in 200 frames, not 1706 to 1731;"
if [ -z "$why" ]; then
	report "next:ctc" PASS "the CTC's channel 0 interrupted $count times in 200 frames through the vector hardware IM2 mode makes for it (28 MHz / 256 / 256); the sandbox prints the native build's lines, a state every frame changes nothing"
else
	report "next:ctc" FAIL "$why"
fi

# A program goes on the SD card. With NextZXOS the test program is copied
# into the card's top folder; the Browser lists it last, sixteen lines down,
# and ENTER runs it with NextZXOS's own loader. The processor ends in the
# program's own code (its interrupt routine at 9191), natively and in the
# sandbox; and the project's card file is the same file after the run.
browse="--press 1100:5:$key_enter"
i=0
while [ $i -lt 16 ]; do browse="$browse --press $((1200 + i * 10)):4:$key_down"; i=$((i + 1)); done
browse="$browse --press 1400:5:$key_enter"
sd_before="$(sha1sum < "$fw/tbblue.mmc")"
nat b-n $nextzxos --program "$prog" --frames 1800 --every 300 $to_menu $browse
box b-s $nextzxos --program "$prog" --frames 1800 --every 300 $to_menu $browse
copied="$(ls -d "$work"/dir-* 2> /dev/null | while read -r d; do [ -f "$d/test.nex" ] && [ -f "$d/tbblue.mmc" ] && echo "$d"; done | tail -1)"
why=""
same b-n b-s || why="$why native and sandbox differ [$(last "$work/b-s.out" | cut -c1-60)] $(tail -1 "$work/b-s.err" | cut -c1-80);"
[ "$(field "$work/b-s.out" pc)" = 9191 ] || why="$why the processor is at $(field "$work/b-s.out" pc), not in the program;"
[ "$(sha1sum < "$fw/tbblue.mmc")" = "$sd_before" ] || why="$why the card file changed;"
[ -n "$copied" ] && [ "$(sha1sum < "$copied/tbblue.mmc")" = "$sd_before" ] || why="$why the native run's copy of the card file changed;"
if [ -z "$why" ]; then
	report "program:sd-card" PASS "the program put on the card is found by the Browser and run by NextZXOS (the processor in its code at frame 1800), the same in the sandbox; the card file is unchanged"
else
	report "program:sd-card" FAIL "$why"
fi

# A game. The SD card ZEsarUX comes with is the Next's own system
# distribution, which has free Next games on it; Warhawk (Rusty Pixels,
# 2019) is one. From switching on: NextZXOS's menu, the Browser, the folders
# GAMES, Next and Warhawk, ENTER on Warhawk.nex; its title at about frame
# 4000; SPACE starts it, and the ship flies right and fires (P and SPACE,
# again and again) to frame 5000. The sandbox prints the native build's
# lines, a state saved and loaded before every frame changes nothing, and
# the game was played (the run is not the run with nothing pressed after
# the game was started).
warhawk=""
add() { warhawk="$warhawk --press $2:4:$1"; }
key_p=$(hex $((1 << 19)))
add "$key_enter" 1100
i=0; while [ $i -lt 8 ]; do add "$key_down" $((1200 + i * 10)); i=$((i + 1)); done
add "$key_enter" 1300
i=0; while [ $i -lt 4 ]; do add "$key_down" $((1350 + i * 10)); i=$((i + 1)); done
add "$key_enter" 1400
i=0; while [ $i -lt 15 ]; do add "$key_down" $((1450 + i * 10)); i=$((i + 1)); done
add "$key_enter" 1620
i=0; while [ $i -lt 2 ]; do add "$key_down" $((1670 + i * 10)); i=$((i + 1)); done
add "$key_enter" 1720
add "$key_space" 4100
started="$warhawk"
i=0; while [ $i -lt 21 ]; do add "$key_space" $((4300 + i * 30)); add "$key_p" $((4310 + i * 30)); i=$((i + 1)); done
# shellcheck disable=SC2086
nat g-n $nextzxos --frames 5000 --every 500 $to_menu $warhawk
# shellcheck disable=SC2086
box g-s $nextzxos --frames 5000 --every 500 $to_menu $warhawk
# shellcheck disable=SC2086
box g-r $nextzxos --frames 5000 --every 500 $to_menu $warhawk --rerecord
# shellcheck disable=SC2086
box g-idle $nextzxos --frames 5000 --every 500 $to_menu $started
why=""
same g-n g-s || why="$why native and sandbox differ [$(last "$work/g-s.out" | cut -c1-60)] $(tail -1 "$work/g-s.err" | cut -c1-80);"
same g-s g-r || why="$why a state every frame changed the run [$(last "$work/g-r.out" | cut -c1-60)];"
[ -n "$(vh g-idle)" ] && [ "$(vh g-s)" != "$(vh g-idle)" ] || why="$why the game played and the game left alone end alike;"
if [ -z "$why" ]; then
	report "game:warhawk" PASS "Warhawk from the SD card through NextZXOS's Browser, started and played to frame 5000: the sandbox prints the native build's lines, a state every frame changes nothing, and it is not the run left alone"
else
	report "game:warhawk" FAIL "$why"
fi

# Another game from that card: Nextoid (a bat-and-ball game for the Next),
# in GAMES/Next/Nextoid/NEXTOID.NEX. From switching on, through the Browser
# as for Warhawk to its menu by about frame 2500; 1 (keyboard) and S start
# level 1; SPACE launches the ball, P is held, and the ball breaks a brick
# (the score is 20 at frame 3400). The sandbox prints the native build's
# lines every 500 frames, a state saved and loaded before every frame
# changes nothing, and the run is not the run with nothing pressed after
# the game was started (there the ball is never launched).
nextoid=""
addn() { nextoid="$nextoid --press $2:${3:-4}:$1"; }
key_s=$(hex $((1 << 21))); key_1=$(hex 1)
addn "$key_enter" 1100
i=0; while [ $i -lt 8 ]; do addn "$key_down" $((1200 + i * 10)); i=$((i + 1)); done
addn "$key_enter" 1300
i=0; while [ $i -lt 4 ]; do addn "$key_down" $((1350 + i * 10)); i=$((i + 1)); done
addn "$key_enter" 1400
i=0; while [ $i -lt 8 ]; do addn "$key_down" $((1450 + i * 10)); i=$((i + 1)); done
addn "$key_enter" 1620
i=0; while [ $i -lt 2 ]; do addn "$key_down" $((1670 + i * 10)); i=$((i + 1)); done
addn "$key_enter" 1720
addn "$key_1" 2600 10
addn "$key_s" 2700 10
started="$nextoid"
addn "$key_space" 3100 10
addn "$key_p" 3150 60
addn "$key_space" 3250 5
# shellcheck disable=SC2086
nat nx-n $nextzxos --frames 3400 --every 500 $to_menu $nextoid
# shellcheck disable=SC2086
box nx-s $nextzxos --frames 3400 --every 500 $to_menu $nextoid
# shellcheck disable=SC2086
box nx-r $nextzxos --frames 3400 --every 500 $to_menu $nextoid --rerecord
# shellcheck disable=SC2086
box nx-idle $nextzxos --frames 3400 --every 500 $to_menu $started
why=""
same nx-n nx-s || why="$why native and sandbox differ [$(last "$work/nx-s.out" | cut -c1-60)] $(tail -1 "$work/nx-s.err" | cut -c1-80);"
same nx-s nx-r || why="$why a state every frame changed the run [$(last "$work/nx-r.out" | cut -c1-60)];"
[ -n "$(vh nx-idle)" ] && [ "$(vh nx-s)" != "$(vh nx-idle)" ] || why="$why the game played and the game left alone end alike;"
if [ -z "$why" ]; then
	report "game:nextoid" PASS "Nextoid from the SD card through NextZXOS's Browser, started and played to frame 3400: the sandbox prints the native build's lines, a state every frame changes nothing, and it is not the run left alone"
else
	report "game:nextoid" FAIL "$why"
fi

# The System/Next 24.11 card for emulators (specnext.com/latestdistro, a
# 1 GiB image, NextZXOS 2.09), when ZESARUX_SN_CARD names it: it is not in
# this repository and is not copied into it; the test reads it where it is,
# or a copy in its own scratch folder. The Next's firmware cannot read this
# card (docs/PLAN.md, "The System/Next 24.11 card"), so the Next starts
# with "card": the core does the firmware's work from the card. From
# switching on: SPACE leaves the welcome pages, ENTER opens the Browser,
# GAMES, Next, Revival Survival, RevivalSurvival.nex; its title by frame
# 2500; SPACE, 1 (Arcade) and SPACE through the story start a round; the
# joystick (and its keys) moves the player right and up and fires, and the
# score is 2150 at frame 5200. The sandbox prints the native build's lines
# every 500 frames, a state saved and loaded before every frame changes
# nothing, and the run is not the run left alone after the round started.
# With the card in a project, the core suggests "card".
sn_card="${ZESARUX_SN_CARD:-}"
if [ -z "$sn_card" ] || [ ! -f "$sn_card" ]; then
	report "game:sn-card" SKIP "ZESARUX_SN_CARD is not set to the System/Next 24.11 card for emulators (cspect-next-1gb.img), which is not in this repository"
else
	sn="--set boot=card --sd $sn_card"
	key_1=$(hex 1)
	rs=""
	addr() { rs="$rs --press $2:${3:-4}:$1"; }
	addr "$key_space" 150 5
	addr "$key_enter" 400 5
	i=0; while [ $i -lt 6 ]; do addr "$key_down" $((500 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 600 5
	i=0; while [ $i -lt 4 ]; do addr "$key_down" $((650 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 720 5
	i=0; while [ $i -lt 12 ]; do addr "$key_down" $((800 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 940 5
	i=0; while [ $i -lt 2 ]; do addr "$key_down" $((1020 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 1060 5
	addr "$key_space" 3000 10
	addr "$key_1" 3300 10
	addr "$key_space" 3700 10
	addr "$key_space" 3900 10
	addr "$key_space" 4100 10
	started="$rs"
	right=$(hex $(((1 << 45) | (1 << 19)))); up=$(hex $(((1 << 42) | (1 << 10)))); fire=$(hex $(((1 << 46) | (1 << 39))))
	addr "$right" 4500 120
	addr "$up" 4650 100
	addr "$fire" 4800 10
	addr "$right" 4850 150
	# shellcheck disable=SC2086
	nat sn-n $sn --frames 5200 --every 500 $rs
	# shellcheck disable=SC2086
	box sn-s $sn --frames 5200 --every 500 $rs
	# shellcheck disable=SC2086
	box sn-r $sn --frames 5200 --every 500 $rs --rerecord
	# shellcheck disable=SC2086
	box sn-idle $sn --frames 5200 --every 500 $started
	why=""
	same sn-n sn-s || why="$why native and sandbox differ [$(last "$work/sn-s.out" | cut -c1-60)] $(tail -1 "$work/sn-s.err" | cut -c1-80);"
	same sn-s sn-r || why="$why a state every frame changed the run [$(last "$work/sn-r.out" | cut -c1-60)];"
	[ -n "$(vh sn-idle)" ] && [ "$(vh sn-s)" != "$(vh sn-idle)" ] || why="$why the game played and the game left alone end alike;"
	if [ -n "$chimera_root" ] && [ -x "$chimera_engine" ] && [ -f "$package" ]; then
		mkdir -p "$work/sn"
		cp "$sn_card" "$work/sn/"
		sn_copy="$work/sn/$(basename "$sn_card")"
		python3 "$here/tests/make-movie.py" 10 > "$work/sn/none.movie"
		env -u DISPLAY "$chimera_engine" --new-project "$work/sn/sn.chimeraProject" "$package" --file "sd=$sn_copy" --movie "$work/sn/none.movie" > "$work/sn/new.log" 2>&1
		env -u DISPLAY "$chimera_engine" --project "$work/sn/sn.chimeraProject" "$package" --suggest > "$work/sn-suggest.out" 2>&1
		grep -q '^{"values":{"boot":"card"}' "$work/sn-suggest.out" || why="$why the core does not suggest \"card\" for this card [$(tail -1 "$work/sn-suggest.out" | cut -c1-60)];"
		rm -f "$sn_copy"
	fi
	if [ -z "$why" ]; then
		report "game:sn-card" PASS "System/Next 24.11's card started with \"card\": Revival Survival from its Browser, started and played to frame 5200; the sandbox prints the native build's lines, a state every frame changes nothing, it is not the run left alone$( [ -n "$chimera_root" ] && echo "; the core suggests \"card\" for it")"
	else
		report "game:sn-card" FAIL "$why"
	fi
fi

# Dark Star from the same card: a .z80 snapshot of a 128K Spectrum
# (GAMES/Classic48/DARKSTAR.Z80, "modified for the Next" by Design Design),
# which NextZXOS's Browser hands to its snapshot loader (snapload.bas). The
# loader ends by writing Next register 2 bit 3, the Multiface's NMI, and the
# Multiface's ROM puts the snapshot's registers back (patch 0005; before it
# the screen stayed black). From switching on: SPACE leaves the welcome
# pages, ENTER opens the Browser, GAMES, Classic48, DARKSTAR.Z80; its Hall
# of Fame by frame 2500; SPACE starts it, P and O turn the ship. The sandbox
# prints the native build's lines every 500 frames, a state saved and
# loaded before every frame changes nothing, and the game played is not the
# game left alone.
if [ -z "$sn_card" ] || [ ! -f "$sn_card" ]; then
	report "game:sn-snapshot" SKIP "ZESARUX_SN_CARD is not set to the System/Next 24.11 card for emulators (cspect-next-1gb.img), which is not in this repository"
else
	sn="--set boot=card --sd $sn_card"
	rs=""
	addr() { rs="$rs --press $2:${3:-4}:$1"; }
	addr "$key_space" 150 5
	addr "$key_enter" 400 5
	i=0; while [ $i -lt 6 ]; do addr "$key_down" $((500 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 600 5
	i=0; while [ $i -lt 3 ]; do addr "$key_down" $((650 + i * 10)); i=$((i + 1)); done
	addr "$key_enter" 700 5
	addr "$key_down" 780; addr "$key_down" 790
	addr "$key_enter" 820 5
	addr "$key_space" 2600 10
	started="$rs"
	key_o=$(hex $((1 << 18))); key_p=$(hex $((1 << 19)))
	addr "$key_p" 2900 50
	addr "$key_o" 3000 50
	addr "$key_space" 3100 5
	# shellcheck disable=SC2086
	nat ds-n $sn --frames 3600 --every 500 $rs
	# shellcheck disable=SC2086
	box ds-s $sn --frames 3600 --every 500 $rs
	# shellcheck disable=SC2086
	box ds-r $sn --frames 3600 --every 500 $rs --rerecord
	# shellcheck disable=SC2086
	box ds-idle $sn --frames 3600 --every 500 $started
	why=""
	same ds-n ds-s || why="$why native and sandbox differ [$(last "$work/ds-s.out" | cut -c1-60)] $(tail -1 "$work/ds-s.err" | cut -c1-80);"
	same ds-s ds-r || why="$why a state every frame changed the run [$(last "$work/ds-r.out" | cut -c1-60)];"
	[ -n "$(vh ds-idle)" ] && [ "$(vh ds-s)" != "$(vh ds-idle)" ] || why="$why the game played and the game left alone end alike;"
	if [ -z "$why" ]; then
		report "game:sn-snapshot" PASS "Dark Star, a 128K .z80 snapshot on System/Next 24.11's card, loaded by NextZXOS's snapshot loader through the Multiface's NMI, started and played to frame 3600: the sandbox prints the native build's lines, a state every frame changes nothing, it is not the run left alone"
	else
		report "game:sn-snapshot" FAIL "$why"
	fi
fi

# A 128K snapshot in the fast start. ZEsarUX loads a .z80 into the Next
# (--snap-no-change-machine: by default it turned into a 128K, and the
# core's memory exports then pointed past the machine's memory) one bank at
# a time through port 0x7ffd, and patch 0006 gives the Next the snapshot's
# own 0x7ffd afterwards, as ZEsarUX does for a 128K. The snapshot is made
# by tests/make-z80.py: its program is in bank 3 at 0xC000, fills the
# screen and stops at 0xC00E; bank 7, the last one loaded, has a jump to
# itself at 0xC000. The sandbox prints the native build's lines and a state
# every frame changes nothing.
z80="$work/test128.z80"
python3 "$here/tests/make-z80.py" "$z80" || exit 2
nat z8-n $fast --program "$z80" --frames 100 --every 50
box z8-s $fast --program "$z80" --frames 100 --every 50
box z8-r $fast --program "$z80" --frames 100 --every 50 --rerecord
why=""
same z8-n z8-s || why="$why native and sandbox differ [$(last "$work/z8-s.out" | cut -c1-60)] $(tail -1 "$work/z8-s.err" | cut -c1-80);"
same z8-s z8-r || why="$why a state every frame changed the run;"
[ "$(field "$work/z8-s.out" pc)" = c00e ] || why="$why the processor is at $(field "$work/z8-s.out" pc), not at the end of the snapshot's program (c00e) $(tail -1 "$work/z8-n.err" | cut -c1-80);"
if [ -z "$why" ]; then
	report "snapshot:128k" PASS "a 128K .z80 snapshot in the fast start runs its program from the bank its own 0x7ffd pages in; the sandbox prints the native build's lines, a state every frame changes nothing"
else
	report "snapshot:128k" FAIL "$why"
fi

# What cannot be run is refused with a reason: a way of starting there is
# not, NextZXOS with no SD card, NextZXOS with no boot ROM, and a card
# bigger than the 1 GiB the machine's memory has room for (2 GiB of zeros,
# made here, taking no room on the disk).
box x-boot $nextzxos --set boot=floppy --frames 5
box x-nosd --firmware "$fw/tbblue_loader.rom" --frames 5
box x-norom --sd "$fw/tbblue.mmc" --frames 5
truncate -s 2G "$work/big.img"
box x-big --set boot=card --sd "$work/big.img" --frames 5
rm -f "$work/big.img"
why=""
grep -q "^Init: error.sd_too_big.2048.1024" "$work/x-big.err" || why="$why a card of 2 GiB is not refused for its size [$(grep Init "$work/x-big.err" | cut -c1-60)];"
grep -q "^Init: error.bad_boot.floppy" "$work/x-boot.err" || why="$why an unknown start is not refused [$(grep Init "$work/x-boot.err" | cut -c1-60)];"
grep -q "^Init: error.no_sd" "$work/x-nosd.err" || why="$why no SD card is not refused [$(grep Init "$work/x-nosd.err" | cut -c1-60)];"
grep -q "^Init: .*tbblue_loader.rom" "$work/x-norom.err" || why="$why a missing boot ROM is not named [$(grep Init "$work/x-norom.err" | cut -c1-60)];"
if [ -z "$why" ]; then
	report "settings:refused" PASS "an unknown start, NextZXOS with no SD card, NextZXOS with no boot ROM and a card of 2 GiB are each refused with a reason: $(sed -n 's/^Init: //p' "$work/x-norom.err" | tr -d '\037' | cut -c1-70)"
else
	report "settings:refused" FAIL "$why"
fi

# The package in the frontend's own engine (chimera-run, from a Chimera
# checkout named by CHIMERA_ROOT), as a project:
#  - a movie with keys, recorded there and played back by another process,
#    shows at a frame the picture this repository's own runner has there;
#  - the same movie with the engine's own save and load before every frame
#    shows the same picture;
#  - a movie's row has the columns the package declares, and the movie
#    recorded is the movie that was played;
#  - NextZXOS with no boot ROM handed over does not start, and the ROM is
#    named;
#  - the start the core suggests for a project, with and without a card.
engine="$chimera_engine"
same_picture="$here/tests/same-picture.py"
if [ -z "$chimera_root" ] || [ ! -x "$engine" ] || [ ! -f "$package" ]; then
	why="set CHIMERA_ROOT to a Chimera checkout with build/dll/chimera-run, and make the package (waterbox/build-package.sh -o build/pkg)"
	report "frontend:picture" SKIP "$why"
	report "frontend:states" SKIP "$why"
	report "frontend:movie" SKIP "$why"
	report "frontend:firmware" SKIP "$why"
	report "frontend:suggest" SKIP "$why"
	report "frontend:built-in-rom" SKIP "$why"
else
	fe_frames=1500
	shot=1499
	movie_buttons="200:5:29 700:5:39 1000:30:42 1040:20:46 1100:5:10"
	# shellcheck disable=SC2086
	python3 "$here/tests/make-movie.py" "$fe_frames" $movie_buttons > "$work/keys.movie"
	fe_dir="$work/fe"
	mkdir -p "$fe_dir"
	cp "$fw/tbblue.mmc" "$fe_dir/"
	mk() { # <project> <movie> [options]: a project of the card and the movie
		mk_p="$1" mk_m="$2"; shift 2
		env -u DISPLAY "$engine" --new-project "$mk_p" "$package" --file "sd=$fe_dir/tbblue.mmc" --movie "$mk_m" "$@" > "$mk_p.log" 2>&1
	}
	fw_arg="tbblue_loader.rom=$fw/tbblue_loader.rom"
	mk "$fe_dir/rec.chimeraProject" "$work/keys.movie" --firmware "$fw_arg"
	env -u DISPLAY "$engine" --project "$fe_dir/rec.chimeraProject" "$package" --files "$fe_dir" --firmware "$fw_arg" \
		--record "$work/fe.movie" --screenshot "$shot=$work/fe-a.tga" > "$work/fe-record.out" 2>&1
	mk "$fe_dir/play.chimeraProject" "$work/fe.movie" --firmware "$fw_arg"
	env -u DISPLAY "$engine" --project "$fe_dir/play.chimeraProject" "$package" --files "$fe_dir" --firmware "$fw_arg" \
		--screenshot "$shot=$work/fe.tga" > "$work/fe-play.out" 2>&1
	played=$?
	# shellcheck disable=SC2046,SC2086
	box fe $nextzxos --frames $((shot + 1)) --ppm "$work/fe.ppm" $(python3 "$here/tests/make-movie.py" "$fe_frames" $movie_buttons --as-presses)
	verdict="$(python3 "$same_picture" "$work/fe.tga" "$work/fe.ppm" 2>&1)"; same_rc=$?
	if [ "$played" = 0 ] && grep -q "^frames=$fe_frames" "$work/fe-play.out" && [ "$same_rc" = 0 ] && cmp -s "$work/fe-a.tga" "$work/fe.tga"; then
		report "frontend:picture" PASS "a movie of $fe_frames frames with keys and the joystick, recorded in chimera-run and played back by another process: its picture at frame $shot is this repository's runner's ($verdict)"
	else
		report "frontend:picture" FAIL "chimera-run exit $played; pictures: $verdict; $(tail -1 "$work/fe-play.out")"
	fi
	env -u DISPLAY "$engine" --project "$fe_dir/play.chimeraProject" "$package" --files "$fe_dir" --firmware "$fw_arg" \
		--rerecord --screenshot "$shot=$work/fe-states.tga" > "$work/fe-states.out" 2>&1
	stated=$?
	verdict="$(python3 "$same_picture" "$work/fe-states.tga" "$work/fe.ppm" 2>&1)"; same_rc=$?
	if [ "$stated" = 0 ] && [ "$same_rc" = 0 ]; then
		report "frontend:states" PASS "the same movie with the engine's save and load before every frame: the same picture at frame $shot ($verdict)"
	else
		report "frontend:states" FAIL "chimera-run --rerecord exit $stated; pictures: $verdict; $(tail -1 "$work/fe-states.out")"
	fi
	row="$(grep -m1 '^|' "$work/fe.movie" | tr -d '\r')"
	held="$(grep -c '[0-9A-Za-z<^$_%!]' "$work/fe.movie")"
	if [ "$(printf '%s' "$row" | tr -cd '|' | wc -c)" = 3 ] && [ "${#row}" = 51 ] && [ "$held" -gt 0 ] && cmp -s "$work/keys.movie" "$work/fe.movie"; then
		report "frontend:movie" PASS "a recorded movie's row has 42 columns for the keyboard and the two resets and 6 for the joystick, and the movie recorded is the movie that was played ($held rows with a button held)"
	else
		report "frontend:movie" FAIL "a row is [$row] (${#row} characters); $held rows with a button; recorded $(wc -l < "$work/fe.movie" 2> /dev/null) rows of $(wc -l < "$work/keys.movie")"
	fi
	mk "$fe_dir/nofw.chimeraProject" "$work/keys.movie"
	env -u DISPLAY "$engine" --project "$fe_dir/nofw.chimeraProject" "$package" --files "$fe_dir" --frames 60 > "$work/fe-nofw.out" 2>&1
	refused=$?
	if [ "$refused" != 0 ] && grep -q "tbblue_loader.rom" "$work/fe-nofw.out" "$fe_dir/nofw.chimeraProject.log"; then
		report "frontend:firmware" PASS "NextZXOS with no boot ROM handed over does not start, and the ROM is named (exit $refused)"
	else
		report "frontend:firmware" FAIL "no boot ROM: exit $refused [$(tail -1 "$work/fe-nofw.out" | cut -c1-90)]"
	fi
	# What the core suggests before a machine is built: a project with only
	# a program starts fast (there is no card to start NextZXOS from); one
	# with the card starts NextZXOS.
	cp "$prog" "$fe_dir/"
	env -u DISPLAY "$engine" --new-project "$fe_dir/progonly.chimeraProject" "$package" --file "program=$fe_dir/test.nex" \
		--movie "$work/keys.movie" > "$fe_dir/progonly.log" 2>&1
	env -u DISPLAY "$engine" --project "$fe_dir/progonly.chimeraProject" "$package" --suggest > "$work/sg-prog.out" 2>&1
	env -u DISPLAY "$engine" --project "$fe_dir/rec.chimeraProject" "$package" --suggest > "$work/sg-sd.out" 2>&1
	if grep -q '^{"values":{"boot":"fast"}' "$work/sg-prog.out" && grep -q '^{"values":{"boot":"nextzxos"}' "$work/sg-sd.out"; then
		report "frontend:suggest" PASS "the core suggests the fast start for a project with a program and no card, and NextZXOS for one with the card"
	else
		report "frontend:suggest" FAIL "program only [$(tail -1 "$work/sg-prog.out" | cut -c1-60)] with the card [$(tail -1 "$work/sg-sd.out" | cut -c1-60)]"
	fi
	# The fast start in the frontend with no firmware handed over at all:
	# the project of the test program, set to start fast, runs, and its
	# picture at frame 99 is this repository's runner's.
	env -u DISPLAY "$engine" --new-project "$fe_dir/fast.chimeraProject" "$package" --file "program=$fe_dir/test.nex" \
		--settings '{"boot":"fast"}' --movie "$work/keys.movie" > "$fe_dir/fast.log" 2>&1
	env -u DISPLAY "$engine" --project "$fe_dir/fast.chimeraProject" "$package" --files "$fe_dir" --frames 100 \
		--screenshot "99=$work/fe-fast.tga" > "$work/fe-fast.out" 2>&1
	fast_rc=$?
	# shellcheck disable=SC2046,SC2086
	box fe-fast $fast --program "$prog" --frames 100 --ppm "$work/fe-fast.ppm" $(python3 "$here/tests/make-movie.py" "$fe_frames" $movie_buttons --as-presses)
	verdict="$(python3 "$same_picture" "$work/fe-fast.tga" "$work/fe-fast.ppm" 2>&1)"; same_rc=$?
	if [ "$fast_rc" = 0 ] && [ "$same_rc" = 0 ]; then
		report "frontend:built-in-rom" PASS "a project that starts fast runs in chimera-run with no firmware handed over; its picture at frame 99 is this repository's runner's ($verdict)"
	else
		report "frontend:built-in-rom" FAIL "chimera-run exit $fast_rc [$(tail -1 "$work/fe-fast.out" | cut -c1-80)]; pictures: $verdict"
	fi
fi

echo
echo "$ok ok, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ]
