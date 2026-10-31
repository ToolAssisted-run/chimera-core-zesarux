/* ZEsarUX is given the ROM that is built into the core.
 *
 * ZEsarUX loads a machine's ROM in three steps (cpu.c, rom_load): it looks
 * for the file by name in a list of folders (find_sharedfile), asks its
 * size (get_file_size) and reads it (fopen). The link wraps all three
 * (meson.build). For the name of a built-in ROM the first answers with a
 * path of its own, which no mounted file can have; the size and the open
 * of that path are answered from the core's memory. Every other name goes
 * to ZEsarUX's own functions.
 *
 * A built-in ROM wins over a file of the same name: what a machine runs on
 * must not depend on what happens to be mounted beside it. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "chimera_roms.h"

#define BUILT_IN "chimera-built-in/"

static const struct chimera_rom *by_name(const char *name)
{
	for (const struct chimera_rom *r = chimera_roms; r->name; r++)
		if (!strcmp(r->name, name)) return r;
	return NULL;
}

/* the ROM a path made by __wrap_find_sharedfile names */
static const struct chimera_rom *by_path(const char *path)
{
	if (!path || strncmp(path, BUILT_IN, sizeof BUILT_IN - 1)) return NULL;
	return by_name(path + sizeof BUILT_IN - 1);
}

int __real_find_sharedfile(char *name, char *path);
int __wrap_find_sharedfile(char *name, char *path)
{
	if (name && by_name(name)) {
		if (path) snprintf(path, 64, BUILT_IN "%s", name);
		return 1;
	}
	return __real_find_sharedfile(name, path);
}

long long int __real_get_file_size(char *path);
long long int __wrap_get_file_size(char *path)
{
	const struct chimera_rom *r = by_path(path);
	return r ? (long long int)r->size : __real_get_file_size(path);
}

FILE *chimera_rom_fopen(const char *path, const char *mode)
{
	const struct chimera_rom *r = by_path(path);
	if (!r || !mode || mode[0] != 'r' || strchr(mode, '+')) return NULL;
	return fmemopen((void *)r->data, r->size, "rb");
}
