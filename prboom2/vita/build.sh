#!/usr/bin/env bash
# Build dsda-doom for PS Vita.
#   Requirements: VitaSDK (VITASDK set, $VITASDK/bin in PATH), cmake >= 3.17,
#   a host C compiler (for the dsda-doom.wad resource tool).
# Output: build-vita/src/dsda-doom.vpk
set -euo pipefail

: "${VITASDK:?VITASDK is not set - install VitaSDK first (see vita/README.md)}"

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/.." && pwd)
build=${BUILD_DIR:-$src/build-vita}

if [[ ${1:-} == --deps ]]; then
	vdpm install \
		sdl2 sdl2_mixer libsndfile libzip zlib libmad libvorbis libogg flac \
		opusfile opus mpg123 lame libmodplug libxmp-lite bzip2 xz zstd openssl \
		libvita2d freetype libpng libjpeg-turbo
fi

cmake -S "$src" -B "$build" \
	-DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
	-DCMAKE_BUILD_TYPE=Release \
	-DENABLE_PACKAGING=OFF

cmake --build "$build" --parallel "$(nproc 2>/dev/null || echo 4)"

printf '\nVPK: %s\n' "$build/src/dsda-doom.vpk"
