#!/bin/bash
# The window test: the package loaded by Chimera's window (run headless: it
# needs no display), not by its headless runner. Three things are asked:
#  - NextZXOS started from the SD card in a project and run for a number of
#    frames with nothing pressed leaves the Next's whole memory ("RAM", 1.75
#    MiB) byte for byte what the native build leaves (so the boot ROM handed
#    over as firmware was found, the card was read and the system started);
#  - a setting made in the project reaches the machine: with Start set to
#    "fast" and the test program in the project, the memory at that frame is
#    another, and is what the native build has with that setting;
#  - the keys the package ships become Chimera's keys.
#
# Usage: waterbox/tests/run-frontend.sh --chimera-root <chimera checkout> [--frames N]
# Needs: Chimera built with its window and chimera-run, the package
# (waterbox/build-package.sh -o build/pkg, or ZESARUX_PACKAGE=<file>), the
# native build. The Next's boot ROM, 48K ROM and SD card come from the
# pinned ZEsarUX submodule, or from ZESARUX_TEST_FIRMWARE=<folder>.
# QT_LIB_DIR=<folder> names where Qt's libraries are when the window does
# not find them by itself (Chimera's tools/ci-chimera.sh installs Qt into
# <its folder>/6.12.0/gcc_64).
set -u

here="$(cd "$(dirname "$0")" && pwd)"
wb="$(cd "$here/.." && pwd)"
root="$(cd "$wb/.." && pwd)"
frames=1000
chimera_root=""
while [ $# -gt 0 ]; do
	case "$1" in
		--chimera-root) chimera_root="$2"; shift ;;
		--frames) frames="$2"; shift ;;
		-*) echo "unknown option: $1" >&2; exit 2 ;;
		*) break ;;
	esac
	shift
done
[ -n "$chimera_root" ] && [ -d "$chimera_root" ] || { echo "say where Chimera is: --chimera-root <chimera checkout>" >&2; exit 2; }
chimera_root="$(cd "$chimera_root" && pwd)"

chimera_exe="$chimera_root/build/meson-linux/source/ui/chimera"
crun="$chimera_root/build/dll/chimera-run"
package="${ZESARUX_PACKAGE:-$root/build/pkg/zesarux.chimeraCore}"
rn="$root/build/meson-native/run-native"
fw="${ZESARUX_TEST_FIRMWARE:-$root/extern/zesarux/src}"
[ -x "$chimera_exe" ] || { echo "Chimera's window is not built: $chimera_exe" >&2; exit 1; }
[ -x "$crun" ] || { echo "chimera-run is not built: $crun" >&2; exit 1; }
[ -f "$package" ] || { echo "the package is not there: $package (run waterbox/build-package.sh -o build/pkg)" >&2; exit 1; }
[ -x "$rn" ] || { echo "the native build is not there: $rn" >&2; exit 1; }
for f in tbblue_loader.rom 48.rom tbblue.mmc; do
	[ -f "$fw/$f" ] || { echo "no $f in $fw (git submodule update --init, or ZESARUX_TEST_FIRMWARE)" >&2; exit 1; }
done

work="$(mktemp -d)"
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
prog="$work/test.nex"
python3 "$here/make-nex.py" "$prog" || exit 1

# Chimera's window, run headless: the package's own folder as the cores
# folder, its settings, cache and recovery files the run's own, and nothing
# carried over from a Chimera installed on this machine
[ -n "${QT_LIB_DIR:-}" ] && export LD_LIBRARY_PATH="$QT_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ALSOFT_DRIVERS=null CHIMERA_CORES_DIR="$(dirname "$package")" CHIMERA_QT_CACHE="$work/qt-cache" \
	CHIMERA_QT_RECOVERY="$work/qt-recovery" CHIMERA_QT_HASHES="$work/qt-file-hashes.txt" \
	CHIMERA_QT_CHIMERA_SETTINGS="$work/no-chimera-config.ini"

ok=0
failed=0
report() { printf '%-22s %-6s %s\n' "$1" "$2" "$3"; case "$2" in PASS) ok=$((ok + 1)) ;; *) failed=$((failed + 1)) ;; esac; }

