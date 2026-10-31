/* Sandbox-only replacements for C library calls whose system calls miniBox
 * does not have. One static link means defining these here takes the place
 * of the C library's versions. Not compiled into the native reference.
 *
 * ZEsarUX asks whether a file exists before it opens it (a ROM, the SD
 * card, a program). In a sandbox there are files mounted by name and no
 * folders: a path is "there" when it can be opened, and what it is is what
 * the open file says. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

/* "./name" is the mounted file "name": the sandbox's files have no folders,
 * and ZEsarUX puts the current folder (".", chimera_compat.c) in front of a
 * file given on its command line */
static const char *bare(const char *path)
{
	while (path && path[0] == '.' && path[1] == '/') path += 2;
	return path;
}

int access(const char *path, int mode)
{
	path = bare(path);
	(void)mode;
	if (!path || !*path) {
		errno = ENOENT;
		return -1;
	}
	const int fd = open(path, O_RDONLY);
	if (fd < 0) {
		errno = ENOENT;
		return -1;
	}
	close(fd);
	return 0;
}

/* The C library's fstat() is fstatat() of an empty path on an open file, so
 * this function is asked both things: about an open file (answered by the
 * system call the sandbox does have), and about a name (answered by
 * opening it). */
int fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	path = bare(path);
	if (path && !*path && (flags & AT_EMPTY_PATH)) {
		const long r = syscall(SYS_fstat, dirfd, st);
		if (r < 0) {
			errno = (int)-r > 0 && r > -4096 ? (int)-r : EBADF;
			return -1;
		}
		return 0;
	}
	if (!path || !*path) {
		errno = ENOENT;
		return -1;
	}
	const int fd = open(path, O_RDONLY);
	if (fd < 0) {
		errno = ENOENT;
		return -1;
	}
	const long r = syscall(SYS_fstat, fd, st);
	close(fd);
	if (r != 0) {
		errno = EIO;
		return -1;
	}
	/* ZEsarUX opens only what is a regular file. A mounted file is one,
	 * whatever kind the sandbox says (it may say none). */
	if (!S_ISREG(st->st_mode))
		st->st_mode = (st->st_mode & ~S_IFMT) | S_IFREG;
	return 0;
}

int stat(const char *path, struct stat *st) { return fstatat(AT_FDCWD, path, st, 0); }
int lstat(const char *path, struct stat *st) { return fstatat(AT_FDCWD, path, st, 0); }

/* ZEsarUX prints a backtrace from its fault handler with glibc's
 * backtrace(); musl has none. The handler is never installed in the core
 * (signal() is the core's, chimera_compat.c), so these only have to link. */
int backtrace(void **buffer, int size)
{
	(void)buffer;
	(void)size;
	return 0;
}
void backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
	(void)buffer;
	(void)size;
	(void)fd;
}

/* A file's name as ZEsarUX makes it from a command line ("./tbblue.mmc",
 * chimera_compat.c) is the mounted file's bare name. The guest's link wraps
 * fopen and open (meson.build). A built-in ROM is opened from the core's
 * memory (chimera_roms.c). */
#include <stdio.h>
#include "chimera_roms.h"
FILE *__real_fopen(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode)
{
	FILE *rom = chimera_rom_fopen(path, mode);
	return rom ? rom : __real_fopen(bare(path), mode);
}
int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
	return __real_open(bare(path), flags, 0644);
}
