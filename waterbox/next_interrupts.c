/* next_interrupts.c - the Next's CTC (counter/timer) and its interrupt
 * controller's "hardware IM2 mode", which ZEsarUX 13.0 does not have.
 *
 * What the Next's documentation says, in the System/Next distribution's
 * docs/EXTRA-HW/io-port-system (registers.txt and PORTS.TXT):
 *
 *  - The CTC is "a standard Zilog part" with channels on ports 0x183B
 *    upwards, "temporarily reduced to four channels from eight": 0x183B,
 *    0x193B, 0x1A3B and 0x1B3B here. Each channel's zero count (ZC/TO) is
 *    fed into the next channel's CLK/TRG input. A control word with D2 = 0
 *    leaves a channel in its hard reset state; a soft reset (D1 = 1) with
 *    D2 = 0 puts it there, with D2 = 1 it waits for a time constant and
 *    then runs.
 *  - Next register 0xC0 bit 0 chooses how the maskable interrupt is made:
 *    a pulse, as the Spectrum's ULA makes it (ZEsarUX does this already),
 *    or "hw im2 mode", in which the interrupt controller hands the
 *    processor a vector of its own: bits 7:5 from 0xC0, bits 4:1 the
 *    device (0 the line interrupt, highest priority; 3 to 10 the CTC
 *    channels; 11 the ULA), bit 0 zero.
 *  - 0xC4, 0xC5 and 0xC6 enable each device's interrupt (0xC4's line and
 *    ULA bits are the same bits as register 0x22's); 0xC8, 0xC9 and 0xCA
 *    say which devices made an interrupt (written with a 1, a bit is
 *    cleared; in hw im2 mode it reads as set while that interrupt is still
 *    pending).
 *
 * The CTC runs on the Next's 28 MHz clock: eight clock ticks for each of
 * the processor's T-states at 3.5 MHz, whatever speed the processor runs
 * at (TX-1696 sets a channel to 28 MHz / 16 / 112 = 15625 interrupts a
 * second, the rate of its sampled music).
 *
 * In hw im2 mode an acknowledged device is "in service" until the
 * processor runs RETI, as a Zilog peripheral on a daisy chain is: it, and
 * every device of lower priority, waits until then. ZEsarUX's own line and
 * ULA interrupts are kept in pulse mode; in hw im2 mode this file makes
 * them as well, so that they get their vectors.
 *
 * Not done: the UARTs' interrupts (ZEsarUX's UART makes none), the DMA
 * being interrupted (registers 0xCC to 0xCE), the CTC's trigger edge
 * (D4) and its vector word (the Next makes the vectors).
 *
 * Everything here is plain memory of the core, so a savestate holds it.
 */
#include <stdint.h>

#include "cpu.h"
#include "screen.h"
#include "tbblue.h"

extern z80_bit interrupcion_maskable_generada;
extern int testados_desde_inicio_pulso_interrupcion;

#define CTC_CHANNELS 4
#define SRC_LINE 0
#define SRC_CTC0 3
#define SRC_ULA 11

enum { CTC_HARD_RESET, CTC_WANTS_CONSTANT, CTC_WANTS_TRIGGER, CTC_RUNNING };

struct ctc_channel {
	uint8_t control;      /* the last control word */
	uint8_t constant;     /* the time constant, 0 meaning 256 */
	uint8_t state;
	uint8_t next_is_constant; /* the control word said a constant follows */
	uint16_t counter;     /* the down counter */
	uint16_t prescaled;   /* clock ticks into the prescaler */
};

static struct ctc_channel g_ctc[CTC_CHANNELS];
static uint16_t g_pending;   /* hw im2 mode: devices waiting to interrupt */
static uint16_t g_inservice; /* hw im2 mode: devices acknowledged, before RETI */
static int g_lastLine = -1;
static int g_lastT;
static int g_lastTurbo;
static int g_clockRest;      /* T-states times 8 not yet a whole tick */

int chimera_next_hwim2(void) { return MACHINE_IS_TBBLUE && (tbblue_registers[0xC0] & 1); }

static int enabled(int src)
{
	if (src == SRC_LINE) return tbblue_registers[0x22] & 2;
	if (src == SRC_ULA) return !(tbblue_registers[0x22] & 4);
	if (src >= SRC_CTC0 && src < SRC_CTC0 + 8) return tbblue_registers[0xC5] & (1 << (src - SRC_CTC0));
	return 0;
}

static void set_status(int src)
{
	if (src == SRC_LINE) tbblue_registers[0xC8] |= 2;
	else if (src == SRC_ULA) tbblue_registers[0xC8] |= 1;
	else if (src >= SRC_CTC0 && src < SRC_CTC0 + 8) tbblue_registers[0xC9] |= 1 << (src - SRC_CTC0);
}

