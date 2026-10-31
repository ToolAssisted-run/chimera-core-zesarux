#!/bin/sh
# Writes patches/ from the patched extern/zesarux tree: the reverse of
# apply-patches.sh. A patch is the tree's difference in a list of files. Edit
# the tree, run this, and apply-patches.sh must then say "already applied".
# A file is in ONE patch, whole.
set -eu
trap '' PIPE
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
tree="$root/extern/zesarux"

# name | files (space separated), in the order the patches apply
series='
0001-chimera-switched-on-by-a-call|src/start.c
0002-chimera-the-machine-is-the-clock|src/timer.c
0003-chimera-hooks-for-sound-input-and-frames|src/audio/audio.c src/operaciones.c src/cores/core_spectrum.c
0004-chimera-fat32-cards-for-emulators-and-a-lent-card|src/third_party/ff.c src/third_party/diskio.c
0005-chimera-next-registers-nextzxos-2.09-needs|src/machines/tbblue.c src/machines/tbblue.h src/storage/diviface.c src/ula.c
0006-chimera-a-128k-snapshot-on-the-next|src/snap/snap.c
'

rm -f "$root"/patches/*.patch
printf '%s\n' "$series" | grep '|' | while IFS='|' read -r name files; do
	# shellcheck disable=SC2086
	git -C "$tree" diff -- $files > "$root/patches/$name.patch"
	echo "$name.patch: $(grep -c '^diff --git' "$root/patches/$name.patch") files"
done
# a changed file no patch names would be lost by a fresh clone
left="$(git -C "$tree" status --porcelain | wc -l)"
named="$(cat "$root"/patches/*.patch | grep -c '^diff --git')"
[ "$left" -eq "$named" ] || { echo "extern/zesarux has $left changed files and the patches name $named: add the rest to the series" >&2; exit 1; }
