#define _GNU_SOURCE
/* The native reference of the core: the same exports the sandbox has
 * (cinterface.c, compiled for the host), called directly, a frame at a
 * time, by the same harness the sandboxed runner uses (gate-harness.h).
 *
 *   run-native <work directory, which must not exist> [options]
 *
 * The options are the harness's. Natively there are no states. The core
 * opens "settings", "slots" and its files where it is started, as it finds
 * them mounted in the sandbox: the directory is this run's own, and each
 * file is copied into it under its bare name.
 */
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gate-harness.h"

int Init(void);
const char *GetLoadError(void);
void FrameAdvance(uint64_t);
int InputWasRead(void);
int16_t *GetAudio(void);
int GetAudioSampleCount(void);
uint32_t *GetVideoBgra(void);
int GetVideoWidth(void);
int GetVideoHeight(void);
int GetVsyncNumerator(void);
int GetVsyncDenominator(void);
uint8_t *GetMemoryDomainPtr(int);
int64_t GetMemoryDomainSize(int);
uint8_t *DebugMemoryPtr(void);
int64_t DebugMemorySize(void);
uint32_t GetTstates(void);
uint32_t DebugPc(void);

/* The sandbox's clock is the project's start and stands still; natively the
 * link wraps clock_gettime (meson.build) to say the same. */
#include <time.h>
static uint64_t g_clock_start;
int __wrap_clock_gettime(clockid_t id, struct timespec *ts)
{
	(void)id;
	ts->tv_sec = (time_t)g_clock_start;
	ts->tv_nsec = 0;
	return 0;
}

static int write_file(const char *name, const char *text)
{
	FILE *f = fopen(name, "wb");
	if (!f)
		return 0;
	fputs(text, f);
	return fclose(f) == 0;
}

static int copy_file(const char *from, const char *to)
{
	FILE *in = fopen(from, "rb"), *out = in ? fopen(to, "wb") : NULL;
	if (!out) {
		if (in)
			fclose(in);
		return 0;
	}
	static char buf[1 << 20];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, in)) > 0)
		fwrite(buf, 1, n, out);
	fclose(in);
	return fclose(out) == 0;
}

/* a path given relative to where the runner was started still names the
 * same file after the runner has moved into its work directory */
static const char *absolute(const char *path)
{
	if (!path || path[0] == '/')
		return path;
	/* realpath, not getcwd: the link answers getcwd with "." for the
	 * emulator (chimera_compat.c), and this file is linked with it */
	char cwd[PATH_MAX], *out = (char *)malloc(PATH_MAX * 2);
	if (!out || !realpath(".", cwd))
		return path;
	snprintf(out, PATH_MAX * 2, "%s/%s", cwd, path);
	return out;
}

static int bring(const char *path)
{
	if (!path)
		return 1;
	if (!copy_file(path, harness_base(path))) {
		fprintf(stderr, "run-native: cannot copy %s\n", path);
		return 0;
	}
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: run-native <work directory> [options]\n");
		return 2;
	}
	struct harness_opts o;
	if (!harness_parse(argc, argv, 2, &o))
		return 2;
	g_clock_start = o.clock_start;
	o.sd = absolute(o.sd);
	o.program = absolute(o.program);
	o.ppm = absolute(o.ppm);
	o.dump_ram = absolute(o.dump_ram);
	for (int i = 0; i < o.firmwares; i++)
		o.firmware[i] = absolute(o.firmware[i]);
	if (mkdir(argv[1], 0777) != 0 || chdir(argv[1]) != 0) {
		fprintf(stderr, "run-native: %s must be a directory that can be made and is not there yet\n", argv[1]);
		return 2;
	}
	if (!bring(o.sd) || !bring(o.program))
		return 2;
	for (int i = 0; i < o.firmwares; i++)
		if (!bring(o.firmware[i]))
			return 2;
	char slots[2048];
	harness_slots_json(&o, slots, sizeof slots);
	if (!write_file("settings", o.settings_json) || !write_file("slots", slots)) {
		fprintf(stderr, "run-native: cannot write into %s\n", argv[1]);
		return 2;
	}
	const struct harness_core core = {
		Init, GetLoadError, FrameAdvance, InputWasRead, GetAudio, GetAudioSampleCount, GetVideoBgra, GetVideoWidth,
		GetVideoHeight, GetVsyncNumerator, GetVsyncDenominator, GetMemoryDomainPtr, GetMemoryDomainSize,
		DebugMemoryPtr, DebugMemorySize, GetTstates, DebugPc, NULL, NULL, NULL,
	};
	const int result = harness_run(&core, &o);
	fflush(stdout);
	fflush(stderr);
	return result;
}
