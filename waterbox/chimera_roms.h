/* The ROMs built into the core, and how ZEsarUX is given them. */
#ifndef CHIMERA_ZESARUX_ROMS_H
#define CHIMERA_ZESARUX_ROMS_H

#include <stddef.h>
#include <stdio.h>

struct chimera_rom {
	const char *name; /* ZEsarUX's name for the file: "48.rom" */
	const unsigned char *data;
	size_t size;
};
/* ended by an entry with no name; written by waterbox/gen-roms.py */
extern const struct chimera_rom chimera_roms[];

/* The built-in ROM a path names, opened for reading from the core's own
 * memory; NULL for any other path. The link's fopen wrap asks this first
 * (guest-syscalls.c in the sandbox, run-native.c natively). */
FILE *chimera_rom_fopen(const char *path, const char *mode);

#endif
