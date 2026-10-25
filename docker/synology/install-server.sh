#!/bin/sh
# Copies the server programs out of a CMake build tree into one self-contained directory:
#
#   <dest>/domain-server, assignment-client (optional ice-server)
#   <dest>/resources/   the domain server's web interface and settings description
#   <dest>/plugins/     the assignment client's codec plugins
#   <dest>/lib/         the tree's own shared libraries (Qt and the rest come from the system)
#
# Usage: install-server.sh <cmake build dir> <dest>
set -e

BUILD_DIR=$(cd "$1" && pwd)
DEST=$2

# This script is for image assembly only, never a data directory.
[ "$DEST" = /opt/darkride ] || { echo "Expected /opt/darkride" >&2; exit 1; }
[ ! -e "$DEST" ] || { echo "Destination already exists" >&2; exit 1; }
mkdir -p "$DEST/lib" "$DEST/plugins"

cp "$BUILD_DIR/domain-server/domain-server" "$DEST/"
cp -rL "$BUILD_DIR/domain-server/resources" "$DEST/"
cp "$BUILD_DIR/assignment-client/assignment-client" "$DEST/"
test -s "$BUILD_DIR/assignment-client/resources/wasm/darkride-js.wasm"
mkdir -p "$DEST/resources/wasm"
cp "$BUILD_DIR/assignment-client/resources/wasm/darkride-js.wasm" "$DEST/resources/wasm/"
# Both codecs are required for this beta; do not publish a silently reduced build.
test -s "$BUILD_DIR/assignment-client/plugins/libtivoli-pcm-codec-plugin.so"
test -s "$BUILD_DIR/assignment-client/plugins/libtivoli-opus-codec-plugin.so"
cp "$BUILD_DIR/assignment-client/plugins/"*.so "$DEST/plugins/"
if [ -f "$BUILD_DIR/ice-server/ice-server" ]; then
    cp "$BUILD_DIR/ice-server/ice-server" "$DEST/"
fi

# The libraries built in the tree are found from what the programs and plugins load.
for binary in "$DEST"/domain-server "$DEST"/assignment-client "$DEST"/ice-server "$DEST"/plugins/*.so; do
    if [ ! -f "$binary" ]; then continue; fi
    ldd "$binary" | awk -v build="$BUILD_DIR/" 'index($3, build) == 1 { print $3 }'
done | sort -u | while read -r library; do
    cp "$library" "$DEST/lib/"
done

# Debug information is most of the size; leave it out unless KEEP_SYMBOLS is set.
if [ -z "$KEEP_SYMBOLS" ]; then
    strip --strip-unneeded "$DEST"/domain-server "$DEST"/assignment-client "$DEST"/lib/*.so* "$DEST"/plugins/*.so
    if [ -f "$DEST/ice-server" ]; then strip --strip-unneeded "$DEST/ice-server"; fi
fi

# Point the programs at lib/ instead of the build tree.
for binary in "$DEST"/domain-server "$DEST"/assignment-client "$DEST"/ice-server; do
    if [ -f "$binary" ]; then patchelf --set-rpath '$ORIGIN/lib' "$binary"; fi
done
for library in "$DEST"/lib/*.so*; do
    if [ -f "$library" ]; then patchelf --set-rpath '$ORIGIN' "$library"; fi
done
for plugin in "$DEST"/plugins/*.so; do
    if [ -f "$plugin" ]; then patchelf --set-rpath '$ORIGIN/../lib' "$plugin"; fi
done

echo "Installed the server to $DEST:"
ls -l "$DEST" "$DEST/lib" "$DEST/plugins"
