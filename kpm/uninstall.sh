#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
echo "Removing package-owned files only: $SELF_DIR"
SCRIPTLET=/mnt/us/documents/ktm.sh
if [ -f "$SCRIPTLET" ] && grep -q '^# ktm-managed-scriptlet-v1$' "$SCRIPTLET"; then
    rm -f "$SCRIPTLET"
fi
if [ -f /mnt/us/ktm-icon.png ]; then
    rm -f /mnt/us/ktm-icon.png
fi
echo "Persistent data is retained at /var/local/ktm or /mnt/us/ktm."
