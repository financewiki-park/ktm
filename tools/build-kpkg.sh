#!/bin/sh
set -eu
BIN=${1:?binary path required}
PLATFORM=${2:?platform required}
case "$PLATFORM" in
  kindlehf|kindlepw2) : ;;
  *) echo "platform must be kindlehf or kindlepw2" >&2; exit 2 ;;
esac
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=${KTM_VERSION:-0.1.1}
OUT="$ROOT/dist/ktm-${PLATFORM}-${VERSION}"
rm -rf "$OUT"
mkdir -p "$OUT/bin"
cp "$BIN" "$OUT/bin/ktm"
cp "$ROOT/kpm/manifest.json" "$ROOT/kpm/install.sh" "$ROOT/kpm/launch.sh" "$ROOT/kpm/uninstall.sh" "$OUT/"
mkdir -p "$OUT/assets" "$OUT/scriptlet"
cp "$ROOT/assets/ktm-icon.png" "$OUT/assets/ktm-icon.png"
cp "$ROOT/kpm/scriptlet/ktm.sh" "$OUT/scriptlet/ktm.sh"
cp -R "$ROOT/kterm" "$OUT/"
chmod 700 "$OUT"/*.sh "$OUT/bin/ktm" "$OUT/scriptlet/ktm.sh"
chmod 644 "$OUT/assets/ktm-icon.png"
mkdir -p "$ROOT/dist"
tar -C "$OUT" -czf "$ROOT/dist/ktm-${PLATFORM}.kpkg" .
echo "$ROOT/dist/ktm-${PLATFORM}.kpkg"
