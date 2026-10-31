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
static int g_cardBoot; /* Start "card": the core does the firmware's work */

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

/* The SD card the machine has in its memory, lent to the FAT reader
 * ZEsarUX uses for its own --copy-file-to-mmc (FatFs, drive 0). Patch 0004
 * lets the card be lent instead of FatFs reading a second copy of the file
 * into memory, so what FatFs writes lands in the machine's card; and it
 * reads a card whose boot sector says FAT32 as FAT32, as NextZXOS does
 * (the System/Next cards for emulators have too few clusters for FatFs's
 * own rule). The project's file is never written. */
extern char fatfs_disk_zero_path[];
extern BYTE *fatfs_disk_zero_memory;
extern long long int fatfs_disk_zero_tamanyo;
extern BYTE *chimera_fatfs_borrowed_card;
extern long long int chimera_fatfs_borrowed_size;
extern FATFS FatFs_util_copy_files_to_mmc;
extern int menu_mmc_image_montada;
extern z80_bit menu_current_drive_mmc_image;
extern z80_byte *mmc_memory_pointer[];
extern long long int mmc_size[];
extern z80_bit mmc_file_inserted_hdf[];
static int card_lend(void)
{
	if (!mmc_memory_pointer[0] || mmc_file_inserted_hdf[0].v) {
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.sd_not_raw");
		return 0;
	}
	snprintf(fatfs_disk_zero_path, PATH_MAX, "%s", mmc_file_name[0]);
	chimera_fatfs_borrowed_card = mmc_memory_pointer[0];
	chimera_fatfs_borrowed_size = mmc_size[0];
	if (f_mount(&FatFs_util_copy_files_to_mmc, "", 1) != FR_OK) {
		chimera_fatfs_borrowed_card = NULL;
		fatfs_disk_zero_memory = NULL;
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.sd_not_fat");
		return 0;
	}
	return 1;
}
static void card_return(void)
{
	f_mount(0, "", 0);
	chimera_fatfs_borrowed_card = NULL;
	fatfs_disk_zero_memory = NULL; /* the machine's, not FatFs's to free */
}

/* The program is copied into the root folder of the card (util_copy_file,
 * ZEsarUX's own). A file of that name already on the card is replaced. */
static int copy_to_sd(const char *program)
{
	if (!card_lend()) return 0;
	menu_mmc_image_montada = 1;
	static char source[512], destination[600];
	snprintf(source, sizeof source, "%s", program);
	snprintf(destination, sizeof destination, "0://%s", program);
	chimera_last_error[0] = '\0';
	util_copy_file(source, destination);
	const int copied = !chimera_last_error[0];
	menu_mmc_image_montada = 0;
	menu_current_drive_mmc_image.v = 0;
	card_return();
	return copied;
}

/* ---- Start "card": NextZXOS from the card, the firmware's work done here ----
 * The Next's firmware (TBBLUE.FW on the card, run by the boot ROM) reads
 * /machines/next/config.ini and menu.ini (or menu.def), loads the keymap
 * and the ROMs the chosen menu line names into the Next's memory, sets the
 * Next's peripheral registers, sets the machine type and soft-resets into
 * the ROM. This does the same, from the card in the machine's memory, in
 * the order the firmware does it (its source: the card's
 * /src/firmware/app/src/boot.c and config.c, firmware 1.44), as CSpect
 * starts these cards. The firmware itself cannot: it reads its card with
 * FatFs R0.12b, which takes a FAT32 volume of fewer than 65525 clusters
 * for FAT16 and refuses it ("Error opening 'menu.ini/.def'!"). */
static int card_read(const char *name, uint8_t *out, unsigned int size, unsigned int *got)
{
	static char path[128];
	snprintf(path, sizeof path, "0:/machines/next/%s", name);
	FIL f;
	if (f_open(&f, path, FA_READ) != FR_OK) return 0;
	UINT n = 0;
	const FRESULT r = f_read(&f, out, size, &n);
	f_close(&f);
	*got = n;
	return r == FR_OK;
}

