#!/bin/sh
set -eu
BIN=${1:?binary path required}
PLATFORM=${2:?platform required}
case "$PLATFORM" in
  kindlehf|kindlepw2) : ;;
  *) echo "platform must be kindlehf or kindlepw2" >&2; exit 2 ;;
esac
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/dist/ktm-${PLATFORM}-0.1.0"
rm -rf "$OUT"
mkdir -p "$OUT/bin"
cp "$BIN" "$OUT/bin/ktm"
cp "$ROOT/kpm/manifest.json" "$ROOT/kpm/install.sh" "$ROOT/kpm/launch.sh" "$ROOT/kpm/uninstall.sh" "$OUT/"
cp -R "$ROOT/kterm" "$OUT/"
chmod 700 "$OUT"/*.sh "$OUT/bin/ktm"
mkdir -p "$ROOT/dist"
tar -C "$ROOT/dist" -czf "$ROOT/dist/ktm-${PLATFORM}.kpkg" "$(basename "$OUT")"
echo "$ROOT/dist/ktm-${PLATFORM}.kpkg"
