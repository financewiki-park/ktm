#!/bin/sh
set -eu
BIN=${1:?binary path required}
PLATFORM=${2:?platform required}
INPUT_RELAY=${3:?terminal input relay path required}
case "$PLATFORM" in
  kindlehf) : ;;
  *) echo "this release supports kindlehf only" >&2; exit 2 ;;
esac
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=${KTM_VERSION:-0.1.12}
OUT="$ROOT/dist/ktm-${PLATFORM}-${VERSION}"
# Never ship a stale binary left in a staging directory by an older build.
if [ -e "$OUT" ]; then OUT=$(mktemp -d "$ROOT/dist/ktm-${PLATFORM}-${VERSION}.XXXXXX"); fi
mkdir -p "$OUT/bin"
cp "$BIN" "$OUT/bin/ktm"
cp "$INPUT_RELAY" "$OUT/bin/ktm-input"
cp "$ROOT/kpm/manifest.json" "$ROOT/kpm/install.sh" "$ROOT/kpm/launch.sh" "$ROOT/kpm/uninstall.sh" "$ROOT/kpm/ui.sh" "$ROOT/kpm/session.sh" "$OUT/"
mkdir -p "$OUT/assets" "$OUT/scriptlet"
cp "$ROOT/assets/ktm-icon.png" "$OUT/assets/ktm-icon.png"
cp "$ROOT/kpm/scriptlet/ktm.sh" "$OUT/scriptlet/ktm.sh"
cp -R "$ROOT/kterm" "$OUT/"
chmod 700 "$OUT"/*.sh "$OUT/bin/ktm" "$OUT/bin/ktm-input" "$OUT/scriptlet/ktm.sh"
chmod 644 "$OUT/assets/ktm-icon.png"
mkdir -p "$ROOT/dist"
# KPM 0.2.x expects a gzip tar archive whose manifest is a top-level
# "manifest.json" entry.  Do not archive "." here: that creates an
# implementation-dependent "./manifest.json" path which older KPM builds
# reject while reading the package manifest.
tar -C "$OUT" -czf "$ROOT/dist/ktm-${PLATFORM}.kpkg" \
  manifest.json install.sh launch.sh uninstall.sh ui.sh session.sh assets scriptlet bin kterm
python3 "$ROOT/tools/verify-package.py" "$ROOT/dist/ktm-${PLATFORM}.kpkg"
echo "$ROOT/dist/ktm-${PLATFORM}.kpkg"