/* the firmware's settings, its names and defaults (config.c) */
enum { S_SCANDOUBLER, S_5060, S_TIMEX, S_PSGMODE, S_INTSND, S_STEREO, S_TURBOSOUND,
	S_DIVMMC, S_MF, S_JOY1, S_JOY2, S_PS2, S_DMA, S_SCANLINES, S_TURBOKEY, S_DEFAULT,
	S_TIMING, S_ISS23, S_DIVPORTS, S_DAC, S_AY48, S_UARTI2C, S_KMOUSE, S_ULAPLUS,
	S_HDMISOUND, S_BEEPMODE, S_BTNSWAP, S_MOUSEDPI, S_ESPRESET, S_COUNT };
static const char *const kFwNames[S_COUNT] = { "scandoubler", "50_60hz", "timex", "psgmode",
	"intsnd", "stereomode", "turbosound", "divmmc", "mf", "joystick1", "joystick2", "ps2", "dma",
	"scanlines", "turbokey", "default", "timing", "keyb_issue", "divports", "dac", "ay48",
	"uart_i2c", "kmouse", "ulaplus", "hdmisound", "beepmode", "buttonswap", "mousedpi", "espreset" };
static const uint8_t kFwDefaults[S_COUNT] = { 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 3, 1, 0, 0, 1, 0, 8,
	0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0 };

/* the next comma-separated field of a menu line, spaces kept */
static const char *field_next(const char *p, char *out, int size)
{
	int n = 0;
	while (*p && *p != ',' && *p != '\r' && *p != '\n') {
		if (n < size - 1) out[n++] = *p;
		p++;
	}
	out[n] = '\0';
	return *p == ',' ? p + 1 : p;
}

static void nextreg(z80_byte reg, z80_byte value) { tbblue_set_value_port_position(reg, value); }

