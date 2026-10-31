/* cinterface.c - ZEsarUX behind Chimera's guest interface.
 *
 * ZEsarUX is a desktop program: a main() that reads a command line and a
 * settings file, opens a window and runs until told to stop. Here it is
 * switched on by its own zesarux_main(), with the frontend's settings
 * written as the command line ZEsarUX already understands; patch 0001 makes
 * it return once the machine is set up instead of entering its loop, and
 * the core then runs the machine one frame at a time with ZEsarUX's own
 * step function (cpu_core_loop). Its display and sound card are the core's
 * (scrchimera.c, audiochimera.c), its clock is the machine's own
 * (chimera_compat.c, patch 0002).
 *
 * This file compiles IDENTICALLY for the guest (miniBox's emulibc) and for
 * the native reference build (native-shim/emulibc.h): the tests compare
 * the two.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include <emulibc.h>
#include <waterbox_settings.h>
#include <waterbox_slots.h>

#include "cpu.h"
#include "debug.h"
#include "ff.h"
#include "joystick.h"
#include "mmc.h"
#include "utils.h"
#include "operaciones.h"
#include "start.h"
#include "tbblue.h"

#include "chimera_glue.h"

/* ---- the wire: waterbox.config "input.buttons" order -----------------------
 * The forty keys of a Spectrum's keyboard, row by row as they are printed
 * (the Next's own keyboard has these forty, and its extra keys are
 * combinations of them); then Reset and Hard Reset; then a joystick of four
 * directions and two fire buttons. */
struct key { z80_byte *row; z80_byte bit; };
static const struct key kKeys[40] = {
	{ &puerto_63486, 1 }, { &puerto_63486, 2 }, { &puerto_63486, 4 }, { &puerto_63486, 8 }, { &puerto_63486, 16 },
	{ &puerto_61438, 16 }, { &puerto_61438, 8 }, { &puerto_61438, 4 }, { &puerto_61438, 2 }, { &puerto_61438, 1 },
	{ &puerto_64510, 1 }, { &puerto_64510, 2 }, { &puerto_64510, 4 }, { &puerto_64510, 8 }, { &puerto_64510, 16 },
	{ &puerto_57342, 16 }, { &puerto_57342, 8 }, { &puerto_57342, 4 }, { &puerto_57342, 2 }, { &puerto_57342, 1 },
	{ &puerto_65022, 1 }, { &puerto_65022, 2 }, { &puerto_65022, 4 }, { &puerto_65022, 8 }, { &puerto_65022, 16 },
	{ &puerto_49150, 16 }, { &puerto_49150, 8 }, { &puerto_49150, 4 }, { &puerto_49150, 2 }, { &puerto_49150, 1 },
	{ &puerto_65278, 1 }, { &puerto_65278, 2 }, { &puerto_65278, 4 }, { &puerto_65278, 8 }, { &puerto_65278, 16 },
	{ &puerto_32766, 16 }, { &puerto_32766, 8 }, { &puerto_32766, 4 }, { &puerto_32766, 2 }, { &puerto_32766, 1 },
};
enum {
	KEY_COUNT = 40,
	BTN_RESET = KEY_COUNT, /* the Next's soft reset */
	BTN_HARD_RESET,        /* the Next's hard reset: the boot ROM starts again */
	BTN_JOY, /* Up, Down, Left, Right, Fire, Fire 2 */
	BTN_COUNT = BTN_JOY + 6,
};
/* ZEsarUX's joystick state (puerto_especial_joystick): Right, Left, Down,
 * Up, Fire, Fire 2 are bits 0 to 5; the port a program reads is made from
 * it by the joystick mode the machine is in */
static const z80_byte kJoyBits[6] = { 8, 4, 2, 1, 16, 32 };

/* A text a person reads is a key of languages/en.json, each of its
 * arguments after a TEXT_ARG (chimera docs/translations.md). */
#define TEXT_ARG "\x1f"

