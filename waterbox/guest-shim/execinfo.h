/* execinfo.h for the sandbox build: glibc's backtrace functions, which
 * musl does not have. ZEsarUX's fault handler includes this; the handler is
 * never installed in the core, and waterbox/guest-syscalls.c answers the two
 * functions with nothing. */
#ifndef CHIMERA_GUEST_EXECINFO_H
#define CHIMERA_GUEST_EXECINFO_H
int backtrace(void **buffer, int size);
void backtrace_symbols_fd(void *const *buffer, int size, int fd);
#endif
