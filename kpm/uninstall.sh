#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
echo "Removing package-owned files only: $SELF_DIR"
echo "Persistent data is retained at /var/local/ktm or /mnt/us/ktm."
