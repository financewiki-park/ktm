#!/bin/sh
set -eu
BIN=${1:?binary path required}
PLATFORM=${2:?platform required}
NATIVE_TERM=${3:?native terminal path required}
case "$PLATFORM" in
  kindlehf|kindlepw2) : ;;
  *) echo "platform must be kindlehf or kindlepw2" >&2; exit 2 ;;
esac
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=${KTM_VERSION:-0.1.10}
OUT="$ROOT/dist/ktm-${PLATFORM}-${VERSION}"
rm -rf "$OUT"
mkdir -p "$OUT/bin"
cp "$BIN" "$OUT/bin/ktm"
cp "$NATIVE_TERM" "$OUT/bin/ktmterm"
cp "$ROOT/kpm/manifest.json" "$ROOT/kpm/install.sh" "$ROOT/kpm/launch.sh" "$ROOT/kpm/uninstall.sh" "$ROOT/kpm/ui.sh" "$ROOT/kpm/session.sh" "$OUT/"
mkdir -p "$OUT/assets" "$OUT/scriptlet"
cp "$ROOT/assets/ktm-icon.png" "$OUT/assets/ktm-icon.png"
cp "$ROOT/kpm/scriptlet/ktm.sh" "$OUT/scriptlet/ktm.sh"
cp -R "$ROOT/kterm" "$OUT/"
chmod 700 "$OUT"/*.sh "$OUT/bin/ktm" "$OUT/bin/ktmterm" "$OUT/scriptlet/ktm.sh"
chmod 644 "$OUT/assets/ktm-icon.png"
mkdir -p "$ROOT/dist"
# KPM 0.2.x expects a gzip tar archive whose manifest is a top-level
# "manifest.json" entry.  Do not archive "." here: that creates an
# implementation-dependent "./manifest.json" path which older KPM builds
# reject while reading the package manifest.
tar -C "$OUT" -czf "$ROOT/dist/ktm-${PLATFORM}.kpkg" \
  manifest.json install.sh launch.sh uninstall.sh ui.sh session.sh assets scriptlet bin kterm
echo "$ROOT/dist/ktm-${PLATFORM}.kpkg"