static int card_boot(void)
{
	static uint8_t buf[0x10000];
	unsigned int got = 0;
	if (!card_lend()) return 0;
	int ok = 0;
	uint8_t st[S_COUNT];
	memcpy(st, kFwDefaults, sizeof st);

	/* the firmware starts at 28 MHz, in config mode */
	tbblue_registers[3] = 0;
	nextreg(7, 3);
	nextreg(6, 0xa0);

	/* config.ini: name=value lines, each value no more than the firmware
	 * allows is not checked here; the card for emulators has none */
	if (card_read("config.ini", buf, sizeof buf - 1, &got)) {
		buf[got] = 0;
		for (char *line = (char *)buf; line && *line; ) {
			char *end = strchr(line, '\n');
			if (end) *end = 0;
			for (int i = 0; i < S_COUNT; i++) {
				const size_t len = strlen(kFwNames[i]);
				if (!strncmp(line, kFwNames[i], len) && line[len] == '=') {
					st[i] = (uint8_t)atoi(line + len + 1);
					break;
				}
			}
			line = end ? end + 1 : NULL;
		}
	}

	/* the menu: menu.ini, else menu.def; the line config.ini's "default"
	 * names (the first when it names none) */
	char title[64] = "", mode_s[8] = "", timing_s[8] = "", rom[64] = "", divrom[64] = "", mfrom[64] = "";
	got = 0;
	if (!card_read("menu.ini", buf, sizeof buf - 1, &got) && !card_read("menu.def", buf, sizeof buf - 1, &got)) {
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.card_no_menu");
		goto out;
	}
	buf[got] = 0;
	{
		int count = 0, found = 0;
		for (const char *line = (const char *)buf; *line; ) {
			if (!strncmp(line, "menu=", 5)) {
				char t[64], m[8], v[8], r[64], d[64], f[64];
				const char *p = line + 5;
				p = field_next(p, t, sizeof t);
				p = field_next(p, m, sizeof m);
				p = field_next(p, v, sizeof v);
				p = field_next(p, r, sizeof r);
				p = field_next(p, d, sizeof d);
				p = field_next(p, f, sizeof f);
				if (r[0]) {
					if (count <= st[S_DEFAULT] || !found) {
						snprintf(title, sizeof title, "%s", t);
						snprintf(mode_s, sizeof mode_s, "%s", m);
						snprintf(timing_s, sizeof timing_s, "%s", v);
						snprintf(rom, sizeof rom, "%s", r);
						snprintf(divrom, sizeof divrom, "%s", d);
						snprintf(mfrom, sizeof mfrom, "%s", f);
						found = 1;
					}
					count++;
				}
			}
			const char *nl = strchr(line, '\n');
			line = nl ? nl + 1 : line + strlen(line);
		}
		if (!found) {
			snprintf(chimera_last_error, sizeof chimera_last_error, "error.card_no_menu");
			goto out;
		}
	}
	const int mode = atoi(mode_s);
	(void)timing_s;
	(void)title;

	/* a ROM a menu line names turns its device on; <none> turns it off */
	if (mfrom[0]) {
		if (!strncmp(mfrom, "<none>", 6)) { st[S_MF] = 0; mfrom[0] = 0; }
		else st[S_MF] = 1;
	}
	if (divrom[0]) {
		if (!strncmp(divrom, "<none>", 6)) { st[S_DIVMMC] = 0; divrom[0] = 0; }
		else st[S_DIVMMC] = 1;
	}

	/* the keymap, into the Next's keymap registers */
	if (card_read("keymap.bin", buf, 1024, &got) && got == 1024) {
		nextreg(0x28, 0);
		nextreg(0x29, 0);
		for (int i = 0; i < 1024; i += 2) {
			nextreg(0x2a, buf[i]);
			nextreg(0x2b, buf[i + 1]);
		}
	}
	/* the keys the two joysticks' extra buttons are (boot.c keyjoy_default) */
	static const z80_byte keyjoy[11] = { 0x28, 0x29, 0x08, 0x10, 0x38, 0x3a, 0x30, 0x09, 0x2c, 0x01, 0x02 };
	for (int j = 0; j < 2; j++) {
		nextreg(0x28, 0x80);
		nextreg(0x29, (z80_byte)(j << 4));
		for (int k = 0; k < 11; k++) nextreg(0x2b, keyjoy[k]);
	}

	/* the ROMs, into the Next's memory: divMMC's at 0x10000, the
	 * Multiface's at 0x14000, the Spectrum's (1, 2 or 4 pages of 16 KiB by
	 * the machine) from 0 */
	z80_byte mftype = 0;
	const char *mf = NULL;
	if (st[S_MF]) {
		switch (mode) {
		case 0: mftype = 3; mf = "mf1.rom"; break;
		case 2: mftype = 0; mf = "mf3.rom"; break;
		default: mftype = 1; mf = "mf128.rom"; break;
		}
	}
	if (mfrom[0]) {
		mf = mfrom;
		if (!strncmp(mf, "mf128v1.rom", 11) || !strncmp(mf, "mf128v12.rom", 12)) mftype = 2;
	}
	const char *div = st[S_DIVMMC] ? "esxmmc.bin" : NULL;
	if (divrom[0]) div = divrom;
	if (div) {
		if (!card_read(div, memoria_spectrum + 0x10000, 8192, &got) || got != 8192) {
			snprintf(chimera_last_error, sizeof chimera_last_error, "error.card_no_rom" TEXT_ARG "%s", div);
			goto out;
		}
	}
	if (mf) {
		if (!card_read(mf, memoria_spectrum + 0x14000, 8192, &got) || got != 8192) {
			snprintf(chimera_last_error, sizeof chimera_last_error, "error.card_no_rom" TEXT_ARG "%s", mf);
			goto out;
		}
	}
	const unsigned int rom_size = (mode == 0 ? 1 : mode == 2 ? 4 : 2) * 16384u;
	if (!card_read(rom, memoria_spectrum, rom_size, &got) || got != rom_size) {
		snprintf(chimera_last_error, sizeof chimera_last_error, "error.card_no_rom" TEXT_ARG "%s", rom);
		goto out;
	}

	/* the peripheral registers (boot.c init_registers) */
	z80_byte v = (z80_byte)(((st[S_JOY1] & 3) << 6) | ((st[S_JOY2] & 3) << 4));
	if (st[S_JOY1] & 4) v |= 0x08;
	if (st[S_5060]) v |= 0x04;
	if (st[S_JOY2] & 4) v |= 0x02;
	if (st[S_SCANDOUBLER]) v |= 0x01;
	nextreg(5, v);
	v = (z80_byte)(st[S_PSGMODE] & 3);
	if (st[S_TURBOKEY]) v |= 0x80;
	if (st[S_BEEPMODE]) v |= 0x40;
	if (st[S_DIVMMC]) v |= 0x10;
	if (st[S_MF]) v |= 0x08;
	if (st[S_PS2]) v |= 0x04;
	nextreg(6, v);
	v = 0;
	if (st[S_STEREO]) v |= 0x20;
	if (st[S_INTSND]) v |= 0x10;
	if (st[S_DAC]) v |= 0x08;
	if (st[S_TIMEX]) v |= 0x04;
	if (st[S_TURBOSOUND]) v |= 0x02;
	if (st[S_ISS23]) v |= 0x01;
	nextreg(8, v);
	v = (z80_byte)(st[S_SCANLINES] & 3);
	if (!st[S_HDMISOUND]) v |= 0x04;
	nextreg(9, v);
	v = (z80_byte)(st[S_MOUSEDPI] & 3);
	if (st[S_BTNSWAP]) v |= 0x08;
	if (st[S_DIVMMC]) v |= 0x10;
	v |= (z80_byte)(mftype << 6);
	nextreg(10, v);
	z80_byte en[4];
	en[3] = (z80_byte)((st[S_ULAPLUS] ? 0x01 : 0) | (st[S_DMA] ? 0x02 : 0));
	en[2] = st[S_DAC] ? 0xff : 0x01;
	en[1] = (z80_byte)((st[S_DIVPORTS] ? 0x09 : 0) | (st[S_MF] ? 0x02 : 0) | (st[S_UARTI2C] ? 0x14 : 0) | (st[S_KMOUSE] ? 0x20 : 0));
	switch (mode) {
	case 0: en[0] = 0xc0; if (!st[S_AY48]) en[2] &= 0xfe; break;
	case 1: en[0] = 0xc2; break;
	case 2: en[0] = 0xda; break;
	default: en[0] = 0xc6; en[2] &= 0xfb; break;
	}
	for (int i = 0; i < 4; i++) nextreg((z80_byte)(0x82 + i), en[i]);

	/* the machine type, then a soft reset into the ROM */
	nextreg(3, (z80_byte)(0x80 | ((mode + 1) << 4) | (mode + 1)));
	nextreg(2, st[S_ESPRESET] ? 0x81 : 0x01);
	ok = 1;
out:
	card_return();
	return ok;
}