static char g_loadError[640];
static int g_inited;
static int g_inputRead;
static uint8_t g_setButtons[BTN_COUNT];
static uint8_t g_held[BTN_COUNT];
static uint64_t g_frames;

void chimera_input_read(void) { g_inputRead = 1; }

/* told by ZEsarUX at the end of each of the machine's frames (patch 0003,
 * core_spectrum_fin_frame_pantalla), when the frame has been drawn */
static int g_frameDone;
void chimera_frame_end(void) { g_frameDone = 1; }

/* A setting's value from the settings the frontend mounted, or the default. */
static const char *setting(const char *name, const char *fallback, char *out, int size)
{
	snprintf(out, size, "%s", fallback);
	wbx_setting_str(name, out, size);
	return out;
}
static int on(const char *name, int fallback)
{
	char v[16];
	setting(name, fallback ? "true" : "false", v, sizeof v);
	return !strcmp(v, "true") || !strcmp(v, "1");
}

/* the first file of a project slot, or "" */
static void slot_file(const char *slot, char *out, int size)
{
	out[0] = '\0';
	if (wbx_slot_count(slot) > 0) wbx_slot_name(slot, 0, out, size);
}

ECL_EXPORT const char *GetLoadError(void) { return g_loadError; }

/* The program is copied into the root folder of the SD card the machine has
 * in its memory, by the FAT reader ZEsarUX uses for the same (its
 * --copy-file-to-mmc, util_copy_files_to_mmc_doit), which reads the card's
 * file into a buffer of its own; the buffer then becomes the machine's card
 * instead of being written back to the file. The project's file is never
 * written. A file of that name already on the card is replaced. */
extern char fatfs_disk_zero_path[];
extern BYTE *fatfs_disk_zero_memory;
extern long long int fatfs_disk_zero_tamanyo;
extern FATFS FatFs_util_copy_files_to_mmc;
extern int menu_mmc_image_montada;
extern z80_bit menu_current_drive_mmc_image;
extern z80_byte *mmc_memory_pointer[];
extern long long int mmc_size[];
extern z80_bit mmc_file_inserted_hdf[];
static int copy_to_sd(const char *program)
{
	if (!mmc_memory_pointer[0] || mmc_file_inserted_hdf[0].v) {
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.sd_not_raw");
		return 0;
	}
	snprintf(fatfs_disk_zero_path, PATH_MAX, "%s", mmc_file_name[0]);
	if (f_mount(&FatFs_util_copy_files_to_mmc, "", 1) != FR_OK) {
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.sd_not_fat");
		return 0;
	}
	menu_mmc_image_montada = 1;
	static char source[512], destination[600];
	snprintf(source, sizeof source, "%s", program);
	snprintf(destination, sizeof destination, "0://%s", program);
	chimera_last_error[0] = '\0';
	util_copy_file(source, destination);
	const int copied = !chimera_last_error[0] && fatfs_disk_zero_tamanyo == mmc_size[0];
	if (copied) memcpy(mmc_memory_pointer[0], fatfs_disk_zero_memory, (size_t)mmc_size[0]);
	menu_mmc_image_montada = 0;
	menu_current_drive_mmc_image.v = 0;
	f_mount(0, "", 0);
	free(fatfs_disk_zero_memory);
	fatfs_disk_zero_memory = NULL;
	if (!copied && !chimera_last_error[0])
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.sd_size");
	return copied;
}

/* What the New Project window is told before the machine is built (called
 * instead of Init, with the project's files mounted): with an SD card in the
 * project the Next starts with NextZXOS from it; with a program and no card
 * it starts fast, since NextZXOS has no card to start from. Nothing runs. */
