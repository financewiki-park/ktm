#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${KINDLE_PLATFORM:-}" != "" ] && [ "${KINDLE_PLATFORM}" != "kindlehf" ] && [ "${KINDLE_PLATFORM}" != "kindlepw2" ]; then
    echo "Unsupported KINDLE_PLATFORM: ${KINDLE_PLATFORM}" >&2
    exit 1
fi
chmod 700 "$SELF_DIR/launch.sh" "$SELF_DIR/uninstall.sh" "$SELF_DIR/bin/ktm"
echo "ktm installed without rootfs modification."
echo "Use the launcher for setup, pair, sync, inbox, last, copy, send, status, diagnose."
