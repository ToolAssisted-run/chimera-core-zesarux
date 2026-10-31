# chimera-core-zesarux: ZEsarUX's ZX Spectrum Next (TBBlue) in miniBox's
# sandbox, as a Chimera core.
#
# Native reference build (run-native + run-wbx under the build directory):
#     meson setup build/meson-native -Dminibox_dir=<miniBox> && ninja -C build/meson-native
# Guest (core.wbx) build via the cross file waterbox/setup-guest.sh writes
# (needs miniBox's guest toolchain):
#     sh waterbox/setup-guest.sh -m <miniBox> && ninja -C build/meson-guest core.wbx
#
# Upstream builds with its own configure script, which looks at the machine
# and writes compileoptions.h and a Makefile. Neither is used here, because
# the core is built twice (host and sandbox) and the two must be the same
# program:
#  - compileoptions.h is one hand-written file (waterbox/config/), what
#    configure wrote for the null drivers, no threads and no network, with
#    nothing of the machine it ran on;
#  - the source list below is what upstream's Makefile compiled for those
#    options, less the null video and audio drivers (the core's own take
#    their place: waterbox/scrchimera.c, waterbox/audiochimera.c) and
#    main_unix.c (the core is started by a call);
#  - third_party/playtzx.c is left out: its licence is unknown (ZEsarUX's
#    licenses/LICENSE_playtzx says so). It turns a .tzx tape into sound for
#    ZEsarUX's "real tape" player; waterbox/chimera_compat.c answers that it
#    could not.
project('chimera-core-zesarux', 'c',
  default_options: ['c_std=gnu11', 'optimization=2', 'debug=false', 'warning_level=0'])

zs = 'extern/zesarux/src'

# the submodule pin is upstream as it is; the patches are laid over it
run_command(files('waterbox/apply-patches.sh'), check: true)

mb = get_option('minibox_dir')
if mb == ''
  error('pass -Dminibox_dir=<miniBox checkout>')
endif

add_project_arguments([
  '-fsigned-char',
  '-fno-strict-aliasing',
  # one answer from a floating-point expression on every machine
  '-ffp-contract=off',
  '-DCHIMERA_CORE',
  '-Wno-implicit-function-declaration', '-Wno-int-conversion', '-Wno-incompatible-pointer-types',
], language: 'c')

zs_inc = include_directories('waterbox/config', 'waterbox', zs, zs / 'audio', zs / 'copy_interfaces',
  zs / 'cores', zs / 'cpus', zs / 'machines', zs / 'menu', zs / 'snap', zs / 'storage',
  zs / 'soundchips', zs / 'third_party', zs / 'video', zs / 'video_chips', zs / 'zrcp', zs / 'zxvision')

