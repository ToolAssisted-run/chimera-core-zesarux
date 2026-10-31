/* audiochimera.c - the core's sound card, in place of ZEsarUX's "null"
 * audio driver.
 *
 * ZEsarUX mixes one stereo sample of 8 bits for each of the machine's
 * scan lines (312 a frame) and collects them in a buffer that its audio
 * driver is handed every few frames. The core takes them one by one as they
 * are made instead (a hook in audio_send_stereo_sample, patch 0003), so
 * that each frame's sound is that frame's: chimera_audio_sample below. This
 * file is compiled INSTEAD of audio/audionull.c (meson.build), with the
 * same function names, so ZEsarUX still believes it runs on the null
 * driver ("--ao null"), whose buffer goes nowhere.
 */
#include <stdio.h>

#include "audio.h"
#include "audionull.h"
#include "cpu.h"

#include "chimera_glue.h"

int16_t chimera_audio[CHIMERA_AUDIO_MAX_PAIRS * 2];
int chimera_audio_pairs;

void chimera_audio_sample(char left, char right)
{
	if (chimera_audio_pairs >= CHIMERA_AUDIO_MAX_PAIRS) return;
	/* 8 bits signed, as 16 */
	chimera_audio[chimera_audio_pairs * 2] = (int16_t)((signed char)left * 256);
	chimera_audio[chimera_audio_pairs * 2 + 1] = (int16_t)((signed char)right * 256);
	chimera_audio_pairs++;
}

int audionull_init(void)
{
	audio_set_driver_name("null");
	return 0;
}

void audionull_send_frame(char *buffer) { (void)buffer; }

int audionull_thread_finish(void) { return 0; }

void audionull_end(void)
{
	audionull_thread_finish();
	audio_playing.v = 0;
}

void audionull_get_buffer_info(int *buffer_size, int *current_size)
{
	*buffer_size = 0;
	*current_size = 0;
}

int audionull_can_record_input(void) { return 0; }
