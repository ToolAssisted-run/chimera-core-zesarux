/* What the core's own layers under ZEsarUX (scrchimera.c, audiochimera.c,
 * chimera_compat.c) share with the adapter (cinterface.c). */
#ifndef CHIMERA_ZESARUX_GLUE_H_
#define CHIMERA_ZESARUX_GLUE_H_

#include <stdint.h>

/* the picture: the largest the machine can draw, and the part of it in use */
#define CHIMERA_VIDEO_MAX_W 1024
#define CHIMERA_VIDEO_MAX_H 768
extern uint32_t chimera_video[CHIMERA_VIDEO_MAX_W * CHIMERA_VIDEO_MAX_H];
extern int chimera_video_w, chimera_video_h;
/* the size the display has now, as ZEsarUX computes it */
void chimera_video_update_size(void);

/* the sound of the frame being run, as stereo pairs */
#define CHIMERA_AUDIO_MAX_PAIRS 4096
extern int16_t chimera_audio[CHIMERA_AUDIO_MAX_PAIRS * 2];
extern int chimera_audio_pairs;

/* the machine's clock (chimera_compat.c) */
extern int64_t chimera_clock_start;
extern uint64_t chimera_clock_frames;
extern int chimera_clock_fps_num, chimera_clock_fps_den;

/* the last error ZEsarUX reported, for the reason a start fails */
extern char chimera_last_error[512];

#endif