zs_sources = files(
  zs / 'about_logo.c', zs / 'assemble.c', zs / 'atomic.c', zs / 'audio/audio.c',
  zs / 'audio/audio_ayplayer.c', zs / 'audio/audio_sine_table.c', zs / 'autoselectoptions.c',
  zs / 'chardetect.c', zs / 'chardevice.c', zs / 'charset.c', zs / 'codetests.c', zs / 'contend.c',
  zs / 'copy_interfaces/defcon.c', zs / 'copy_interfaces/dinamid3.c',
  zs / 'copy_interfaces/hilow_barbanegra.c', zs / 'copy_interfaces/interface007.c',
  zs / 'copy_interfaces/mhpokeador.c', zs / 'copy_interfaces/multiface.c',
  zs / 'copy_interfaces/phoenix.c', zs / 'copy_interfaces/ramjet.c',
  zs / 'copy_interfaces/specmate.c', zs / 'copy_interfaces/transtape.c', zs / 'cores/core_ace.c',
  zs / 'cores/core_coleco.c', zs / 'cores/core_cpc.c', zs / 'cores/core_mk14.c',
  zs / 'cores/core_msx.c', zs / 'cores/core_pcw.c', zs / 'cores/core_ql.c',
  zs / 'cores/core_reduced_spectrum.c', zs / 'cores/core_sam.c', zs / 'cores/core_sg1000.c',
  zs / 'cores/core_sms.c', zs / 'cores/core_spectrum.c', zs / 'cores/core_svi.c',
  zs / 'cores/core_z88.c', zs / 'cores/core_zx8081.c', zs / 'cpu.c', zs / 'cpus/m68kcpu.c',
  zs / 'cpus/m68kdasm.c', zs / 'cpus/m68kopac.c', zs / 'cpus/m68kopdm.c', zs / 'cpus/m68kopnz.c',
  zs / 'cpus/m68kops.c', zs / 'cpus/scmp.c', zs / 'cpus/z80_codprddfd.c',
  zs / 'cpus/z80_codpred.c', zs / 'cpus/z80_codsinpr.c', zs / 'datagear.c', zs / 'debug.c',
  zs / 'debug_timings.c', zs / 'ds1307.c', zs / 'dsk.c', zs / 'enhanced_zx81_read.c',
  zs / 'expression_parser.c', zs / 'joystick.c', zs / 'machines/baseconf.c',
  zs / 'machines/chloe.c', zs / 'machines/chrome.c', zs / 'machines/coleco.c',
  zs / 'machines/cpc.c', zs / 'machines/jupiterace.c', zs / 'machines/mk14.c',
  zs / 'machines/msx.c', zs / 'machines/pcw.c', zs / 'machines/prism.c', zs / 'machines/ql.c',
  zs / 'machines/ql_i8049.c', zs / 'machines/ql_qdos_handler.c', zs / 'machines/ql_zx8302.c',
  zs / 'machines/sam.c', zs / 'machines/sg1000.c', zs / 'machines/sms.c', zs / 'machines/svi.c',
  zs / 'machines/tbblue.c', zs / 'machines/timex.c', zs / 'machines/tsconf.c',
  zs / 'machines/z88.c', zs / 'machines/zx8081.c', zs / 'machines/zxevo.c',
  zs / 'machines/zxuno.c', zs / 'mem128.c', zs / 'menu/menu_bitmaps.c',
  zs / 'menu/menu_debug_cpu.c', zs / 'menu/menu_file_viewer_browser.c', zs / 'menu/menu_filesel.c',
  zs / 'menu/menu_items.c', zs / 'menu/menu_items_settings.c', zs / 'menu/menu_items_storage.c',
  zs / 'menu/menu_zeng_online.c', zs / 'network.c', zs / 'operaciones.c', zs / 'printers.c',
  zs / 'realjoystick.c', zs / 'sensors.c', zs / 'settings.c', zs / 'snap/snap.c',
  zs / 'snap/snap_nex.c', zs / 'snap/snap_ram.c', zs / 'snap/snap_rzx.c', zs / 'snap/snap_spg.c',
  zs / 'snap/snap_z81.c', zs / 'snap/snap_zsf.c', zs / 'snap/snap_zx8081.c',
  zs / 'snap/spg_depack.c', zs / 'soundchips/ay38912.c', zs / 'soundchips/gs.c',
  zs / 'soundchips/saa_simul.c', zs / 'soundchips/sn76489an.c', zs / 'spritefinder.c',
  zs / 'start.c', zs / 'stats.c', zs / 'storage/atomlite.c', zs / 'storage/betadisk.c',
  zs / 'storage/dandanator.c', zs / 'storage/divide.c', zs / 'storage/diviface.c',
  zs / 'storage/divmmc.c', zs / 'storage/esxdos_handler.c', zs / 'storage/hilow_datadrive.c',
  zs / 'storage/hilow_datadrive_audio.c', zs / 'storage/ide.c', zs / 'storage/if1.c',
  zs / 'storage/ifrom.c', zs / 'storage/kartusho.c', zs / 'storage/lec.c',
  zs / 'storage/microdrive.c', zs / 'storage/microdrive_raw.c', zs / 'storage/mmc.c',
  zs / 'storage/pd765.c', zs / 'storage/plus3dos_handler.c', zs / 'storage/samram.c',
  zs / 'storage/superupgrade.c', zs / 'storage/tape.c', zs / 'storage/tape_pzx.c',
  zs / 'storage/tape_smp.c', zs / 'storage/tape_tap.c', zs / 'storage/tape_tzx.c',
  zs / 'storage/zxmmcplus.c', zs / 'storage/zxpand.c', zs / 'textspeech.c',
  zs / 'third_party/disassemble.c', zs / 'third_party/diskio.c', zs / 'third_party/ff.c',
  zs / 'third_party/ffsystem.c', zs / 'third_party/ffunicode.c', zs / 'third_party/mdvtool.c',
  zs / 'third_party/scl2trd.c', zs / 'third_party/zip.c',
  zs / 'timer.c', zs / 'tv.c', zs / 'uartbridge.c', zs / 'ula.c', zs / 'utils.c',
  zs / 'utils_math.c', zs / 'utils_text_adventure.c', zs / 'video/screen.c',
  zs / 'video/screen_fx.c', zs / 'video_chips/spectra.c', zs / 'video_chips/spritechip.c',
  zs / 'video_chips/ulaplus.c', zs / 'video_chips/vdp_9918a.c', zs / 'video_chips/vdp_9918a_sms.c',
  zs / 'video_chips/video_chips_common.c', zs / 'zrcp/remote.c', zs / 'zrcp/zeng.c',
  zs / 'zrcp/zeng_online.c', zs / 'zrcp/zeng_online_client.c', zs / 'zvfs.c',
  zs / 'zxvision/zxvision.c', zs / 'zxvision/zxvision_topbar.c',
  # the core's own layers under ZEsarUX
  'waterbox/scrchimera.c', 'waterbox/audiochimera.c', 'waterbox/chimera_compat.c',
  'waterbox/chimera_roms.c', 'waterbox/next_interrupts.c',
)