/* A device has made its interrupt: its status bit is set whether or not
 * its interrupt is enabled ("polled mode" when it is not); an enabled one
 * waits in hw im2 mode, and makes a pulse otherwise (the line interrupt's
 * and the ULA's pulses are ZEsarUX's own) */
static void event(int src)
{
	set_status(src);
	if (!enabled(src)) return;
	if (chimera_next_hwim2()) g_pending |= 1 << src;
	else if (src >= SRC_CTC0 && src < SRC_CTC0 + 8 && iff1.v) {
		interrupcion_maskable_generada.v = 1;
		testados_desde_inicio_pulso_interrupcion = 0;
	}
}

/* the device that interrupts next: the highest priority one pending, if
 * none of the same or a higher priority is in service */
static int next_source(void)
{
	for (int src = 0; src < 14; src++) {
		if (g_inservice & (1 << src)) return -1;
		if (g_pending & (1 << src)) return src;
	}
	return -1;
}

/* ---- the CTC ------------------------------------------------------------ */

static void ctc_trigger(int ch);

static void ctc_zero(int ch)
{
	struct ctc_channel *c = &g_ctc[ch];
	c->counter = c->constant ? c->constant : 256;
	event(SRC_CTC0 + ch);
	if (ch + 1 < CTC_CHANNELS) ctc_trigger(ch + 1);
}

/* an edge on the channel's CLK/TRG input: the previous channel's zero */
static void ctc_trigger(int ch)
{
	struct ctc_channel *c = &g_ctc[ch];
	if (c->state == CTC_WANTS_TRIGGER) {
		c->state = CTC_RUNNING;
		c->prescaled = 0;
	} else if (c->state == CTC_RUNNING && (c->control & 0x40)) {
		if (--c->counter == 0) ctc_zero(ch);
	}
}

static void ctc_clock(unsigned ticks)
{
	for (int ch = 0; ch < CTC_CHANNELS; ch++) {
		struct ctc_channel *c = &g_ctc[ch];
		if (c->state != CTC_RUNNING || (c->control & 0x40)) continue; /* counter mode counts edges */
		const unsigned prescale = (c->control & 0x20) ? 256 : 16;
		unsigned total = c->prescaled + ticks;
		while (total >= prescale) {
			total -= prescale;
			if (--c->counter == 0) ctc_zero(ch);
		}
		c->prescaled = (uint16_t)total;
	}
}

static int ctc_channel_of(z80_int port) { return (port >> 8) - 0x18; }

int chimera_next_is_ctc_port(z80_int port)
{
	return (port & 0xFF) == 0x3B && (port >> 8) >= 0x18 && (port >> 8) < 0x18 + CTC_CHANNELS;
}

void chimera_next_ctc_write(z80_int port, z80_byte value)
{
	struct ctc_channel *c = &g_ctc[ctc_channel_of(port)];
	if (c->next_is_constant) {
		c->next_is_constant = 0;
		c->constant = value;
		if (c->state == CTC_WANTS_CONSTANT) {
			c->counter = value ? value : 256;
			c->prescaled = 0;
			/* a timer with D3 = 1 starts on a CLK/TRG edge; a counter
			 * counts them */
			c->state = (!(c->control & 0x40) && (c->control & 0x08)) ? CTC_WANTS_TRIGGER : CTC_RUNNING;
		}
		return;
	}
	if (!(value & 1)) return; /* a vector word: the Next makes the vectors */
	/* a control word. In the hard reset state, one with D2 = 0 is ignored */
	if (c->state == CTC_HARD_RESET && !(value & 4)) return;
	c->control = value;
	/* D7, the channel's interrupt, is the same bit as its bit in 0xC5 */
	const int bit = 1 << ctc_channel_of(port);
	if (value & 0x80) tbblue_registers[0xC5] |= bit;
	else {
		tbblue_registers[0xC5] &= ~bit;
		g_pending &= ~(1 << (SRC_CTC0 + ctc_channel_of(port)));
	}
	if (value & 2) c->state = (value & 4) ? CTC_WANTS_CONSTANT : CTC_HARD_RESET;
	else if (c->state == CTC_HARD_RESET) c->state = CTC_WANTS_CONSTANT;
	if (value & 4) c->next_is_constant = 1;
}

z80_byte chimera_next_ctc_read(z80_int port)
{
	return (z80_byte)g_ctc[ctc_channel_of(port)].counter;
}

/* ---- the interrupt controller ------------------------------------------ */

/* a write to one of the controller's registers; 1 when it was handled here
 * and must not be stored as written (0x22 and 0xC0 are stored by ZEsarUX) */
