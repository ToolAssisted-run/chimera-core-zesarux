/* compileoptions.h for the core, written by hand.
 *
 * ZEsarUX's configure writes this file from what it finds on the machine it
 * runs on, with the date and the folder it was run in. The core is built
 * twice (for the host and for the sandbox) and both must be the same
 * program, built the same on every machine, so this is the file configure
 * wrote for these options with nothing of a machine left in it:
 *
 *   ./configure --disable-stdout --disable-simpletext --disable-curses
 *     --disable-cursesw --disable-aa --disable-caca --disable-xwindows
 *     --disable-xext --disable-sdl --disable-fbdev --disable-dsp
 *     --disable-onebitspeaker --disable-alsa --disable-pulse
 *     --disable-coreaudio --disable-sndfile --disable-pthreads
 *     --disable-networking --disable-linuxrealjoystick --disable-visualmem
 *     --disable-cpustats
 *
 * That is the "null" video and audio drivers (which the core replaces with
 * its own, waterbox/scrchimera.c and audiochimera.c), no threads and no
 * network. The "visual memory" and processor statistics of ZEsarUX's debug
 * windows are off: they write down every read of the memory, so that a RAM
 * tool looking at the machine would change it.
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#define NETWORKING_DISABLED
#define EMULATE_MEMPTR
#define EMULATE_CONTEND
#define PUTPIXELCACHE

#define COMPILATION_DATE "chimera core build"
#define COMPILATION_SYSTEM "chimera"
#define COMPILATION_SYSTEM_RELEASE ""
#define COMPILATION_MACHINE_HARDWARE_NAME "x86_64"
#define CONFIGURE_OPTIONS "chimera core build"
#define COMPILE_VARIABLES " NETWORKING_DISABLED EMULATE_MEMPTR EMULATE_CONTEND PUTPIXELCACHE"
#define COMPILE_INITIALCFLAGS ""
#define COMPILE_INITIALLDFLAGS ""
#define COMPILE_INITIALLIBS ""
#define COMPILE_FINALCFLAGS ""
#define COMPILE_FINALLDFLAGS ""
#define COMPILE_FINALLIBS ""
#define INSTALL_PREFIX "."

#define LINES_SOURCE 0
#define TOTAL_C_FILES 0
#define TOTAL_H_FILES 0
#define TOTAL_COMMENTS 0
#define TOTAL_TODO_ITEMS 0

/* a program can read this through ZEsarUX's own I/O port (ula.c): it is a
   constant, never the time of the build */
#define BUILDNUMBER "0"

#endif