ECL_EXPORT const char *SuggestSettings(void)
{
	static char answer[256];
	char sd[512], program[512];
	slot_file("sd", sd, sizeof sd);
	slot_file("program", program, sizeof program);
	if (sd[0])
		snprintf(answer, sizeof answer, "{\"values\":{\"boot\":\"nextzxos\"},\"note\":\"suggest.nextzxos\"}");
	else if (program[0])
		snprintf(answer, sizeof answer, "{\"values\":{\"boot\":\"fast\"},\"note\":\"suggest.fast\"}");
	else
		snprintf(answer, sizeof answer, "{\"values\":{},\"note\":\"\"}");
	return answer;
}

ECL_EXPORT int Init(void)
{
	g_loadError[0] = '\0';

	static char boot[16], sd[512], program[512];
	setting("boot", "nextzxos", boot, sizeof boot);
	const int fast = !strcmp(boot, "fast");
	if (!fast && strcmp(boot, "nextzxos")) {
		snprintf(g_loadError, sizeof g_loadError, "error.bad_boot" TEXT_ARG "%s", boot);
		return 0;
	}
	slot_file("sd", sd, sizeof sd);
	slot_file("program", program, sizeof program);
	if (!fast && !sd[0]) {
		snprintf(g_loadError, sizeof g_loadError, "error.no_sd");
		return 0;
	}
	/* The machine's calendar clock starts where the project says: the
	 * sandbox's clock is that second, and stands still (miniBox
	 * MACHINE-SPEC, clock_gettime). ZEsarUX's own clock is that second plus
	 * the frames run (chimera_compat.c). */
	struct timespec ts = { 0, 0 };
	clock_gettime(CLOCK_REALTIME, &ts);
	chimera_clock_start = (int64_t)ts.tv_sec;
	chimera_clock_frames = 0;
	chimera_clock_fps_num = 50;
	chimera_clock_fps_den = 1;

	/* The settings, as ZEsarUX's command line. What is not said here is
	 * ZEsarUX's own default. Said because a core must:
	 *  --noconfigfile        no settings file of anybody's (first, or it
	 *                        is not read);
	 *  --vo null --ao null   the core's display and sound card, which take
	 *                        the place of the null drivers;
	 *  --realvideo           the display as the machine makes it, line by
	 *                        line (the Next's layers, copper and sprites
	 *                        need it);
	 *  --zoom 1, no frameskip, no footer, no welcome, no wizard, no first
	 *                        aid: nobody is there to look at ZEsarUX's own
	 *                        interface, and every frame is drawn;
	 *  --mmc-no-persistent-writes   what the machine writes to its SD card
	 *                        stays in its memory (and in its states), and
	 *                        never reaches the file;
	 *  --nex-no-automount-esxdos    a .nex file is loaded into memory only;
	 *                        no folder of the host is offered to it.
	 */
	static char *argv[64];
	int argc = 0;
	argv[argc++] = (char *)"zesarux";
	argv[argc++] = (char *)"--noconfigfile";
	argv[argc++] = (char *)"--machine";
	argv[argc++] = (char *)"TBBlue";
	argv[argc++] = (char *)"--vo";
	argv[argc++] = (char *)"null";
	argv[argc++] = (char *)"--ao";
	argv[argc++] = (char *)"null";
	argv[argc++] = (char *)"--verbose";
	argv[argc++] = (char *)"1";
	argv[argc++] = (char *)"--realvideo";
	argv[argc++] = (char *)"--zoom";
	argv[argc++] = (char *)"1";
	argv[argc++] = (char *)"--frameskip";
	argv[argc++] = (char *)"0";
	argv[argc++] = (char *)"--disable-autoframeskip";
	argv[argc++] = (char *)"--disablefooter";
	argv[argc++] = (char *)"--nowelcomemessage";
	argv[argc++] = (char *)"--nosplash";
	argv[argc++] = (char *)"--disable-first-start-wizard";
	argv[argc++] = (char *)"--disable-all-first-aid";
	argv[argc++] = (char *)"--no-saveconf-on-exit";
	argv[argc++] = (char *)"--quickexit";
	argv[argc++] = (char *)"--tbblue-autoconfigure-sd-already-asked";
	argv[argc++] = (char *)"--nex-no-automount-esxdos";
	/* NextZXOS sets the processor's speed itself (up to 28 MHz). ZEsarUX
	 * holds it at 7 MHz inside the ROM by default, to spare the host; a
	 * Next does not, and the setting says which. */
	argv[argc++] = (char *)(on("limit_rom_turbo", 0) ? "--denyturbotbbluerom" : "--allowturbotbbluerom");
	if (fast) {
		argv[argc++] = (char *)"--tbblue-fast-boot-mode";
	} else {
		argv[argc++] = (char *)"--mmc-file";
		argv[argc++] = sd;
		argv[argc++] = (char *)"--enable-mmc";
		argv[argc++] = (char *)"--enable-divmmc-ports";
		argv[argc++] = (char *)"--mmc-no-persistent-writes";
	}
	/* With NextZXOS the program goes on the SD card (below), where the
	 * Browser finds it, as on a Next; ZEsarUX loads it straight into the
	 * machine only in fast boot. */
	if (program[0] && fast) argv[argc++] = program;
	argv[argc] = NULL;

	chimera_last_error[0] = '\0';
	if (zesarux_main(argc, argv) != 0) {
		snprintf(g_loadError, sizeof g_loadError, "error.not_started" TEXT_ARG "%s",
			chimera_last_error[0] ? chimera_last_error : "error.no_reason");
		return 0;
	}
	if (chimera_last_error[0]) {
		/* ZEsarUX starts the machine even when a file could not be used */
		snprintf(g_loadError, sizeof g_loadError, "error.not_started" TEXT_ARG "%s", chimera_last_error);
		return 0;
	}
	if (program[0] && !fast && !copy_to_sd(program)) {
		snprintf(g_loadError, sizeof g_loadError, "error.not_on_sd" TEXT_ARG "%s",
			chimera_last_error[0] ? chimera_last_error : "error.no_reason");
		return 0;
	}
	chimera_video_update_size();
	g_inited = 1;
	return 1;
}

