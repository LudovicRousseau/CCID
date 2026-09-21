#!/bin/sh

set -x
set -e

BUILD_DIR=builddir
INSTALL_DIR=/tmp/pcsc

# special options for macOS
PKG_CONFIG_PATH=$(pwd)/MacOSX
export PKG_CONFIG_PATH

CFLAGS="$CFLAGS -DRESPONSECODE_DEFINED_IN_WINTYPES_H"
export CFLAGS

rm -rf "$BUILD_DIR"

meson setup "$BUILD_DIR" \
	-Dpcsclite=false \
	-Dos_log=true \
	-Dclass=false \
	-Dcomposite-as-multislot=true \
	-Dextra_bundle_id=foo \
	-Dudev-rules=false \
	"$@"

meson compile -C "$BUILD_DIR"

DESTDIR="$INSTALL_DIR" meson install -C "$BUILD_DIR"
find "$INSTALL_DIR"

# meson dist -C "$BUILD_DIR"
