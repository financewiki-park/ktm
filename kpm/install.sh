#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${KINDLE_PLATFORM:-}" != "" ] && [ "${KINDLE_PLATFORM}" != "kindlehf" ] && [ "${KINDLE_PLATFORM}" != "kindlepw2" ]; then
    echo "Unsupported KINDLE_PLATFORM: ${KINDLE_PLATFORM}" >&2
    exit 1
fi
SCRIPTLET=/mnt/us/documents/ktm.sh
ICON=/mnt/us/ktm-icon.png
if [ ! -d /mnt/us/documents ]; then
    echo "Kindle documents directory is unavailable" >&2
    exit 1
fi
cp "$SELF_DIR/assets/ktm-icon.png" "$ICON"
cp "$SELF_DIR/scriptlet/ktm.sh" "$SCRIPTLET"
chmod 700 "$SELF_DIR/launch.sh" "$SELF_DIR/uninstall.sh" "$SELF_DIR/bin/ktm" "$SELF_DIR/bin/ktmterm" "$SCRIPTLET"
chmod 644 "$ICON"
echo "ktm installed without rootfs modification."
echo "The ktm Scriptlet should appear in the Kindle Library."