ECL_EXPORT void SetButton(int32_t index, int32_t state)
{
	if (index >= 0 && index < BTN_COUNT) g_setButtons[index] = state ? 1 : 0;
}

ECL_EXPORT void SetAxis(int32_t index, int32_t value) { (void)index; (void)value; }

ECL_EXPORT void FrameAdvance(uint64_t packed)
{
	uint8_t now[BTN_COUNT];
	for (int i = 0; i < BTN_COUNT; i++)
		now[i] = g_setButtons[i] | (uint8_t)((packed >> i) & 1);

	if (now[BTN_HARD_RESET] && !g_held[BTN_HARD_RESET]) hard_reset_cpu();
	else if (now[BTN_RESET] && !g_held[BTN_RESET]) reset_cpu();
	/* levels, not events: the keyboard and the joystick are what is held
	 * in this frame */
	puerto_65278 = puerto_65022 = puerto_64510 = puerto_63486 = 255;
	puerto_61438 = puerto_57342 = puerto_49150 = puerto_32766 = 255;
	for (int i = 0; i < KEY_COUNT; i++)
		if (now[i]) *kKeys[i].row &= (z80_byte)~kKeys[i].bit;
	z80_byte joy = 0;
	for (int b = 0; b < 6; b++)
		if (now[BTN_JOY + b]) joy |= kJoyBits[b];
	puerto_especial_joystick = joy;
	memcpy(g_held, now, sizeof g_held);

	g_inputRead = 0;
	chimera_audio_pairs = 0;
	/* ZEsarUX's own loop (start.c emulator_main_loop), until the machine
	 * has run its frame (a reset in the middle of one does not end it) */
	g_frameDone = 0;
	while (!g_frameDone)
		cpu_core_loop();
	g_frames++;
	chimera_clock_frames = g_frames;
}