/* The biggest SD card image the machine can hold: ZEsarUX keeps the whole
 * card in the machine's memory, and the memory the package declares
 * (waterbox.config, memoryLayoutMiB: 1280 MiB of mmap) has room for a card
 * of 1 GiB beside ZEsarUX's own tables. */
#define CARD_MAX_BYTES (1024LL << 20)

/* the size of a mounted file, or -1 */
static long long file_size(const char *name)
{
	FILE *f = fopen(name, "rb");
	if (!f) return -1;
	long long n = -1;
	if (fseek(f, 0, SEEK_END) == 0) n = (long long)ftell(f);
	fclose(f);
	return n;
}

/* Whether the Next's firmware cannot read this card: the first partition's
 * boot sector is laid out as FAT32 but the volume has too few clusters for
 * FAT32 by the rule the firmware's FatFs goes by (65525 or more), as on the
 * System/Next cards made for emulators. Such a card starts with "card". */
static int firmware_cannot_read(const char *name)
{
	FILE *f = fopen(name, "rb");
	if (!f) return 0;
	unsigned char b[512];
	int answer = 0;
	if (fread(b, 1, 512, f) == 512 && b[510] == 0x55 && b[511] == 0xAA) {
		const unsigned long start = b[454] | (b[455] << 8) | ((unsigned long)b[456] << 16) | ((unsigned long)b[457] << 24);
		if (fseek(f, (long)(start * 512), SEEK_SET) == 0 && fread(b, 1, 512, f) == 512) {
			const unsigned bps = b[11] | (b[12] << 8), spc = b[13], rsv = b[14] | (b[15] << 8), nfats = b[16];
			const unsigned rootent = b[17] | (b[18] << 8), fat16 = b[22] | (b[23] << 8);
			unsigned long total = b[19] | (b[20] << 8);
			if (!total) total = b[32] | (b[33] << 8) | ((unsigned long)b[34] << 16) | ((unsigned long)b[35] << 24);
			const unsigned long fat32 = b[36] | (b[37] << 8) | ((unsigned long)b[38] << 16) | ((unsigned long)b[39] << 24);
			if (bps == 512 && spc && fat16 == 0 && rootent == 0 && total > rsv + nfats * fat32) {
				const unsigned long clusters = (total - rsv - nfats * fat32) / spc;
				answer = clusters <= 0xFFF5;
			}
		}
	}
	fclose(f);
	return answer;
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
	if (sd[0] && firmware_cannot_read(sd))
		snprintf(answer, sizeof answer, "{\"values\":{\"boot\":\"card\"},\"note\":\"suggest.card\"}");
	else if (sd[0])
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
	g_cardBoot = !strcmp(boot, "card");
	if (!fast && !g_cardBoot && strcmp(boot, "nextzxos")) {
		snprintf(g_loadError, sizeof g_loadError, "error.bad_boot" TEXT_ARG "%s", boot);
		return 0;
	}
	slot_file("sd", sd, sizeof sd);
	slot_file("program", program, sizeof program);
	if (!fast && !sd[0]) {
		snprintf(g_loadError, sizeof g_loadError, "error.no_sd");
		return 0;
	}
	if (!fast && file_size(sd) > CARD_MAX_BYTES) {
		snprintf(g_loadError, sizeof g_loadError, "error.sd_too_big" TEXT_ARG "%lld" TEXT_ARG "%lld",
			file_size(sd) >> 20, CARD_MAX_BYTES >> 20);
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
	/* "card" starts as "fast" does, with no boot ROM, and with the card;
	 * card_boot then does the firmware's work before the first frame */
	if (fast || g_cardBoot)
		argv[argc++] = (char *)"--tbblue-fast-boot-mode";
	if (!fast) {
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
	if (g_cardBoot) {
		chimera_last_error[0] = '\0';
		if (!card_boot()) {
			snprintf(g_loadError, sizeof g_loadError, "error.card_not_started" TEXT_ARG "%s",
				chimera_last_error[0] ? chimera_last_error : "error.no_reason");
			return 0;
		}
		tbblue_fast_boot_mode.v = 0;
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

	if (now[BTN_HARD_RESET] && !g_held[BTN_HARD_RESET]) {
		if (g_cardBoot) {
			/* no boot ROM to start again: the machine is reset as the
			 * fast start resets it, and the firmware's work done again */
			tbblue_fast_boot_mode.v = 1;
			hard_reset_cpu();
			tbblue_fast_boot_mode.v = 0;
			card_boot();
		} else {
			hard_reset_cpu();
		}
	}
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
