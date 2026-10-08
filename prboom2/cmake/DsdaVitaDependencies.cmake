# PS Vita dependencies (VitaSDK, static libraries from vdpm).
#
# All libraries are static on the Vita, so the transitive dependencies are
# listed explicitly and in link order instead of relying on find modules.
# OpenGL is not available: the build links no-op GL/GLU stubs and the engine
# always uses the software renderer (see src/vita/).
#
# Required vdpm packages:
#   sdl2 sdl2_mixer libsndfile libzip zlib libmad libvorbis libogg flac
#   opusfile opus mpg123 lame libmodplug libxmp-lite bzip2 xz zstd openssl
#   libvita2d (launcher only)

include_guard()

if(NOT DEFINED ENV{VITASDK} AND NOT DEFINED VITASDK)
  message(FATAL_ERROR "VITASDK is not set")
endif()
if(NOT DEFINED VITASDK)
  set(VITASDK $ENV{VITASDK})
endif()

set(DSDA_VITA_PREFIX "${VITASDK}/arm-vita-eabi")

add_library(dsda_dependencies INTERFACE IMPORTED)
add_library(dsda::dependencies ALIAS dsda_dependencies)

target_include_directories(dsda_dependencies
  INTERFACE
  "${DSDA_VITA_PREFIX}/include"
  "${DSDA_VITA_PREFIX}/include/SDL2"
)

# Optional music backends that are known to build against VitaSDK packages.
set(HAVE_LIBMAD TRUE)
set(HAVE_LIBVORBISFILE TRUE)

target_link_libraries(dsda_dependencies
  INTERFACE
  dsda_vita_glstubs

  SDL2_mixer
  sndfile
  mp3lame
  mpg123
  FLAC
  opusfile
  opus
  vorbisenc
  vorbisfile
  vorbis
  ogg
  modplug
  xmp-lite
  mad

  zip
  ssl
  crypto
  zstd
  bz2
  lzma
  z

  SDL2

  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCommonDialog_stub
  SceCtrl_stub
  SceDisplay_stub
  SceGxm_stub
  SceHid_stub
  SceIme_stub
  SceKernelDmacMgr_stub
  SceMotion_stub
  ScePower_stub
  SceSysmodule_stub
  SceTouch_stub

  stdc++
  pthread
  m
)