ECL_EXPORT uint32_t *GetVideoBgra(void) { return chimera_video; }
ECL_EXPORT int GetVideoWidth(void) { return chimera_video_w; }
ECL_EXPORT int GetVideoHeight(void) { return chimera_video_h; }

ECL_EXPORT int16_t *GetAudio(void) { return chimera_audio; }
ECL_EXPORT int GetAudioSampleCount(void) { return chimera_audio_pairs; }

ECL_EXPORT int GetVsyncNumerator(void) { return 50; }
ECL_EXPORT int GetVsyncDenominator(void) { return 1; }

ECL_EXPORT int InputWasRead(void) { return g_inputRead; }

/* The machine's memory for RAM tools. "RAM" is the Next's memory as its
 * own numbering has it: 16 KiB bank 0 (8 KiB pages 0 and 1) at the start,
 * up to bank 111 (page 223), 1.75 MiB. Below it in ZEsarUX's block are the
 * ROMs and the divMMC's memory, which RAM tools leave alone. */
#define NEXT_RAM_OFFSET 0x40000
#define NEXT_RAM_SIZE (TBBLUE_TOTAL_RAM - NEXT_RAM_OFFSET)
ECL_EXPORT int GetMemoryDomainCount(void) { return g_inited ? 1 : 0; }
ECL_EXPORT const char *GetMemoryDomainName(int i) { return i == 0 ? "RAM" : ""; }
ECL_EXPORT uint8_t *GetMemoryDomainPtr(int i) { return i == 0 && g_inited ? memoria_spectrum + NEXT_RAM_OFFSET : NULL; }
ECL_EXPORT int64_t GetMemoryDomainSize(int i) { return i == 0 && g_inited ? NEXT_RAM_SIZE : 0; }
ECL_EXPORT int GetMemoryDomainWritable(int i) { return i == 0 ? 1 : 0; }

/* The machine's memory as its processor sees it: 64 KiB, with whichever
 * pages are mapped in. This is the map a program's own addresses are in. */
ECL_EXPORT int32_t GetBusCount(void) { return g_inited ? 1 : 0; }
ECL_EXPORT const char *GetBusName(int32_t i) { return i == 0 ? "System Bus" : ""; }
ECL_EXPORT int64_t GetBusSize(int32_t i) { return i == 0 ? 0x10000 : 0; }
ECL_EXPORT int32_t GetBusWritable(int32_t i) { return i == 0 ? 1 : 0; }
ECL_EXPORT int32_t PeekBus(int32_t bus, int32_t addr)
{
	if (bus != 0 || !g_inited) return 0;
	return peek_byte_no_time((z80_int)addr);
}
ECL_EXPORT void PokeBus(int32_t bus, int32_t addr, int32_t value)
{
	if (bus != 0 || !g_inited) return;
	poke_byte_no_time((z80_int)addr, (z80_byte)value);
}
ECL_EXPORT const uint8_t *ReadBus(int32_t bus, int64_t addr, int32_t len)
{
	/* an answer to a tool is in no savestate */
	ECL_INVISIBLE static uint8_t buffer[0x10000];
	if (bus != 0 || !g_inited || len < 0 || len > (int32_t)sizeof buffer) return NULL;
	for (int32_t i = 0; i < len; i++)
		buffer[i] = peek_byte_no_time((z80_int)(addr + i));
	return buffer;
}

/* For the tests: how far the machine has run, and where its processor is. */
ECL_EXPORT uint64_t GetFrameCount(void) { return g_frames; }
ECL_EXPORT uint32_t GetTstates(void) { return (uint32_t)t_estados; }
ECL_EXPORT uint32_t DebugPc(void) { return reg_pc; }
/* the whole of ZEsarUX's memory block of the machine (ROMs, divMMC, RAM) */
ECL_EXPORT uint8_t *DebugMemoryPtr(void) { return memoria_spectrum; }
ECL_EXPORT int64_t DebugMemorySize(void) { return g_inited ? TBBLUE_TOTAL_RAM : 0; }
