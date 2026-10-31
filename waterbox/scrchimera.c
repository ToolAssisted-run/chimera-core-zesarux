/* scrchimera.c - the core's display, in place of ZEsarUX's "null" video
 * driver.
 *
 * ZEsarUX draws through a video driver: the emulator works out each pixel
 * of the emulated display and hands it to the driver's putpixel functions.
 * Its "null" driver throws them away. This file is compiled INSTEAD of
 * video/scrnull.c (meson.build), with the same function names, so ZEsarUX
 * still believes it runs on the null driver ("--vo null"), and keeps the
 * picture: every pixel goes into chimera_video, which the core hands to
 * Chimera. Nothing of ZEsarUX's own interface (its menus, its footer, the
 * ZX Desktop) is drawn: only the machine's display, with its border.
 *
 * The driver's name stays "null", which ZEsarUX reads to mean that no
 * window of its own can open: an error is a message, never a menu.
 */
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "debug.h"
#include "scrnull.h"
#include "screen.h"
#include "settings.h"
#include "tbblue.h"
#include "tsconf.h"

#include "chimera_glue.h"

uint32_t chimera_video[CHIMERA_VIDEO_MAX_W * CHIMERA_VIDEO_MAX_H];
int chimera_video_w = 1, chimera_video_h = 1;
char chimera_last_error[512];

void chimera_video_update_size(void)
{
	int w = screen_get_emulated_display_width_zoom_border_en();
	int h = screen_get_emulated_display_height_zoom_border_en();
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	if (w > CHIMERA_VIDEO_MAX_W) w = CHIMERA_VIDEO_MAX_W;
	if (h > CHIMERA_VIDEO_MAX_H) h = CHIMERA_VIDEO_MAX_H;
	chimera_video_w = w;
	chimera_video_h = h;
}

static void scrchimera_putpixel_final_rgb(int x, int y, unsigned int color_rgb)
{
	/* the display's pixels only: what is beyond it (ZEsarUX's footer and
	 * desktop) is not the machine's */
	if (x < 0 || y < 0 || x >= chimera_video_w || y >= chimera_video_h) return;
	chimera_video[y * chimera_video_w + x] = 0xff000000u | (color_rgb & 0xffffffu);
}

static void scrchimera_putpixel_final(int x, int y, unsigned int color)
{
	scrchimera_putpixel_final_rgb(x, y, spectrum_colortable[color]);
}

static void scrchimera_putchar_menu(int x, int y, z80_byte caracter, int tinta, int papel)
{
	(void)x; (void)y; (void)caracter; (void)tinta; (void)papel;
}

static void scrchimera_putchar_footer(int x, int y, z80_byte caracter, int tinta, int papel)
{
	(void)x; (void)y; (void)caracter; (void)tinta; (void)papel;
}

static void scrchimera_set_fullscreen(void) {}
static void scrchimera_reset_fullscreen(void) {}
static void scrchimera_detectedchar_print(z80_byte caracter) { (void)caracter; }
static void scrchimera_z88_cpc_load_keymap(void) {}
static int scrchimera_get_menu_width(void) { return 32 / screen_reduce_menu_ancho; }
static int scrchimera_get_menu_height(void) { return 24 / screen_reduce_menu_alto; }

int scrnull_init(void)
{
	scr_debug_registers = scrnull_debug_registers;
	scr_messages_debug = scrnull_messages_debug;
	scr_putchar_menu = scrchimera_putchar_menu;
	scr_putchar_footer = scrchimera_putchar_footer;
	/* ZEsarUX's menus are never open, so a pixel of the machine is a pixel
	 * of the picture: no layers to mix */
	scr_putpixel = scrchimera_putpixel_final;
	scr_putpixel_final = scrchimera_putpixel_final;
	scr_putpixel_final_rgb = scrchimera_putpixel_final_rgb;
	scr_tiene_colores = 1;
	scr_get_menu_width = scrchimera_get_menu_width;
	scr_get_menu_height = scrchimera_get_menu_height;
	scr_set_driver_name("null");
	scr_set_fullscreen = scrchimera_set_fullscreen;
	scr_reset_fullscreen = scrchimera_reset_fullscreen;
	scr_z88_cpc_load_keymap = scrchimera_z88_cpc_load_keymap;
	scr_detectedchar_print = scrchimera_detectedchar_print;
	scr_update_window_title = NULL;
	scr_get_text_clipboard = NULL;
	chimera_video_update_size();
	return 0;
}

void scrnull_end(void) {}

void scrnull_refresca_pantalla_solo_driver(void) {}

/* The machine's display drawn into chimera_video, as ZEsarUX's own drivers
 * draw it (scrfbdev.c), less its menus and footer. Called by ZEsarUX at the
 * end of each of the machine's frames. */
void scrnull_refresca_pantalla(void)
{
	if (sem_screen_refresh_reallocate_layers) return;
	sem_screen_refresh_reallocate_layers = 1;
	chimera_video_update_size();

	if (MACHINE_IS_PRISM) {
		screen_prism_refresca_pantalla();
	} else if (MACHINE_IS_TBBLUE) {
		screen_tbblue_refresca_pantalla();
	} else if (MACHINE_IS_SPECTRUM) {
		if (MACHINE_IS_TSCONF) screen_tsconf_refresca_pantalla();
		else if (rainbow_enabled.v) scr_refresca_pantalla_rainbow_comun_spectrum();
		else scr_refresca_pantalla_comun();
	}

	sem_screen_refresh_reallocate_layers = 0;
}

z80_byte scrnull_lee_puerto(z80_byte puerto_h, z80_byte puerto_l)
{
	(void)puerto_h; (void)puerto_l;
	return 255;
}

/* the keyboard is set by the core before each frame (cinterface.c) */
void scrnull_actualiza_tablas_teclado(void) {}

void scrnull_debug_registers(void) {}

/* ZEsarUX's messages go to standard error, for whoever runs the core (an
 * error is also kept as the reason a start failed: chimera_compat.c) */
void scrnull_messages_debug(char *s)
{
	fprintf(stderr, "zesarux: %s\n", s);
}