int chimera_next_register_write(z80_byte reg, z80_byte value)
{
	switch (reg) {
	case 0x22:
		/* its line and ULA bits are 0xC4's */
		tbblue_registers[0xC4] = (tbblue_registers[0xC4] & 0x80) | (value & 2) | ((value & 4) ? 0 : 1);
		return 0;
	case 0xC0:
		/* leaving hw im2 mode forgets what was waiting */
		if (!(value & 1)) g_pending = g_inservice = 0;
		return 0;
	case 0xC4:
		tbblue_registers[0xC4] = value & 0x83;
		tbblue_registers[0x22] = (tbblue_registers[0x22] & 0xF9) | (value & 2) | ((value & 1) ? 0 : 4);
		return 1;
	case 0xC5:
		tbblue_registers[0xC5] = value;
		for (int ch = 0; ch < CTC_CHANNELS; ch++)
			g_ctc[ch].control = (z80_byte)((g_ctc[ch].control & 0x7F) | ((value >> ch) & 1 ? 0x80 : 0));
		g_pending &= (uint16_t)~(((~value) & 0xFF) << SRC_CTC0);
		return 1;
	case 0xC8: case 0xC9: case 0xCA: {
		/* a 1 clears a status bit; a pending interrupt keeps its own */
		tbblue_registers[reg] &= (z80_byte)~value;
		if (reg == 0xC8) {
			if (g_pending & (1 << SRC_LINE)) tbblue_registers[0xC8] |= 2;
			if (g_pending & (1 << SRC_ULA)) tbblue_registers[0xC8] |= 1;
		} else if (reg == 0xC9) {
			tbblue_registers[0xC9] |= (z80_byte)(g_pending >> SRC_CTC0);
		}
		return 1;
	}
	}
	return 0;
}

/* register 0xC0 as read: bits 2:1 the processor's interrupt mode */
z80_byte chimera_next_register_read_c0(void)
{
	return (z80_byte)((tbblue_registers[0xC0] & 0xF9) | ((im_mode & 3) << 1));
}

/* the processor takes the interrupt: the vector for its low byte (0xFF
 * when no device is waiting, as on the bus with nothing on it) */
z80_byte chimera_next_int_ack(void)
{
	const int src = next_source();
	if (src < 0) return 0xFF;
	g_pending &= (uint16_t)~(1 << src);
	g_inservice |= (uint16_t)(1 << src);
	return (z80_byte)((tbblue_registers[0xC0] & 0xE0) | (src << 1));
}

/* RETI: the device in service with the highest priority is done */
void chimera_next_reti(void)
{
	if (!MACHINE_IS_TBBLUE) return;
	for (int src = 0; src < 14; src++)
		if (g_inservice & (1 << src)) {
			g_inservice &= (uint16_t)~(1 << src);
			return;
		}
}

/* the end of a frame: the ULA's interrupt */
void chimera_next_frame_interrupt(void)
{
	event(SRC_ULA);
}

/* after each instruction: the CTC's clock, the line interrupt, and in hw
 * im2 mode the request to the processor */
void chimera_next_step(void)
{
	/* T-states run since the last call; the frame's end takes a frame's
	 * worth off t_estados, and a change of speed rescales it */
	if (cpu_turbo_speed != g_lastTurbo) {
		g_lastTurbo = cpu_turbo_speed;
		g_lastT = t_estados;
	}
	int delta = t_estados - g_lastT;
	if (delta < 0) delta += screen_testados_total;
	if (delta < 0) delta = 0;
	g_lastT = t_estados;
	const int eighths = g_clockRest + delta * 8;
	ctc_clock((unsigned)(eighths / cpu_turbo_speed));
	g_clockRest = eighths % cpu_turbo_speed;

	const int line = tbblue_get_current_raster_position();
	if (line != g_lastLine) {
		g_lastLine = line;
		if (line == (tbblue_registers[0x23] | ((tbblue_registers[0x22] & 1) << 8))) event(SRC_LINE);
	}

	if (chimera_next_hwim2() && iff1.v && next_source() >= 0) {
		interrupcion_maskable_generada.v = 1;
		testados_desde_inicio_pulso_interrupcion = 0;
	}
}

/* a reset of the Next: the CTC's channels in their hard reset state, the
 * controller's registers as the documentation gives them after a soft
 * reset */
void chimera_next_interrupts_reset(void)
{
	for (int ch = 0; ch < CTC_CHANNELS; ch++) g_ctc[ch] = (struct ctc_channel){ 0 };
	g_pending = g_inservice = 0;
	tbblue_registers[0xC0] = 0;
	tbblue_registers[0xC4] = 0x81;
	tbblue_registers[0xC5] = 0;
	tbblue_registers[0xC6] = 0;
	tbblue_registers[0x22] &= 0xF9;
}