# A game runs only in a project (Chimera, 2026-10-26): each run opens one,
# made with chimera-run --new-project, with the settings and firmware given.
make_project() { # <tag> <settings JSON> <firmware id=path> <slot=path>...
	local tag="$1" settings="$2" firmware="$3"; shift 3
	local dir="$work/project.$tag" files=() s
	rm -rf "$dir" && mkdir -p "$dir"
	: > "$dir/empty.movie"
	for s in "$@"; do
		cp "${s#*=}" "$dir/"
		files+=(--file "${s%%=*}=$dir/$(basename "${s#*=}")")
	done
	"$crun" --new-project "$dir/$tag.chimeraProject" "$package" "${files[@]}" --movie "$dir/empty.movie" \
		--settings "$settings" --firmware "$firmware" > "$dir/make.log" 2>&1 && echo "$dir/$tag.chimeraProject"
}
run_frontend() { # <tag> <project> <firmware id> <firmware path> <frames> [lua script]
	local tag="$1" project="$2" fw_id="$3" fw_path="$4" nframes="$5" lua="${6:-frontend-ram.lua}"
	local job="$work/job.$tag.txt" out="$work/$tag.ram.bin"
	[ "$lua" = dump-controls.lua ] && out="$work/$tag.controls.json"
	{
		echo "frames=$nframes"
		echo "out=$out"
		echo "meta=$work/$tag.meta.txt"
		echo "shot="
	} > "$job"
	# the firmware, remembered as the Firmware window remembers a file
	# chosen by hand (keyed under the core's name)
	python3 "$here/settings-config.py" "$work/settings.$tag.json" '{}' "{\"$fw_id\": \"$fw_path\"}"
	( cd "$chimera_root" && MINIHAWK_JOB="$job" timeout 900 "$chimera_exe" --headless \
		"--config=$work/settings.$tag.json" "--core=$package" "--lua=$here/$lua" "--project=$project" ) > "$work/$tag.log" 2>&1
	[ -f "$work/$tag.meta.txt" ] && grep -q "^status=OK" "$work/$tag.meta.txt"
}
native_ram() { # <tag> <options...>
	local tag="$1"; shift
	"$rn" "$work/native-dir.$tag" --frames "$frames" --dump-ram "$work/native.$tag.ram.bin" "$@" > "$work/native.$tag.txt" 2>&1
}

loader="$fw/tbblue_loader.rom"
if ! native_ram base --firmware "$loader" --sd "$fw/tbblue.mmc"; then
	report "window:memory" FAIL "the native runner failed: $(tail -1 "$work/native.base.txt")"
elif ! project="$(make_project base '{}' "tbblue_loader.rom=$loader" "sd=$fw/tbblue.mmc")"; then
	report "window:memory" FAIL "no project made: $(tail -1 "$work/project.base/make.log")"
elif ! run_frontend base "$project" tbblue_loader.rom "$loader" "$frames"; then
	report "window:memory" FAIL "Chimera did not finish the run: $(grep '^detail=' "$work/base.meta.txt" 2>/dev/null) $(tail -2 "$work/base.log" | tr '\n' ' ' | cut -c1-200)"
elif cmp -s "$work/native.base.ram.bin" "$work/base.ram.bin"; then
	report "window:memory" PASS "NextZXOS started from the SD card in Chimera's window and run for $frames frames: the Next's memory ($(wc -c < "$work/base.ram.bin") bytes) is the native build's"
else
	report "window:memory" FAIL "the memory differs from the native build's"
fi

rom48="$fw/48.rom"
if ! native_ram fast --firmware "$rom48" --set boot=fast --program "$prog"; then
	report "window:setting" FAIL "the native runner failed: $(tail -1 "$work/native.fast.txt")"
elif ! project="$(make_project fast '{"boot": "fast"}' "48.rom=$rom48" "program=$prog")"; then
	report "window:setting" FAIL "no project made: $(tail -1 "$work/project.fast/make.log")"
elif ! run_frontend fast "$project" 48.rom "$rom48" "$frames"; then
	report "window:setting" FAIL "Chimera did not finish the run: $(tail -2 "$work/fast.log" | tr '\n' ' ' | cut -c1-200)"
elif cmp -s "$work/native.fast.ram.bin" "$work/fast.ram.bin" && ! cmp -s "$work/fast.ram.bin" "$work/base.ram.bin"; then
	report "window:setting" PASS "with Start set to fast and the test program in the project, the memory at frame $frames is another, and is the native build's with that setting"
else
	report "window:setting" FAIL "fast start: same as native $(cmp -s "$work/native.fast.ram.bin" "$work/fast.ram.bin" && echo yes || echo no); differs from NextZXOS $(cmp -s "$work/fast.ram.bin" "$work/base.ram.bin" && echo no || echo yes)"
fi

# the keys the package ships are the machine's when nobody changed them (the
# window keeps only the keys a person changed; fresh settings have none)
if [ -n "${project:-}" ] && run_frontend keys "$project" 48.rom "$rom48" 1 dump-controls.lua; then
	if python3 "$here/check-keybinds.py" "$work/keys.controls.json" "$wb/default_keybinds.json" "ZX Spectrum Next Keyboard and Joystick" > "$work/keys.txt" 2>&1; then
		report "window:keys" PASS "$(cat "$work/keys.txt")"
	else
		report "window:keys" FAIL "$(head -1 "$work/keys.txt")"
	fi
else
	report "window:keys" FAIL "Chimera did not finish the run: $(tail -2 "$work/keys.log" | tr '\n' ' ' | cut -c1-200)"
fi

echo
echo "$ok ok, $failed failed"
[ "$failed" -eq 0 ]