# The 48K Spectrum's ROM, for the Next's fast start, is built into the core
# (user-decided, 2026-10-31): read from the pinned submodule at build time
# by waterbox/gen-roms.py, which checks its SHA-1; this repository holds no
# ROM. waterbox/chimera_roms.c hands it to ZEsarUX.
zs_sources += custom_target('chimera_roms_data',
  input: [files('waterbox/gen-roms.py'), files(zs / '48.rom')],
  output: 'chimera_roms_data.c',
  command: [find_program('python3'), '@INPUT0@', meson.current_source_dir() / zs, '@OUTPUT@'])

# What ZEsarUX asks of the system that is the machine's business: the
# clock, the C library's random numbers, sleeping, signal handlers, the
# system's name and the current folder; and ZEsarUX's own messages, to keep
# an error as the reason a start failed (waterbox/chimera_compat.c); and the
# finding of a ROM, which the built-in ROM answers (waterbox/chimera_roms.c,
# with fopen below). Wrapped at the link, for every object.
link_wrap = []
foreach f : ['time', 'gettimeofday', 'localtime', 'mktime', 'rand', 'srand', 'sleep', 'usleep', 'signal', 'uname',
             'getcwd', 'debug_printf', 'find_sharedfile', 'get_file_size']
  link_wrap += '-Wl,--wrap=' + f
endforeach

if meson.is_cross_build()
  # ---- the guest: core.wbx --------------------------------------------------
  mbdir = meson.get_external_property('minibox_dir')
  mbuild = meson.get_external_property('minibox_build')
  sr = meson.get_external_property('guest_sysroot')

  executable('core', zs_sources + files('waterbox/cinterface.c', 'waterbox/guest-syscalls.c'),
    name_suffix: 'wbx',
    include_directories: [zs_inc, include_directories('waterbox/guest-shim')],
    # "./name" is the mounted file "name" (waterbox/guest-syscalls.c)
    link_args: link_wrap + ['-Wl,--wrap=fopen', '-Wl,--wrap=open'] + [
      '-static', '-no-pie',
      '-Wl,--eh-frame-hdr', '-Wl,-O2', '-Wl,--no-relax', '-Wl,-z,stack-size=8388608',
      '-T', mbdir / 'source/guest/linkscript.T',
      mbuild / 'source/guest/emulibc.c.o',
      '-L' + sr / 'lib', '-lgcc', '-lc'])
else
  # ---- the native reference -------------------------------------------------
  m = meson.get_compiler('c').find_library('m', required: false)
  executable('run-native', zs_sources + files('waterbox/cinterface.c', 'waterbox/run-native.c'),
    include_directories: [include_directories('waterbox/native-shim'), zs_inc],
    c_args: ['-I' + mb / 'source/guest/include', '-I' + mb / 'extern/jsmn'],
    # the sandbox's clock, which stands still at the project's start, and
    # fopen (fopen64 here), which opens the built-in ROM (waterbox/run-native.c)
    link_args: link_wrap + ['-Wl,--wrap=clock_gettime', '-Wl,--wrap=fopen', '-Wl,--wrap=fopen64'],
    dependencies: [m])

  mbhost = mb / 'build/meson-linux/source/host'
  executable('run-wbx', files('waterbox/run-wbx.c'),
    c_args: ['-I' + mb / 'source/host'],
    link_args: [mbhost / 'libminiboxhost.so'],
    build_rpath: mbhost)
endif
