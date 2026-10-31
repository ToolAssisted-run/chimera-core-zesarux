/* chimera_compat.c - what ZEsarUX asks of the system, answered by the core.
 *
 * ZEsarUX reads the host's clock in many places: to pace itself, to seed
 * its noise generators and the machine's first RAM contents, for the
 * real-time clock a program can read, for file dates. A core must give the
 * same run for the same input every time, so none of it may reach the host.
 * The link wraps these functions (-Wl,--wrap=<name>, meson.build) for every
 * object of the emulator, natively and in the sandbox alike, and the
 * answers here are the machine's own:
 *
 *  - the time is the project's start date and time (the setting
 *    "clock_start", seconds since 1970 in UTC) plus the frames run so far
 *    over the frame rate. It moves only when the machine runs;
 *  - the local time is that time in UTC: no time zone of the host;
 *  - rand() is a generator of the core's own, seeded by nobody;
 *  - sleeping returns at once (pacing is the frontend's);
 *  - signal handlers are not installed: a fault is the sandbox's business;
 *  - the system's name is the same everywhere (ZEsarUX shows it in its
 *    menus, which the core never opens; the sandbox has no such call);
 *  - an error ZEsarUX reports is kept (chimera_last_error) as the reason a
 *    start failed, whether or not its display is up yet to show it;
 *  - the current folder is ".": ZEsarUX makes the name of a file given on
 *    its command line absolute by putting the current folder in front, and
 *    the sandbox has neither folders nor that call. "./name" is opened as
 *    "name" in the sandbox (guest-syscalls.c).
 */
#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>

#include "chimera_glue.h"

int64_t chimera_clock_start;
uint64_t chimera_clock_frames;
int chimera_clock_fps_num = 50, chimera_clock_fps_den = 1;

/* the machine's time, in microseconds since 1970 */
static int64_t now_us(void)
{
	const int64_t us = (int64_t)(chimera_clock_frames * 1000000ull * (uint64_t)chimera_clock_fps_den /
	                             (uint64_t)chimera_clock_fps_num);
	return chimera_clock_start * 1000000 + us;
}

/* For the tests' control only (waterbox/run-gate.sh, time:virtual): with
 * CHIMERA_TEST_HOST_CLOCK set, the native build reads the host's clock as
 * ZEsarUX does on its own, to show that the test can tell. The sandbox has
 * no environment, so there this is never on. */
#include <stdlib.h>
time_t __real_time(time_t *t);
int __real_gettimeofday(struct timeval *tv, void *tz);
static int host_clock(void)
{
	static int known, on;
	if (!known) {
		const char *v = getenv("CHIMERA_TEST_HOST_CLOCK");
		on = v && *v;
		known = 1;
	}
	return on;
}

time_t __wrap_time(time_t *t)
{
	if (host_clock()) return __real_time(t);
	const time_t v = (time_t)(now_us() / 1000000);
	if (t) *t = v;
	return v;
}

int __wrap_gettimeofday(struct timeval *tv, void *tz)
{
	if (host_clock()) return __real_gettimeofday(tv, tz);
	(void)tz;
	if (tv) {
		const int64_t us = now_us();
		tv->tv_sec = (time_t)(us / 1000000);
		tv->tv_usec = (suseconds_t)(us % 1000000);
	}
	return 0;
}

struct tm *__wrap_localtime(const time_t *t)
{
	static struct tm out;
	return gmtime_r(t, &out);
}

time_t __wrap_mktime(struct tm *tm) { return timegm(tm); }

static uint32_t g_rand = 1;
int __wrap_rand(void)
{
	g_rand = g_rand * 1103515245u + 12345u;
	return (int)((g_rand >> 16) & 0x7fff);
}
void __wrap_srand(unsigned seed) { (void)seed; }

unsigned __wrap_sleep(unsigned s) { (void)s; return 0; }
int __wrap_usleep(unsigned us) { (void)us; return 0; }

typedef void (*chimera_sighandler)(int);
chimera_sighandler __wrap_signal(int sig, chimera_sighandler h)
{
	(void)sig;
	(void)h;
	return SIG_DFL;
}

int __wrap_uname(struct utsname *u)
{
	memset(u, 0, sizeof *u);
	strcpy(u->sysname, "Linux");
	strcpy(u->nodename, "chimera");
	strcpy(u->release, "chimera");
	strcpy(u->version, "chimera");
	strcpy(u->machine, "x86_64");
	return 0;
}

char *__wrap_getcwd(char *buf, size_t size)
{
	if (!buf || size < 2) return NULL;
	strcpy(buf, ".");
	return buf;
}

/* PlayTZX is left out of the core (meson.build): ZEsarUX's own licence note
 * for it says its licence is unknown. ZEsarUX calls it for one thing, to
 * turn a .tzx tape into sound for its "real tape" player; it is answered
 * that this failed, which ZEsarUX reports as it reports a bad file. A .tzx
 * tape inserted the ordinary way is read by ZEsarUX's own code. */
int main_playtzx(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return 1;
}

/* Every message of ZEsarUX's passes through debug_printf; the link wraps it
 * (meson.build), so an error is kept before ZEsarUX shows or prints it.
 * The message itself goes on as it came. */
#include <stdarg.h>
#include <stdio.h>
#include "debug.h"
void __real_debug_printf(int debuglevel, const char *format, ...);
void __wrap_debug_printf(int debuglevel, const char *format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof text, format, args);
	va_end(args);
	if ((debuglevel & 0xFF) == VERBOSE_ERR)
		snprintf(chimera_last_error, sizeof chimera_last_error, "%s", text);
	__real_debug_printf(debuglevel, "%s", text);
}
